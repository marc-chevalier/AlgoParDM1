CC=gcc
FLAGBASE= -W -Wextra -Wcast-qual -Wcast-align -Wfloat-equal -Wshadow -Wpointer-arith -Wunreachable-code -Wchar-subscripts -Wcomment -Wformat -Werror-implicit-function-declaration -Wmain -Wmissing-braces -Wparentheses -Wsequence-point -Wreturn-type -Wswitch -Wuninitialized -Wundef -Wwrite-strings -Wsign-compare -Wmissing-declarations -pedantic -Wconversion -Wmissing-noreturn -Wall -Wunused -Wsign-conversion -Wunused -Wstrict-aliasing -Wstrict-overflow -Wconversion -Wdisabled-optimization -Wsuggest-attribute=const -Wsuggest-attribute=noreturn -Wsuggest-attribute=pure -Wlogical-op -Wunsafe-loop-optimizations
CFLAGS= -O3 $(FLAGBASE)
EXEC=setup average

all: $(EXEC) 

purge: clean all

debug: CFLAGS = -D DEBUG -g $(FLAGBASE)
debug: all

setup:
	mkdir -p obj

average: obj/average.o
	$(CC) -o $@ $^ $(CFLAGS)

obj/average.o: src/average.c
	$(CC) -c -o $@ $< $(CFLAGS)
	
clean:
	rm -f obj/*.o
