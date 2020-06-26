#!/bin/bash

run() {
  echo -n "$1"
  output=$(./build/baseline --thread-count $1 --run-length 5)
  while ! [[ $output =~ "throughput:" ]]; do
    output=$(./build/baseline --thread-count $1 --run-length 5)
  done
  echo "$output" | grep -o " .*GB/s"
}

for threads in {1..32}; do
  run $threads
done
