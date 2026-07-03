#pragma once

#include <QMainWindow>
#include <memory>

#include "../controller/search_types.h"
#include "../model/indexing_observer.h"
#include "../controller/search_observer.h"
#include "../controller/command_history.h"

class SearchService;
class IndexEngine;
class SearchBarWidget;
class ResultListWidget;
class QLabel;
class QProgressBar;
class QAction;

/**
 * @brief SwiftSearch 主窗口。
 *
 * 实现 IndexingObserver 和 SearchObserver 接口，
 * 使得后台扫描/搜索线程完成工作后 UI 自动刷新。
 *
 * 同时管理 CommandHistory 以提供撤销/重做功能。
 *
 * @see IndexEngine, SearchService, CommandHistory
 */
class MainWindow : public QMainWindow,
                   public swiftsearch::IndexingObserver,
                   public swiftsearch::SearchObserver {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

  // ── IndexingObserver 接口 ──
  void OnIndexingProgress(int files_indexed, int64_t total_indexed) override;
  void OnIndexingFinished(int64_t total_files) override;
  void OnIndexingError(const std::string& error) override;

  // ── SearchObserver 接口 ──
  void OnSearchResultsReady(const std::vector<::SearchResult>& results,
                            const ::SearchQuery& query) override;
  void OnSearchError(const std::string& error) override;

 private slots:
  void OnSearchTriggered(const QString& query_text);
  void OnFilterTextChanged(const QString& text);
  void OnQtResultsReady(const QList<::SearchResult>& results);
  void OnSettingsRequested();
  void OnStartIndexing();
  void OnQtIndexingProgress(int files_indexed);
  void OnQtIndexingFinished();
  void OnUndo();
  void OnRedo();

 private:
  void SetupUi();
  void SetupServices();
  void SetupConnections();
  void SetupMenuBar();
  void SetupStatusBar();
  void UpdateUndoRedoActions();
  void TryAutoIndex();
  void RebuildFileTree();

  SearchBarWidget* search_bar_ = nullptr;
  ResultListWidget* result_list_ = nullptr;

  std::unique_ptr<IndexEngine> index_engine_;
  std::unique_ptr<SearchService> search_service_;

  swiftsearch::CommandHistory command_history_;

  QLabel* status_label_ = nullptr;
  QProgressBar* progress_bar_ = nullptr;

  QAction* undo_action_ = nullptr;
  QAction* redo_action_ = nullptr;

  QString default_dir_;
  bool first_tree_built_ = false;
};
