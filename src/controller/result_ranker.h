#pragma once

#include <QString>
#include "../model/file_entry.h"

/**
 * @brief 搜索结果相关性评分器。
 *
 * 根据文件名匹配度、路径匹配度和文件大小因子
 * 计算每个搜索结果的相关性评分。
 *
 * 评分规则（优先级递减）：
 * - 文件名精确匹配: 10.0 (×2 权重)
 * - 文件名前缀匹配: 8.0 (×2)
 * - 通配符匹配:      6.0 (×2)
 * - 子串匹配:        3.0 (×2)
 * - 路径匹配:        同文件名规则 (×1)
 * - 文件大小因子:    log2(size+1) * 0.1
 *
 * @see SearchResult
 */
class ResultRanker {
 public:
  /**
   * @brief 计算文件条目的相关性评分。
   * @param entry 文件条目
   * @param name_pattern 用户输入的文件名匹配模式
   * @param path_pattern 用户输入的路径匹配模式
   * @return 相关性评分（0.0 表示完全不匹配）
   */
  static double Score(const FileEntry& entry,
                      const QString& name_pattern,
                      const QString& path_pattern);

 private:
  static double FuzzyMatchScore(const QString& text, const QString& pattern);
  static bool ContainsSequence(const QString& text, const QString& pattern, bool case_sensitive);
};
