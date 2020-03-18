#!/usr/bin/env bash
# Wirepas Oy
cd lib || exit 1
make clean
make
cd ../test || exit 1
make test_mode=1 clean
make test_mode=1 rxnode=1
