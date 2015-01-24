CC=`cat ../.compiler`
INCLUDEDIRS=-I../include
CCFLAGS=-g -fPIC -fstrict-aliasing -O2 -Wall -Werror
ECFLAGS=`cat ../.ecflags`
LDFLAGS=-shared -g
ELFLAGS=`cat ../.elflags`
TARGET=libpsched.so

all:
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ECFLAGS} -c event.c
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ECFLAGS} -c mm.c
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ECFLAGS} -c sig.c
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ECFLAGS} -c sched.c
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ECFLAGS} -c thread.c
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ECFLAGS} -c timespec.c
	${CC} ${LDFLAGS} -o ${TARGET} event.o mm.o sig.o sched.o thread.o timespec.o ${ELFLAGS}

clean:
	rm -f *.o
	rm -f ${TARGET}
