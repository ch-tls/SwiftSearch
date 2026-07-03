#pragma once

/**
 * @brief 线程安全的文件操作命令基类，支持撤销/重做。
 *
 * 具体命令类实现 Execute() 与 Unexecute()，
 * 由 CommandHistory 统一管理生命周期和调用顺序。
 */

#include <QString>
#include <memory>
#include <string>

class IndexDatabase;
class QClipboard;

namespace swiftsearch {

/**
 * @brief 文件操作命令抽象基类。
 *
 * 所有文件级别的用户操作（删除、复制、打开）都封装为
 * FileCommand 子类，以实现统一的撤销/重做。
 */
class FileCommand {
 public:
  virtual ~FileCommand() = default;

  /**
   * @brief 执行命令。
   * @return true 成功，false 失败
   */
  virtual bool Execute() = 0;

  /**
   * @brief 撤销命令，恢复执行前的状态。
   * @return true 成功，false 失败
   */
  virtual bool Unexecute() = 0;

  /**
   * @brief 返回人类可读的命令描述，用于 UI 显示。
   */
  virtual QString Description() const = 0;
};

// ────────────────────────────────────────────────────

/**
 * @brief 从索引数据库中删除文件条目。
 *
 * 执行时保存 FileEntry 快照用于撤销恢复。
 * @note 线程安全：通过 QMutex 保护数据库访问。
 */
class DeleteFileFromIndexCommand final : public FileCommand {
 public:
  /**
   * @param database 目标索引数据库
   * @param file_path 要删除的文件完整路径
   */
  DeleteFileFromIndexCommand(IndexDatabase* database, QString file_path);

  bool Execute() override;
  bool Unexecute() override;
  QString Description() const override;

 private:
  IndexDatabase* database_;
  QString file_path_;
  bool had_entry_ = false;
  int64_t saved_size_ = 0;
  QString saved_name_;
  QString saved_modified_time_;
};

// ────────────────────────────────────────────────────

/**
 * @brief 复制文件路径到系统剪贴板。
 *
 * 撤销时恢复到剪贴板的先前内容。
 */
class CopyFilePathCommand final : public FileCommand {
 public:
  /**
   * @param clipboard 系统剪贴板对象
   * @param file_path 要复制的文件路径
   */
  CopyFilePathCommand(QClipboard* clipboard, QString file_path);

  bool Execute() override;
  bool Unexecute() override;
  QString Description() const override;

 private:
  QClipboard* clipboard_;
  QString file_path_;
  QString previous_clipboard_text_;
};

// ────────────────────────────────────────────────────

/**
 * @brief 通过系统默认程序打开文件。
 *
 * 此操作通常不可撤销。
 */
class OpenFileCommand final : public FileCommand {
 public:
  /**
   * @param file_path 要打开的文件完整路径
   */
  explicit OpenFileCommand(QString file_path);

  bool Execute() override;
  bool Unexecute() override;
  QString Description() const override;

 private:
  QString file_path_;
};

// ────────────────────────────────────────────────────

/**
 * @brief 在系统文件管理器中打开文件所在目录。
 *
 * 与 OpenFileCommand 不同，此命令始终打开父目录而非文件本身。
 * 操作不可撤销。
 */
class OpenFileLocationCommand final : public FileCommand {
 public:
  explicit OpenFileLocationCommand(QString file_path);

  bool Execute() override;
  bool Unexecute() override;
  QString Description() const override;

 private:
  QString file_path_;
};

}  // namespace swiftsearch
