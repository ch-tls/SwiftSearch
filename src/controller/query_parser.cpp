/**
 * @file query_parser.cpp
 * @brief QueryParser 实现：将用户输入的原始查询字符串解析为结构化 SearchQuery。
 *
 * 支持的查询语法：
 * - size>100M       设置最小文件大小
 * - size<1G         设置最大文件大小（支持 K/M/G/T 单位）
 * - re:pattern      启用正则表达式模式
 * - 其余文本         作为文件名匹配模式
 *
 * @see QueryParser, SearchQuery
 */

#include "query_parser.h"

#include <QRegularExpression>
#include <QStringList>

/**
 * @brief 解析原始查询字符串。
 *
 * 按以下顺序提取结构化信息：
 * 1. 大小过滤条件 (size>/<)
 * 2. 正则标志 (re:)
 * 3. 剩余文本作为文件名模式
 *
 * @param raw_query 用户输入的原始查询字符串
 * @return 解析后的 SearchQuery
 */
SearchQuery QueryParser::Parse(const QString& raw_query) {
  SearchQuery query;
  QString remaining = raw_query.trimmed();

  static const QRegularExpression kSizeRegex(R"(size\s*[:<>]\s*(\d+)([KMGT]?))",
                                             QRegularExpression::CaseInsensitiveOption);
  auto size_match = kSizeRegex.match(remaining);
  if (size_match.hasMatch()) {
    int64_t value = size_match.captured(1).toLongLong();
    QString unit = size_match.captured(2).toUpper();
    if (unit == "K") value *= 1024;
    else if (unit == "M") value *= 1024 * 1024;
    else if (unit == "G") value *= 1024LL * 1024 * 1024;
    else if (unit == "T") value *= 1024LL * 1024 * 1024 * 1024;

    if (remaining.contains("size>")) {
      query.min_size = value;
    } else if (remaining.contains("size<")) {
      query.max_size = value;
    }
    remaining.remove(size_match.captured(0));
  }

  static const QRegularExpression kRegexFlag(R"(re\s*:\s*)",
                                            QRegularExpression::CaseInsensitiveOption);
  if (kRegexFlag.match(remaining).hasMatch()) {
    query.use_regex = true;
    remaining.remove(kRegexFlag);
  }

  query.name_pattern = remaining.trimmed();

  return query;
}
