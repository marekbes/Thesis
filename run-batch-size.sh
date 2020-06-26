#!/bin/bash
run() {
  echo -n "$4 $3 $5 $6"
  output=$(./build/proofOfConcept --nodes $1 --thread-count $2 --merger $3 --marker $4 --batch-size $6 --run-length 20 --window-slide $5 --window-size $5 --input $(pwd)\\ 2>&1)
  if [ $? -ne 0 ]; then
    echo $output
  fi
  echo "$output" | grep -o " .*GB/s" | tr -d '\n'
  echo "$output" | grep -o " 5th .*$"
}

for marker in Clock Count; do
  for merger in Direct Delayed Eager; do
    for window in 10000 25000 50000 75000 100000; do
      for batchsize in 8192 16384 32768; do
        run 4 8 $merger $marker $window $batchsize
      done
    done
  done
done
