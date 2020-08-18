#!/bin/bash

# default dir for valgrind files
export VALGRIND_LOGDIR=${HOME}/valgrind
mkdir -p ${VALGRIND_LOGDIR}

# prevent valgrind from reporting leaks in stl code that uses pool allocators
# NOTE: no longer needed?  (see https://gcc.gnu.org/onlinedocs/libstdc++/manual/debug.html#debug.memory)
export GLIBCXX_FORCE_NEW=1

# set valgrid options in env
# general options
VALGRIND_OPTS="--tool=memcheck"
VALGRIND_OPTS="${VALGRIND_OPTS}  --log-file=$VALGRIND_LOGDIR/valgrind-%p.out"       # report to separate file (vs. stderr)
VALGRIND_OPTS="${VALGRIND_OPTS}  --time-stamp=yes"                                  # helpful for correlating valgrind output against logs, cores, etc.
VALGRIND_OPTS="${VALGRIND_OPTS}  --wall-clock=yes"                                  # print wall-clock time (as opposed to delta)
VALGRIND_OPTS="${VALGRIND_OPTS}  --verbose"                                         # report library loads, etc.
VALGRIND_OPTS="${VALGRIND_OPTS}  --error-limit=no"                                  # required for java, occi which both generate lots of memory errors
VALGRIND_OPTS="${VALGRIND_OPTS}  --trace-children=yes "                             # required for java (?)
VALGRIND_OPTS="${VALGRIND_OPTS}  --smc-check=all"                                   # check for self-modifying code -- required for java (?)
VALGRIND_OPTS="${VALGRIND_OPTS}  --track-origins=yes"                               # for memory errors, report stack trace where block was allocated
VALGRIND_OPTS="${VALGRIND_OPTS}  --num-callers=50"                                  # stack traces in jvm can be very deep (50 is max for VG 3.7.0)
VALGRIND_OPTS="${VALGRIND_OPTS}  --keep-debuginfo=yes"                              # retain symbols after shared libs closed
# leak checking options
VALGRIND_OPTS="${VALGRIND_OPTS}  --leak-check=yes"
VALGRIND_OPTS="${VALGRIND_OPTS}  --leak-resolution=high"                            # control which leaks are combined by valgrind
VALGRIND_OPTS="${VALGRIND_OPTS}  --show-reachable=yes --show-possibly-lost=yes"     # these can be filtered by scripts if not desired, so always report
VALGRIND_OPTS="${VALGRIND_OPTS}  --errors-for-leak-kinds=none"                      # dont treat leaks as errors
VALGRIND_OPTS="${VALGRIND_OPTS}  --error-exitcode=1"                                # return non-zero if memory errors (not leaks) found
export VALGRIND_OPTS

valgrind "$@"