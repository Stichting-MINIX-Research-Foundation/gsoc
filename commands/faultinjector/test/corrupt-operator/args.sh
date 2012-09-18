#!/usr/bin/env bash

ARGS="-fault-rand-seed=123 -fault-functions fault_test_corrupt_operator -fault-prob-global=1000 -fault-prob-corrupt-operator=1000"
OUTPUT_FROM=fault_test_corrupt_operator_start
OUTPUT_TO=fault_test_corrupt_operator_end
RAND_SEED=123
