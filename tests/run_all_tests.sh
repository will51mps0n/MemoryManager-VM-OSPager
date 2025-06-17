#!/usr/bin/env bash

# Move up and build all test executables
cd ..
echo "Building test executables:"
make tests_only

# Return to the tests directory
cd tests

# Prepare output directories
mkdir -p output
mkdir -p output/dysm
mkdir -p mem_output

# Master test log
echo "Running all tests:" > testOutputs
echo >> testOutputs

# Loop over each compiled test executable
for test_exec in ../test_*; do
  test_name=$(basename "$test_exec")

  # Skip if not a valid test binary
  if [[ -d "$test_exec" || ! -x "$test_exec" ]]; then
    continue
  fi

  # Extract memory page count from filename (e.g. test_name.4.cpp → 4)
  mem=$(echo "$test_name" | grep -oE '\.[0-9]+' | tr -d '.')

  echo "Running $test_name" | tee -a testOutputs

  # Start pager in background and redirect memory output
  ../vm_pager -m "$mem" -s 256 > "mem_output/${test_name}.mem" 2>&1 &
  PAGER_PID=$!

  sleep 0.5  # Give pager time to start

  # Run the test and save its stdout
  "$test_exec" > "output/${test_name}.out" 2>&1

  # Stop pager after test completes
  kill "$PAGER_PID" 2>/dev/null
  wait "$PAGER_PID" 2>/dev/null

  # Optional: include summary logs if you want everything in one file
  echo "=== ${test_name}.out ===" >> testOutputs
  cat "output/${test_name}.out" >> testOutputs
  echo "=== ${test_name}.mem ===" >> testOutputs
  cat "mem_output/${test_name}.mem" >> testOutputs
  echo >> testOutputs

  echo "Finished $test_name"
done

# Move .dSYM folders to output/dysm if present (macOS debug symbols)
for dsym_dir in ../test_*.dSYM; do
  if [[ -d "$dsym_dir" ]]; then
    mv "$dsym_dir" output/dysm/
  fi
done

echo "All done. Outputs in tests/output/, memory logs in tests/mem_output/"
