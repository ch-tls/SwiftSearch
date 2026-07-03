#pragma once

#include <QObject>
#include <QThread>
#include <QStringList>
#include <memory>
#include <mutex>
#include <vector>

#include "file_entry.h"

namespace swiftsearch {
class IndexingObserver;
}

class FileScanner;
class IndexDatabase;

/**
 * @brief 文件系统索引引擎，管理扫描线程与索引数据库。
 *
 * IndexEngine 负责协调 FileScanner 的后台扫描工作，
 * 将扫描结果写入 IndexDatabase，同时通过 Qt signal-slot
 * 和 IndexingObserver 接口通知上层。
 *
 * @note 扫描在独立 QThread 中执行，所有 public 方法线程安全。
 * @see FileScanner, IndexDatabase, IndexingObserver
 */
class IndexEngine : public QObject {
  Q_OBJECT

 public:
  explicit IndexEngine(QObject* parent = nullptr);
  ~IndexEngine() override;

  /**
   * @brief 初始化索引数据库。
   * @param db_path SQLite 数据库文件路径
   * @return true 成功，false 失败
   */
  bool Initialize(const QString& db_path);

  /** @brief 开始索引指定目录。 */
  void StartIndexing(const QStringList& paths);

  /** @brief 停止当前索引任务。 */
  void StopIndexing();

  /** @brief 暂停索引。 */
  void PauseIndexing();

  /** @brief 恢复索引。 */
  void ResumeIndexing();

  /** @brief 检查指定路径是否已索引。 */
  bool IsIndexed(const QString& path) const;

  /** @brief 获取已索引文件总数。 */
  int64_t TotalFilesIndexed() const;

  /** @brief 扫描线程是否正在运行。 */
  bool IsRunning() const;

  /** @brief 获取底层 IndexDatabase 指针。 */
  IndexDatabase* GetDatabase() const;

  /**
   * @brief 注册一个索引观察者。
   * @param observer 观察者（通过 shared_ptr 管理生命周期）
   */
  void AddObserver(std::shared_ptr<swiftsearch::IndexingObserver> observer);

  /**
   * @brief 注销一个索引观察者。
   * @param observer 要移除的观察者
   */
  void RemoveObserver(std::shared_ptr<swiftsearch::IndexingObserver> observer);

 signals:
  void ProgressUpdated(int files_indexed);
  void IndexingFinished();
  void IndexingError(const QString& message);

 private slots:
  void OnFileFound(const FileEntry& entry);

 private:
  void EnsureScannerThread();
  void NotifyProgress(int files_indexed);
  void NotifyFinished();
  void NotifyError(const QString& message);

  std::unique_ptr<IndexDatabase> index_database_;
  std::unique_ptr<FileScanner> file_scanner_;
  std::unique_ptr<QThread> scanner_thread_;
  bool is_initialized_ = false;

  mutable std::mutex observer_mutex_;
  std::vector<std::weak_ptr<swiftsearch::IndexingObserver>> observers_;
};
