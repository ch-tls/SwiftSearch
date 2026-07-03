/**
 * @file log_manager.cpp
 * @brief LogManager 实现：单例日志系统，支持分级日志、文件输出和自动轮转。
 *
 * 功能：
 * - 同时输出到 stdout 和日志文件
 * - 日志级别过滤（Debug/Info/Warning/Error）
 * - 自动日志文件轮转（按大小，最多 5 个备份）
 * - 线程安全（QMutex 保护）
 * - 每行日志包含时间戳、级别、源文件和行号
 *
 * @see LogManager
 */

#include "log_manager.h"

#include <QDir>
#include <QFileInfo>
#include <iostream>

namespace swiftsearch {

LogManager& LogManager::Instance() {
  static LogManager instance;
  return instance;
}

LogManager::~LogManager() {
  QMutexLocker locker(&mutex_);
  if (log_stream_.device() != nullptr) {
    log_stream_.flush();
  }
  if (log_file_.isOpen()) {
    log_file_.close();
  }
}

/**
 * @brief 记录一条日志。
 *
 * 格式：[时间戳] [级别] 文件:行号  消息\n
 * 同时输出到 stdout 和日志文件（若已初始化）。
 * 日志消息会被 QMutex 保护。
 *
 * @param level 日志级别
 * @param file 源文件名
 * @param line 源文件行号
 * @param message 日志消息内容
 */
void LogManager::Log(LogLevel level, const char* file, int line, const QString& message) {
  if (level < min_level_) return;

  QMutexLocker locker(&mutex_);

  QString formatted = QString("[%1] [%2] %3:%4  %5\n")
                          .arg(FormatTimestamp(),
                               LevelToString(level),
                               QString::fromUtf8(file),
                               QString::number(line),
                               message);

  std::cout << formatted.toStdString() << std::flush;

  if (file_initialized_ && log_file_.isOpen()) {
    log_stream_ << formatted;
    log_stream_.flush();
    RotateIfNeeded();
  }
}

/**
 * @brief 设置日志文件路径。自动创建父目录并以追加模式打开文件。
 *
 * @param path 日志文件路径，空路径表示禁用文件日志
 */
void LogManager::SetLogFile(const QString& path) {
  QMutexLocker locker(&mutex_);

  if (file_initialized_ && log_file_.isOpen()) {
    log_stream_.flush();
    log_file_.close();
    file_initialized_ = false;
  }

  log_file_path_ = path;
  if (path.isEmpty()) return;

  QFileInfo fi(path);
  QDir().mkpath(fi.absolutePath());

  log_file_.setFileName(path);
  if (log_file_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    log_stream_.setDevice(&log_file_);
    file_initialized_ = true;
  }
}

/** @brief 设置最低日志记录级别，低于此级别的消息将被丢弃。 */
void LogManager::SetMinLevel(LogLevel level) {
  QMutexLocker locker(&mutex_);
  min_level_ = level;
}

/** @brief 立即刷新日志文件缓冲区。 */
void LogManager::Flush() {
  QMutexLocker locker(&mutex_);
  if (log_stream_.device() != nullptr) {
    log_stream_.flush();
  }
}

/**
 * @brief 按文件大小自动轮转日志文件。
 *
 * 当日志文件超过 kMaxFileSize (10MB) 时：
 * - 将现有文件重命名为 .1, .2, ... .N（最多 kMaxRotationFiles=5）
 * - 重新打开日志文件以截断模式写入
 */
void LogManager::RotateIfNeeded() {
  if (log_file_.size() < kMaxFileSize) return;

  log_stream_.flush();
  log_file_.close();

  for (int i = kMaxRotationFiles - 1; i >= 0; --i) {
    QString old_path = log_file_path_ + (i == 0 ? "" : QString(".%1").arg(i));
    QString new_path = log_file_path_ + QString(".%1").arg(i + 1);
    QFile::remove(new_path);
    if (QFile::exists(old_path)) {
      QFile::rename(old_path, new_path);
    }
  }

  (void)log_file_.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
  log_stream_.setDevice(&log_file_);
}

/** @brief 日志级别转换为字符串表示。 */
const char* LogManager::LevelToString(LogLevel level) {
  switch (level) {
    case LogLevel::Debug:   return "DEBUG";
    case LogLevel::Info:    return "INFO";
    case LogLevel::Warning: return "WARNING";
    case LogLevel::Error:   return "ERROR";
  }
  return "???";
}

/** @brief 生成当前时间戳字符串（精度到毫秒）。 */
QString LogManager::FormatTimestamp() {
  return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
}

}  // namespace swiftsearch
