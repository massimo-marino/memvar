# memvar

C++20 implementation of a variable with memory of its past values.
It does not compile for C++17.

## Description

The header `memvar.h` implements a small, header-only C++20 library, in namespace `mv`, whose central artifact is a class template `memvar<T>`: a scalar or string variable that **remembers its own past values**. Every assignment or arithmetic mutation does not overwrite the variable's state in the usual C++ sense; instead it *pushes* a new value onto a bounded history buffer, discarding the oldest entry once a configurable capacity is exceeded.

A second class template, `memvarTimed<T, Time, Clock>`, extends `memvar<T>` by attaching a timestamp to every historical value, expressed as a duration since the object's construction ("epoch").

This is a useful primitive for domains where "what was this variable a moment ago" matters as much as "what is it now" — undo buffers, telemetry/sample smoothing, simple change-tracking, watchpoint-style debugging aids, or lightweight time-series sampling of scalar quantities.

See in `doc/` a full analysis of what this library can and cannot do, open issues included.

`memvar.h` is **NOT** thread-safe.

## Requirements

At least use`cmake 3.26` to compile the sources.

The cmake files compile with `C++20`.

The unit tests are implemented in googletest: be sure you have installed googletest to compile.

To run the performance tests, a RAM of 16GB is needed. If less RAM is available, just reduce the value of `historyCapacity` (line 16 in perfTest.cpp).

## Run Unit Tests

```bash
$ git clone https://github.com:massimo-marino/memvar.git
$ cd memvar/unitTests
$ mkdir build
$ cd build
$ cmake ..
$ make
$ ./memvar-unit-tests
```

## Run Performance Tests

```bash
$ git clone https://github.com:massimo-marino/memvar.git
$ cd memvar/perfTests
$ mkdir build
$ cd build
$ cmake ..
$ make
$ ./perfTests
```

## How to Use it

See the source code and the unit tests for examples of use, as well as the documentation in `doc/`.

## Example: Yet Another Way to Compute the Fibonacci Numbers

```C++
void fibonacciNumbers()
{
  // The type stored in the memvar
  using memvarType = uint64_t;
  // define the memvar with a history capacity of 100 values
  // and store fib(0) = 0
  mv::memvar<memvarType> fibs{0, 100};
  // store fib(1) = 1
  fibs = 1;
  // compute and store fib(2) = fib(1) + fib(0)
  // through fib(93) = fib(92) + fib(91)
  for (int i = 1; i <= 92; ++i)
  {
    // compute fib(n+1) = fib(n) + fib(n-1)
    fibs += fibs(1);
  }
  // print the first 94 fibonacci numbers
  std::cout << "fibs: "; fibs.printHistoryData();
}
```
