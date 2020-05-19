#!/bin/bash

run() {
      echo -n "$4 $3 $1 $(($2 * $1))"
      output=`./cmake-build-debug/proofOfConcept --nodes $1 --thread-count $2 --merger $3 --marker $4 --run-length 20`
      if [ $? -ne 0 ]; then
          echo $output
      fi
      echo "$output" | grep -o " .*GB/s" | tr -d '\n'
      echo "$output" | grep -o " 5th .*$"
}

for marker in clock count
do
  for merger in delayed direct eager
  do
    for threads in 1 2 4 8
    do
      run 1 $threads $merger $marker
    done
    for nodes in 2 3 4
    do
        run $nodes 8 $merger $marker
    done
  done
done
