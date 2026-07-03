#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>

/**
 * @brief 文件索引条目，描述一个被索引的文件或目录。
 *
 * FileEntry 是 IndexDatabase 和 search 结果之间的基本数据单元。
 * 通过 Q_DECLARE_METATYPE 注册为 Qt 元类型，支持跨线程 signal-slot 传递。
 *
 * @see IndexDatabase, SearchResult
 */
struct FileEntry {
  QString path;              ///< 文件绝对路径
  QString name;              ///< 文件名（不含路径）
  int64_t size = 0;          ///< 文件大小（字节）
  QDateTime modified_time;   ///< 最后修改时间
  bool is_directory = false; ///< 是否为目录
};

Q_DECLARE_METATYPE(FileEntry)
