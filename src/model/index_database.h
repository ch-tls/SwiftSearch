#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QList>
#include <memory>
#include <string>

#include "file_entry.h"

/**
 * @brief 基于 SQLite 的文件索引数据库。
 *
 * 提供文件条目的 CRUD 操作和按路径/名称/大小的查询功能。
 * 使用 SQLite WAL 模式获得更好的并发写入性能。
 *
 * @note 所有查询方法返回 QList<FileEntry>，按数据库自然顺序排列。
 * @see FileEntry
 */
class IndexDatabase : public QObject {
  Q_OBJECT

 public:
  /**
   * @param db_path SQLite 数据库文件路径
   * @param parent 父 QObject
   */
  explicit IndexDatabase(const QString& db_path, QObject* parent = nullptr);
  ~IndexDatabase() override;

  /** @brief 打开数据库并创建表结构。 */
  bool Initialize(QString* error_out = nullptr);

  /** @brief 插入或替换单条文件记录。 */
  bool InsertFile(const FileEntry& entry);

  /** @brief 批量插入文件记录（在事务中执行）。 */
  bool InsertFiles(const QList<FileEntry>& entries);

  /** @brief 删除指定路径的文件记录。 */
  bool RemoveFile(const QString& file_path);

  /** @brief 清空所有索引数据。 */
  bool ClearAll();

  /** @brief 按路径模糊查询。 */
  QList<FileEntry> QueryByPath(const QString& path_pattern) const;

  /** @brief 按文件名模糊查询。 */
  QList<FileEntry> QueryByName(const QString& name_pattern) const;

  /** @brief 返回全部索引条目。 */
  QList<FileEntry> QueryAll() const;

  /** @brief 按文件大小范围查询。 */
  QList<FileEntry> QueryBySize(int64_t min_size, int64_t max_size) const;

  /** @brief 返回已索引文件总数。 */
  int64_t TotalCount() const;

  /** @brief 返回已索引文件总大小。 */
  int64_t TotalSize() const;

 private:
  bool CreateTables();
  FileEntry RowToEntry(const class QSqlQuery& query) const;

  QSqlDatabase database_;
  QString db_path_;
};
