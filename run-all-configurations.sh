#!/bin/bash

run() {
  echo -n "$4 $3 $1 $(($2 * $1)) $5"
  output=$(./build/proofOfConcept --nodes $1 --thread-count $2 --merger $3 --marker $4 --run-length 20 --window-size $5 --window-slide $5)
  if [ $? -ne 0 ]; then
    echo $output
  fi
  echo "$output" | grep -o " .*GB/s" | tr -d '\n'
  echo "$output" | grep -o " 5th .*$"
}

for window in 10000 50000 100000; do
  for marker in Clock Count; do
    for merger in Delayed Direct Eager; do
      for threads in 1 2 4 8; do
        run 1 $threads $merger $marker $window
      done
      for nodes in 2 3 4; do
        run $nodes 8 $merger $marker $window
      done
    done
  done
done
