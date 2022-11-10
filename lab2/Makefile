#================#
# COMPILER FLAGS #
#================#

CCFLAGS += -D L1D_LINESIZE=$(shell getconf LEVEL1_DCACHE_LINESIZE) -std=c11 -Werror -Wall -O0 -pthread -lrt

#=============#
# COMPILATION #
#=============#

spin_lock_test : spin_lock_test.c SpinLocks.o SpinLockBenchmarks.o
	gcc    ${CCFLAGS} $< -o $@ SpinLocks.o SpinLockBenchmarks.o

%.o : %.c
	gcc -c ${CCFLAGS} $< -o $@

%.asm : %.c
	gcc -S ${CCFLAGS} $< -o $@

clean:
	rm -f *.o
	rm -f *.asm
	rm spin_lock_test

.PHONY: clean