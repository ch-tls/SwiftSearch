/**
 * @file main_window.cpp
 * @brief MainWindow 实现：应用主窗口，连接 Model/Controller/View 三层。
 *
 * MainWindow 是 SwiftSearch 的 UI 入口，职责包括：
 * - 初始化 IndexEngine 和 SearchService (SetupServices)
 * - 构建菜单栏、搜索栏、结果列表、状态栏 (SetupUi / SetupMenuBar)
 * - 连接信号槽，转发用户操作到 Controller 层
 * - 实现 IndexingObserver 和 SearchObserver，将结果更新到 UI
 * - 管理 CommandHistory 的 undo/redo 动作
 * - 支持自动索引（启动时恢复上次设置的默认目录）
 *
 * @see MainWindow, IndexEngine, SearchService, CommandHistory
 */

#include "main_window.h"
#include "search_bar_widget.h"
#include "result_list_widget.h"
#include "settings_dialog.h"
#include "../model/index_engine.h"
#include "../model/index_database.h"
#include "../controller/search_service.h"
#include "../controller/query_parser.h"
#include "../controller/file_command.h"
#include "../controller/command_history.h"
#include "../model/indexing_observer.h"
#include "../controller/search_observer.h"
#include "../util/log_manager.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QClipboard>
#include <QSettings>
#include <QTimer>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("SwiftSearch");
  setWindowIcon(QIcon(":/icons/app.png"));
  resize(960, 640);

  SetupServices();
  SetupUi();
  SetupConnections();
  SetupMenuBar();
  SetupStatusBar();

  QTimer::singleShot(0, this, &MainWindow::TryAutoIndex);

  SWIFT_LOG_INFO("MainWindow initialized");
}

MainWindow::~MainWindow() = default;

// ── IndexingObserver ──

void MainWindow::OnIndexingProgress(int files_indexed, int64_t total_indexed) {
  QMetaObject::invokeMethod(this, [this, files_indexed, total_indexed]() {
    progress_bar_->setValue(progress_bar_->value() + files_indexed);
    status_label_->setText(tr("Indexed %1 files...").arg(total_indexed));
  }, Qt::QueuedConnection);
}

void MainWindow::OnIndexingFinished(int64_t total_files) {
  QMetaObject::invokeMethod(this, [this, total_files]() {
    progress_bar_->setVisible(false);
    search_bar_->setEnabled(true);
    status_label_->setText(tr("Indexing complete — %1 files indexed").arg(total_files));
    RebuildFileTree();
  }, Qt::QueuedConnection);
}

void MainWindow::OnIndexingError(const std::string& error) {
  QMetaObject::invokeMethod(this, [this, msg = QString::fromStdString(error)]() {
    QMessageBox::warning(this, tr("Indexing Error"), msg);
  }, Qt::QueuedConnection);
}

// ── SearchObserver ──

void MainWindow::OnSearchResultsReady(const std::vector<::SearchResult>& results,
                                      const ::SearchQuery& /*query*/) {
  QMetaObject::invokeMethod(this, [this, results]() {
    QList<::SearchResult> qlist;
    qlist.reserve(static_cast<int>(results.size()));
    for (const auto& r : results) {
      qlist.append(r);
    }
    result_list_->SetResults(qlist);
    status_label_->setText(tr("%1 results found").arg(results.size()));
  }, Qt::QueuedConnection);
}

void MainWindow::OnSearchError(const std::string& error) {
  QMetaObject::invokeMethod(this, [this, msg = QString::fromStdString(error)]() {
    QMessageBox::warning(this, tr("Search Error"), msg);
  }, Qt::QueuedConnection);
}

// ── Services ──

/**
 * @brief 初始化应用服务层：创建 IndexEngine、SearchService 并建立信号连接。
 *
 * - 确定数据库路径（AppDataLocation/index.db）
 * - 创建 IndexEngine 并注册 MainWindow 自身为观察者
 * - 创建 SearchService 并注册 MainWindow 自身为观察者
 * - 从 QSettings 加载上次的默认目录设置
 */
void MainWindow::SetupServices() {
  QString db_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (!QDir().mkpath(db_dir)) {
    QMessageBox::critical(this, tr("Initialization Error"),
                          tr("Cannot create data directory: %1\n"
                             "Please check your filesystem permissions.")
                              .arg(db_dir));
    SWIFT_LOG_ERROR(QString("Cannot create data directory: %1").arg(db_dir));
    return;
  }
  QString db_path = db_dir + "/index.db";

  index_engine_ = std::make_unique<IndexEngine>(this);

  connect(index_engine_.get(), &IndexEngine::ProgressUpdated,
          this, &MainWindow::OnQtIndexingProgress);
  connect(index_engine_.get(), &IndexEngine::IndexingFinished,
          this, &MainWindow::OnQtIndexingFinished);
  connect(index_engine_.get(), &IndexEngine::IndexingError,
          this, [this](const QString& msg) {
            QMessageBox::warning(this, "Indexing Error", msg);
          });

  auto self = std::shared_ptr<swiftsearch::IndexingObserver>(
      this, [](swiftsearch::IndexingObserver*) {});
  index_engine_->AddObserver(self);

  index_engine_->Initialize(db_path);

  search_service_ = std::make_unique<SearchService>(index_engine_.get(), this);

  connect(search_service_.get(), &SearchService::ResultsReady,
          this, &MainWindow::OnQtResultsReady);
  connect(search_service_.get(), &SearchService::SearchError,
          this, [this](const QString& msg) {
            QMessageBox::warning(this, "Search Error", msg);
          });

  auto search_self = std::shared_ptr<swiftsearch::SearchObserver>(
      this, [](swiftsearch::SearchObserver*) {});
  search_service_->AddObserver(search_self);

  QSettings settings("SwiftSearch", "SwiftSearch");
  default_dir_ = settings.value("default_dir", "").toString();
}

/** @brief 构建 UI 布局：搜索栏在上方，结果列表在下方。 */
void MainWindow::SetupUi() {
  auto* central_widget = new QWidget(this);
  auto* layout = new QVBoxLayout(central_widget);

  search_bar_ = new SearchBarWidget(central_widget);
  result_list_ = new ResultListWidget(central_widget);

  result_list_->SetCommandHistory(&command_history_);

  layout->addWidget(search_bar_);
  layout->addWidget(result_list_, 1);

  setCentralWidget(central_widget);
}

/** @brief 连接 SearchBarWidget 的 FilterTextChanged 到过滤处理。 */
void MainWindow::SetupConnections() {
  connect(search_bar_, &SearchBarWidget::FilterTextChanged,
          this, &MainWindow::OnFilterTextChanged);
}

/**
 * @brief 构建菜单栏：File（Index/Settings/Quit）、Edit（Undo/Redo）。
 *
 * Undo/Redo 使用标准快捷键 Ctrl+Z / Ctrl+Shift+Z。
 */
void MainWindow::SetupMenuBar() {
  auto* file_menu = menuBar()->addMenu(tr("&File"));

  auto* index_action = file_menu->addAction(tr("&Index Directory..."));
  connect(index_action, &QAction::triggered, this, &MainWindow::OnStartIndexing);

  auto* settings_action = file_menu->addAction(tr("&Settings..."));
  connect(settings_action, &QAction::triggered, this, &MainWindow::OnSettingsRequested);

  file_menu->addSeparator();

  auto* quit_action = file_menu->addAction(tr("&Quit"));
  quit_action->setShortcut(QKeySequence::Quit);
  connect(quit_action, &QAction::triggered, qApp, &QApplication::quit);

  auto* edit_menu = menuBar()->addMenu(tr("&Edit"));

  undo_action_ = edit_menu->addAction(tr("&Undo"));
  undo_action_->setShortcut(QKeySequence::Undo);
  connect(undo_action_, &QAction::triggered, this, &MainWindow::OnUndo);

  redo_action_ = edit_menu->addAction(tr("&Redo"));
  redo_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z));
  connect(redo_action_, &QAction::triggered, this, &MainWindow::OnRedo);

  UpdateUndoRedoActions();
}

/** @brief 构建状态栏：左侧文本标签 + 右侧进度条。 */
void MainWindow::SetupStatusBar() {
  status_label_ = new QLabel(tr("Ready"), this);
  progress_bar_ = new QProgressBar(this);
  progress_bar_->setMaximumWidth(200);
  progress_bar_->setMaximumHeight(16);
  progress_bar_->setVisible(false);

  statusBar()->addWidget(status_label_, 1);
  statusBar()->addPermanentWidget(progress_bar_);
}

// ── Slots ──

/** @brief 解析查询文本并异步执行搜索。 */
void MainWindow::OnSearchTriggered(const QString& query_text) {
  SearchQuery query = QueryParser::Parse(query_text);
  status_label_->setText(tr("Searching..."));
  search_service_->Search(query);
}

/** @brief 过滤文件树：当用户在过滤栏输入文本时调用。 */
void MainWindow::OnFilterTextChanged(const QString& text) {
  result_list_->FilterTree(text);
}

/** @brief 显示搜索结果（Qt 信号回调）。 */
void MainWindow::OnQtResultsReady(const QList<SearchResult>& results) {
  result_list_->SetResults(results);
  status_label_->setText(tr("%1 results found").arg(results.size()));
}

/**
 * @brief 打开设置对话框，若默认目录变更则重新索引。
 *
 * 使用 QSettings 持久化设置，支持跨 session 保存。
 */
void MainWindow::OnSettingsRequested() {
  QString old_dir = default_dir_;
  SettingsDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    QString new_dir = dialog.DefaultDirectory();
    if (new_dir != old_dir) {
      default_dir_ = new_dir;
      QSettings settings("SwiftSearch", "SwiftSearch");
      settings.setValue("default_dir", default_dir_);
      first_tree_built_ = false;
      if (!default_dir_.isEmpty()) {
        result_list_->Clear();
        status_label_->setText(tr("Indexing..."));
        progress_bar_->setVisible(true);
        progress_bar_->setValue(0);
        search_bar_->setEnabled(false);
        index_engine_->StartIndexing({default_dir_});
      }
    }
  }
}

/** @brief 弹出目录选择对话框并启动索引。 */
void MainWindow::OnStartIndexing() {
  QString dir = QFileDialog::getExistingDirectory(this, tr("Select Directory to Index"),
                                                   default_dir_.isEmpty()
                                                       ? QDir::homePath()
                                                       : default_dir_);
  if (dir.isEmpty()) return;

  QStringList paths{dir};
  status_label_->setText(tr("Indexing..."));
  progress_bar_->setVisible(true);
  progress_bar_->setValue(0);
  search_bar_->setEnabled(false);

  index_engine_->StartIndexing(paths);
}

/** @brief 索引进度回调：更新进度条和状态标签。 */
void MainWindow::OnQtIndexingProgress(int files_indexed) {
  progress_bar_->setValue(progress_bar_->value() + files_indexed);
  status_label_->setText(tr("Indexed %1 files...")
                             .arg(index_engine_->TotalFilesIndexed()));
}

/** @brief 索引完成回调：隐藏进度条、恢复搜索栏、重建文件树。 */
void MainWindow::OnQtIndexingFinished() {
  progress_bar_->setVisible(false);
  search_bar_->setEnabled(true);
  status_label_->setText(tr("Indexing complete — %1 files indexed")
                             .arg(index_engine_->TotalFilesIndexed()));
  RebuildFileTree();
}

/** @brief 撤销最后一条命令并更新 UI 状态。 */
void MainWindow::OnUndo() {
  command_history_.Undo();
  UpdateUndoRedoActions();
}

/** @brief 重做最后一条撤销的命令并更新 UI 状态。 */
void MainWindow::OnRedo() {
  command_history_.Redo();
  UpdateUndoRedoActions();
}

/** @brief 更新 Undo/Redo 菜单动作的可用状态和描述文本。 */
void MainWindow::UpdateUndoRedoActions() {
  if (undo_action_) {
    undo_action_->setEnabled(command_history_.CanUndo());
    if (command_history_.CanUndo()) {
      undo_action_->setText(tr("Undo %1").arg(command_history_.UndoDescription()));
    } else {
      undo_action_->setText(tr("Undo"));
    }
  }
  if (redo_action_) {
    redo_action_->setEnabled(command_history_.CanRedo());
    if (command_history_.CanRedo()) {
      redo_action_->setText(tr("Redo %1").arg(command_history_.RedoDescription()));
    } else {
      redo_action_->setText(tr("Redo"));
    }
  }
}

/**
 * @brief 启动时自动索引默认目录。
 *
 * 使用 QTimer::singleShot(0) 延迟执行，确保 UI 完全初始化后再开始索引。
 */
void MainWindow::TryAutoIndex() {
  if (default_dir_.isEmpty()) return;
  if (!QDir(default_dir_).exists()) {
    SWIFT_LOG_WARNING(QString("Default index directory does not exist: %1").arg(default_dir_));
    return;
  }

  SWIFT_LOG_INFO(QString("Auto-indexing default directory: %1").arg(default_dir_));
  status_label_->setText(tr("Indexing..."));
  progress_bar_->setVisible(true);
  progress_bar_->setValue(0);
  search_bar_->setEnabled(false);

  index_engine_->StartIndexing({default_dir_});
}

/**
 * @brief 从数据库读取全部文件并重建文件树视图。
 *
 * 在索引完成后和首次启动自动索引后调用。
 */
void MainWindow::RebuildFileTree() {
  auto* db = index_engine_->GetDatabase();
  if (!db) return;

  QList<FileEntry> entries = db->QueryAll();
  result_list_->BuildTree(entries);
  first_tree_built_ = true;

  SWIFT_LOG_DEBUG(QString("File tree rebuilt with %1 entries").arg(entries.size()));
}
