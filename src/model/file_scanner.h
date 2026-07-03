/**
 * @file file_scanner.h
 * @brief 文件系统扫描器，递归遍历目录并发射文件发现信号。
 *
 * FileScanner 在独立 QThread 中运行，支持暂停/停止控制。
 * Windows 上可选用 USN Journal 快速扫描，Linux 上支持 inotify 实时监听。
 *
 * @see FileEntry, IndexEngine, IndexDatabase
 */
#pragma once

#include <QObject>
#include <QStringList>
#include <QThread>
#include <QAtomicInt>

#include "file_entry.h"

#ifdef Q_OS_LINUX
#include <unordered_map>
#include <vector>
#include "platform_handles.h"
#endif

/**
 * @brief 文件系统扫描器，支持递归目录遍历和 OS 级文件变更监听。
 *
 * 扫描流程：
 * 1. 调用 Set* 方法配置过滤参数
 * 2. 调用 Start() 启动扫描（需在 QThread 中运行）
 * 3. 通过信号接收 FileFound / ProgressUpdated / ScanFinished
 *
 * 线程安全：所有 public 方法可在任意线程调用，QAtomicInt 防护。
 */
class FileScanner : public QObject {
  Q_OBJECT

 public:
  /** @brief 构造扫描器。 */
  explicit FileScanner(QObject* parent = nullptr);

  /** @brief 析构时自动停止扫描并释放 OS 资源。 */
  ~FileScanner() override;

  /** @brief 设置待扫描的根目录列表。 */
  void SetRootPaths(const QStringList& paths);

  /** @brief 设置文件名包含模式（通配符）。空列表表示全部包含。 */
  void SetIncludePatterns(const QStringList& patterns);

  /** @brief 设置文件名排除模式（通配符）。 */
  void SetExcludePatterns(const QStringList& patterns);

  /** @brief 设置最大扫描深度，-1 表示无限制。 */
  void SetMaxDepth(int depth);

  /** @brief 设置最小文件大小阈值（字节），-1 表示无限制。 */
  void SetMinSize(int64_t bytes);

  /** @brief 设置最大文件大小阈值（字节），-1 表示无限制。 */
  void SetMaxSize(int64_t bytes);

 public slots:
  /** @brief 开始扫描。遍历 root_paths_ 并递归进入子目录。 */
  void Start();

  /** @brief 停止扫描，设置 running_ 标志为 0。 */
  void Stop();

  /** @brief 暂停扫描，扫描循环将阻塞等待。 */
  void Pause();

  /** @brief 恢复暂停的扫描。 */
  void Resume();

#ifdef Q_OS_LINUX
  /** @brief 启动 inotify 文件变更监听。 */
  void WatchChanges(const QStringList& paths);

  /** @brief 停止 inotify 监听并释放所有 watch 描述符。 */
  void StopWatching();
#endif

 signals:
  /** @brief 每扫描 100 个文件时发射一次进度。 */
  void ProgressUpdated(int files_scanned);

  /** @brief 发现符合过滤条件的文件时发射。 */
  void FileFound(const FileEntry& entry);

  /** @brief 扫描完成时发射。 */
  void ScanFinished();

  /** @brief 扫描过程中发生错误时发射。 */
  void ScanError(const QString& message);

#ifdef Q_OS_LINUX
  /** @brief inotify 检测到文件修改/新建时发射。 */
  void FileChanged(const FileEntry& entry);

  /** @brief inotify 检测到文件删除/移走时发射。 */
  void FileRemoved(const QString& path);

  /** @brief inotify 监听出错时发射。 */
  void WatchError(const QString& message);
#endif

 private:
  /** @brief 递归扫描单个目录及其子目录。 */
  void ScanDirectory(const QString& dir_path, int current_depth);

  /** @brief 检查文件路径是否匹配包含/排除模式。 */
  bool ShouldInclude(const QString& file_path) const;

#ifdef Q_OS_WIN
  /** @brief 使用 USN Journal 快速获取文件变更。MSVC only，MinGW 回退到目录遍历。 */
  void StartUsnScan();
#endif

#ifdef Q_OS_LINUX
  /** @brief 为指定路径列表启动 inotify 监听。 */
  void StartInotifyWatch(const QStringList& paths);

  /** @brief 递归为目录及其所有子目录添加 inotify watch。 */
  void AddWatchRecursive(const QString& dir_path);

  /** @brief 解析 inotify 事件缓冲区并发射相应信号。 */
  void ProcessInotifyEvents(const char* buffer, ssize_t length);
#endif

  QStringList root_paths_;         ///< 待扫描的根目录列表
  QStringList include_patterns_;   ///< 文件名包含模式（通配符）
  QStringList exclude_patterns_;   ///< 文件名排除模式（通配符）
  int max_depth_ = -1;             ///< 最大扫描深度，-1 无限制
  int64_t min_size_ = -1;          ///< 最小文件大小阈值，-1 无限制
  int64_t max_size_ = -1;          ///< 最大文件大小阈值，-1 无限制

  QAtomicInt running_{0};  ///< 扫描运行标志，0=停止，1=运行
  QAtomicInt paused_{0};   ///< 暂停标志，0=运行，1=暂停

#ifdef Q_OS_WIN
  bool use_usn_journal_ =
#ifdef __MINGW32__
      false;  ///< MinGW 环境下不支持 USN，强制回退
#else
      true;   ///< MSVC 环境下默认启用 USN Journal
#endif
#endif

#ifdef Q_OS_LINUX
  InotifyFd inotify_fd_;                               ///< inotify 文件描述符（RAII 封装）
  std::unordered_map<int, QString> watch_path_map_;   ///< watch 描述符 → 目录路径 映射
  std::vector<InotifyWatch> watches_;                  ///< 所有注册的 inotify watch（RAII）
  bool watching_changes_ = false;                      ///< 是否正在监听文件变更
#endif
};
