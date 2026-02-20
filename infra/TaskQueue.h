#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace mbb
{

class TaskQueue
{
  public:
    explicit TaskQueue(size_t workerCount);
    ~TaskQueue();

    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    void Enqueue(std::function<void()> task);
    void Stop();

  private:
    void WorkerLoop();

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    std::atomic<bool> stopping_{false};
};

} // namespace mbb
