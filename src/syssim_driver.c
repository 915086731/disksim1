/*
 * A sample skeleton for a system simulator that calls DiskSim as
 * a slave.
 *
 * Contributed by Eran Gabber of Lucent Technologies - Bell Laboratories
 *
 * Usage:
 *	syssim <parameters file> <output file> <max. block number>
 * Example:
 *	syssim parv.seagate out 2676846
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include "syssim_driver.h"


#define	BLOCK	4096
#define	SECTOR	512
#define	BLOCK2SECTOR	(BLOCK/SECTOR)

typedef	struct	{
	int n;
	double sum;
	double sqr;
} Stat;


static SysTime now = 0;		/* current time */
static SysTime next_event = -1;	/* next event */
static int completed = 0;	/* last request was completed */
static Stat st;


void
panic(const char *s)
{
	perror(s);
	exit(1);
}


void
add_statistics(Stat *s, double x)
{
	s->n++;
	s->sum += x;
	s->sqr += x*x;
}


void
print_statistics(Stat *s, const char *title)
{
	double avg, std;

	avg = s->sum/s->n;
	std = sqrt((s->sqr - 2*avg*s->sum + s->n*avg*avg) / s->n);
	printf("%s: n=%d average=%f std. deviation=%f\n", title, s->n, avg, std);
}


/*
 * Schedule next callback at time t.
 * Note that there is only *one* outstanding callback at any given time.
 * The callback is for the earliest event.
 */
void
syssim_schedule_callback(void (*f)(double), SysTime t)
{
	next_event = t;
}


/*
 * de-scehdule a callback.
 */
void
syssim_deschedule_callback(void (*f)())
{
	next_event = -1;
}


void
syssim_report_completion(SysTime t, Request *r)
{
	completed = 1;
	now = t;
	add_statistics(&st, t - r->start);
}


void
main(int argc, char *argv[])
{
	int i;
	int nsectors;
	struct stat buf;
	Request r;

	if (argc != 4 || (nsectors = atoi(argv[3])) <= 0) {
		fprintf(stderr, "usage: %s <param file> <output file> <#sectors>\n",
			argv[0]);
		exit(1);
	}

	if (stat(argv[1], &buf) < 0)
		panic(argv[1]);

	disksim_initialize(argv[1], argv[2]);
	srand48(1);

	for (i = 0; i < 1000; i++) {
		r.start = now;
		r.type = 'R';
		r.devno = 0;
		r.blkno = BLOCK2SECTOR*(lrand48()%(nsectors/BLOCK2SECTOR));
		r.bytecount = BLOCK;
		completed = 0;
		disksim_request_arrive(now, &r);

		/* Process events until this I/O is completed */
		while(next_event >= 0) {
			now = next_event;
			next_event = -1;
			disksim_internal_event(now);
		}

		if (!completed) {
			fprintf(stderr,
				"%s: internal error. Last event not completed %d\n",
				argv[0], i);
			exit(1);
		}
	}

	disksim_shutdown(now);

	print_statistics(&st, "response time");

	exit(0);
}
