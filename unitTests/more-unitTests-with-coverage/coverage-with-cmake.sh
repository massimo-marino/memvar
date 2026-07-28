#!/bin/bash
# 1. Configure — coverage flag must be passed here, at cmake config time
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DMEMVAR_ENABLE_COVERAGE=ON

# 2. Build
cmake --build build -j

# 3. Run the tests (this generates the .gcda files)
./build/memvar_tests

# 4. Generate the report (needs gcovr: pip install gcovr / apt install gcovr)
gcovr -r . --object-directory build --filter '../../memvar.h' -s

