#!/bin/bash

for (( i = 1; i <= 4; i++ )); do
    ./build/inputgen --nodes $i
done

bash ./run-baseline.sh | tee results_baseline.txt
bash ./run-all-configurations.sh | tee results_configurations.txt
bash ./run-sliding-configurations.sh | tee results_sliding.txt

bash ./plot_all.sh
