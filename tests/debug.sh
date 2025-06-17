#!/usr/bin/env bash

cd ..

echo "Building tests"
make tests_only

cd tests

mkdir -p output/debug
echo "Running failing tests with debug:" > debug_output.txt
echo >> debug_output.txt

# FAILURES :(
failing_tests=(
  "test_FB_multi_write.4"
  "test_SB_cross_page_filenameß.4"
  "test_SB_swap_evict_load.4"
)

for test_name in "${failing_tests[@]}"; do
  test_exec="../$test_name"
  
  # Check if executable exists
  if [[ -x "$test_exec" ]]; then

    echo "Running $test_name with debugging:" | tee -a debug_output.txt

      echo "debug out:" | tee -a debug_output.txt
      lldb -o "run" -o "bt all" -o "quit" -- "$test_exec" | tee -a "output/debug_${test_name}.log" 2>&1
      echo "" | tee -a debug_output.txt
   
    echo "normal out:" | tee -a debug_output.txt
    "$test_exec" | tee -a "output/debug_${test_name}.out" 2>&1 || echo "Process exited with error code $?" | tee -a debug_output.txt
    echo "" | tee -a debug_output.txt
  else
    echo " $test_exec not found" | tee -a debug_output.txt
  fi
  
  echo >> debug_output.txt
done

echo "done"