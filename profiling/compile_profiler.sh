#/bin/bash

gcc -Wextra -Wall -Werror -pedantic -flto -O3 -march=native x9_profiler.c ../x9.c -o X9_PROF
