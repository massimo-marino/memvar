# memvar

C++17 implementation of a variable with memory of its past values.

## Requirements

`cmake 3.26.3` is used to compile the sources.

The cmake file compiles with `C++17`.

The unit tests are implemented in googletest: be sure you have installed googletest to compile.

To run the performance tests, a RAM of 16GB is needed. If less RAM is available, just reduce the value of `historyCapacity` (line 15 in perfTest.cpp).

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
If needed (cmake fails), copy `FindGMock.cmake` to the Modules directory of cmake.

In my installation located in my home, it is in `~/cmake-3.26.3-linux-x86_64/share/cmake-3.26/Modules/`

## Run Performance Tests

```bash
$ git clone https://github.com:massimo-marino/memvar.git
$ cd memvar/perfTests
$ mkdir build
$ cd build
$ cmake ..
$ make
$ ./perfests
```

## How to Use it

See the source code and the unit tests for examples of use.

## Example: Yet Another Way to Compute the Fibonacci Numbers
```C++
void fibonacciNumbers()
{
  // The type stored in the memvar
  using memvarType = uint64_t;
  // define the memvar with a history capacity of 100 values
  // and store fib(0) = 0
  memvar::memvar<memvarType> fibs{0, 100};
  // store fib(1) = 1
  fibs = 1;
  // compute and store fib(2) = fib(1) + fib(0)
  // through fib(93) = fib(92) + fib(91)
  for(int i = 1; i <= 92; ++i)
  {
    // compute fib(n+1) = fib(n) + fib(n-1)
    fibs += getHistoryValue(fibs, 1);
  }
  // print the first 94 fibonacci numbers
  std::cout << "fibs: "; fibs.printHistoryData();
}
```
