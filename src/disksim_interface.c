
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

/*****************************************************************************/
/*

This file offers a suggested interface between disksim and a full system
simulator.  Using this interface (which assumes only a few characteristics
of the system simulator), disksim will act as a slave of the system simulator,
providing disk request completion indications in time for an interrupt to be
generated in the system simulation.  Specifically, disksim code will only be
executed when invoked by one of the procedures in this file.

Using ths interface requires only two significant functionalities of the
system simulation environment:

1. the ability to function correctly without knowing when a disk request
will complete at the time that it is initiated.  Via the code in this file,
the system simulation will be informed at some later point in its view of
time that the request is completed.  (At this time, the appropriate "disk
request completion" interrupt could be inserted into the system simulation.)

2. the ability for disksim to register a callback with the system simulation
environment.  That is, this interface code must be able to say, "please
invoke this function when the simulated time reaches X".  It is also helpful
to be able to "deschedule" a callback at a later time -- but lack of this
support can certainly be worked around.

(NOTE: using this interface requires compiling disksim.c with -DEXTERNAL.)

*/
/*****************************************************************************/


#include "disksim_global.h"
#include "disksim_ioface.h"
#include "syssim_driver.h"


/* called once at simulation initialization time */

void disksim_initialize (const char *pfile, const char *ofile)
{
   const char *argv[6];
   extern void disksim_extface_io_done_notify();

/*
   fprintf (stder, "disksim_initialize\n");
*/

   argv[0] = "disksim";
   argv[1] = pfile;
   argv[2] = ofile;
   argv[3] = "external";
   argv[4] = "0";
   argv[5] = "0";
   disksim_main (6, argv);

   set_external_io_done_notify (disksim_extface_io_done_notify);

/*
   fprintf (stder, "disksim_initialize done\n");
*/
}


/* called once at simulation shutdown time */

void disksim_shutdown (SysTime syssimtime)
{
   double curtime = SYSSIMTIME_TO_MS(syssimtime);

/*
   fprintf (stderr, "disksim_shutdown\n");
*/

   if ((curtime + 0.0001) < simtime) {
      fprintf (stderr, "external time is ahead of disksim time: %f > %f\n", curtime, simtime);
      exit(0);
   }

   simtime = curtime;
   cleanstats();
   printstats();

/*
   fprintf (stderr, "disksim_shutdown done\n");
*/
}


/* Prints the current disksim statistics.  This call does not reset the    */
/* statistics, so the simulation can continue to run and calculate running */
/* totals.  "syssimtime" should be the current simulated time of the       */
/* system-level simulation.                                                */

void disksim_dump_stats (SysTime syssimtime)
{
   double curtime = SYSSIMTIME_TO_MS(syssimtime);

/*
   fprintf (stderr, "disksim_dump_stats\n");
*/

   if ((intq) && (intq->time < curtime) && ((intq->time + 0.0001) >= curtime)) {
      curtime = intq->time;
   }
   if (((curtime + 0.0001) < simtime) || ((intq) && (intq->time < curtime))) {
      fprintf (stderr, "external time is mismatched with disksim time: %f vs. %f (%f)\n", curtime, simtime, ((intq) ? intq->time : 0.0));
      exit (0);
   }

   simtime = curtime;
   cleanstats();
   printstats();

/*
   fprintf (stderr, "disksim_dump_stats done\n");
*/
}


/* This is the callback for handling internal disksim events while running */
/* as a slave of a system-level simulation.  "syssimtime" should be the    */
/* current simulated time of the system-level simulation.                  */

void disksim_internal_event (SysTime syssimtime)
{
   double curtime = SYSSIMTIME_TO_MS(syssimtime);

/*
   fprintf (stderr, "disksim_internal_event\n");
*/

   /* if next event time is less than now, error.  Also, if no event is  */
   /* ready to be handled, then this is a spurious callback -- it should */
   /* not be possible with the descheduling below (allow it if it is not */
   /* possible to deschedule.                                            */

   if ((intq == NULL) || ((intq->time + 0.0001) < curtime)) {
      fprintf (stderr, "external time is ahead of disksim time: %f > %f\n", curtime, ((intq) ? intq->time : 0.0));
      exit (0);
   }

/*
    fprintf(stderr, "disksim_internal_event: intq->time=%f curtime=%f\n", intq->time, curtime);
*/

   /* while next event time is same as now, handle next event */
   ASSERT (intq->time >= simtime);
   while ((intq != NULL) && (intq->time <= (curtime + 0.0001))) {

/*
      fprintf (stderr, "handling internal event: type %d\n", intq->type);
*/

      disksim_simulate_event();
   }

   if (intq != NULL) {
      syssim_schedule_callback (disksim_internal_event, MS_TO_SYSSIMTIME(intq->time));
   }

/*
   fprintf (stderr, "disksim_internal_event done\n");
*/
}


/* This is the disksim callback for reporting completion of a disk request */
/* to the system-level simulation -- the system-level simulation should    */
/* incorporate this completion as appropriate (probably by inserting a     */
/* simulated "disk completion interrupt" at the specified simulated time). */
/* Based on the requestdesc pointed to by "curr->buf" (below), the         */
/* system-level simulation should be able to determine which request       */
/* completed.  (A ptr to the system-level simulator's request structure    */
/* is a reasonable use of "curr->buf".)                                    */

void disksim_extface_io_done_notify (ioreq_event *curr)
{
/*
fprintf (stderr, "disksim_extface_io_done_notify\n");
*/

   syssim_report_completion (MS_TO_SYSSIMTIME(simtime), (Request *)curr->buf);

/*
fprintf (stderr, "disksim_extface_io_done_notify done\n");
*/
}


/* This function should be called by the system-level simulation when    */
/* it wants to issue a request into disksim.  "syssimtime" should be the */
/* system-level simulation time at which the request should "arrive".    */
/* "requestdesc" is the system-level simulator's description of the      */
/* request (device number, block number, length, etc.).                  */

void disksim_request_arrive (SysTime syssimtime, Request *requestdesc)
{
   double curtime = SYSSIMTIME_TO_MS(syssimtime);
   ioreq_event *new = (ioreq_event *) getfromextraq();

/*
fprintf (stderr, "disksim_request_arrive\n");
*/

   assert (new != NULL);
   new->type = IO_REQUEST_ARRIVE;
   new->time = curtime;
   new->busno = 0;
   new->devno = requestdesc->devno;
   new->blkno = requestdesc->blkno;
   new->bcount = requestdesc->bytecount / 512;
   new->flags = isread(requestdesc) ? READ : WRITE;
   new->cause = 0;
   new->opid = 0;
   new->buf = requestdesc;

   io_map_trace_request (new);

   /* issue it into simulator */
   if (intq) {
      syssim_deschedule_callback (disksim_internal_event);
   }
   addtointq (new);

   /* while next event time is same as now, handle next event */
   while ((intq != NULL) && (intq->time <= (curtime + 0.0001))) {
      disksim_simulate_event ();
   }

   if (intq) {
      syssim_schedule_callback (disksim_internal_event, MS_TO_SYSSIMTIME(intq->time));
   }

/*
fprintf (stderr, "disksim_request_arrive done\n");
*/
}

