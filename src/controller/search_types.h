#pragma once

#include <QString>
#include "../model/file_entry.h"

/**
 * @brief 搜索查询条件。
 *
 * 支持按文件名、路径、内容、大小范围过滤，
 * 可指定是否大小写敏感、是否使用正则表达式。
 *
 * @see SearchService, QueryParser
 */
struct SearchQuery {
  QString name_pattern;       ///< 文件名匹配模式（支持通配符）
  QString path_pattern;       ///< 路径匹配模式
  QString content_pattern;    ///< 文件内容匹配模式（预留）
  int64_t min_size = -1;      ///< 最小文件大小（-1 表示不限制）
  int64_t max_size = -1;      ///< 最大文件大小（-1 表示不限制）
  bool case_sensitive = false;///< 是否大小写敏感
  bool use_regex = false;     ///< 是否使用正则表达式
  int max_results = 500;      ///< 最大返回结果数
};

/**
 * @brief 搜索结果条目。
 *
 * 包含匹配到的 FileEntry 及其相关性评分。
 * 评分越高表示匹配越精确。
 *
 * @see ResultRanker
 */
struct SearchResult {
  FileEntry file_entry;         ///< 文件条目
  double relevance_score = 0.0; ///< 相关性评分
};

Q_DECLARE_METATYPE(SearchResult)
Q_DECLARE_METATYPE(SearchQuery)
