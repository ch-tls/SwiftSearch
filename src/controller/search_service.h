#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <memory>
#include <mutex>
#include <vector>

#include "search_types.h"

namespace swiftsearch {
class SearchObserver;
}

class IndexEngine;

/**
 * @brief 搜索服务，封装查询执行与结果排序。
 *
 * 支持异步（Search）和同步（SearchSync）两种搜索模式。
 * 异步搜索在线程池中执行，通过 signal-slot 和 SearchObserver 接口
 * 通知上层。
 *
 * @note SearchSync 在调用线程中执行，适用于单元测试。
 * @see SearchQuery, SearchResult, SearchObserver
 */
class SearchService : public QObject {
  Q_OBJECT

 public:
  /**
   * @param index_engine 索引引擎指针（非拥有）
   * @param parent 父 QObject
   */
  explicit SearchService(IndexEngine* index_engine, QObject* parent = nullptr);
  ~SearchService() override;

  /** @brief 异步执行搜索（在线程池中运行）。 */
  void Search(const SearchQuery& query);

  /** @brief 取消当前搜索。 */
  void CancelSearch();

  /** @brief 同步执行搜索（在当前线程中运行）。 */
  QList<SearchResult> SearchSync(const SearchQuery& query);

  /**
   * @brief 注册搜索观察者。
   * @param observer 观察者（shared_ptr 管理生命周期）
   */
  void AddObserver(std::shared_ptr<swiftsearch::SearchObserver> observer);

  /**
   * @brief 注销搜索观察者。
   * @param observer 要移除的观察者
   */
  void RemoveObserver(std::shared_ptr<swiftsearch::SearchObserver> observer);

 signals:
  void ResultsReady(const QList<SearchResult>& results);
  void SearchError(const QString& message);

 private:
  QList<SearchResult> ExecuteSearch(const SearchQuery& query);
  void NotifyResults(const QList<SearchResult>& results, const SearchQuery& query);
  void NotifyError(const QString& message);

  IndexEngine* index_engine_;
  bool cancelled_ = false;

  mutable std::mutex observer_mutex_;
  std::vector<std::weak_ptr<swiftsearch::SearchObserver>> observers_;
  SearchQuery last_query_;
};
