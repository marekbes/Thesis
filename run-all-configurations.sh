#!/bin/bash
for marker in count clock
do
  for merger in eager delayed
  do
    for nodes in 1
    do
      for threads in 1 2 4 8
      do
          echo -n "$marker $merger $nodes $(($nodes * $threads)) "
          ./cmake-build-debug/proofOfConcept --nodes $nodes --thread-count $threads --merger $merger --marker $marker --run-length 20 | grep throughput
      done
    done
    for nodes in 2 3 4
    do
      for threads in 8
      do
          echo -n "$marker $merger $nodes $(($nodes * $threads)) "
          ./cmake-build-debug/proofOfConcept --nodes $nodes --thread-count $threads --merger $merger --marker $marker --run-length 20 | grep throughput
      done
    done
  done
done
