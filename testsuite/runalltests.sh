#!/bin/bash

for i in basic schema_base schema ; do
	echo "Running testsuite $i"
	(
		cd $i
		./runtests.sh
	)
done
