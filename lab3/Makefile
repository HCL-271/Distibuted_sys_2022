#================#
# COMPILER FLAGS #
#================#

CCFLAGS +=-std=c11 -Werror -Wall -O0 -pthread -lrt

#=============#
# COMPILATION #
#=============#

stack_test : stack_test.c Stack.o StackBenchmarks.o
	gcc ${CCFLAGS} $< -o $@ Stack.o StackBenchmarks.o

%.o : %.c
	gcc -c ${CCFLAGS} $< -o $@

%.asm : %.c
	gcc -S ${CCFLAGS} $< -o $@

clean:
	rm -f *.o
	rm -f *.asm
	rm stack_test

.PHONY: clean
