/**
 * @file index_database.cpp
 * @brief IndexDatabase 实现：SQLite 文件索引的增删改查操作。
 *
 * 数据库表结构：
 *   files(id, path UNIQUE, name, size, modified_time, is_directory)
 *
 * 使用 WAL 日志模式、NORMAL 同步级别和 8MB 缓存以优化并发读写性能。
 *
 * @see IndexDatabase, FileEntry
 */

#include "index_database.h"
#include "../util/log_manager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

IndexDatabase::IndexDatabase(const QString& db_path, QObject* parent)
    : QObject(parent), db_path_(db_path) {}

IndexDatabase::~IndexDatabase() {
  QString conn_name = db_path_;
  if (database_.isOpen()) {
    database_.close();
  }
  database_ = QSqlDatabase();
  QSqlDatabase::removeDatabase(conn_name);
}

/**
 * @brief 初始化数据库连接并创建表结构。
 *
 * 使用命名连接避免与全局默认连接冲突。
 * 设置 WAL 模式 + NORMAL 同步以平衡性能和安全性。
 *
 * @param error_out 可选的错误信息输出参数
 * @return 是否初始化成功
 */
bool IndexDatabase::Initialize(QString* error_out) {
  try {
    database_ = QSqlDatabase::addDatabase("QSQLITE", db_path_);
    if (!database_.isValid()) {
      QString msg = QString("SQLite driver not available — check that sqldrivers/qsqlite.dll (or .so) is deployed alongside the executable");
      SWIFT_LOG_ERROR(msg);
      if (error_out) *error_out = msg;
      return false;
    }

    database_.setDatabaseName(db_path_);

    if (!database_.open()) {
      QString msg = QString("Failed to open database: %1").arg(database_.lastError().text());
      SWIFT_LOG_ERROR(msg);
      if (error_out) *error_out = msg;
      return false;
    }

    return CreateTables();
  } catch (const std::exception& e) {
    QString msg = QString("IndexDatabase::Initialize exception: %1").arg(e.what());
    SWIFT_LOG_ERROR(msg);
    if (error_out) *error_out = msg;
    return false;
  }
}

/**
 * @brief 创建 files 表和相关索引。
 *
 * 表结构: id (自增主键), path (唯一), name, size, modified_time, is_directory
 * 索引: idx_files_name (按名称查询), idx_files_size (按大小范围查询)
 * 性能优化: WAL 日志、NORMAL 同步、8MB 页面缓存
 *
 * @return 是否创建成功
 */
bool IndexDatabase::CreateTables() {
  QSqlQuery query(database_);

  const QString kCreateFilesTable = R"(
    CREATE TABLE IF NOT EXISTS files (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      path TEXT UNIQUE NOT NULL,
      name TEXT NOT NULL,
      size INTEGER NOT NULL DEFAULT 0,
      modified_time TEXT NOT NULL,
      is_directory INTEGER NOT NULL DEFAULT 0
    )
  )";

  if (!query.exec(kCreateFilesTable)) {
    SWIFT_LOG_ERROR(QString("Create files table failed: %1").arg(query.lastError().text()));
    return false;
  }

  const QString kCreateIndexPath = R"(
    CREATE INDEX IF NOT EXISTS idx_files_name ON files(name)
  )";
  query.exec(kCreateIndexPath);

  const QString kCreateIndexSize = R"(
    CREATE INDEX IF NOT EXISTS idx_files_size ON files(size)
  )";
  query.exec(kCreateIndexSize);

  query.exec("PRAGMA journal_mode=WAL");
  query.exec("PRAGMA synchronous=NORMAL");
  query.exec("PRAGMA cache_size=-8000");

  SWIFT_LOG_DEBUG(QString("Database tables created: %1").arg(db_path_));
  return true;
}

/**
 * @brief 插入或替换单条文件记录（按 path 唯一约束）。
 *
 * 使用 INSERT OR REPLACE: 若 path 已存在则更新所有字段，否则新增。
 *
 * @param entry 待插入的文件条目
 * @return 是否成功
 */
bool IndexDatabase::InsertFile(const FileEntry& entry) {
  try {
    QSqlQuery query(database_);
    query.prepare(R"(
      INSERT OR REPLACE INTO files (path, name, size, modified_time, is_directory)
      VALUES (:path, :name, :size, :modified_time, :is_directory)
    )");
    query.bindValue(":path", entry.path);
    query.bindValue(":name", entry.name);
    query.bindValue(":size", static_cast<qlonglong>(entry.size));
    query.bindValue(":modified_time", entry.modified_time.toString(Qt::ISODate));
    query.bindValue(":is_directory", entry.is_directory ? 1 : 0);

    if (!query.exec()) {
      SWIFT_LOG_WARNING(QString("InsertFile failed for %1: %2").arg(entry.path, query.lastError().text()));
      return false;
    }
    return true;
  } catch (const std::exception& e) {
    SWIFT_LOG_ERROR(QString("IndexDatabase::InsertFile exception: %1").arg(e.what()));
    return false;
  }
}

/**
 * @brief 批量插入文件记录，使用事务保证原子性。
 *
 * 所有插入在同一事务中执行：任一失败则回滚全部。
 *
 * @param entries 待插入的文件条目列表
 * @return 是否全部插入成功
 */
bool IndexDatabase::InsertFiles(const QList<FileEntry>& entries) {
  try {
    database_.transaction();
    QSqlQuery query(database_);
    query.prepare(R"(
      INSERT OR REPLACE INTO files (path, name, size, modified_time, is_directory)
      VALUES (:path, :name, :size, :modified_time, :is_directory)
    )");

    for (const auto& entry : entries) {
      query.bindValue(":path", entry.path);
      query.bindValue(":name", entry.name);
      query.bindValue(":size", static_cast<qlonglong>(entry.size));
      query.bindValue(":modified_time", entry.modified_time.toString(Qt::ISODate));
      query.bindValue(":is_directory", entry.is_directory ? 1 : 0);

      if (!query.exec()) {
        SWIFT_LOG_WARNING(QString("InsertFiles batch failed at %1: %2").arg(entry.path, query.lastError().text()));
        database_.rollback();
        return false;
      }
    }

    return database_.commit();
  } catch (const std::exception& e) {
    SWIFT_LOG_ERROR(QString("IndexDatabase::InsertFiles exception: %1").arg(e.what()));
    return false;
  }
}

/**
 * @brief 按路径删除文件索引记录。
 *
 * @param file_path 待删除文件的绝对路径
 * @return 是否成功
 */
bool IndexDatabase::RemoveFile(const QString& file_path) {
  try {
    QSqlQuery query(database_);
    query.prepare("DELETE FROM files WHERE path = :path");
    query.bindValue(":path", file_path);

    if (!query.exec()) {
      SWIFT_LOG_WARNING(QString("RemoveFile failed for %1: %2").arg(file_path, query.lastError().text()));
      return false;
    }
    return true;
  } catch (const std::exception& e) {
    SWIFT_LOG_ERROR(QString("IndexDatabase::RemoveFile exception: %1").arg(e.what()));
    return false;
  }
}

/** @brief 清空所有索引记录。 */
bool IndexDatabase::ClearAll() {
  QSqlQuery query(database_);
  return query.exec("DELETE FROM files");
}

/** @brief 按路径模糊查询（LIKE 模式）。 */
QList<FileEntry> IndexDatabase::QueryByPath(const QString& path_pattern) const {
  QList<FileEntry> results;
  QSqlQuery query(database_);
  query.prepare("SELECT * FROM files WHERE path LIKE :pattern");
  query.bindValue(":pattern", "%" + path_pattern + "%");

  if (query.exec()) {
    while (query.next()) {
      results.append(RowToEntry(query));
    }
  }
  return results;
}

/** @brief 按文件名模糊查询（LIKE 模式）。 */
QList<FileEntry> IndexDatabase::QueryByName(const QString& name_pattern) const {
  QList<FileEntry> results;
  QSqlQuery query(database_);
  query.prepare("SELECT * FROM files WHERE name LIKE :pattern");
  query.bindValue(":pattern", "%" + name_pattern + "%");

  if (query.exec()) {
    while (query.next()) {
      results.append(RowToEntry(query));
    }
  }
  return results;
}

/** @brief 查询全部已索引文件。 */
QList<FileEntry> IndexDatabase::QueryAll() const {
  QList<FileEntry> results;
  QSqlQuery query(database_);

  if (query.exec("SELECT * FROM files")) {
    while (query.next()) {
      results.append(RowToEntry(query));
    }
  }
  return results;
}

/** @brief 按文件大小范围查询（BETWEEN）。 */
QList<FileEntry> IndexDatabase::QueryBySize(int64_t min_size, int64_t max_size) const {
  QList<FileEntry> results;
  QSqlQuery query(database_);
  query.prepare("SELECT * FROM files WHERE size BETWEEN :min AND :max");
  query.bindValue(":min", static_cast<qlonglong>(min_size));
  query.bindValue(":max", static_cast<qlonglong>(max_size));

  if (query.exec()) {
    while (query.next()) {
      results.append(RowToEntry(query));
    }
  }
  return results;
}

/** @brief 获取已索引文件总数。 */
int64_t IndexDatabase::TotalCount() const {
  QSqlQuery query(database_);
  if (query.exec("SELECT COUNT(*) FROM files") && query.next()) {
    return query.value(0).toLongLong();
  }
  return 0;
}

/** @brief 获取已索引文件总大小（字节）。 */
int64_t IndexDatabase::TotalSize() const {
  QSqlQuery query(database_);
  if (query.exec("SELECT COALESCE(SUM(size), 0) FROM files") && query.next()) {
    return query.value(0).toLongLong();
  }
  return 0;
}

/**
 * @brief 将 QSqlQuery 当前行转换为 FileEntry 对象。
 *
 * 时间字段以 ISO 格式存储，读取时反序列化为 QDateTime。
 */
FileEntry IndexDatabase::RowToEntry(const QSqlQuery& query) const {
  FileEntry entry;
  entry.path = query.value("path").toString();
  entry.name = query.value("name").toString();
  entry.size = query.value("size").toLongLong();
  entry.modified_time = QDateTime::fromString(query.value("modified_time").toString(), Qt::ISODate);
  entry.is_directory = query.value("is_directory").toBool();
  return entry;
}
