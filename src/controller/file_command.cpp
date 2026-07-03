/**
 * @file file_command.cpp
 * @brief Command 模式实现：文件操作命令的 execute/undo 逻辑。
 *
 * 提供四种可撤销的文件操作：
 * - DeleteFileFromIndexCommand：从索引中删除文件（可恢复）
 * - CopyFilePathCommand：复制路径到剪贴板（可恢复旧内容）
 * - OpenFileCommand：用系统关联程序打开文件（不可撤销）
 * - OpenFileLocationCommand：在文件管理器中定位文件（不可撤销）
 *
 * @see FileCommand, CommandHistory
 */

#include "file_command.h"

#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>

#include "../model/index_database.h"
#include "../util/log_manager.h"

namespace swiftsearch {

// ── DeleteFileFromIndexCommand ──

/**
 * @brief 构造函数：在构造时即查询数据库，保存待删除条目的快照以供撤销时恢复。
 *
 * @param database 索引数据库指针
 * @param file_path 待删除文件的绝对路径
 */
DeleteFileFromIndexCommand::DeleteFileFromIndexCommand(IndexDatabase* database,
                                                       QString file_path)
    : database_(database), file_path_(std::move(file_path)) {
  if (database_) {
    auto entries = database_->QueryByPath(file_path_);
    if (!entries.isEmpty()) {
      had_entry_ = true;
      saved_name_ = entries[0].name;
      saved_size_ = entries[0].size;
      saved_modified_time_ = entries[0].modified_time.toString(Qt::ISODate);
    }
  }
}

/** @brief 执行：从索引数据库中删除文件记录。 */
bool DeleteFileFromIndexCommand::Execute() {
  if (!database_) {
    SWIFT_LOG_ERROR("DeleteFileFromIndexCommand: null database");
    return false;
  }
  try {
    bool result = database_->RemoveFile(file_path_);
    if (result) {
      SWIFT_LOG_INFO(QString("Deleted from index: %1").arg(file_path_));
    }
    return result;
  } catch (const std::exception& e) {
    SWIFT_LOG_ERROR(QString("DeleteFileFromIndexCommand::Execute: %1").arg(e.what()));
    return false;
  }
}

/** @brief 撤销：将之前保存的文件快照重新插入索引数据库。 */
bool DeleteFileFromIndexCommand::Unexecute() {
  if (!database_ || !had_entry_) {
    return false;
  }
  try {
    FileEntry entry;
    entry.path = file_path_;
    entry.name = saved_name_;
    entry.size = saved_size_;
    entry.modified_time = QDateTime::fromString(saved_modified_time_, Qt::ISODate);
    bool result = database_->InsertFile(entry);
    if (result) {
      SWIFT_LOG_INFO(QString("Restored to index: %1").arg(file_path_));
    }
    return result;
  } catch (const std::exception& e) {
    SWIFT_LOG_ERROR(QString("DeleteFileFromIndexCommand::Unexecute: %1").arg(e.what()));
    return false;
  }
}

QString DeleteFileFromIndexCommand::Description() const {
  return QString("Delete '%1' from index").arg(file_path_);
}

// ── CopyFilePathCommand ──

/** @brief 构造函数：在构造时即保存剪贴板当前内容，以供撤销时恢复。
 *
 * @param clipboard 系统剪贴板指针
 * @param file_path 待复制的文件路径
 */
CopyFilePathCommand::CopyFilePathCommand(QClipboard* clipboard, QString file_path)
    : clipboard_(clipboard), file_path_(std::move(file_path)) {
  if (clipboard_) {
    previous_clipboard_text_ = clipboard_->text();
  }
}

/** @brief 执行：将文件路径复制到系统剪贴板。 */
bool CopyFilePathCommand::Execute() {
  if (!clipboard_) return false;
  clipboard_->setText(file_path_);
  SWIFT_LOG_DEBUG(QString("Copied to clipboard: %1").arg(file_path_));
  return true;
}

/** @brief 撤销：恢复剪贴板为此命令执行前的内容。 */
bool CopyFilePathCommand::Unexecute() {
  if (!clipboard_) return false;
  clipboard_->setText(previous_clipboard_text_);
  return true;
}

QString CopyFilePathCommand::Description() const {
  return QString("Copy path '%1'").arg(file_path_);
}

// ── OpenFileCommand ──

OpenFileCommand::OpenFileCommand(QString file_path) : file_path_(std::move(file_path)) {}

/** @brief 执行：通过 QDesktopServices 用系统关联程序打开文件。
 *
 * @note 此操作不可撤销（文件打开无法"关闭"已触发的系统调用）。
 */
bool OpenFileCommand::Execute() {
  QUrl url = QUrl::fromLocalFile(file_path_);
  bool result = QDesktopServices::openUrl(url);
  if (result) {
    SWIFT_LOG_INFO(QString("Opened file: %1").arg(file_path_));
  } else {
    SWIFT_LOG_WARNING(QString("Failed to open file: %1").arg(file_path_));
  }
  return result;
}

/** @brief 撤销：不支持。打开文件操作不可撤销。 */
bool OpenFileCommand::Unexecute() {
  return false;
}

QString OpenFileCommand::Description() const {
  return QString("Open '%1'").arg(file_path_);
}

// ── OpenFileLocationCommand ──

OpenFileLocationCommand::OpenFileLocationCommand(QString file_path)
    : file_path_(std::move(file_path)) {}

/** @brief 执行：在系统文件管理器中打开文件所在目录。
 *
 * @note 此操作不可撤销。
 */
bool OpenFileLocationCommand::Execute() {
  QString parent_dir = QFileInfo(file_path_).absolutePath();
  QUrl url = QUrl::fromLocalFile(parent_dir);
  bool result = QDesktopServices::openUrl(url);
  if (result) {
    SWIFT_LOG_INFO(QString("Opened file location: %1").arg(parent_dir));
  } else {
    SWIFT_LOG_WARNING(QString("Failed to open file location: %1").arg(parent_dir));
  }
  return result;
}

/** @brief 撤销：不支持。打开文件位置不可撤销。 */
bool OpenFileLocationCommand::Unexecute() {
  return false;
}

QString OpenFileLocationCommand::Description() const {
  return QString("Open location of '%1'").arg(file_path_);
}

}  // namespace swiftsearch
