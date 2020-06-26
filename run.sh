#!/bin/bash

bash ./run-baseline.sh | tee results_baseline.txt
bash ./run-all-configurations.sh | tee results_configurations.txt
bash ./run-sliding-configurations.sh | tee results_sliding.txt

bash ./plot_all.sh