# memvar

C++17 implementation of variable with memory of its past values. Aka a *memoized variable*, or a *variable with memoization*.

## Requirements

The cmake file compiles with `-std=c++17`.

The unit tests are implemented in googletest: be sure you have installed googletest to compile.


## Install and Run Unit Tests

```bash
$ git clone https://github.com:massimo-marino/memvar.git
$ cd memvar
$ mkdir build
$ cd build
$ cmake ..
$ make
$ cd src/unitTests
$ ./unitTests
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
