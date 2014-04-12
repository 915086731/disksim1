
/*
 * DiskSim Storage Subsystem Simulation Environment
 * Authors: Greg Ganger, Bruce Worthington, Yale Patt
 *
 * Copyright (C) 1993, 1995, 1997 The Regents of the University of Michigan 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 *  This software is provided AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
 * EXPRESSED OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE REGENTS
 * OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE FOR ANY DAMAGES,
 * INCLUDING SPECIAL , INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,
 * WITH RESPECT TO ANY CLAIM ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN IF IT HAS
 * BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH DAMAGES
 *
 * The names and trademarks of copyright holders or authors may NOT be
 * used in advertising or publicity pertaining to the software without
 * specific, written prior permission. Title to copyright in this software
 * and any associated documentation will at all times remain with copyright
 * holders.
 */

#ifndef DISKSIM_GLOBAL_H
#define DISKSIM_GLOBAL_H

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#ifndef MAXINT
#define MAXINT	0x7FFFFFFF
#endif

#include "disksim_assertlib.h"

#define ALLOCSIZE	8192


#define TRUE	1
#define FALSE	0

#define BITS_PER_INT_MASK	0x0000001F
#define INV_BITS_PER_INT_MASK	0xFFFFFFE0

#define _BIG_ENDIAN	1
#define _LITTLE_ENDIAN	2

/* Trace Formats */

#define ASCII		1
#define HPL		2
#define DEC		3
#define VALIDATE	4
#define RAW		5
#define DEFAULT		ASCII

/* Time conversions */

#define MILLI	1000
#define MICRO	1000000
#define NANO	1000000000

#define SECONDS_PER_MINUTE	60
#define SECONDS_PER_HOUR	3600
#define SECONDS_PER_DAY		86400

/* Flags components */

#define WRITE		0x00000000
#define READ		0x00000001
#define TIME_CRITICAL	0x00000002
#define TIME_LIMITED	0x00000004
#define TIMED_OUT	0x00000008
#define HALF_OUT	0x00000010
#define MAPPED		0x00000020
#define READ_AFTR_WRITE 0x00000040
#define SYNCHRONOUS	0x00000080
#define ASYNCHRONOUS	0x00000100
#define IO_FLAG_PAGEIO	0x00000200
#define SEQ		0x40000000
#define LOCAL		0x20000000

/* Event Type Ranges */

#define NULL_EVENT      	0
#define PF_MIN_EVENT    	1
#define PF_MAX_EVENT    	97
#define INTR_EVENT		98
#define INTEND_EVENT		99
#define IO_MIN_EVENT		100
#define IO_MAX_EVENT		120
#define TIMER_EXPIRED		121

/* Interrupt vector types */

#define CLOCK_INTERRUPT		100
#define SUBCLOCK_INTERRUPT	101
#define IO_INTERRUPT    	102

#define MSECS_PER_MIN		60000

/* must enable this on Suns and Alphas */
#if 1
#define u_int32_t	unsigned int
#define int32_t		int
#endif

typedef union {
   u_int32_t	value;
   char		byte[4];
} intchar;

#define StaticAssert(c) switch (c) case 0: case (c):

typedef struct foo {
   double time;
   int type;
   struct ev *next;
   struct ev *prev;
   int    temp;
} foo;

#define DISKSIM_EVENT_SIZE	128
#define DISKSIM_EVENT_SPACESIZE	(DISKSIM_EVENT_SIZE - sizeof(struct foo))

typedef struct ev {
   double time;
   int type;
   struct ev *next;
   struct ev *prev;
   int    temp;
   char space[DISKSIM_EVENT_SPACESIZE];
} event;

typedef struct ioreq_ev {
   double time;
   int    type;
   struct ioreq_ev *next;
   struct ioreq_ev *prev;
   int    bcount;
   int    blkno;
   u_int  flags;
   u_int  busno;
   u_int  slotno;
   int    devno;
   int    opid;
   void  *buf;
   int    cause;
   int    tempint1;
   int    tempint2;
   void  *tempptr1;
   void  *tempptr2;
} ioreq_event;

typedef struct timer_ev {
   double time;
   int type;
   struct timer_ev *next;
   struct timer_ev *prev;
   void (*func)();
   int    val;
   void  *ptr;
} timer_event;

typedef struct intr_ev {
   double time;
   int    type;
   struct intr_ev * next;
   struct intr_ev * prev;
   int    vector;
   int    oldstate;
   int    flags;
   event  *eventlist;
   event  *infoptr;
   double runtime;
} intr_event;

extern double simtime;
extern int totalreqs;
extern double warmuptime;
extern int curlbolt;
extern int traceformat;
extern FILE *statdeffile;
extern FILE *outputfile;

#define	min(x,y)	((x) < (y) ? (x) : (y))

#define	max(x,y)	((x) < (y) ? (y) : (x))

#define	wrap(x,y)	((y) < (x) ? 1 : 0)

#define diff(x,y)	((x) < (y) ? (y)-(x) : (x)-(y))

#define rounduptomult(val,mult)	((val) + ((mult) - (((val)-1) % (mult))) - 1)

/* Global disksim_intr.c functions */

extern void intr_request();

/* Global disksim.c functions */

extern void simstop();
extern void addtoextraq();
extern void addlisttoextraq();
extern event * getfromextraq();
extern event * event_copy();
extern void addtointq();
extern int removefromintq();
extern void scanparam_int();
extern void getparam_int();
extern void getparam_double();
extern void getparam_bool();

/* disksim.c functions used for external control */

extern event *intq;
extern void disksim_main();
extern void set_external_io_done_notify();
extern void cleanstats();
extern void printstats();
extern void disksim_simulate_event();

/* redundant prototype needed because SunOS hides drand48... */

extern double drand48();

#endif  /* DISKSIM_GLOBAL_H */

