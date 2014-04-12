/*
 * Definitions for simple system simulator that uses DiskSim as a slave.
 *
 * Contributed by Eran Gabber of Lucent Technologies - Bell Laboratories
 *
 */

typedef	double SysTime;		/* system time in seconds.usec */

typedef	struct	{		/* System level request */
	char type;		/* 'R' denotes read; 'W' denotes write */
	short devno;
	unsigned long blkno;
	int bytecount;
	SysTime start;
} Request;

/* routines for translating between the system-level simulation's simulated */
/* time format (whatever it is) and disksim's simulated time format (a      */
/* double, representing the number of milliseconds from the simulation's    */
/* initialization).                                                         */

/* In this example, system time is in seconds since initialization */
#define SYSSIMTIME_TO_MS(syssimtime)    (syssimtime*1e3)
#define MS_TO_SYSSIMTIME(curtime)       (curtime/1e3)

/* routine for determining a read request */
#define	isread(r)	((r->type == 'R') || (r->type == 'r'))

extern void disksim_initialize(const char *pfile, const char *ofile);
extern void disksim_shutdown (SysTime syssimtime);
extern void disksim_dump_stats(SysTime syssimtime);
extern void disksim_internal_event(SysTime t);
extern void syssim_report_completion(SysTime syssimtime, Request *r);
extern void disksim_request_arrive(SysTime syssimtime, Request *requestdesc);
extern void syssim_schedule_callback(void (*f)(double), SysTime t);
extern void syssim_report_completion(SysTime t, Request *r);
extern void syssim_deschedule_callback(void (*f)());

