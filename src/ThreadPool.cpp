#include "ThreadPool.h"

#include <stdexcept>

ThreadPool::ThreadPool(std::size_t thread_count) {
  if (thread_count == 0) thread_count = 1;
  workers_.reserve(thread_count);
  for (std::size_t i = 0; i < thread_count; ++i) {
    workers_.emplace_back([this] { worker_loop(); });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = true;
  }
  cv_.notify_all();
  for (auto& t : workers_) {
    if (t.joinable()) t.join();
  }

  // Ensure any remaining queued tasks are released.
  {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!tasks_.empty()) {
      tasks_.pop();
    }
  }
}

void ThreadPool::enqueue(std::function<void()> task) {
  if (!task) throw std::invalid_argument("ThreadPool::enqueue: empty task");
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_) throw std::runtime_error("ThreadPool::enqueue: pool stopped");
    tasks_.push(std::move(task));
  }
  cv_.notify_one();
}

void ThreadPool::worker_loop() {
  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
      if (stop_ && tasks_.empty()) return;
      task = std::move(tasks_.front());
      tasks_.pop();
    }
    try {
      task();
    } catch (...) {
      // Swallow exceptions from tasks to keep worker thread alive.
    }
  }
}

