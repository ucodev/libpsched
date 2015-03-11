CC=`cat ../.compiler`
INCLUDEDIRS=-I../include
CCFLAGS=-pedantic -fstrict-aliasing -Wall -Werror -pthread -g
LDFLAGS=../src/libpsched.so -pthread -lpall -s
ELFLAGS=`cat ../.elflags`
ARCHFLAGS=`cat ../.archflags`

all:
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ARCHFLAGS} -c eg_psched_sig_basic.c
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ARCHFLAGS} -c eg_psched_thread_basic.c
	${CC} ${INCLUDEDIRS} ${CCFLAGS} ${ARCHFLAGS} -c eg_psched_timer_ul.c
	${CC} -o eg_psched_sig_basic eg_psched_sig_basic.o ${LDFLAGS} ${ELFLAGS}
	${CC} -o eg_psched_thread_basic eg_psched_thread_basic.o ${LDFLAGS} ${ELFLAGS}
	${CC} -o eg_psched_timer_ul eg_psched_timer_ul.o ${LDFLAGS} ${ELFLAGS}

clean:
	rm -f *.o
	rm -f eg_psched_sig_basic
	rm -f eg_psched_thread_basic
	rm -f eg_psched_timer_ul

