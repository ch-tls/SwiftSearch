/**
 * @file search_bar_widget.h
 * @brief 搜索/过滤栏组件：包含输入框和清除按钮。
 *
 * 在结果页面上方显示，输入文本时实时过滤文件树中的可见条目。
 *
 * @see SearchBarWidget, ResultListWidget
 */
#pragma once

#include <QWidget>

class QLineEdit;
class QPushButton;
class QCompleter;

/**
 * @brief 搜索/过滤栏组件。
 *
 * 提供 QLineEdit 输入框 + 清除按钮：
 * - 文本变更时发射 FilterTextChanged 信号
 * - 清除按钮点击时清空输入并聚焦
 */
class SearchBarWidget : public QWidget {
  Q_OBJECT

 public:
  /** @brief 构造搜索栏组件并初始化 UI。 */
  explicit SearchBarWidget(QWidget* parent = nullptr);

 signals:
  /** @brief 当搜索/过滤文本变更时发射。 */
  void FilterTextChanged(const QString& text);

 private slots:
  /** @brief 输入文本变更时更新清除按钮可见性并发射信号。 */
  void OnTextChanged(const QString& text);

  /** @brief 清除按钮点击时清空输入框。 */
  void OnClearClicked();

 private:
  /** @brief 构建布局：水平排列的 QLineEdit + QPushButton。 */
  void SetupUi();

  /** @brief 根据输入内容控制清除按钮的可见性。 */
  void UpdateClearButton();

  QLineEdit* search_input_ = nullptr;   ///< 搜索/过滤文本输入框
  QPushButton* clear_button_ = nullptr; ///< 清除按钮
};
