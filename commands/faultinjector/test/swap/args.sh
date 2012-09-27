#!/usr/bin/env bash

ARGS="-fault-rand-seed=123 -fault-functions fault_test_no_load -fault-prob-global=1000 -fault-prob-swap=1000"
OUTPUT_FROM=fault_test_no_load_start
OUTPUT_TO=fault_test_no_load_end
RAND_SEED=123
