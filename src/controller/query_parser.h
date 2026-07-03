#pragma once

#include <QString>
#include "search_types.h"

/**
 * @brief 查询字符串解析器。
 *
 * 将用户输入的原始查询字符串（如 "*.cpp size>1M"）
 * 解析为结构化的 SearchQuery 对象。
 *
 * @see SearchQuery, SearchService
 */
class QueryParser {
 public:
  /**
   * @brief 解析原始查询字符串。
   * @param raw_query 用户输入文本
   * @return 结构化的 SearchQuery
   */
  static SearchQuery Parse(const QString& raw_query);
};
