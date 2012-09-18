#!/usr/bin/env bash

ARGS="-fault-rand-seed=123 -fault-functions fault_test_rnd_load -fault-prob-global=1000 -fault-prob-rnd-load=1000"
OUTPUT_FROM=fault_test_rnd_load_start
OUTPUT_TO=fault_test_rnd_load_end
RAND_SEED=123
