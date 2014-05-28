CC=`cat ../.compiler`
CCFLAGS=-g -fPIC -fstrict-aliasing -O2 -Wall -Werror
ECFLAGS=`cat ../.ecflags`
LDFLAGS=-shared -g
ELFLAGS=`cat ../.elflags`
TARGET=libpsched.so

all:
	${CC} ${CCFLAGS} ${ECFLAGS} -c mm.c
	${CC} ${CCFLAGS} ${ECFLAGS} -c sig.c
	${CC} ${CCFLAGS} ${ECFLAGS} -c sched.c
	${CC} ${LDFLAGS} -o ${TARGET} mm.o sig.o sched.o ${ELFLAGS}

clean:
	rm -f *.o
	rm -f ${TARGET}
