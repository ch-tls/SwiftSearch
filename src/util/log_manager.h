#pragma once

/**
 * @brief 结构化日志系统。
 *
 * 提供四级日志（Debug/Info/Warning/Error），支持控制台+文件双路输出，
 * 文件名+行号自动记录，日志文件超过 5MB 自动轮转。
 *
 * 线程安全：所有公开方法通过内部 QMutex 保护。
 *
 * @note 使用 SWIFT_LOG_INFO("message") 等宏进行调用，
 *       宏自动注入 __FILE__ 和 __LINE__。
 *
 * 使用示例:
 * @code
 *   LogManager::Instance().SetLogFile("/var/log/swiftsearch.log");
 *   SWIFT_LOG_INFO("Engine initialized with db: {}", db_path.toStdString());
 *   SWIFT_LOG_WARNING("Low disk space on volume {}", volume.toStdString());
 *   SWIFT_LOG_ERROR("Failed to open file: {}", file_path.toStdString());
 * @endcode
 */

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QMutexLocker>
#include <QDateTime>

namespace swiftsearch {

/** @brief 日志严重级别，由低到高。 */
enum class LogLevel {
  Debug = 0,   ///< 调试信息，开发阶段使用
  Info = 1,    ///< 常规运行时信息
  Warning = 2, ///< 警告，不影响功能但需关注
  Error = 3    ///< 错误，影响功能
};

/**
 * @brief 线程安全的单例日志管理器。
 */
class LogManager {
 public:
  /** @brief 获取全局单例实例。 */
  static LogManager& Instance();

  /**
   * @brief 输出一条日志。
   * @param level 日志级别
   * @param file 源文件名（通常由宏注入 __FILE__）
   * @param line 行号（通常由宏注入 __LINE__）
   * @param message 日志消息
   */
  void Log(LogLevel level, const char* file, int line, const QString& message);

  /**
   * @brief 设置日志文件输出路径。设为空字符串则仅控制台输出。
   * @param path 日志文件路径
   */
  void SetLogFile(const QString& path);

  /**
   * @brief 设置最低输出级别，低于此级别的日志被丢弃。
   * @param level 最低级别
   */
  void SetMinLevel(LogLevel level);

  /** @brief 强制刷新文件缓冲区。 */
  void Flush();

 private:
  LogManager() = default;
  ~LogManager();

  LogManager(const LogManager&) = delete;
  LogManager& operator=(const LogManager&) = delete;

  void RotateIfNeeded();
  static const char* LevelToString(LogLevel level);
  static QString FormatTimestamp();

  QMutex mutex_;
  LogLevel min_level_ = LogLevel::Debug;
  QString log_file_path_;
  QFile log_file_;
  QTextStream log_stream_;
  bool file_initialized_ = false;
  static constexpr qint64 kMaxFileSize = 5 * 1024 * 1024;  ///< 5 MB
  static constexpr int kMaxRotationFiles = 3;
};

}  // namespace swiftsearch

// ── 便捷宏 ──

#define SWIFT_LOG_DEBUG(msg) \
  swiftsearch::LogManager::Instance().Log(swiftsearch::LogLevel::Debug, __FILE__, __LINE__, msg)

#define SWIFT_LOG_INFO(msg) \
  swiftsearch::LogManager::Instance().Log(swiftsearch::LogLevel::Info, __FILE__, __LINE__, msg)

#define SWIFT_LOG_WARNING(msg) \
  swiftsearch::LogManager::Instance().Log(swiftsearch::LogLevel::Warning, __FILE__, __LINE__, msg)

#define SWIFT_LOG_ERROR(msg) \
  swiftsearch::LogManager::Instance().Log(swiftsearch::LogLevel::Error, __FILE__, __LINE__, msg)
