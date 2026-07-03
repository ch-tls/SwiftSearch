/**
 * @file platform_handles.h
 * @brief 平台原生资源的 RAII 封装类。
 *
 * 为 Windows 和 Linux 的文件系统句柄提供自动生命周期管理：
 * - Windows: Win32Handle (HANDLE), Win32FindHandle (FindClose), Win32HeapBuffer (HeapFree)
 * - Linux: InotifyFd (inotify 文件描述符), InotifyWatch (inotify 监视), DirHandle (DIR*)
 *
 * 全部支持移动语义，禁止拷贝。
 *
 * @see FileScanner
 */
#pragma once

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef Q_OS_LINUX
#include <dirent.h>
#include <sys/inotify.h>
#include <unistd.h>
#endif

#ifdef Q_OS_WIN

/**
 * @brief Windows HANDLE 的 RAII 封装，析构时自动调用 CloseHandle。
 */
class Win32Handle final {
 public:
  Win32Handle() = default;

  explicit Win32Handle(HANDLE handle) noexcept : handle_(handle) {}

  ~Win32Handle() { Close(); }

  Win32Handle(const Win32Handle&) = delete;
  Win32Handle& operator=(const Win32Handle&) = delete;

  Win32Handle(Win32Handle&& other) noexcept : handle_(other.handle_) {
    other.handle_ = INVALID_HANDLE_VALUE;
  }

  Win32Handle& operator=(Win32Handle&& other) noexcept {
    if (this != &other) {
      Close();
      handle_ = other.handle_;
      other.handle_ = INVALID_HANDLE_VALUE;
    }
    return *this;
  }

  bool IsValid() const noexcept {
    return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr;
  }

  HANDLE Get() const noexcept { return handle_; }

  HANDLE* ReleaseAndGetAddressOf() noexcept {
    Close();
    return &handle_;
  }

  void Close() noexcept {
    if (IsValid()) {
      CloseHandle(handle_);
      handle_ = INVALID_HANDLE_VALUE;
    }
  }

  explicit operator bool() const noexcept { return IsValid(); }

 private:
  HANDLE handle_ = INVALID_HANDLE_VALUE;
};

/**
 * @brief Windows FindFirstFile/FindNextFile 搜索句柄的 RAII 封装，析构时自动调用 FindClose。
 */
class Win32FindHandle final {
 public:
  Win32FindHandle() = default;

  explicit Win32FindHandle(HANDLE handle) noexcept : handle_(handle) {}

  ~Win32FindHandle() { Close(); }

  Win32FindHandle(const Win32FindHandle&) = delete;
  Win32FindHandle& operator=(const Win32FindHandle&) = delete;

  Win32FindHandle(Win32FindHandle&& other) noexcept : handle_(other.handle_) {
    other.handle_ = INVALID_HANDLE_VALUE;
  }

  Win32FindHandle& operator=(Win32FindHandle&& other) noexcept {
    if (this != &other) {
      Close();
      handle_ = other.handle_;
      other.handle_ = INVALID_HANDLE_VALUE;
    }
    return *this;
  }

  bool IsValid() const noexcept {
    return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr;
  }

  HANDLE Get() const noexcept { return handle_; }

  void Close() noexcept {
    if (IsValid()) {
      FindClose(handle_);
      handle_ = INVALID_HANDLE_VALUE;
    }
  }

  explicit operator bool() const noexcept { return IsValid(); }

 private:
  HANDLE handle_ = INVALID_HANDLE_VALUE;
};

/**
 * @brief Windows 堆内存缓冲区的 RAII 封装，析构时自动调用 HeapFree。
 */
class Win32HeapBuffer final {
 public:
  Win32HeapBuffer() = default;

  explicit Win32HeapBuffer(size_t size_bytes) {
    buffer_ = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size_bytes);
    size_ = buffer_ ? size_bytes : 0;
  }

  ~Win32HeapBuffer() { Free(); }

  Win32HeapBuffer(const Win32HeapBuffer&) = delete;
  Win32HeapBuffer& operator=(const Win32HeapBuffer&) = delete;

  Win32HeapBuffer(Win32HeapBuffer&& other) noexcept
      : buffer_(other.buffer_), size_(other.size_) {
    other.buffer_ = nullptr;
    other.size_ = 0;
  }

  Win32HeapBuffer& operator=(Win32HeapBuffer&& other) noexcept {
    if (this != &other) {
      Free();
      buffer_ = other.buffer_;
      size_ = other.size_;
      other.buffer_ = nullptr;
      other.size_ = 0;
    }
    return *this;
  }

  void* Get() const noexcept { return buffer_; }
  size_t Size() const noexcept { return size_; }
  explicit operator bool() const noexcept { return buffer_ != nullptr; }

  void Free() noexcept {
    if (buffer_) {
      HeapFree(GetProcessHeap(), 0, buffer_);
      buffer_ = nullptr;
      size_ = 0;
    }
  }

 private:
  void* buffer_ = nullptr;
  size_t size_ = 0;
};

#endif  // Q_OS_WIN

#ifdef Q_OS_LINUX

/**
 * @brief Linux inotify 文件描述符的 RAII 封装，析构时自动调用 close。
 */
class InotifyFd final {
 public:
  InotifyFd() = default;

  explicit InotifyFd(int fd) noexcept : fd_(fd) {}

  ~InotifyFd() { Close(); }

  InotifyFd(const InotifyFd&) = delete;
  InotifyFd& operator=(const InotifyFd&) = delete;

  InotifyFd(InotifyFd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

  InotifyFd& operator=(InotifyFd&& other) noexcept {
    if (this != &other) {
      Close();
      fd_ = other.fd_;
      other.fd_ = -1;
    }
    return *this;
  }

  bool IsValid() const noexcept { return fd_ >= 0; }
  int Get() const noexcept { return fd_; }
  explicit operator bool() const noexcept { return IsValid(); }

  void Close() noexcept {
    if (IsValid()) {
      close(fd_);
      fd_ = -1;
    }
  }

 private:
  int fd_ = -1;
};

/**
 * @brief Linux inotify 监视描述符的 RAII 封装。
 *
 * 同时持有 inotify 文件描述符和 watch 描述符，析构时自动调用
 * inotify_rm_watch 移除监视，防止文件描述符泄漏。
 */
class InotifyWatch final {
 public:
  InotifyWatch() = default;

  InotifyWatch(int inotify_fd, int watch_descriptor) noexcept
      : inotify_fd_(inotify_fd), wd_(watch_descriptor) {}

  ~InotifyWatch() { Remove(); }

  InotifyWatch(const InotifyWatch&) = delete;
  InotifyWatch& operator=(const InotifyWatch&) = delete;

  InotifyWatch(InotifyWatch&& other) noexcept
      : inotify_fd_(other.inotify_fd_), wd_(other.wd_) {
    other.inotify_fd_ = -1;
    other.wd_ = -1;
  }

  InotifyWatch& operator=(InotifyWatch&& other) noexcept {
    if (this != &other) {
      Remove();
      inotify_fd_ = other.inotify_fd_;
      wd_ = other.wd_;
      other.inotify_fd_ = -1;
      other.wd_ = -1;
    }
    return *this;
  }

  bool IsValid() const noexcept { return inotify_fd_ >= 0 && wd_ >= 0; }
  int Get() const noexcept { return wd_; }
  explicit operator bool() const noexcept { return IsValid(); }

  void Remove() noexcept {
    if (IsValid()) {
      inotify_rm_watch(inotify_fd_, wd_);
      inotify_fd_ = -1;
      wd_ = -1;
    }
  }

 private:
  int inotify_fd_ = -1;
  int wd_ = -1;
};

/**
 * @brief Linux DIR* 目录句柄的 RAII 封装，析构时自动调用 closedir。
 */
class DirHandle final {
 public:
  DirHandle() = default;

  explicit DirHandle(DIR* dir) noexcept : dir_(dir) {}

  ~DirHandle() { Close(); }

  DirHandle(const DirHandle&) = delete;
  DirHandle& operator=(const DirHandle&) = delete;

  DirHandle(DirHandle&& other) noexcept : dir_(other.dir_) { other.dir_ = nullptr; }

  DirHandle& operator=(DirHandle&& other) noexcept {
    if (this != &other) {
      Close();
      dir_ = other.dir_;
      other.dir_ = nullptr;
    }
    return *this;
  }

  bool IsValid() const noexcept { return dir_ != nullptr; }
  DIR* Get() const noexcept { return dir_; }
  explicit operator bool() const noexcept { return IsValid(); }

  void Close() noexcept {
    if (IsValid()) {
      closedir(dir_);
      dir_ = nullptr;
    }
  }

 private:
  DIR* dir_ = nullptr;
};

#endif  // Q_OS_LINUX
