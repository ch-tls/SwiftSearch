/**
 * @file result_list_widget.h
 * @brief 搜索结果列表组件：以树形结构展示索引文件，支持过滤和右键上下文菜单。
 *
 * ResultListWidget 构建并维护文件系统的树形视图，叶子节点为文件，
 * 中间节点为目录。支持通配符过滤（就地隐藏不匹配条目）和
 * 右键菜单操作（打开、复制路径、删除索引等）。
 *
 * @see ResultListWidget, CommandHistory, FileCommand
 */
#pragma once

#include <QWidget>
#include <QList>
#include <QHash>

#include "../controller/search_types.h"
#include "../model/file_entry.h"

class QTreeWidget;
class QTreeWidgetItem;

namespace swiftsearch {
class CommandHistory;
}

/**
 * @brief 文件搜索结果列表，使用 QTreeWidget 展示文件的树形层次结构。
 *
 * 两种展示模式：
 * - SetResults：平面列表模式（搜索结果）
 * - BuildTree：树形目录模式（索引浏览）
 *
 * 支持：
 * - 通配符过滤（就地隐藏，不重建树）
 * - 右键上下文菜单（打开/打开位置/复制路径/复制目录/删除索引）
 * - 命令历史集成（所有右键操作可撤销）
 */
class ResultListWidget : public QWidget {
  Q_OBJECT

 public:
  /** @brief 构造结果列表组件并初始化 UI。 */
  explicit ResultListWidget(QWidget* parent = nullptr);

  /** @brief 设置扁平搜索结果显示。 */
  void SetResults(const QList<::SearchResult>& results);

  /** @brief 清除所有条目。 */
  void Clear();

  /** @brief 从文件条目构建树形目录视图（索引浏览模式）。 */
  void BuildTree(const QList<FileEntry>& entries);

  /** @brief 按查询文本过滤树中可见条目。 */
  void FilterTree(const QString& query_text);

  /** @brief 清除过滤，显示所有条目。 */
  void ClearFilter();

  /** @brief 设置命令历史，启用右键菜单操作的 undo/redo 支持。 */
  void SetCommandHistory(swiftsearch::CommandHistory* history);

 signals:
  /** @brief 当用户双击文件条目时发射。 */
  void FileDoubleClicked(const QString& file_path);

 protected:
  /** @brief 右键菜单处理：显示文件操作菜单。 */
  void contextMenuEvent(QContextMenuEvent* event) override;

 private:
  /** @brief 构建 UI：四列 TreeWidget（名称/路径/大小/修改时间）。 */
  void SetupUi();

  /** @brief 格式化文件大小为人类可读字符串（B/KB/MB/GB/TB）。 */
  static QString FormatFileSize(int64_t bytes);

  QTreeWidget* tree_widget_ = nullptr;                 ///< 核心树形控件
  swiftsearch::CommandHistory* command_history_ = nullptr; ///< 命令历史（用于撤销）
  QHash<QString, QTreeWidgetItem*> path_map_;          ///< 目录路径 → TreeWidgetItem 映射
};
