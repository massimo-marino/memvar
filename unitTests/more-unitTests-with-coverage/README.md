# memvar.h — GoogleTest suite

Unit tests for `memvar.h` (`mv::memvar<T>` and `mv::memvarTimed<T>`),
built with GoogleTest, C++20.

## Layout

```
memvar.h                the header under test (unmodified)
memvar_test.cpp         tests, self-contained (includes a small
                        bip::bigint test double to exercise the
                        is_bigint<T> extension point)
CMakeLists.txt          CMake build
coverage-with-cmake.sh  script for building by choosing the safe mode
```

## Build & run

### CMake

With coverage, use the script coverage-with-cmake.sh:

```sh
#!/bin/bash
#
if [[ "$1" != "0" && "$1" != "1" && "$1" != "2" ]]; then
  echo "Error: Missing/Wrong argument for safe mode value: it must be 0, 1, or 2.";
  echo "usage: coverage-with-cmake.sh { 0 | 1 | 2 }";
  exit 1;
fi
#
SAFE_MODE="${1}"
BUILD_DIR="build${SAFE_MODE}"
echo "building for safe mode: ${SAFE_MODE} - build dir: ${BUILD_DIR}"
#
rm -vf ./memvar.h
cp -v ../../memvar.h .
rm -rf build build0 build1 build2
#
# 1. Configure — coverage flag must be passed here, at cmake config time
#cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DMEMVAR_ENABLE_COVERAGE=ON
cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug -DMEMVAR_ENABLE_COVERAGE=ON -DMEMVAR_TEST_GCOV_DUMP=ON -DMEMVAR_SAFEREF_CHECK=${SAFE_MODE}

# 2. Build
cmake --build "${BUILD_DIR}"

# 3. Run the tests (this generates the .gcda files)
#./build/memvar_tests
ctest --test-dir "${BUILD_DIR}"

# 4. Generate the report (needs gcovr: pip install gcovr / apt install gcovr)
gcovr -r . --object-directory "${BUILD_DIR}" --filter 'memvar.h' -s
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
