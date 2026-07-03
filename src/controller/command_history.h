#pragma once

/**
 * @brief 命令历史管理器，提供撤销/重做功能。
 *
 * 维护两个栈：undo_stack_（已执行命令）和 redo_stack_（已撤销命令）。
 * 新命令执行时清空 redo_stack_。
 * 线程安全：所有公共方法通过 QMutex 保护。
 *
 * @note 最大历史深度可通过 kMaxHistory 配置（默认 100）。
 */

#include <QString>
#include <memory>
#include <mutex>
#include <vector>

namespace swiftsearch {

class FileCommand;

class CommandHistory {
 public:
  CommandHistory() = default;

  /** @brief 执行命令并将其推入撤销栈。执行失败时不入栈。 */
  bool Execute(std::unique_ptr<FileCommand> command);

  /** @brief 撤销最近一次命令。 */
  bool Undo();

  /** @brief 重做最近一次撤销的命令。 */
  bool Redo();

  /** @brief 是否有可撤销的命令。 */
  bool CanUndo() const;

  /** @brief 是否有可重做的命令。 */
  bool CanRedo() const;

  /** @brief 最近可撤销命令的描述（用于 UI 显示）。 */
  QString UndoDescription() const;

  /** @brief 最近可重做命令的描述。 */
  QString RedoDescription() const;

  /** @brief 清空所有历史。 */
  void Clear();

 private:
  void EnforceHistoryLimit();

  static constexpr int kMaxHistory = 100;

  mutable std::recursive_mutex mutex_;
  std::vector<std::unique_ptr<FileCommand>> undo_stack_;
  std::vector<std::unique_ptr<FileCommand>> redo_stack_;
};

}  // namespace swiftsearch
