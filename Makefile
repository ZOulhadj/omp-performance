# Makefile for Coursework Part 2

#
# C compiler and options for Intel
#
CC=     icc -O3 -qopenmp -std=c99
LIB=    -lm

#
# C compiler and options for GNU
#
#CC=     gcc -O3 -fopenmp -std=c99
#LIB=	-lm

#
# Object files
#

OBJ1=    bin/solver1.o bin/function.o
OBJ2=    bin/solver2_shared.o bin/function.o
OBJ3=    bin/solver2_separate.o bin/function.o

#
# Compile
#

all: bin/solver1 bin/solver2_shared bin/solver2_separate

bin:
	mkdir -p bin

bin/solver1:   $(OBJ1)
	$(CC) -o $@ $(OBJ1) $(LIB)

bin/solver2_shared:   $(OBJ2)
	$(CC) -o $@ $(OBJ2) $(LIB)

bin/solver2_separate:   $(OBJ3)
	$(CC) -o $@ $(OBJ3) $(LIB)

bin/%.o: src/%.c | bin
	$(CC) -c $< -o $@

#
# Clean out object files and the executable.
#
clean:
	rm bin/*.o bin/solver1 bin/solver2_shared bin/solver2_separate
	rm -rf bin/
