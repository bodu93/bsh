CFLAGS = -g -O2 -Wall

all : bsh

bsh : util.o parse.o bsh.o
	${CC} ${CFLAGS} -o $@ $^

.PHONY : clean

clean:
	-rm -rf bsh bsh.dSYM util.o parse.o bsh.o
