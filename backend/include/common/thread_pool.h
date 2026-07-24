// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT
//
// A small fixed-size thread pool used to service HTTP connections and
// crawl tasks without spawning a thread per unit of work.

#ifndef DISTRIBUTED_SEARCH_COMMON_THREAD_POOL_H_
#define DISTRIBUTED_SEARCH_COMMON_THREAD_POOL_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace dse {
namespace common {

class ThreadPool {
 public:
  explicit ThreadPool(size_t num_threads);
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  // Enqueues a task and returns a future for its result.
  template <typename Fn, typename... Args>
  auto Enqueue(Fn&& fn, Args&&... args)
      -> std::future<std::invoke_result_t<Fn, Args...>> {
    using ReturnType = std::invoke_result_t<Fn, Args...>;

    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));
    std::future<ReturnType> result = task->get_future();
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      if (stopping_) {
        throw std::runtime_error("ThreadPool: Enqueue on stopped pool");
      }
      tasks_.emplace([task]() { (*task)(); });
    }
    condition_.notify_one();
    return result;
  }

  size_t PendingTasks();

 private:
  void WorkerLoop();

  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex queue_mutex_;
  std::condition_variable condition_;
  std::atomic<bool> stopping_{false};
};

}  // namespace common
}  // namespace dse

#endif  // DISTRIBUTED_SEARCH_COMMON_THREAD_POOL_H_
