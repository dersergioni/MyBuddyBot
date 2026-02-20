#include "infra/TaskQueue.h"

#include "core/Logger.h"

namespace mbb
{

TaskQueue::TaskQueue(size_t workerCount)
{
    if (workerCount == 0)
    {
        workerCount = 1;
    }

    workers_.reserve(workerCount);
    for (size_t i = 0; i < workerCount; ++i)
    {
        workers_.emplace_back(&TaskQueue::WorkerLoop, this);
    }
}

TaskQueue::~TaskQueue()
{
    Stop();
}

void TaskQueue::Enqueue(std::function<void()> task)
{
    if (!task)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_)
        {
            return;
        }
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void TaskQueue::Stop()
{
    const bool wasStopping = stopping_.exchange(true);
    if (wasStopping)
    {
        return;
    }

    cv_.notify_all();
    for (auto& worker : workers_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
    workers_.clear();
}

void TaskQueue::WorkerLoop()
{
    while (true)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
            if (stopping_ && tasks_.empty())
            {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }

        try
        {
            task();
        }
        catch (const std::exception& e)
        {
            Logger::Error(std::string("TaskQueue task failed: ") + e.what());
        }
        catch (...)
        {
            Logger::Error("TaskQueue task failed with unknown exception");
        }
    }
}

} // namespace mbb
