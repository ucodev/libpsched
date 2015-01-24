CC=`cat ../.compiler`
INCLUDEDIRS=-I../include
CCFLAGS=-pedantic -fstrict-aliasing -O2 -Wall -Werror -pthread
LDFLAGS=../src/libpsched.so -pthread -lpall -lrt -s
ELFLAGS=`cat ../.elflags`

all:
	${CC} ${INCLUDEDIRS} ${CCFLAGS} -c eg_psched_sig_basic.c
	${CC} ${INCLUDEDIRS} ${CCFLAGS} -c eg_psched_thread_basic.c
	${CC} -o eg_psched_sig_basic eg_psched_sig_basic.o ${LDFLAGS} ${ELFLAGS}
	${CC} -o eg_psched_thread_basic eg_psched_thread_basic.o ${LDFLAGS} ${ELFLAGS}

clean:
	rm -f *.o
	rm -f eg_psched_sig_basic
	rm -f eg_psched_thread_basic

