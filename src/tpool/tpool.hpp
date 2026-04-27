/// Very simple implementation of a thread pool.

#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
  ThreadPool() = delete;                             // Default constructor.
  ~ThreadPool() noexcept;                            // Destructor.
  ThreadPool(const ThreadPool&) = delete;            // Copy constructor.
  ThreadPool& operator=(const ThreadPool&) = delete; // Copy constructor.
  ThreadPool(ThreadPool&&) = delete;                 // Move constructor.
  ThreadPool& operator=(ThreadPool&&) = delete;      // Move assignment.

  explicit ThreadPool(std::size_t n);
  void push(std::function<void()>);

private:
  static void runner(ThreadPool& e);
  std::queue<std::function<void()>> tasks;
  std::condition_variable condvar;
  std::vector<std::thread> pool;
  std::mutex mutex;
  bool stop;
};

/// Return the number of threads in a simpler signature.
inline std::size_t hardware_threads() noexcept {
  return static_cast<std::size_t>(std::thread::hardware_concurrency());
}
