//
// perfTest.h
//
// Created by massimo on 8/9/18.
//
#pragma once

#include "../memvar.h"
#include <chrono>

////////////////////////////////////////////////////////////////////////////////
// a simple function's performance timer
template <typename Clock = std::chrono::high_resolution_clock>
struct perftimer
{
  template <typename F, typename... Args>
  static
  auto
  duration(F&& f, Args... args)
  {
    typename Clock::time_point start = Clock::now();

    std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    // pre-C++17
    //f(std::forward<Args>(args)...);

    typename Clock::time_point end = Clock::now();

    return std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
  }
};
