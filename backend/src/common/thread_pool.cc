// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#include "common/thread_pool.h"

namespace dse {
namespace common {

ThreadPool::ThreadPool(size_t num_threads) {
  for (size_t i = 0; i < num_threads; ++i) {
    workers_.emplace_back([this] { WorkerLoop(); });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    stopping_ = true;
  }
  condition_.notify_all();
  for (std::thread& worker : workers_) {
    if (worker.joinable()) worker.join();
  }
}

size_t ThreadPool::PendingTasks() {
  std::unique_lock<std::mutex> lock(queue_mutex_);
  return tasks_.size();
}

void ThreadPool::WorkerLoop() {
  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      condition_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
      if (stopping_ && tasks_.empty()) return;
      task = std::move(tasks_.front());
      tasks_.pop();
    }
    task();
  }
}

}  // namespace common
}  // namespace dse
