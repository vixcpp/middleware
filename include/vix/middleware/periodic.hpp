/**
 *
 *  @file periodic.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_PERIODIC_HPP
#define VIX_PERIODIC_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

#include <vix/executor/IExecutor.hpp>
#include <vix/executor/TaskOptions.hpp>

namespace vix::middleware
{
  class PeriodicTask final
  {
  public:
    using Clock = std::chrono::steady_clock;

    PeriodicTask(
        vix::executor::IExecutor &ex,
        std::chrono::milliseconds interval,
        std::function<void()> job,
        vix::executor::TaskOptions opt = {})
        : ex_(&ex),
          interval_(interval),
          job_(std::move(job)),
          opt_(opt)
    {
    }

    ~PeriodicTask()
    {
      stop();
    }

    PeriodicTask(const PeriodicTask &) = delete;
    PeriodicTask &operator=(const PeriodicTask &) = delete;
    PeriodicTask(PeriodicTask &&) = delete;
    PeriodicTask &operator=(PeriodicTask &&) = delete;

    void start()
    {
      if (running_.exchange(true))
        return;

      stop_flag_.store(false);
      thread_ = std::thread([this]()
                            { run_loop_(); });
    }

    void stop()
    {
      if (!running_.exchange(false))
        return;

      stop_flag_.store(true);
      if (thread_.joinable())
        thread_.join();
    }

    bool is_running() const noexcept
    {
      return running_.load();
    }

  private:
    void run_loop_()
    {
      auto next = Clock::now() + interval_;

      while (!stop_flag_.load(std::memory_order_relaxed))
      {
        std::this_thread::sleep_until(next);
        next += interval_;

        if (stop_flag_.load(std::memory_order_relaxed))
          break;

        if (ex_ && job_)
        {
          ex_->post(job_, opt_);
        }
      }
    }

  private:
    vix::executor::IExecutor *ex_{nullptr};
    std::chrono::milliseconds interval_{1000};
    std::function<void()> job_{};
    vix::executor::TaskOptions opt_{};
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_flag_{false};
    std::thread thread_;
  };

} // namespace vix::middleware

#endif
