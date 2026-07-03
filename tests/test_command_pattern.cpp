#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "controller/file_command.h"
#include "controller/command_history.h"

using namespace swiftsearch;

class MockUndoableCommand final : public FileCommand {
 public:
  bool Execute() override {
    executed = true;
    return true;
  }

  bool Unexecute() override {
    undone = true;
    return true;
  }

  QString Description() const override {
    return "MockUndoableCommand";
  }

  bool executed = false;
  bool undone = false;
};

TEST(CommandHistoryTest, ExecuteAddsToUndoStack) {
  CommandHistory history;
  EXPECT_FALSE(history.CanUndo());

  bool result = history.Execute(std::make_unique<MockUndoableCommand>());
  EXPECT_TRUE(result);
  EXPECT_TRUE(history.CanUndo());
  EXPECT_FALSE(history.CanRedo());
}

TEST(CommandHistoryTest, UndoMovesToRedoStack) {
  CommandHistory history;

  history.Execute(std::make_unique<MockUndoableCommand>());
  ASSERT_TRUE(history.CanUndo());

  bool result = history.Undo();
  EXPECT_TRUE(result);
  EXPECT_FALSE(history.CanUndo());
  EXPECT_TRUE(history.CanRedo());
}

TEST(CommandHistoryTest, RedoMovesBackToUndoStack) {
  CommandHistory history;

  history.Execute(std::make_unique<MockUndoableCommand>());
  history.Undo();
  ASSERT_TRUE(history.CanRedo());

  bool result = history.Redo();
  EXPECT_TRUE(result);
  EXPECT_TRUE(history.CanUndo());
  EXPECT_FALSE(history.CanRedo());
}

TEST(CommandHistoryTest, NewCommandClearsRedoStack) {
  CommandHistory history;

  history.Execute(std::make_unique<MockUndoableCommand>());
  history.Undo();
  ASSERT_TRUE(history.CanRedo());

  history.Execute(std::make_unique<MockUndoableCommand>());
  EXPECT_FALSE(history.CanRedo());
}

TEST(CommandHistoryTest, UndoEmptyStackReturnsFalse) {
  CommandHistory history;
  EXPECT_FALSE(history.Undo());
}

TEST(CommandHistoryTest, RedoEmptyStackReturnsFalse) {
  CommandHistory history;
  EXPECT_FALSE(history.Redo());
}

TEST(CommandHistoryTest, DescriptionReturnsCorrectString) {
  auto cmd = std::make_unique<MockUndoableCommand>();
  EXPECT_EQ(cmd->Description().toStdString(), "MockUndoableCommand");
}

TEST(CommandHistoryTest, ClearEmptiesBothStacks) {
  CommandHistory history;

  history.Execute(std::make_unique<MockUndoableCommand>());
  history.Clear();
  EXPECT_FALSE(history.CanUndo());
  EXPECT_FALSE(history.CanRedo());
}

TEST(CommandHistoryTest, HistoryLimitEnforced) {
  CommandHistory history;

  for (int i = 0; i < 120; ++i) {
    history.Execute(std::make_unique<MockUndoableCommand>());
  }

  EXPECT_TRUE(history.CanUndo());

  for (int i = 0; i < 100; ++i) {
    history.Undo();
  }

  EXPECT_FALSE(history.CanUndo());
}

TEST(CommandHistoryTest, ExecuteNullReturnsFalse) {
  CommandHistory history;
  EXPECT_FALSE(history.Execute(nullptr));
}

TEST(CommandHistoryTest, UndoDescriptionWhenEmpty) {
  CommandHistory history;
  EXPECT_TRUE(history.UndoDescription().isEmpty());
}

TEST(CommandHistoryTest, UndoDescriptionWhenNotEmpty) {
  CommandHistory history;
  history.Execute(std::make_unique<MockUndoableCommand>());
  EXPECT_FALSE(history.UndoDescription().isEmpty());
}

TEST(CommandDescription, DeleteCommandFormat) {
  auto cmd = std::make_unique<DeleteFileFromIndexCommand>(
      nullptr, QString("/home/test/doc.pdf"));

  QString desc = cmd->Description();
  EXPECT_FALSE(desc.isEmpty());
  EXPECT_TRUE(desc.contains("doc.pdf"));
  EXPECT_TRUE(desc.contains("Delete"));

  EXPECT_FALSE(cmd->Execute());
  EXPECT_FALSE(cmd->Unexecute());
}

TEST(CommandDescription, CopyCommandFormat) {
  auto cmd = std::make_unique<CopyFilePathCommand>(
      nullptr, QString("/home/test/doc.pdf"));

  QString desc = cmd->Description();
  EXPECT_FALSE(desc.isEmpty());
  EXPECT_TRUE(desc.contains("Copy"));
  EXPECT_TRUE(desc.contains("doc.pdf"));
}

TEST(CommandDescription, OpenCommandFormat) {
  auto cmd = std::make_unique<OpenFileCommand>(
      QString("/home/test/doc.pdf"));

  QString desc = cmd->Description();
  EXPECT_FALSE(desc.isEmpty());
  EXPECT_TRUE(desc.contains("Open"));
  EXPECT_TRUE(desc.contains("doc.pdf"));
}
