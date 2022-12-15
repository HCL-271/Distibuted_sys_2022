#================#
# COMPILER FLAGS #
#================#

CCFLAGS +=-std=gnu11 -Werror -Wall -O0 -pthread -lrt

#=============#
# COMPILATION #
#=============#

skiplist_test : skiplist_test.c SkipList.o SkipListBenchmarks.o
	gcc ${CCFLAGS} $< -o $@ SkipList.o SkipListBenchmarks.o

%.o : %.c
	gcc -c ${CCFLAGS} $< -o $@

%.asm : %.c
	gcc -S ${CCFLAGS} $< -o $@

clean:
	rm -f *.o
	rm -f *.asm
	rm skiplist_test

.PHONY: clean
