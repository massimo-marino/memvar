//
// perfTest.h
//
#pragma once

#include <chrono>
#include <functional>
////////////////////////////////////////////////////////////////////////////////
// a simple function's performance timer
using namespace std::chrono_literals;

struct perftimer
{
  using Clock = std::chrono::steady_clock;
  using duration_sec = std::chrono::duration<double>;

  template <typename F, typename... Args>
  [[nodiscard]] static auto duration(F&& f, Args&&... args) {
    typename Clock::time_point start = Clock::now();

    std::invoke(std::forward<F>(f), std::forward<Args>(args)...);

    typename Clock::time_point end = Clock::now();

    // returns the duration as seconds
    return std::chrono::duration_cast<duration_sec>(end - start);
  }

  [[nodiscard]] static auto to_msec(const duration_sec& d) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
  }

  [[nodiscard]] static auto to_usec(const duration_sec& d) {
    return std::chrono::duration_cast<std::chrono::microseconds>(d).count();
  }

  [[nodiscard]] static auto to_nsec(const duration_sec& d) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(d).count();
  }
};  // struct perftimer
