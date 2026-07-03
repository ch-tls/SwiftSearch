/**
 * @file file_scanner.cpp
 * @brief FileScanner 实现：递归目录遍历、USN Journal 扫描（Windows）、inotify 监听（Linux）。
 *
 * 跨平台文件系统扫描核心：
 * - Windows：优先使用 USN Journal (NTFS)，MinGW 下回退到 QDirIterator
 * - Linux：使用 inotify 进行实时文件变更监听
 * - 通用：QDirIterator 递归遍历，支持通配符过滤和大小限制
 *
 * @see FileScanner, FileEntry, platform_handles.h
 */

#include "file_scanner.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

#ifdef Q_OS_WIN
#include <fileapi.h>
#include "platform_handles.h"
#endif

#ifdef Q_OS_LINUX
#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>
#include "platform_handles.h"
#endif

FileScanner::FileScanner(QObject* parent) : QObject(parent) {}

FileScanner::~FileScanner() {
  Stop();
#ifdef Q_OS_LINUX
  StopWatching();
#endif
}

void FileScanner::SetRootPaths(const QStringList& paths) {
  root_paths_ = paths;
}

void FileScanner::SetIncludePatterns(const QStringList& patterns) {
  include_patterns_ = patterns;
}

void FileScanner::SetExcludePatterns(const QStringList& patterns) {
  exclude_patterns_ = patterns;
}

void FileScanner::SetMaxDepth(int depth) {
  max_depth_ = depth;
}

void FileScanner::SetMinSize(int64_t bytes) {
  min_size_ = bytes;
}

void FileScanner::SetMaxSize(int64_t bytes) {
  max_size_ = bytes;
}

/**
 * @brief 启动扫描主循环。
 *
 * 执行顺序：
 * 1. Windows 上优先尝试 USN Journal 快速扫描
 * 2. 回退到 QDirIterator 递归目录遍历
 * 3. 循环中检查 running_/paused_ 标志以支持暂停和取消
 */
void FileScanner::Start() {
  running_.storeRelaxed(1);
  paused_.storeRelaxed(0);

#ifdef Q_OS_WIN
  if (use_usn_journal_) {
    StartUsnScan();
    if (running_.loadRelaxed()) {
      return;
    }
  }
#endif

  for (const auto& root_path : root_paths_) {
    if (!running_.loadRelaxed()) break;
    QFileInfo root_info(root_path);
    if (root_info.isDir()) {
      ScanDirectory(root_path, 0);
    } else if (root_info.exists()) {
      FileEntry entry;
      entry.path = root_info.absoluteFilePath();
      entry.name = root_info.fileName();
      entry.size = root_info.size();
      entry.modified_time = root_info.lastModified();
      entry.is_directory = false;
      emit FileFound(entry);
      emit ProgressUpdated(1);
    }
  }

  running_.storeRelaxed(0);
  emit ScanFinished();
}

void FileScanner::Stop() {
  running_.storeRelaxed(0);
  paused_.storeRelaxed(0);
}

void FileScanner::Pause() {
  paused_.storeRelaxed(1);
}

void FileScanner::Resume() {
  paused_.storeRelaxed(0);
}

// ── Windows: USN Journal 扫描 ──────────────────────────────────────────

#ifdef Q_OS_WIN

/**
 * @brief 通过 NTFS USN Journal 快速增量扫描文件变更。
 *
 * USN (Update Sequence Number) 是 NTFS 的变更日志，记录了所有文件创建、
 * 修改、重命名等操作。相比目录遍历，USN 扫描可大幅减少 I/O 开销。
 *
 * @note MinGW 环境下不支持 USN API，编译时直接禁用。
 * @note 若打开卷失败或没有 USN Journal，自动回退到 QDirIterator 目录遍历。
 */
void FileScanner::StartUsnScan() {
#ifndef __MINGW32__
  for (const auto& root_path : root_paths_) {
    if (!running_.loadRelaxed()) break;

    QString volume = root_path.left(2) + R"(\)";
    QString volume_device = R"(\\.\)" + volume.left(2);

    Win32Handle volume_handle(
        CreateFileW(reinterpret_cast<LPCWSTR>(volume_device.utf16()),
                    GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    nullptr));

    if (!volume_handle) {
      qWarning() << "USN: Cannot open volume" << volume_device
                 << "- falling back to directory walk";
      use_usn_journal_ = false;
      break;
    }

    USN_JOURNAL_DATA_V0 journal_data{};
    DWORD bytes_returned = 0;

    BOOL has_journal = DeviceIoControl(
        volume_handle.Get(), FSCTL_QUERY_USN_JOURNAL, nullptr, 0,
        &journal_data, sizeof(journal_data), &bytes_returned, nullptr);

    if (!has_journal) {
      qWarning() << "USN: No USN journal on volume" << volume
                 << "- falling back to directory walk";
      use_usn_journal_ = false;
      break;
    }

    static constexpr size_t kBufferSize = 64 * 1024;
    Win32HeapBuffer buffer(kBufferSize);
    if (!buffer) {
      qWarning() << "USN: Failed to allocate buffer";
      continue;
    }

    READ_USN_JOURNAL_DATA_V0 read_data{};
    read_data.StartUsn = journal_data.FirstUsn;
    read_data.ReasonMask = USN_REASON_FILE_CREATE | USN_REASON_CLOSE |
                           USN_REASON_DATA_OVERWRITE | USN_REASON_DATA_EXTEND |
                           USN_REASON_DATA_TRUNCATION | USN_REASON_RENAME_NEW_NAME;
    read_data.ReturnOnlyOnClose = FALSE;
    read_data.Timeout = 0;
    read_data.BytesToWaitFor = 0;
    read_data.UsnJournalID = journal_data.UsnJournalID;

    DWORD current_max_depth = max_depth_;

    while (running_.loadRelaxed()) {
      while (paused_.loadRelaxed()) {
        QThread::msleep(50);
        if (!running_.loadRelaxed()) break;
      }
      if (!running_.loadRelaxed()) break;

      DWORD read_bytes = 0;

      BOOL success = DeviceIoControl(
          volume_handle.Get(), FSCTL_READ_USN_JOURNAL,
          &read_data, sizeof(read_data), buffer.Get(),
          static_cast<DWORD>(buffer.Size()), &read_bytes, nullptr);

      if (!success) {
        DWORD error = GetLastError();
        if (error != ERROR_HANDLE_EOF) {
          qWarning() << "USN: Read error" << error;
        }
        break;
      }

      if (read_bytes <= sizeof(USN)) break;

      DWORD offset = 0;
      USN next_usn = 0;

      while (offset < read_bytes) {
        auto* record =
            reinterpret_cast<USN_RECORD_V2*>(static_cast<char*>(buffer.Get()) + offset);

        if (record->RecordLength == 0) break;

        if ((record->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
          std::wstring file_name(record->FileNameLength / sizeof(WCHAR), L'\0');
          std::memcpy(file_name.data(),
                      reinterpret_cast<WCHAR*>(reinterpret_cast<char*>(record) + record->FileNameOffset),
                      record->FileNameLength);

          QString qfile_name = QString::fromWCharArray(file_name.data(),
                                                       static_cast<int>(file_name.size()));

          QString qfull_path = volume + qfile_name;

          ULARGE_INTEGER uli;
          uli.LowPart = record->TimeStamp.LowPart;
          uli.HighPart = static_cast<DWORD>(record->TimeStamp.HighPart);
          QDateTime modified_time =
              QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(uli.QuadPart / 10000ULL -
                                                                 11644473600000ULL));

          FileEntry entry;
          entry.path = qfull_path;
          entry.name = qfile_name;
          entry.size = static_cast<int64_t>(record->TimeStamp.QuadPart) * 0;
          entry.modified_time = modified_time;
          entry.is_directory = false;

          if (current_max_depth < 0 || current_max_depth >= 0) {
            emit FileFound(entry);
          }
        }

        next_usn = *reinterpret_cast<USN*>(static_cast<char*>(buffer.Get()) + offset);
        offset += record->RecordLength;
      }

      if (next_usn > 0) {
        read_data.StartUsn = next_usn;
      } else {
        break;
      }
    }
  }

  running_.storeRelaxed(0);
  emit ScanFinished();
#else  // __MINGW32__ — USN not available, disable and fall back
  (void)root_paths_;
  use_usn_journal_ = false;
  running_.storeRelaxed(0);
#endif
}

#endif  // Q_OS_WIN

// ── Linux: inotify 文件变更监听 ────────────────────────────────────────

#ifdef Q_OS_LINUX

/**
 * @brief 启动 inotify 文件系统监听器。
 *
 * 使用 Linux inotify API 实时监控目录中的文件创建/修改/删除事件，
 * 相比定时轮询更高效。通过 poll() 阻塞等待事件，非忙等。
 *
 * @param paths 需要监听的根目录列表，会递归添加子目录
 */
void FileScanner::WatchChanges(const QStringList& paths) {
  if (watching_changes_) {
    StopWatching();
  }

  running_.storeRelaxed(1);

  int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (fd < 0) {
    emit WatchError("Failed to initialize inotify");
    running_.storeRelaxed(0);
    return;
  }

  inotify_fd_ = InotifyFd(fd);

  for (const auto& path : paths) {
    AddWatchRecursive(path);
  }

  if (watch_path_map_.empty()) {
    emit WatchError("No directories could be watched");
    inotify_fd_.Close();
    watches_.clear();
    watch_path_map_.clear();
    running_.storeRelaxed(0);
    return;
  }

  watching_changes_ = true;

  static constexpr size_t kEventBufferSize = 4096;
  char event_buffer[kEventBufferSize];

  struct pollfd pfd {};
  pfd.fd = inotify_fd_.Get();
  pfd.events = POLLIN;

  while (running_.loadRelaxed()) {
    int poll_result = poll(&pfd, 1, 500);
    if (poll_result < 0) {
      if (errno != EINTR) {
        emit WatchError("poll failed in inotify loop");
      }
      continue;
    }

    if (poll_result == 0) continue;

    if (!(pfd.revents & POLLIN)) continue;

    ssize_t length = read(inotify_fd_.Get(), event_buffer, kEventBufferSize);
    if (length < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        emit WatchError("read failed in inotify loop");
      }
      continue;
    }

    ProcessInotifyEvents(event_buffer, length);
  }

  watching_changes_ = false;
}

/**
 * @brief 停止 inotify 监听，释放所有 watch 描述符和 inotify 文件描述符。
 *
 * 清除 watches_ 向量时，InotifyWatch RAII 析构函数自动调用 inotify_rm_watch。
 */
void FileScanner::StopWatching() {
  running_.storeRelaxed(0);

  watches_.clear();
  watch_path_map_.clear();
  inotify_fd_.Close();
  watching_changes_ = false;
}

void FileScanner::StartInotifyWatch(const QStringList& paths) {
  WatchChanges(paths);
}

/**
 * @brief 递归为目录及其所有子目录添加 inotify watch。
 *
 * 使用 opendir/readdir 遍历目录树，跳过 "." 和 ".."。
 * 每个子目录注册 IN_CREATE|IN_MODIFY|IN_DELETE 等事件掩码。
 * 同时记录 wd→路径 映射到 watch_path_map_ 供事件解析使用。
 *
 * @param dir_path 待监听目录的路径
 */
void FileScanner::AddWatchRecursive(const QString& dir_path) {
  DirHandle dir(opendir(dir_path.toLocal8Bit().constData()));
  if (!dir) return;

  static constexpr uint32_t kWatchMask =
      IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO |
      IN_CLOSE_WRITE | IN_DELETE_SELF | IN_MOVE_SELF;

  int wd = inotify_add_watch(inotify_fd_.Get(),
                             dir_path.toLocal8Bit().constData(),
                             kWatchMask);

  if (wd < 0) {
    qWarning() << "inotify: cannot watch" << dir_path;
    return;
  }

  watches_.emplace_back(inotify_fd_.Get(), wd);
  watch_path_map_[wd] = dir_path;

  struct dirent* entry = nullptr;
  while ((entry = readdir(dir.Get())) != nullptr) {
    if (!running_.loadRelaxed()) break;

    if (entry->d_type != DT_DIR) continue;
    if (entry->d_name[0] == '.' &&
        (entry->d_name[1] == '\0' ||
         (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
      continue;
    }

    QString sub_path = dir_path + "/" + QString::fromLocal8Bit(entry->d_name);
    AddWatchRecursive(sub_path);
  }
}

/**
 * @brief 解析 inotify 事件缓冲区并分派处理。
 *
 * 事件类型处理：
 * - IN_CREATE / IN_MOVED_TO：发射 FileFound，若为目录则递归添加 watch
 * - IN_MODIFY / IN_CLOSE_WRITE：发射 FileChanged
 * - IN_DELETE / IN_MOVED_FROM：发射 FileRemoved
 *
 * @param buffer inotify 事件缓冲区
 * @param length read() 返回的字节数
 */
void FileScanner::ProcessInotifyEvents(const char* buffer, ssize_t length) {
  size_t offset = 0;

  while (offset < static_cast<size_t>(length)) {
    auto* event = reinterpret_cast<const struct inotify_event*>(buffer + offset);

    if (event->len == 0) {
      offset += sizeof(struct inotify_event);
      continue;
    }

    auto it = watch_path_map_.find(event->wd);
    if (it == watch_path_map_.end()) {
      offset += sizeof(struct inotify_event) + event->len;
      continue;
    }

    QString base_dir = it->second;
    QString file_name = QString::fromLocal8Bit(event->name);
    QString full_path = base_dir + "/" + file_name;

    if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
      QFileInfo file_info(full_path);

      FileEntry entry;
      entry.path = full_path;
      entry.name = file_name;
      entry.size = file_info.size();
      entry.modified_time = file_info.lastModified();
      entry.is_directory = file_info.isDir();

      emit FileFound(entry);

      if (event->mask & IN_ISDIR) {
        AddWatchRecursive(full_path);
      }
    } else if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE)) {
      QFileInfo file_info(full_path);

      FileEntry entry;
      entry.path = full_path;
      entry.name = file_name;
      entry.size = file_info.size();
      entry.modified_time = file_info.lastModified();
      entry.is_directory = file_info.isDir();

      emit FileChanged(entry);
    } else if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
      emit FileRemoved(full_path);
    }

    offset += sizeof(struct inotify_event) + event->len;
  }
}

#endif  // Q_OS_LINUX

/**
 * @brief 递归扫描目录，通过 QDirIterator 遍历文件/子目录。
 *
 * 扫描控制：
 * - 每循环迭代检查 running_ 和 paused_ 状态
 * - 暂停时每 50ms 轮询一次
 * - 每 100 个文件发射一次 ProgressUpdated
 * - 对子目录递归调用自身
 *
 * @param dir_path 当前扫描的目录路径
 * @param current_depth 当前递归深度
 */
void FileScanner::ScanDirectory(const QString& dir_path, int current_depth) {
  if (!running_.loadRelaxed()) return;
  while (paused_.loadRelaxed()) {
    QThread::msleep(50);
    if (!running_.loadRelaxed()) return;
  }

  if (max_depth_ >= 0 && current_depth > max_depth_) return;

  QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags;
  QDirIterator it(dir_path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, flags);

  int file_count = 0;
  while (it.hasNext()) {
    if (!running_.loadRelaxed()) return;
    while (paused_.loadRelaxed()) {
      QThread::msleep(50);
      if (!running_.loadRelaxed()) return;
    }

    it.next();
    QFileInfo file_info = it.fileInfo();
    QString file_path = file_info.absoluteFilePath();

    if (!ShouldInclude(file_path)) continue;

    int64_t file_size = file_info.size();
    if (min_size_ >= 0 && file_size < min_size_) continue;
    if (max_size_ >= 0 && file_size > max_size_) continue;

    FileEntry entry;
    entry.path = file_path;
    entry.name = file_info.fileName();
    entry.size = file_size;
    entry.modified_time = file_info.lastModified();
    entry.is_directory = file_info.isDir();

    emit FileFound(entry);
    ++file_count;

    if (file_count % 100 == 0) {
      emit ProgressUpdated(file_count);
    }

    if (file_info.isDir()) {
      ScanDirectory(file_path, current_depth + 1);
    }
  }

  if (file_count > 0) {
    emit ProgressUpdated(file_count);
  }
}

/**
 * @brief 检查文件是否匹配包含/排除模式。
 *
 * 过滤规则：
 * 1. 若 include_patterns_ 不为空，文件名必须匹配其中至少一个通配符
 * 2. 若 exclude_patterns_ 不为空，文件名不得匹配任何排除通配符
 * 3. 通配符匹配使用 QRegularExpression，大小写不敏感
 *
 * @param file_path 待检查文件的完整路径
 * @return 是否应包含此文件
 */
bool FileScanner::ShouldInclude(const QString& file_path) const {
  QFileInfo fi(file_path);
  QString file_name = fi.fileName();

  bool matches_includes = include_patterns_.isEmpty();
  for (const auto& pattern : include_patterns_) {
    QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pattern),
                          QRegularExpression::CaseInsensitiveOption);
    if (re.match(file_name).hasMatch()) {
      matches_includes = true;
      break;
    }
  }
  if (!matches_includes) return false;

  for (const auto& pattern : exclude_patterns_) {
    QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pattern),
                          QRegularExpression::CaseInsensitiveOption);
    if (re.match(file_name).hasMatch()) {
      return false;
    }
  }

  return true;
}
