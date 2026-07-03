/**
 * @file sort_util.h
 * @brief 基于 C++20 ranges 的稳定排序工具函数模板。
 *
 * 提供三个排序工具：
 * - StableSort：原地稳定排序
 * - StableSortAndTruncate：排序后截断到指定数量
 * - StableSorted：返回排序后的新副本
 *
 * 全部使用 std::stable_sort 保证等 key 元素保持原始相对顺序。
 *
 * @see SearchService, ResultRanker
 */
#pragma once

#include <algorithm>
#include <ranges>
#include <vector>

namespace swiftsearch::util {

/**
 * @brief 原地稳定排序。
 *
 * @tparam R 随机访问 range 类型
 * @tparam Comp 比较函数类型（默认 less）
 * @param range 待排序的 range
 * @param comp 比较函数对象
 */
template <std::ranges::random_access_range R, typename Comp = std::ranges::less>
void StableSort(R& range, Comp comp = {}) {
  std::stable_sort(std::ranges::begin(range), std::ranges::end(range), comp);
}

/**
 * @brief 排序后截断至 max_count，丢弃排名靠后的元素。
 *
 * 先执行完整排序，然后只保留前 max_count 个结果。
 * 常用于搜索结果的 Top-N 显示。
 *
 * @tparam R 随机访问 range 类型
 * @tparam Comp 比较函数类型（默认 less）
 * @param range 待排序的 range
 * @param max_count 保留的最大元素数量
 * @param comp 比较函数对象
 */
template <std::ranges::random_access_range R, typename Comp = std::ranges::less>
void StableSortAndTruncate(R& range, std::size_t max_count, Comp comp = {}) {
  StableSort(range, comp);
  if (std::ranges::size(range) > max_count) {
    auto offset = static_cast<typename R::difference_type>(max_count);
    range.erase(std::ranges::begin(range) + offset, std::ranges::end(range));
  }
}

/**
 * @brief 排序并返回新容器，不修改原 range。
 *
 * 将 range 元素拷贝到新 std::vector 中排序后返回。
 *
 * @tparam R 随机访问 range 类型
 * @tparam Comp 比较函数类型（默认 less）
 * @param range 待排序的 range
 * @param comp 比较函数对象
 * @return 排序后的 std::vector 副本
 */
template <std::ranges::random_access_range R, typename Comp = std::ranges::less>
auto StableSorted(const R& range, Comp comp = {})
    -> std::vector<std::ranges::range_value_t<R>> {
  std::vector<std::ranges::range_value_t<R>> result(
      std::ranges::begin(range), std::ranges::end(range));
  std::stable_sort(result.begin(), result.end(), comp);
  return result;
}

}  // namespace swiftsearch::util
