#pragma once

/**
 * @brief 搜索结果观察者接口。
 *
 * SearchService 在搜索完成或出错时通知已注册的观察者。
 */

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "search_types.h"

namespace swiftsearch {

/**
 * @brief 搜索事件的观察者接口。
 */
class SearchObserver {
 public:
  virtual ~SearchObserver() = default;

  /**
   * @brief 搜索结果就绪回调。
   * @param results 排序后的搜索结果列表
   * @param query 触发本次搜索的查询条件
   */
  virtual void OnSearchResultsReady(const std::vector<::SearchResult>& results,
                                    const ::SearchQuery& query) = 0;

  /**
   * @brief 搜索错误回调。
   * @param error 错误描述
   */
  virtual void OnSearchError(const std::string& error) = 0;
};

}  // namespace swiftsearch
