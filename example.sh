#!/bin/bash
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:. mpirun.mpich -n 3 ./example
