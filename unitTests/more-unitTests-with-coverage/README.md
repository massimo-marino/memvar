# memvar.h — GoogleTest suite

Unit tests for `memvar.h` (`mv::memvar<T>` and `mv::memvarTimed<T>`),
built with GoogleTest, C++20.

## Layout

```
memvar.h          the header under test (unmodified)
memvar_test.cpp   tests, self-contained (includes a small
                  bip::bigint test double to exercise the
                  is_bigint<T> extension point)
CMakeLists.txt    CMake build (preferred)
Makefile          plain make build, assumes system GoogleTest
```

## Build & run

### Option A — CMake

With coverage:
```sh
# 1. Configure — coverage flag must be passed here, at cmake config time
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DMEMVAR_ENABLE_COVERAGE=ON

# 2. Build
cmake --build build -j

# 3. Run the tests (this generates the .gcda files)
./build/memvar_tests

# 4. Generate the report (needs gcovr: pip install gcovr / apt install gcovr)
gcovr -r . --object-directory build --filter '../../memvar.h' -s
```

No coverarge:
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build          # or: ./build/memvar_tests
```

`CMakeLists.txt` first looks for a system GoogleTest install
(`find_package(GTest)`, e.g. `apt install libgtest-dev`). If none is found it
falls back to downloading GoogleTest v1.14.0 via `FetchContent` — no manual
setup needed either way.

### Option B — Makefile

Requires GoogleTest headers/libs already installed system-wide
(`sudo apt install libgtest-dev` on Debian/Ubuntu, or `brew install
googletest` on macOS).

```sh
make            # build build/memvar_tests
make test       # build + run
make coverage   # build with --coverage and run (gcov/lcov/gcovr friendly)
make clean

# Generate the coverage report (needs gcovr: pip install gcovr / apt install gcovr)
gcovr -r . --object-directory build
```

