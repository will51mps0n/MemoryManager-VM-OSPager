#!/usr/bin/env bash

cd ..

echo "Building test executables:"
make tests_only

cd tests

mkdir -p output/dysm
echo "Running tests:" > testOutputs
echo >> testOutputs

echo "Looking for test files that were made"
for test_exec in ../test_*; do
  test_name=$(basename "$test_exec")

  # Skip if it's a directory 
  if [[ -d "$test_exec" ]] || [[ ! -x "$test_exec" ]]; then
    continue
  fi

  echo "Running $test_name" | tee -a testOutputs
  "$test_exec" > "output/${test_name}.out" 2>&1
done

echo "Moving files to output/dysm"
for dsym_dir in ../test_*.dSYM; do
  if [[ -d "$dsym_dir" ]]; then
    mv "$dsym_dir" output/dysm/
  fi
done

echo "All done. Outputs saved to tests/output/"