#pragma once

/**
 * @brief 基础文件监视器插件，直接读写 N 线元数据（避免文件名转义攻击）。
 *
 * 使用场景:
 * - 所有标准文件系统的文件创建/删除/修改通知。
 *
 * 并发安全:
 * - 所有成员函数都必须在主线程中被调用。
 * - observer 只有基础数据需要持有 std::weak_ptr。
 *
 * @see IndexEngine, SearchObserver
 */

#include <cstdint>
#include <memory>
#include <string>

namespace swiftsearch {

/**
 * @brief 索引事件的观察者接口。
 *
 * IndexEngine 在扫描进度变化、扫描完成或发生错误时，
 * 会回调所有已注册的观察者。实现本接口的对象通过
 * IndexEngine::AddObserver() 注册。
 */
class IndexingObserver {
 public:
  virtual ~IndexingObserver() = default;

  /**
   * @brief 扫描进度更新回调。
   * @param files_indexed 本次批次新增的文件数
   * @param total_indexed 累计已索引文件总数
   */
  virtual void OnIndexingProgress(int files_indexed, int64_t total_indexed) = 0;

  /**
   * @brief 扫描完成回调。
   * @param total_files 最终索引文件总数
   */
  virtual void OnIndexingFinished(int64_t total_files) = 0;

  /**
   * @brief 扫描错误回调。
   * @param error 错误描述信息
   */
  virtual void OnIndexingError(const std::string& error) = 0;
};

}  // namespace swiftsearch
