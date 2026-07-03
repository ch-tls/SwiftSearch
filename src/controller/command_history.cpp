/**
 * @file command_history.cpp
 * @brief CommandHistory 实现：线程安全的命令撤销/重做管理器。
 *
 * 使用双栈实现标准的 undo/redo：
 * - undo_stack_: 已执行的命令（后进先出，用于撤销）
 * - redo_stack_: 已撤销的命令（后进先出，用于重做）
 * - 新命令执行时清空 redo_stack_
 * - 历史深度受 kMaxHistory 限制（默认 50）
 *
 * 线程安全：使用 std::recursive_mutex 保护栈操作。
 *
 * @see CommandHistory, FileCommand
 */

#include "command_history.h"
#include "file_command.h"

#include "../util/log_manager.h"

namespace swiftsearch {

/**
 * @brief 执行命令并将其推入撤销栈，同时清空重做栈。
 *
 * @param command 待执行的命令（unique_ptr，执行成功后所有权转移至撤销栈）
 * @return 是否执行成功
 */
bool CommandHistory::Execute(std::unique_ptr<FileCommand> command) {
  if (!command) return false;

  std::lock_guard<std::recursive_mutex> lock(mutex_);

  try {
    bool success = command->Execute();
    if (success) {
      redo_stack_.clear();
      undo_stack_.push_back(std::move(command));
      EnforceHistoryLimit();
      SWIFT_LOG_DEBUG(QString("Command executed: %1").arg(undo_stack_.back()->Description()));
    } else {
      SWIFT_LOG_WARNING(QString("Command failed: %1").arg(command->Description()));
    }
    return success;
  } catch (const std::exception& e) {
    SWIFT_LOG_ERROR(QString("CommandHistory::Execute exception: %1").arg(e.what()));
    return false;
  }
}

/**
 * @brief 撤销最后一个命令：从撤销栈弹出，执行 Unexecute，推入重做栈。
 *
 * @return 是否撤销成功
 */
bool CommandHistory::Undo() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (undo_stack_.empty()) return false;

  try {
    auto command = std::move(undo_stack_.back());
    undo_stack_.pop_back();

    bool success = command->Unexecute();
    if (success) {
      SWIFT_LOG_INFO(QString("Undo: %1").arg(command->Description()));
      redo_stack_.push_back(std::move(command));
    } else {
      undo_stack_.push_back(std::move(command));
    }
    return success;
  } catch (const std::exception& e) {
    SWIFT_LOG_ERROR(QString("CommandHistory::Undo exception: %1").arg(e.what()));
    return false;
  }
}

/**
 * @brief 重做最后一个撤销的命令：从重做栈弹出，执行 Execute，推入撤销栈。
 *
 * @return 是否重做成功
 */
bool CommandHistory::Redo() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (redo_stack_.empty()) return false;

  try {
    auto command = std::move(redo_stack_.back());
    redo_stack_.pop_back();

    bool success = command->Execute();
    if (success) {
      SWIFT_LOG_INFO(QString("Redo: %1").arg(command->Description()));
      undo_stack_.push_back(std::move(command));
      EnforceHistoryLimit();
    } else {
      redo_stack_.push_back(std::move(command));
    }
    return success;
  } catch (const std::exception& e) {
    SWIFT_LOG_ERROR(QString("CommandHistory::Redo exception: %1").arg(e.what()));
    return false;
  }
}

/** @brief 是否有待撤销的命令。 */
bool CommandHistory::CanUndo() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return !undo_stack_.empty();
}

/** @brief 是否有待重做的命令。 */
bool CommandHistory::CanRedo() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return !redo_stack_.empty();
}

/** @brief 获取撤销栈顶命令的描述文本（用于 UI 显示）。 */
QString CommandHistory::UndoDescription() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (undo_stack_.empty()) return {};
  return undo_stack_.back()->Description();
}

/** @brief 获取重做栈顶命令的描述文本（用于 UI 显示）。 */
QString CommandHistory::RedoDescription() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (redo_stack_.empty()) return {};
  return redo_stack_.back()->Description();
}

/** @brief 清空撤销栈和重做栈。 */
void CommandHistory::Clear() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  undo_stack_.clear();
  redo_stack_.clear();
  SWIFT_LOG_DEBUG("CommandHistory cleared");
}

/**
 * @brief 强制历史栈不超过 kMaxHistory 条。
 *
 * 从最旧的命令开始删除（从 begin() 开始）。在 Execute 和 Redo 后调用。
 */
void CommandHistory::EnforceHistoryLimit() {
  while (static_cast<int>(undo_stack_.size()) > kMaxHistory) {
    undo_stack_.erase(undo_stack_.begin());
  }
}

}  // namespace swiftsearch
