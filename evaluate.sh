#!/bin/bash
set -e

NITERATIONS=10
if [[ ${#} -gt 0 ]]; then
  NITERATIONS=${1}
  shift
fi

NTHREADS=1
if [[ ${#} -gt 0 ]]; then
  NTHREADS=${1}
  shift
fi

DATA_DIR=$(dirname ${0})/data
if [[ ${#} -gt 0 ]]; then
  DATA_DIR=${1}
  shift
fi

MODEL_DIR=$(dirname ${0})/models
if [[ ${#} -gt 0 ]]; then
  MODEL_DIR=${1}
  shift
fi

TRAIN_GOLD=${DATA_DIR}/wsj_02-21.ontonotes.mxpost.sd
TEST_GOLD=${DATA_DIR}/wsj_22.ontonotes.mxpost.sd
TEST_RUN=${DATA_DIR}/wsj_22.ontonotes.mxpost.slash

MODEL_NAME=$(basename ${TRAIN_GOLD})_${NITERATIONS}_${NTHREADS}
MODEL=${MODEL_DIR}/${MODEL_NAME}.model
TEST_OUTPUT=${MODEL_DIR}/${MODEL_NAME}.out

rm -f ${MODEL_DIR}/${MODEL_NAME}*

./dist/english.depparser/train ${TRAIN_GOLD} ${MODEL} ${NITERATIONS} ${NTHREADS}
./dist/english.depparser/depparser ${TEST_RUN} ${TEST_OUTPUT} ${MODEL}
python3 evaluate_nbest.py ${TEST_OUTPUT} ${TEST_GOLD} > ${MODEL_DIR}/${MODEL_NAME}.results

cat ${MODEL_DIR}/${MODEL_NAME}.results
