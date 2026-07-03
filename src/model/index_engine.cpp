/**
 * @file index_engine.cpp
 * @brief IndexEngine 实现：索引生命周期管理，协调 FileScanner 与 IndexDatabase。
 *
 * IndexEngine 是 Model 层的编排器：
 * - 创建并管理 FileScanner（在独立 QThread 中运行）和 IndexDatabase
 * - 通过信号槽连接将扫描结果写入数据库
 * - 实现观察者模式通知进度/完成/错误给上层
 *
 * @see IndexEngine, FileScanner, IndexDatabase, IndexingObserver
 */

#include "index_engine.h"
#include "file_scanner.h"
#include "index_database.h"
#include "indexing_observer.h"
#include "../util/log_manager.h"

#include <QFileInfo>

IndexEngine::IndexEngine(QObject* parent) : QObject(parent) {}

IndexEngine::~IndexEngine() {
  StopIndexing();
}

/**
 * @brief 初始化 IndexEngine：创建并打开 IndexDatabase。
 *
 * @param db_path SQLite 数据库文件路径
 * @return 是否初始化成功
 */
bool IndexEngine::Initialize(const QString& db_path) {
  SWIFT_LOG_INFO(QString("Initializing IndexEngine with db: %1").arg(db_path));
  try {
    index_database_ = std::make_unique<IndexDatabase>(db_path, this);
    QString db_error;
    if (!index_database_->Initialize(&db_error)) {
      QString msg = QString("Failed to initialize database: %1").arg(db_error);
      NotifyError(msg);
      emit IndexingError(msg);
      return false;
    }
    is_initialized_ = true;
    SWIFT_LOG_INFO("IndexEngine initialized successfully");
    return true;
  } catch (const std::exception& e) {
    SWIFT_LOG_ERROR(QString("IndexEngine::Initialize exception: %1").arg(e.what()));
    emit IndexingError(QString("Initialization exception: %1").arg(e.what()));
    return false;
  }
}

/** @brief 注册索引观察者。使用 weak_ptr 避免循环引用，自动清理已失效的观察者。 */
void IndexEngine::AddObserver(std::shared_ptr<swiftsearch::IndexingObserver> observer) {
  std::lock_guard<std::mutex> lock(observer_mutex_);
  observers_.push_back(observer);
}

/** @brief 移除指定观察者，同时清理所有已失效的 weak_ptr。 */
void IndexEngine::RemoveObserver(std::shared_ptr<swiftsearch::IndexingObserver> observer) {
  std::lock_guard<std::mutex> lock(observer_mutex_);
  observers_.erase(
      std::remove_if(observers_.begin(), observers_.end(),
                     [&](const std::weak_ptr<swiftsearch::IndexingObserver>& wp) {
                       auto sp = wp.lock();
                       return !sp || sp == observer;
                     }),
      observers_.end());
}

/** @brief 通知所有观察者索引进度，自动清理已失效的 weak_ptr。 */
void IndexEngine::NotifyProgress(int files_indexed) {
  std::lock_guard<std::mutex> lock(observer_mutex_);
  int64_t total = TotalFilesIndexed();
  for (auto it = observers_.begin(); it != observers_.end();) {
    if (auto observer = it->lock()) {
      observer->OnIndexingProgress(files_indexed, total);
      ++it;
    } else {
      it = observers_.erase(it);
    }
  }
}

/** @brief 通知所有观察者索引已完成。 */
void IndexEngine::NotifyFinished() {
  std::lock_guard<std::mutex> lock(observer_mutex_);
  int64_t total = TotalFilesIndexed();
  for (auto it = observers_.begin(); it != observers_.end();) {
    if (auto observer = it->lock()) {
      observer->OnIndexingFinished(total);
      ++it;
    } else {
      it = observers_.erase(it);
    }
  }
}

/** @brief 通知所有观察者索引发生错误。 */
void IndexEngine::NotifyError(const QString& message) {
  std::lock_guard<std::mutex> lock(observer_mutex_);
  auto std_msg = message.toStdString();
  for (auto it = observers_.begin(); it != observers_.end();) {
    if (auto observer = it->lock()) {
      observer->OnIndexingError(std_msg);
      ++it;
    } else {
      it = observers_.erase(it);
    }
  }
}

/**
 * @brief 按需创建 FileScanner 和 QThread，并建立信号槽连接。
 *
 * FileScanner 移动到独立 QThread 中执行，通过 QThread::started 触发 Start()。
 * 信号连接链：FileScanner::FileFound → OnFileFound → IndexDatabase::InsertFile
 *             FileScanner::ScanFinished → 清理线程 → NotifyFinished
 */
void IndexEngine::EnsureScannerThread() {
  if (scanner_thread_) return;

  file_scanner_ = std::make_unique<FileScanner>();
  scanner_thread_ = std::make_unique<QThread>(this);

  file_scanner_->moveToThread(scanner_thread_.get());

  connect(scanner_thread_.get(), &QThread::started,
          file_scanner_.get(), &FileScanner::Start);
  connect(file_scanner_.get(), &FileScanner::FileFound,
          this, &IndexEngine::OnFileFound);
  connect(file_scanner_.get(), &FileScanner::ProgressUpdated,
          this, [this](int count) {
            emit ProgressUpdated(count);
            NotifyProgress(count);
          });
  connect(file_scanner_.get(), &FileScanner::ScanFinished, this, [this]() {
    scanner_thread_->quit();
    scanner_thread_->wait();
    scanner_thread_.reset();
    file_scanner_.reset();
    emit IndexingFinished();
    NotifyFinished();
    SWIFT_LOG_INFO(QString("Indexing finished: %1 files").arg(TotalFilesIndexed()));
  });
  connect(file_scanner_.get(), &FileScanner::ScanError,
          this, [this](const QString& msg) {
            emit IndexingError(msg);
            NotifyError(msg);
          });
}

/**
 * @brief 启动索引流程。
 *
 * 1. 检查是否已初始化、是否已有索引在进行中
 * 2. 清空旧索引数据 (ClearAll)
 * 3. 创建扫描线程并启动
 *
 * @param paths 待索引的根目录路径列表
 */
void IndexEngine::StartIndexing(const QStringList& paths) {
  if (!is_initialized_) {
    emit IndexingError("IndexEngine not initialized");
    NotifyError("IndexEngine not initialized");
    return;
  }
  if (IsRunning()) {
    emit IndexingError("Indexing already in progress");
    NotifyError("Indexing already in progress");
    return;
  }

  SWIFT_LOG_INFO(QString("Starting indexing for %1 paths").arg(paths.size()));

  try {
    index_database_->ClearAll();
  } catch (const std::exception& e) {
    SWIFT_LOG_ERROR(QString("StartIndexing::ClearAll exception: %1").arg(e.what()));
    emit IndexingError(QString("Failed to clear index: %1").arg(e.what()));
    return;
  }

  EnsureScannerThread();

  file_scanner_->SetRootPaths(paths);
  scanner_thread_->start();
}

/** @brief 停止索引并等待扫描线程退出。 */
void IndexEngine::StopIndexing() {
  SWIFT_LOG_DEBUG("StopIndexing called");
  if (file_scanner_) {
    file_scanner_->Stop();
  }
  if (scanner_thread_) {
    if (scanner_thread_->isRunning()) {
      scanner_thread_->quit();
      scanner_thread_->wait();
    }
    scanner_thread_.reset();
    file_scanner_.reset();
  }
}

/** @brief 暂停正在进行的索引扫描。 */
void IndexEngine::PauseIndexing() {
  SWIFT_LOG_DEBUG("PauseIndexing called");
  if (file_scanner_) {
    file_scanner_->Pause();
  }
}

/** @brief 恢复暂停的索引扫描。 */
void IndexEngine::ResumeIndexing() {
  SWIFT_LOG_DEBUG("ResumeIndexing called");
  if (file_scanner_) {
    file_scanner_->Resume();
  }
}

/** @brief 检查指定路径是否已被索引。 */
bool IndexEngine::IsIndexed(const QString& path) const {
  if (!index_database_) return false;
  QFileInfo fi(path);
  return !index_database_->QueryByPath(fi.absoluteFilePath()).isEmpty();
}

/** @brief 获取已索引文件总数。 */
int64_t IndexEngine::TotalFilesIndexed() const {
  if (!index_database_) return 0;
  return index_database_->TotalCount();
}

/** @brief 是否正在运行扫描线程。 */
bool IndexEngine::IsRunning() const {
  return scanner_thread_ && scanner_thread_->isRunning();
}

/** @brief 获取底层 IndexDatabase 指针。调用方不应接管所有权。 */
IndexDatabase* IndexEngine::GetDatabase() const {
  return index_database_.get();
}

/**
 * @brief 文件发现槽：将扫描到的文件条目写入数据库。
 *
 * 由 FileScanner::FileFound 信号触发，在 GUI 线程中执行。
 */
void IndexEngine::OnFileFound(const FileEntry& entry) {
  try {
    if (index_database_) {
      index_database_->InsertFile(entry);
    }
  } catch (const std::exception& e) {
    SWIFT_LOG_WARNING(QString("IndexEngine::OnFileFound - failed to insert %1: %2")
                          .arg(entry.path, e.what()));
  }
}
