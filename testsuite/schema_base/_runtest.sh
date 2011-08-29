#!/bin/bash

DIR="`dirname \"$0\"`"

LD_LIBRARY_PATH="$DIR"/../../src/.libs "$DIR"/test/test "$@"
