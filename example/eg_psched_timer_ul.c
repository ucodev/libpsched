#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "timer_ul.h"

void event(union sigval arg) {
	printf("Event triggered at: %lu\n", (unsigned long) time(NULL));
}

int main(void) {
	timer_t timer;
	struct sigevent sevp;
	struct itimerspec trigger;

	memset(&sevp, 0, sizeof(struct sigevent));

	sevp.sigev_notify = SIGEV_THREAD;
	sevp.sigev_notify_function = &event;

	trigger.it_value.tv_sec = time(NULL) + 5;
	trigger.it_value.tv_nsec = 0;
	trigger.it_interval.tv_sec = 0;
	trigger.it_interval.tv_nsec = 0;

	if (timer_create_ul(CLOCK_REALTIME, &sevp, &timer) < 0) {
		fprintf(stderr, "timer_create_ul(): %s\n", strerror(errno));
		return 1;
	}

	if (timer_settime_ul(timer, TIMER_ABSTIME, &trigger, NULL) < 0) {
		fprintf(stderr, "timer_settime_ul(): %s\n", strerror(errno));
		return 1;
	}

	printf("Waiting for event (Current time: %lu; Expected trigger at: %lu)...\n", (unsigned long) (trigger.it_value.tv_sec - 5), (unsigned long) trigger.it_value.tv_sec);

	usleep(6000000);

	timer_delete_ul(timer);

	return 0;
}


