CC=`cat ../.compiler`
INCLUDEDIRS=-I../include
CCFLAGS=
ECFLAGS=`cat ../.ecflags`
LDFLAGS=-shared
ELFLAGS=`cat ../.elflags`
ARCHFLAGS=`cat ../.archflags`
TARGET=libpsched.`cat ../.extlib`

all:
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ECFLAGS} ${ARCHFLAGS} -c event.c
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ECFLAGS} ${ARCHFLAGS} -c mm.c
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ECFLAGS} ${ARCHFLAGS} -c sig.c
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ECFLAGS} ${ARCHFLAGS} -c psched.c
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ECFLAGS} ${ARCHFLAGS} -c thread.c
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ECFLAGS} ${ARCHFLAGS} -c timer_ul.c
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ECFLAGS} ${ARCHFLAGS} -c timespec.c
	${CC} ${LDFLAGS} -o ${TARGET} event.o mm.o sig.o psched.o thread.o timer_ul.o timespec.o ${ELFLAGS}

clean:
	rm -f *.o
	rm -f ${TARGET}
