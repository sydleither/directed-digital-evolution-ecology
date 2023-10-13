#!/usr/bin/env bash

REPLICATES=50
EXP_SLUG=2023-10-11-test

SCRATCH_EXP_DIR=/mnt/gs21/scratch/leithers/community-level-selection
HOME_EXP_DIR=/mnt/home/leithers/community_level_selection/directed-digital-evolution-ecology/ecology-experiments

DATA_DIR=${SCRATCH_EXP_DIR}/${EXP_SLUG}
JOB_DIR=${SCRATCH_EXP_DIR}/${EXP_SLUG}/jobs
CONFIG_DIR=${HOME_EXP_DIR}/${EXP_SLUG}/hpcc/config

python3 gen-sub.py --data_dir ${DATA_DIR}  --config_dir ${CONFIG_DIR} --replicates ${REPLICATES} --job_dir ${JOB_DIR}