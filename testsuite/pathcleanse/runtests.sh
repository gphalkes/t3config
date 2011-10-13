#!/bin/bash

make -q || make

LD_LIBRARY_PATH=../../src/.libs "$@" ./test
