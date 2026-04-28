/// Source file of the thread pool.

#include "./tpool.hpp"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

/// Constructor of the class.
ThreadPool::ThreadPool(std::size_t n) : stop(false) {
  pool.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    pool.emplace_back(runner, std::ref(*this));
  }
}

/// Function that each thread will run.
void ThreadPool::runner(ThreadPool& e) {
  std::function<void()> task;
  while (true) {
    {
      // Mutex locked.
      std::unique_lock lock(e.mutex);
      // Mutex unlocked while thread waits.
      e.condvar.wait(lock, [&] { return e.stop || !e.tasks.empty(); });
      // If the stop flag is set and there are not more tasks pending.
      if (e.stop && e.tasks.empty()) {
        return; // The thread stops.
      }
      // Normal execution, call the next task.
      if (!e.tasks.empty()) {
        // Extract the next funtion reference.
        task = std::move(e.tasks.front());
        e.tasks.pop();
      }
    } // Mutex unlocked.
    task();
  }
}

/// Destructor.
ThreadPool::~ThreadPool() noexcept {
  {
    // Mutex locked.
    std::unique_lock lock(this->mutex);
    this->stop = true;
  } // Lock out of scope (Mutex unlocked).
  condvar.notify_all();
  for (auto& thread : this->pool) {
    thread.join();
  }
}

/// Push a new element into the queue.
void ThreadPool::push(std::function<void()> fn) {
  {
    std::unique_lock lock(this->mutex);
    this->tasks.push(std::move(fn));
  }
  this->condvar.notify_one();
}
