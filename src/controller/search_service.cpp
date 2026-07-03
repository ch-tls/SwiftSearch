/**
 * @file search_service.cpp
 * @brief SearchService 实现：异步搜索流水线，通过 QtConcurrent 在线程池中执行。
 *
 * 搜索流程：
 * 1. QueryParser::Parse() 解析用户输入为 SearchQuery
 * 2. Search() 创建 QFutureWatcher，在 QtConcurrent::run 中异步执行 SearchSync
 * 3. ExecuteSearch() 从 IndexDatabase 获取候选项 → ResultRanker 评分 → StableSort → 截断
 * 4. 结果通过信号/观察者通知到 View 层
 *
 * @see SearchService, SearchQuery, SearchResult, QueryParser, ResultRanker
 */

#include "search_service.h"
#include "../model/index_engine.h"
#include "../model/index_database.h"
#include "../util/sort_util.h"
#include "../util/log_manager.h"
#include "query_parser.h"
#include "result_ranker.h"
#include "search_observer.h"

#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

SearchService::SearchService(IndexEngine* index_engine, QObject* parent)
    : QObject(parent), index_engine_(index_engine) {}

SearchService::~SearchService() {
  CancelSearch();
}

/** @brief 注册搜索观察者。使用 weak_ptr 避免循环引用。 */
void SearchService::AddObserver(std::shared_ptr<swiftsearch::SearchObserver> observer) {
  std::lock_guard<std::mutex> lock(observer_mutex_);
  observers_.push_back(observer);
}

/** @brief 移除指定搜索观察者，同时清理已失效的 weak_ptr。 */
void SearchService::RemoveObserver(std::shared_ptr<swiftsearch::SearchObserver> observer) {
  std::lock_guard<std::mutex> lock(observer_mutex_);
  observers_.erase(
      std::remove_if(observers_.begin(), observers_.end(),
                     [&](const std::weak_ptr<swiftsearch::SearchObserver>& wp) {
                       auto sp = wp.lock();
                       return !sp || sp == observer;
                     }),
      observers_.end());
}

/** @brief 将搜索结果通知所有注册的观察者。 */
void SearchService::NotifyResults(const QList<SearchResult>& results,
                                  const SearchQuery& query) {
  std::lock_guard<std::mutex> lock(observer_mutex_);
  std::vector<SearchResult> vec;
  vec.reserve(static_cast<size_t>(results.size()));
  for (const auto& r : results) {
    vec.push_back(r);
  }
  for (auto it = observers_.begin(); it != observers_.end();) {
    if (auto observer = it->lock()) {
      observer->OnSearchResultsReady(vec, query);
      ++it;
    } else {
      it = observers_.erase(it);
    }
  }
}

/** @brief 将搜索错误通知所有注册的观察者。 */
void SearchService::NotifyError(const QString& message) {
  std::lock_guard<std::mutex> lock(observer_mutex_);
  auto std_msg = message.toStdString();
  for (auto it = observers_.begin(); it != observers_.end();) {
    if (auto observer = it->lock()) {
      observer->OnSearchError(std_msg);
      ++it;
    } else {
      it = observers_.erase(it);
    }
  }
}

/**
 * @brief 异步执行搜索查询。
 *
 * 使用 QFutureWatcher 监听 QtConcurrent 任务完成，
 * 完成后通过信号 ResultsReady 和观察者通知结果。
 * watcher 在结束后自动 deleteLater。
 *
 * @param query 搜索查询参数
 */
void SearchService::Search(const SearchQuery& query) {
  last_query_ = query;
  SWIFT_LOG_DEBUG(QString("Async search: name='%1' path='%2'")
                      .arg(query.name_pattern, query.path_pattern));

  auto* watcher = new QFutureWatcher<QList<SearchResult>>(this);

  connect(watcher, &QFutureWatcher<QList<SearchResult>>::finished, this, [this, watcher]() {
    QList<SearchResult> results = watcher->result();
    emit ResultsReady(results);
    NotifyResults(results, last_query_);
    watcher->deleteLater();
  });

  QFuture<QList<SearchResult>> future = QtConcurrent::run([this, query]() {
    return ExecuteSearch(query);
  });

  watcher->setFuture(future);
}

/** @brief 取消正在执行的搜索。设置 cancelled_ 标志，ExecuteSearch 会在迭代中检测。 */
void SearchService::CancelSearch() {
  cancelled_ = true;
}

/** @brief 同步执行搜索，在当前线程直接返回结果。 */
QList<SearchResult> SearchService::SearchSync(const SearchQuery& query) {
  cancelled_ = false;
  last_query_ = query;
  return ExecuteSearch(query);
}

/**
 * @brief 搜索核心实现：数据库查询 → 候选过滤 → 评分排序 → 截断。
 *
 * 查询策略：
 * - 同时有 name 和 path 模式：分别查询后合并去重
 * - 仅 name/仅 path：对应单字段 LIKE 查询
 * - 仅有 size 范围：BETWEEN 查询
 * - 无任何条件：返回全部文件
 *
 * 后处理：
 * - ResultRanker::Score 根据匹配质量打分
 * - StableSort 保持同分记录的原始顺序
 * - 按 max_results 截断结果集
 *
 * @param query 搜索查询参数
 * @return 排序后的搜索结果列表
 */
QList<SearchResult> SearchService::ExecuteSearch(const SearchQuery& query) {
  cancelled_ = false;

  try {
    auto* database = index_engine_->GetDatabase();
    if (!database) {
      QString err = "Index database is not available";
      SWIFT_LOG_ERROR(err);
      QMetaObject::invokeMethod(this, [this, err]() {
        emit SearchError(err);
        NotifyError(err);
      }, Qt::QueuedConnection);
      return {};
    }

    QList<FileEntry> candidates;

    bool has_name = !query.name_pattern.isEmpty();
    bool has_path = !query.path_pattern.isEmpty();

    if (has_name && has_path) {
      QList<FileEntry> name_results = database->QueryByName(query.name_pattern);
      QList<FileEntry> path_results = database->QueryByPath(query.path_pattern);

      QSet<QString> seen;
      for (const auto& e : name_results) {
        seen.insert(e.path);
        candidates.append(e);
      }
      for (const auto& e : path_results) {
        if (!seen.contains(e.path)) {
          candidates.append(e);
        }
      }
    } else if (has_name) {
      candidates = database->QueryByName(query.name_pattern);
    } else if (has_path) {
      candidates = database->QueryByPath(query.path_pattern);
    } else if (query.min_size >= 0 || query.max_size >= 0) {
      int64_t min_s = (query.min_size >= 0) ? query.min_size : 0;
      int64_t max_s = (query.max_size >= 0) ? query.max_size : INT64_MAX;
      candidates = database->QueryBySize(min_s, max_s);
    } else {
      candidates = database->QueryAll();
    }

    QList<SearchResult> results;
    for (const auto& entry : candidates) {
      if (cancelled_) break;

      if (query.min_size >= 0 && entry.size < query.min_size) continue;
      if (query.max_size >= 0 && entry.size > query.max_size) continue;

      double score = ResultRanker::Score(entry, query.name_pattern, query.path_pattern);

      SearchResult result;
      result.file_entry = entry;
      result.relevance_score = score;
      results.append(result);
    }

    swiftsearch::util::StableSort(results, [](const SearchResult& a, const SearchResult& b) {
      return a.relevance_score > b.relevance_score;
    });

    if (static_cast<int>(results.size()) > query.max_results) {
      results.remove(query.max_results, results.size() - query.max_results);
    }

    SWIFT_LOG_DEBUG(QString("Search returned %1 results").arg(results.size()));
    return results;
  } catch (const std::exception& e) {
    QString err = QString("Search exception: %1").arg(e.what());
    SWIFT_LOG_ERROR(err);
    QMetaObject::invokeMethod(this, [this, err]() {
      emit SearchError(err);
      NotifyError(err);
    }, Qt::QueuedConnection);
    return {};
  }
}
