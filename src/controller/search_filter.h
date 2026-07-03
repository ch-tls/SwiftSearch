/**
 * @file search_filter.h
 * @brief C++20 ranges 过滤器适配器，提供声明式的搜索过滤管道。
 *
 * 每个函数返回一个 std::views::filter 闭包，可通过管道操作符 (|) 组合：
 * @code
 * auto results = results
 *     | views::ByNameContains("foo")
 *     | views::ByMinSize(1024)
 *     | views::TopN(100)
 *     | views::Materialize;
 * @endcode
 *
 * @see SearchResult, SearchQuery
 */
#pragma once

#include <ranges>
#include <string_view>
#include <vector>

#include "search_types.h"

namespace swiftsearch::views {

/** @brief 按文件名包含指定关键字过滤（大小写不敏感）。 */
inline auto ByNameContains(std::string_view keyword) {
  auto kw = QString::fromStdString(std::string(keyword));
  return std::views::filter([kw](const SearchResult& r) {
    return r.file_entry.name.contains(kw, Qt::CaseInsensitive);
  });
}

/** @brief 按文件路径包含指定关键字过滤（大小写不敏感）。 */
inline auto ByPathContains(std::string_view keyword) {
  auto kw = QString::fromStdString(std::string(keyword));
  return std::views::filter([kw](const SearchResult& r) {
    return r.file_entry.path.contains(kw, Qt::CaseInsensitive);
  });
}

/** @brief 按最小文件大小过滤（>= min_size）。 */
inline auto ByMinSize(int64_t min_size) {
  return std::views::filter([min_size](const SearchResult& r) {
    return r.file_entry.size >= min_size;
  });
}

/** @brief 按最大文件大小过滤（<= max_size）。 */
inline auto ByMaxSize(int64_t max_size) {
  return std::views::filter([max_size](const SearchResult& r) {
    return r.file_entry.size <= max_size;
  });
}

/** @brief 截取前 n 个结果。 */
inline auto TopN(std::size_t n) {
  return std::views::take(n);
}

/**
 * @brief 将 range 管道结果物化为 std::vector。
 *
 * C++20 views 为惰性求值，Materialize 强制遍历并收集元素。
 */
template <std::ranges::input_range R>
auto Materialize(R&& range)
    -> std::vector<std::ranges::range_value_t<R>> {
  std::vector<std::ranges::range_value_t<R>> result;
  for (auto&& item : range) {
    result.push_back(item);
  }
  return result;
}

}  // namespace swiftsearch::views
