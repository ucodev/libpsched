CC=`cat ../.compiler`
CCFLAGS=-I../src/ -fstrict-aliasing -O2 -Wall -Werror
LDFLAGS=../src/libpsched.so -lpall -lrt -s
ELFLAGS=`cat ../.elflags`

all:
	${CC} ${CCFLAGS} -c eg_psched_basic.c
	${CC} -o eg_psched_basic eg_psched_basic.o ${LDFLAGS} ${ELFLAGS}

clean:
	rm -f *.o
	rm -f eg_psched_basic

