#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

//#include <psched/psched.h>
#include "psched.h"

void timer_handler(void *arg) {
	char *str = arg;

	printf("[Timer]: %s\n", str);
}

void do_work(void) {
	int i = 0;

	for (i = 0; i < 10; i ++)
		sleep(1);

	puts("[Worker]: Work done.");
}

int main(void) {
	psched_t *h;

	/* Initialize psched signal interface */
	if (!(h = psched_sig_init(SIGUSR1))) {
		fprintf(stderr, "psched_sig_init(): %s\n", strerror(errno));

		return 1;
	}

	/* Arm the first timer */
	if (psched_timestamp_arm(h, time(NULL) + 5, 0, 0, &timer_handler, "Hello! This timer has expired.") == (pschedid_t) - 1) {
		fprintf(stderr, "psched_timestamp_arm(): %s\n", strerror(errno));
		psched_destroy(h);

		return 1;
	}

	/* Arm a second timer */
	if (psched_timestamp_arm(h, time(NULL) + 7, 0, 0, &timer_handler, "Hello again! This timer also expired.") == (pschedid_t) - 1) {
		fprintf(stderr, "psched_timestamp_arm(): %s\n", strerror(errno));
		psched_destroy(h);

		return 1;
	}

	/* Simulate some work */
	do_work();

	/* Free handler resources */
	psched_destroy(h);

	/* All good */
	return 0;
}

