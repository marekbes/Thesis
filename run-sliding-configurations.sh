#!/bin/bash

run() {
  echo -n "$4 $3 $5"
  windowSize=$(($5 * 3))
  output=$(./build/proofOfConcept --nodes $1 --thread-count $2 --merger $3 --marker $4 --run-length 20 --window-slide $5 --window-size $windowSize --shared-buffer-size 50000 2>&1)
  if [ $? -ne 0 ]; then
    echo $output
  fi
  echo "$output" | grep -o " .*GB/s" | tr -d '\n'
  echo "$output" | grep -o " 5th .*$"
}

for marker in Count Clock; do
  for merger in Direct Delayed Eager; do
    for slide in 5000 10000 25000 50000 75000 100000; do
      run 4 8 $merger $marker $slide
    done
  done
done
