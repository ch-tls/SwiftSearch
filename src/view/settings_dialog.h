/**
 * @file settings_dialog.h
 * @brief 设置对话框：管理应用配置参数。
 *
 * 可配置项：
 * - 默认索引目录
 * - 数据库路径
 * - 最大扫描深度
 * - 最大搜索结果数
 * - 大小写敏感搜索
 *
 * 所有设置通过 QSettings 持久化到系统配置目录。
 *
 * @see SettingsDialog
 */
#pragma once

#include <QDialog>

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QPushButton;

/**
 * @brief 应用设置对话框。
 *
 * 使用 QFormLayout 布局，提供 Save/Cancel 按钮。
 * 构造时从 QSettings 加载上次设置，Save 时写回。
 */
class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  /** @brief 构建设置对话框并加载已有配置。 */
  explicit SettingsDialog(QWidget* parent = nullptr);

  /** @brief 获取当前设置的默认索引目录。 */
  QString DefaultDirectory() const;

 private slots:
  /** @brief 保存设置到 QSettings 并关闭对话框（accept）。 */
  void OnSave();

  /** @brief 弹出目录选择对话框设置默认索引目录。 */
  void OnBrowseDefaultDir();

 private:
  /** @brief 构建表单布局。 */
  void SetupUi();

  /** @brief 从 QSettings 加载已保存的值到各控件。 */
  void LoadSettings();

  /** @brief 将各控件的值写入 QSettings。 */
  void SaveSettings();

  QLineEdit* default_dir_edit_ = nullptr;     ///< 默认索引目录输入框
  QLineEdit* db_path_edit_ = nullptr;         ///< 数据库路径输入框
  QSpinBox* max_depth_spin_ = nullptr;        ///< 最大扫描深度（-1=无限）
  QSpinBox* max_results_spin_ = nullptr;      ///< 最大搜索结果数
  QCheckBox* case_sensitive_check_ = nullptr; ///< 大小写敏感搜索复选框
};
