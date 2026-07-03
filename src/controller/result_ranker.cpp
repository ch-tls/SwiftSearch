/**
 * @file result_ranker.cpp
 * @brief ResultRanker 实现：搜索结果的相关性评分算法。
 *
 * 评分策略（越精确匹配得分越高）：
 * - 完全匹配：10.0 分
 * - 前缀匹配：8.0 分
 * - 通配符匹配：6.0 分
 * - 子串匹配：3.0 分
 * - 无匹配：0.0 分
 *
 * 文件名匹配权重为路径匹配的 2 倍。
 * 大文件有轻微加分（log2 尺度，系数 0.1）。
 *
 * @see ResultRanker, SearchResult
 */

#include "result_ranker.h"

#include <QRegularExpression>
#include <cmath>

/**
 * @brief 计算文件条目对查询的总体相关度得分。
 *
 * @param entry 候选文件条目
 * @param name_pattern 文件名匹配模式
 * @param path_pattern 路径匹配模式
 * @return 综合相关度得分
 */
double ResultRanker::Score(const FileEntry& entry,
                           const QString& name_pattern,
                           const QString& path_pattern) {
  double score = 0.0;

  if (!name_pattern.isEmpty()) {
    score += FuzzyMatchScore(entry.name, name_pattern) * 2.0;
  }

  if (!path_pattern.isEmpty()) {
    score += FuzzyMatchScore(entry.path, path_pattern) * 1.0;
  }

  if (score > 0.0 && entry.size > 0) {
    score += std::log2(static_cast<double>(entry.size) + 1.0) * 0.1;
  }

  return score;
}

/**
 * @brief 模糊匹配评分：依次尝试精确、前缀、通配符、子串匹配。
 *
 * 匹配等级（降序）：
 * 1. 精确匹配（忽略大小写）：10.0
 * 2. 前缀匹配：8.0
 * 3. 通配符匹配：6.0
 * 4. 子串包含匹配：3.0
 * 5. 不匹配：0.0
 *
 * @param text 目标文本
 * @param pattern 匹配模式
 * @return 匹配得分
 */
double ResultRanker::FuzzyMatchScore(const QString& text, const QString& pattern) {
  if (pattern.isEmpty()) return 0.0;

  QString lower_text = text.toLower();
  QString lower_pattern = pattern.toLower();

  if (lower_text == lower_pattern) {
    return 10.0;
  }

  if (lower_text.startsWith(lower_pattern)) {
    return 8.0;
  }

  QRegularExpression::WildcardConversionOptions options;
  QString wildcard_to_regex_str =
      QRegularExpression::wildcardToRegularExpression(pattern, options);
  QRegularExpression wildcard_re(wildcard_to_regex_str,
                                 QRegularExpression::CaseInsensitiveOption);

  if (wildcard_re.match(text).hasMatch()) {
    return 6.0;
  }

  if (ContainsSequence(text, pattern, false)) {
    return 3.0;
  }

  return 0.0;
}

/** @brief 检查文本是否包含模式字符串（子串匹配）。 */
bool ResultRanker::ContainsSequence(const QString& text, const QString& pattern,
                                    bool case_sensitive) {
  Qt::CaseSensitivity cs = case_sensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
  return text.contains(pattern, cs);
}
