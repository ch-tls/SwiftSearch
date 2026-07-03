/**
 * @file search_bar_widget.cpp
 * @brief SearchBarWidget 实现：搜索栏的 UI 构建和交互逻辑。
 *
 * @see SearchBarWidget
 */

#include "search_bar_widget.h"

#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>

SearchBarWidget::SearchBarWidget(QWidget* parent) : QWidget(parent) {
  SetupUi();
}

void SearchBarWidget::SetupUi() {
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);

  search_input_ = new QLineEdit(this);
  search_input_->setPlaceholderText(tr("Filter files..."));
  search_input_->setMinimumHeight(32);
  search_input_->setClearButtonEnabled(false);

  clear_button_ = new QPushButton(tr("✕"), this);
  clear_button_->setMinimumHeight(32);
  clear_button_->setFixedWidth(36);
  clear_button_->setVisible(false);

  layout->addWidget(search_input_, 1);
  layout->addWidget(clear_button_);

  connect(search_input_, &QLineEdit::textChanged, this, &SearchBarWidget::OnTextChanged);
  connect(clear_button_, &QPushButton::clicked, this, &SearchBarWidget::OnClearClicked);
}

void SearchBarWidget::UpdateClearButton() {
  clear_button_->setVisible(!search_input_->text().isEmpty());
}

void SearchBarWidget::OnTextChanged(const QString& text) {
  UpdateClearButton();
  emit FilterTextChanged(text);
}

void SearchBarWidget::OnClearClicked() {
  search_input_->clear();
  search_input_->setFocus();
}
