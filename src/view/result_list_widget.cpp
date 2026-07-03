/**
 * @file result_list_widget.cpp
 * @brief ResultListWidget 实现：文件树构建、过滤显示和右键菜单操作。
 *
 * 核心功能：
 * - BuildTree：将所有索引文件按目录层级构建 QTreeWidget 树
 * - FilterTree：递归隐藏/展开不匹配过滤条件的条目
 * - contextMenuEvent：右键菜单提供文件操作（通过 CommandHistory 可撤销）
 * - FormatFileSize：格式化文件大小（B/KB/MB/GB/TB）
 *
 * @see ResultListWidget, CommandHistory
 */

#include "result_list_widget.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <functional>

#include "../controller/file_command.h"
#include "../controller/command_history.h"
#include "../model/index_database.h"
#include "../model/index_engine.h"

ResultListWidget::ResultListWidget(QWidget* parent) : QWidget(parent) {
  SetupUi();
}

/** @brief 初始化四列表格（名称/路径/大小/修改时间）和双击处理。 */
void ResultListWidget::SetupUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  tree_widget_ = new QTreeWidget(this);
  tree_widget_->setRootIsDecorated(true);
  tree_widget_->setAlternatingRowColors(true);
  tree_widget_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  tree_widget_->setSortingEnabled(false);
  tree_widget_->setContextMenuPolicy(Qt::DefaultContextMenu);
  tree_widget_->setAnimated(true);

  QStringList headers;
  headers << tr("Name") << tr("Path") << tr("Size") << tr("Modified");
  tree_widget_->setHeaderLabels(headers);
  tree_widget_->setColumnWidth(0, 260);
  tree_widget_->setColumnWidth(1, 280);
  tree_widget_->setColumnWidth(2, 80);
  tree_widget_->setColumnWidth(3, 150);

  layout->addWidget(tree_widget_);

  connect(tree_widget_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
    QString path = item->data(1, Qt::UserRole).toString();
    if (!path.isEmpty()) {
      emit FileDoubleClicked(path);
      QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
  });
}

void ResultListWidget::SetCommandHistory(swiftsearch::CommandHistory* history) {
  command_history_ = history;
}

/**
 * @brief 以平面列表方式显示搜索结果（搜索结果模式）。
 *
 * 清空现有树，为每个 SearchResult 创建顶级 TreeWidgetItem。
 * 目录条目前加 "[目录名]" 前缀以区别。
 */
void ResultListWidget::SetResults(const QList<::SearchResult>& results) {
  Clear();

  for (const auto& result : results) {
    auto* item = new QTreeWidgetItem(tree_widget_);

    QString name = result.file_entry.is_directory
                       ? "[" + result.file_entry.name + "]"
                       : result.file_entry.name;

    item->setText(0, name);
    item->setText(1, result.file_entry.path);
    item->setText(2, FormatFileSize(result.file_entry.size));
    item->setText(3, result.file_entry.modified_time.toString("yyyy-MM-dd hh:mm:ss"));
    item->setData(0, Qt::UserRole, result.file_entry.name);
    item->setData(1, Qt::UserRole, result.file_entry.path);
  }
}

void ResultListWidget::Clear() {
  tree_widget_->clear();
  path_map_.clear();
}

/**
 * @brief 构建文件系统树形视图（索引浏览模式）。
 *
 * 构建策略：
 * 1. 从文件条目收集所有父目录路径，按深度排序
 * 2. 先创建所有目录节点，记录到 path_map_
 * 3. 再按路径排序文件条目，挂载到对应父目录节点下
 *
 * 目录节点按 '/' 深度级别排序，浅层目录优先展示。
 */
void ResultListWidget::BuildTree(const QList<FileEntry>& entries) {
  Clear();
  if (entries.isEmpty()) return;

  QSet<QString> all_dirs;

  for (const auto& entry : entries) {
    QString parent_path = QFileInfo(entry.path).path();
    while (!parent_path.isEmpty() && !all_dirs.contains(parent_path)) {
      all_dirs.insert(parent_path);
      parent_path = QFileInfo(parent_path).path();
    }
  }

  QStringList sorted_dirs = all_dirs.values();
  std::sort(sorted_dirs.begin(), sorted_dirs.end(), [](const QString& a, const QString& b) {
    int da = static_cast<int>(a.count('/'));
    int db = static_cast<int>(b.count('/'));
    if (da != db) return da < db;
    return a < b;
  });

  QSet<QString> entry_dirs;
  for (const auto& entry : entries) {
    if (entry.is_directory) {
      entry_dirs.insert(entry.path);
    }
  }

  for (const auto& dir_path : sorted_dirs) {
    QFileInfo fi(dir_path);
    QString parent_dir = fi.path();
    QTreeWidgetItem* parent = path_map_.value(parent_dir, nullptr);

    QTreeWidgetItem* item;
    if (parent) {
      item = new QTreeWidgetItem(parent);
    } else {
      item = new QTreeWidgetItem(tree_widget_);
    }

    item->setText(0, "[" + fi.fileName() + "]");
    item->setText(1, dir_path);
    item->setData(0, Qt::UserRole, fi.fileName());
    item->setData(1, Qt::UserRole, dir_path);
    path_map_[dir_path] = item;
  }

  QList<FileEntry> sorted_entries = entries;
  std::sort(sorted_entries.begin(), sorted_entries.end(), [](const FileEntry& a, const FileEntry& b) {
    return a.path < b.path;
  });

  for (const auto& entry : sorted_entries) {
    if (entry.is_directory) continue;

    QString parent_path = QFileInfo(entry.path).path();
    QTreeWidgetItem* parent = path_map_.value(parent_path, nullptr);

    QTreeWidgetItem* item;
    if (parent) {
      item = new QTreeWidgetItem(parent);
    } else {
      item = new QTreeWidgetItem(tree_widget_);
    }

    item->setText(0, entry.name);
    item->setText(1, entry.path);
    item->setText(2, FormatFileSize(entry.size));
    item->setText(3, entry.modified_time.toString("yyyy-MM-dd hh:mm:ss"));
    item->setData(0, Qt::UserRole, entry.name);
    item->setData(1, Qt::UserRole, entry.path);
  }
}

/**
 * @brief 过滤文件树：递归遍历节点，隐藏不匹配的叶子。
 *
 * 如果查询包含 ? 或 * 则使用 QRegularExpression::fromWildcard 通配符匹配，
 * 否则自动添加前后缀 * 进行包含匹配（大小写不敏感）。
 *
 * 非叶子目录节点：仅当有至少一个可见子节点时才保持可见，并自动展开。
 */
void ResultListWidget::FilterTree(const QString& query_text) {
  if (query_text.isEmpty()) {
    ClearFilter();
    return;
  }

  bool has_wildcards = query_text.contains('*') || query_text.contains('?');
  auto pattern = has_wildcards
                     ? QRegularExpression::fromWildcard(query_text, Qt::CaseInsensitive)
                     : QRegularExpression::fromWildcard("*" + query_text + "*",
                                                        Qt::CaseInsensitive);

  std::function<bool(QTreeWidgetItem*)> filter_item = [&](QTreeWidgetItem* item) -> bool {
    bool has_visible_child = false;
    for (int i = 0; i < item->childCount(); ++i) {
      if (filter_item(item->child(i))) {
        has_visible_child = true;
      }
    }

    if (item->childCount() == 0) {
      QString name = item->data(0, Qt::UserRole).toString();
      bool matches = pattern.match(name).hasMatch();
      item->setHidden(!matches);
      return matches;
    }

    item->setHidden(!has_visible_child);
    if (has_visible_child) {
      item->setExpanded(true);
    }
    return has_visible_child;
  };

  for (int i = 0; i < tree_widget_->topLevelItemCount(); ++i) {
    filter_item(tree_widget_->topLevelItem(i));
  }
}

/** @brief 清除过滤，递归恢复所有节点的可见性。 */
void ResultListWidget::ClearFilter() {
  std::function<void(QTreeWidgetItem*)> unhide_all = [&](QTreeWidgetItem* item) {
    item->setHidden(false);
    for (int i = 0; i < item->childCount(); ++i) {
      unhide_all(item->child(i));
    }
  };

  for (int i = 0; i < tree_widget_->topLevelItemCount(); ++i) {
    unhide_all(tree_widget_->topLevelItem(i));
  }
}

/**
 * @brief 右键菜单处理：根据菜单选择执行对应的文件操作命令。
 *
 * 菜单选项：Open File / Open File Location / Copy Path / Copy Directory Path / Delete from Index
 *
 * 如果 command_history_ 已设置，操作通过命令模式执行（可撤销）；否则直接执行。
 */
void ResultListWidget::contextMenuEvent(QContextMenuEvent* event) {
  QTreeWidgetItem* item = tree_widget_->itemAt(event->pos());
  if (!item) return;

  QString file_path = item->data(1, Qt::UserRole).toString();
  if (file_path.isEmpty()) return;

  QMenu menu(this);

  auto* open_action = menu.addAction(tr("Open File"));
  auto* open_location_action = menu.addAction(tr("Open File Location"));
  menu.addSeparator();
  auto* copy_action = menu.addAction(tr("Copy Path"));
  auto* copy_dir_action = menu.addAction(tr("Copy Directory Path"));
  menu.addSeparator();
  auto* delete_action = menu.addAction(tr("Delete from Index"));

  QAction* selected = menu.exec(event->globalPos());
  if (!selected) return;

  if (selected == open_action) {
    if (command_history_) {
      auto cmd = std::make_unique<swiftsearch::OpenFileCommand>(file_path);
      command_history_->Execute(std::move(cmd));
    } else {
      QDesktopServices::openUrl(QUrl::fromLocalFile(file_path));
    }
  } else if (selected == open_location_action) {
    if (command_history_) {
      auto cmd = std::make_unique<swiftsearch::OpenFileLocationCommand>(file_path);
      command_history_->Execute(std::move(cmd));
    } else {
      QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file_path).absolutePath()));
    }
  } else if (selected == copy_action) {
    if (command_history_) {
      auto cmd = std::make_unique<swiftsearch::CopyFilePathCommand>(
          QApplication::clipboard(), file_path);
      command_history_->Execute(std::move(cmd));
    } else {
      QApplication::clipboard()->setText(file_path);
    }
  } else if (selected == copy_dir_action) {
    QString parent_path = QFileInfo(file_path).absolutePath();
    if (command_history_) {
      auto cmd = std::make_unique<swiftsearch::CopyFilePathCommand>(
          QApplication::clipboard(), parent_path);
      command_history_->Execute(std::move(cmd));
    } else {
      QApplication::clipboard()->setText(parent_path);
    }
  } else if (selected == delete_action) {
    if (command_history_) {
      if (auto* main_window = window()) {
        auto* index_engine = main_window->findChild<IndexEngine*>();
        if (index_engine) {
          auto cmd = std::make_unique<swiftsearch::DeleteFileFromIndexCommand>(
              index_engine->GetDatabase(), file_path);
          command_history_->Execute(std::move(cmd));
          delete item;
        }
      }
    }
  }
}

/**
 * @brief 格式化文件大小为可读字符串。
 *
 * 转换规则：按 1024 进制递进，最多到 TB：
 * - < 1KB: "N B"（整数）
 * - >= 1KB: "N.N KB/MB/GB/TB"（一位小数）
 *
 * @param bytes 文件大小（字节）
 * @return 格式化后的可读字符串（如 "1.5 MB"）
 */
QString ResultListWidget::FormatFileSize(int64_t bytes) {
  if (bytes < 0) return "-";

  const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  int unit_index = 0;
  double size = static_cast<double>(bytes);

  while (size >= 1024.0 && unit_index < 4) {
    size /= 1024.0;
    ++unit_index;
  }

  return QString::number(size, 'f', unit_index == 0 ? 0 : 1) + " " + units[unit_index];
}
