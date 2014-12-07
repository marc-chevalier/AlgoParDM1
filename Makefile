CC=mpicc
FLAGBASE= -W -Wextra -Wcast-qual -Wcast-align -Wfloat-equal -Wshadow -Wpointer-arith -Wunreachable-code -Wchar-subscripts -Wcomment -Wformat -Werror-implicit-function-declaration -Wmain -Wmissing-braces -Wparentheses -Wsequence-point -Wreturn-type -Wswitch -Wuninitialized -Wundef -Wwrite-strings -Wsign-compare -Wmissing-declarations -pedantic -Wconversion -Wmissing-noreturn -Wall -Wunused -Wsign-conversion -Wunused -Wstrict-aliasing -Wstrict-overflow -Wconversion -Wdisabled-optimization -Wlogical-op -Wunsafe-loop-optimizations -std=c99

UNAME= $(shell uname)
ifeq ($(UNAME), Darwin)
	LIBS = -lX11 -L/opt/X11/lib
else
	LIBS = -lX11
endif

CFLAGS= -O3 $(FLAGBASE) $(LIBS)
EXEC=setup average constants sparse

all: $(EXEC) 

purge: clean all

debug: CFLAGS = -D DEBUG -g $(FLAGBASE) $(LIBS)
debug: all

setup:
	mkdir -p obj

average: obj/average.o
	$(CC) -o $@ $^ $(CFLAGS)

obj/average.o: src/average.c
	$(CC) -c -o $@ $< $(CFLAGS)

constants: obj/constants.o
	$(CC) -o $@ $^ $(CFLAGS)

obj/constants.o: src/constants.c
	$(CC) -c -o $@ $< $(CFLAGS)

sparse: obj/sparse.o
	$(CC) -o $@ $^ $(CFLAGS)

obj/sparse.o: src/sparse.c
	$(CC) -c -o $@ $< $(CFLAGS)
	
clean:
	rm -f obj/*.o
