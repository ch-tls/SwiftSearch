/**
 * @file settings_dialog.cpp
 * @brief SettingsDialog 实现：配置表单的构建、加载和保存。
 *
 * @see SettingsDialog
 */

#include "settings_dialog.h"

#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSettings>
#include <QStandardPaths>
#include <QLabel>
#include <QFileDialog>
#include <QDir>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(tr("Settings"));
  setMinimumWidth(420);
  SetupUi();
  LoadSettings();
}

QString SettingsDialog::DefaultDirectory() const {
  return default_dir_edit_->text();
}

/** @brief 构建表单布局：默认目录（含浏览按钮）、数据库路径、扫描深度/结果数、大小写。 */
void SettingsDialog::SetupUi() {
  auto* layout = new QVBoxLayout(this);

  auto* form_layout = new QFormLayout;

  auto* default_dir_layout = new QHBoxLayout;
  default_dir_edit_ = new QLineEdit(this);
  default_dir_edit_->setPlaceholderText(QDir::homePath());
  auto* browse_btn = new QPushButton(tr("Browse..."), this);
  default_dir_layout->addWidget(default_dir_edit_);
  default_dir_layout->addWidget(browse_btn);
  form_layout->addRow(tr("Default index directory:"), default_dir_layout);

  db_path_edit_ = new QLineEdit(this);
  db_path_edit_->setPlaceholderText(
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/index.db");
  form_layout->addRow(tr("Database path:"), db_path_edit_);

  max_depth_spin_ = new QSpinBox(this);
  max_depth_spin_->setRange(-1, 99);
  max_depth_spin_->setSpecialValueText(tr("Unlimited"));
  max_depth_spin_->setValue(-1);
  form_layout->addRow(tr("Max scan depth:"), max_depth_spin_);

  max_results_spin_ = new QSpinBox(this);
  max_results_spin_->setRange(10, 10000);
  max_results_spin_->setValue(500);
  form_layout->addRow(tr("Max search results:"), max_results_spin_);

  case_sensitive_check_ = new QCheckBox(tr("Case sensitive search"), this);
  form_layout->addRow(tr(""), case_sensitive_check_);

  layout->addLayout(form_layout);
  layout->addSpacing(12);

  auto* button_layout = new QHBoxLayout;
  button_layout->addStretch();

  auto* save_button = new QPushButton(tr("Save"), this);
  auto* cancel_button = new QPushButton(tr("Cancel"), this);

  button_layout->addWidget(save_button);
  button_layout->addWidget(cancel_button);
  layout->addLayout(button_layout);

  connect(browse_btn, &QPushButton::clicked, this, &SettingsDialog::OnBrowseDefaultDir);
  connect(save_button, &QPushButton::clicked, this, &SettingsDialog::OnSave);
  connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);
}

/** @brief 从 QSettings 加载已保存的配置值到 UI 控件。 */
void SettingsDialog::LoadSettings() {
  QSettings settings("SwiftSearch", "SwiftSearch");
  default_dir_edit_->setText(settings.value("default_dir", "").toString());
  db_path_edit_->setText(settings.value("db_path", "").toString());
  max_depth_spin_->setValue(settings.value("max_depth", -1).toInt());
  max_results_spin_->setValue(settings.value("max_results", 500).toInt());
  case_sensitive_check_->setChecked(settings.value("case_sensitive", false).toBool());
}

/** @brief 将 UI 控件的值写入 QSettings 持久化存储。 */
void SettingsDialog::SaveSettings() {
  QSettings settings("SwiftSearch", "SwiftSearch");
  settings.setValue("default_dir", default_dir_edit_->text());
  if (!db_path_edit_->text().isEmpty()) {
    settings.setValue("db_path", db_path_edit_->text());
  }
  settings.setValue("max_depth", max_depth_spin_->value());
  settings.setValue("max_results", max_results_spin_->value());
  settings.setValue("case_sensitive", case_sensitive_check_->isChecked());
}

/** @brief 弹出目录选择对话框，将所选目录写入默认目录输入框。 */
void SettingsDialog::OnBrowseDefaultDir() {
  QString dir = QFileDialog::getExistingDirectory(this, tr("Select Default Directory"),
                                                   default_dir_edit_->text().isEmpty()
                                                       ? QDir::homePath()
                                                       : default_dir_edit_->text());
  if (!dir.isEmpty()) {
    default_dir_edit_->setText(dir);
  }
}

/** @brief 保存设置并关闭对话框（accept）。 */
void SettingsDialog::OnSave() {
  SaveSettings();
  accept();
}
