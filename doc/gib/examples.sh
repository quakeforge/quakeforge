#!/bin/sh
#
# This script is in the public f'ing domain.
# You can burn it to a CD and use it as a
# frisbee for all I care.

for example in *.gib; do \
	carne "$example" >"${example}.out"; \
done

