
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

#ifndef DISKSIM_PFSIM_H
#define DISKSIM_PFSIM_H

#include "disksim_stat.h"

/* Process-flow event types */

#define SLEEP_EVENT             3
#define WAKEUP_EVENT            5
#define IOREQ_EVENT             9
#define IOACC_EVENT             10

#define CPU_EVENT	        50
#define SYNTHIO_EVENT		51
#define IDLELOOP_EVENT		52
#define CSWITCH_EVENT		53

#define MAXCPUS 20

#define PF_SLEEP_FLAG  0x00000002
#define PF_WAKED       0x00000010

#define PF_BUF_WANTED  0x00000040

/* CPU states */

#define CPU_IDLE	0
#define CPU_PROCESS	1
#define CPU_IDLE_WORK	2
#define CPU_INTR	3

/* Process status flags */

#define PROC_SLEEP	1
#define PROC_RUN	2
#define PROC_ONPROC	3

/* event times */

#define PF_INTER_CLOCK_TIME		10.0
#define PF_INTER_LONG_CLOCKS		100
#define PF_LONG_CLOCK_TIME		0.0
#define PF_SHORT_CLOCK_TIME		0.0
#define PF_CSWITCH_TIME			0.0
#define PF_IDLE_CHECK_DISPQ_TIME	0.0
#define PF_IO_WAKEUP_TIME		0.0

typedef struct p {
   int pid;
   int idler;
   u_int pfflags;
   u_int stat;
   u_int runcpu;
   u_int flags;
   void *chan;
   int  ios;
   int  ioreads;
   int  cswitches;
   int	active;
   int  sleeps;
   int  iosleep;
   int  iosleeps;
   double runiosleep;
   double lastsleep;
   double runsleep;
   double runtime;
   double falseidletime;
   double lasteventtime;
   statgen readtimelimitstats;
   statgen writetimelimitstats;
   statgen readmisslimitstats;
   statgen writemisslimitstats;
   ioreq_event *ioreq;
   event *eventlist;
   char *space;
   struct p *livelist;
   struct p *link;
   struct p *next;
} process;

typedef struct {
   double      time;
   int         type;
   event      *next;
   event      *prev;
   int         cpunum;
   process    *procp;
   intr_event *intrp;
} cpu_event;

typedef struct {
   double     scale;
   int        state;
   cpu_event *current;
   int        runpreempt;
   event     *idleevents;
   double     idlestart;
   double     idletime;
   double     falseidletime;
   double     idleworktime;
   int        intrs;
   int        iointrs;
   int        clockintrs;
   int        tickintrs;
   int        extintrs;
   double     runintrtime;
   double     runiointrtime;
   double     runclockintrtime;
   double     runtickintrtime;
   double     runextintrtime;
   int        cswitches;
   double     runswitchtime;
} cpu;

typedef struct {
   double time;
   int    type;
   event * next;
   event * prev;
   int    vector;
} intend_event;

typedef struct {
   double time;
   int    type;
   event * next;
   event * prev;
   process *newprocp;
} cswitch_event;

typedef struct {
   double time;
   int    type;
   event * next;
   event * prev;
} idleloop_event;

typedef struct {
   double time;
   int    type;
   event * next;
   event * prev;
   int    info;
   void  *chan;
   int    iosleep;
   int    sleeptime;
} sleep_event;

typedef struct {
   double time;
   int    type;
   event * next;
   event * prev;
   int    info;
   void  *chan;
   int    dropit;
} wakeup_event;

extern cpu      cpus[];
extern process *process_livelist;
extern process *pf_dispq;
extern process *sleepqueue;
extern int	numcpus;

/* disksim_pfsim.c functions */

extern process * pf_getfromextra_process_q();
extern void      pf_idle_cpu_recheck_dispq();
extern process * pf_new_process();

/* disksim_pfdisp.c functions */

extern void      pf_dispatcher_init();
extern process * pf_disp_get_from_sleep_queue();
extern process * pf_disp_get_specific_from_sleep_queue();
extern void      pf_disp_put_on_sleep_queue();
extern process * pf_dispatch();
extern process * pf_disp_sleep();
extern void      pf_disp_wakeup();

#endif /* DISKSIM_PFSIM_H */

