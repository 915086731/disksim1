
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

#include "disksim_global.h"
#include "disksim_iosim.h"
#include "disksim_stat.h"
#include "disksim_disk.h"
#include "disksim_ioqueue.h"
#include "disksim_bus.h"

extern int disk_printhack;
extern double disk_printhacktime;

extern double disk_last_read_arrival[];
extern double disk_last_read_completion[];
extern double disk_last_write_arrival[];
extern double disk_last_write_completion[];

int currcylno = 0;
int currsurface = 0;
double currtime = 0.0;
double currangle = 0.0;

int swap_forward_only = 1;

/* *ESTIMATED* command processing overheads for buffer cache hits.  These */
/* values are not actually used for determining request service times.    */
double buffer_partial_servtime = 0.000000001;
double reading_buffer_partial_servtime = 0.000000001;
double buffer_whole_servtime = 0.000000000;
double reading_buffer_whole_servtime = 0.000000000;

extern int extra_write_disconnects;
extern int seekdistance;
extern double disk_seek_stoptime;
extern int remapsector;
extern double addtolatency;
extern int trackstart;

int bandstart;

double disk_get_servtime(diskno, req, checkcache, maxtime)
int diskno;
ioreq_event *req;
int checkcache;
double maxtime;
{
   double servtime;

   ASSERT1((diskno >= 0) && (diskno < numdisks), "diskno", diskno);
   servtime = disk_buffer_estimate_servtime(&disks[diskno], req, checkcache, maxtime);
   return(servtime);
}


double disk_get_acctime(diskno, req, maxtime)
int diskno;
ioreq_event *req;
double maxtime;
{
   double acctime;

   ASSERT1((diskno >= 0) && (diskno < numdisks), "diskno", diskno);
   acctime = disk_buffer_estimate_acctime(&disks[diskno], req, maxtime);
   return(acctime);
}


int disk_set_depth(diskno, inbusno, depth, slotno)
int diskno;
int inbusno;
int depth; 
int slotno;
{
   int cnt;

   ASSERT1((diskno >= 0) && (diskno < numdisks), "diskno", diskno);
   cnt = disks[diskno].numinbuses;
   disks[diskno].numinbuses++;
   if ((cnt + 1) > MAXINBUSES) {
      fprintf(stderr, "Too many inbuses specified for disk %d - %d\n", diskno, (cnt+1));
      exit(0);
   }
   disks[diskno].inbuses[cnt] = inbusno;
   disks[diskno].depth[cnt] = depth;
   disks[diskno].slotno[cnt] = slotno;
   return(0);
}


int disk_get_depth(diskno)
int diskno;
{
   ASSERT1((diskno >= 0) && (diskno < numdisks), "diskno", diskno);
   return(disks[diskno].depth[0]);
}


int disk_get_slotno(diskno)
int diskno;
{
   ASSERT1((diskno >= 0) && (diskno < numdisks), "diskno", diskno);
   return(disks[diskno].slotno[0]);
}


int disk_get_inbus(diskno)
int diskno;
{
   ASSERT1((diskno >= 0) && (diskno < numdisks), "diskno", diskno);
   return(disks[diskno].inbuses[0]);
}


int disk_get_maxoutstanding(diskno)
int diskno;
{
   ASSERT1((diskno >= 0) && (diskno < numdisks), "diskno", diskno);
   return(disks[diskno].maxqlen);
}


double disk_get_blktranstime(curr)
ioreq_event *curr;
{
   double tmptime;

   tmptime = bus_get_transfer_time(disk_get_busno(curr), 1, (curr->flags & READ));
   if (tmptime < disks[(curr->devno)].blktranstime) {
      tmptime = disks[(curr->devno)].blktranstime;
   }
   return(tmptime);
}


int disk_get_busno(curr)
ioreq_event *curr;
{
   intchar busno;
   int depth;

   busno.value = curr->busno;
   depth = disks[(curr->devno)].depth[0];
   return(busno.byte[depth]);
}


void disk_send_event_up_path(curr, delay)
ioreq_event *curr;
double delay;
{
   int busno;
   int slotno;
/*
fprintf (outputfile, "Disk_send_event_up_path - devno %d, type %d, cause %d, blkno %d\n", curr->devno, curr->type, curr->cause, curr->blkno);
*/
   busno = disk_get_busno(curr);
   slotno = disks[curr->devno].slotno[0];
   curr->next = disks[(curr->devno)].buswait;
   disks[(curr->devno)].buswait = curr;
   curr->tempint1 = busno;
   curr->time = delay;
   if (disks[curr->devno].busowned == -1) {
/*
fprintf (outputfile, "Must get ownership of the bus first\n");
*/
      if (curr->next) {
	 fprintf(stderr,"Multiple bus requestors detected in disk_send_event_up_path\n");
      }
      if (bus_ownership_get(busno, slotno, curr) == FALSE) {
	 disks[(curr->devno)].stat.requestedbus = simtime;
      } else {
         bus_delay(busno, DISK, curr->devno, delay, curr);
      }
   } else if (disks[curr->devno].busowned == busno) {
/*
fprintf (outputfile, "Already own bus - so send it on up\n");
*/
      bus_delay(busno, DISK, curr->devno, delay, curr);
   } else {
      fprintf(stderr, "Wrong bus owned for transfer desired\n");
      exit(0);
   }
}


void disk_bus_ownership_grant(devno, curr, busno, arbdelay)
int devno;
ioreq_event *curr;
int busno;
double arbdelay;
{
   ioreq_event *tmp;

   if ((devno < 0) || (devno > numdisks)) {
      fprintf(stderr, "Event arrive for illegal disk - devno %d, numdisks %d\n", devno, numdisks);
      exit(0);
   }

   tmp = disks[devno].buswait;
   while ((tmp != NULL) && (tmp != curr)) {
      tmp = tmp->next;
   }
   if (tmp == NULL) {
      fprintf(stderr, "Bus ownership granted to unknown controller request - devno %d, busno %d\n", devno, busno);
      exit(0);
   }
   disks[devno].busowned = busno;
   disks[devno].stat.waitingforbus += simtime - disks[devno].stat.requestedbus;
   ASSERT (arbdelay == (simtime - disks[devno].stat.requestedbus));
   disks[devno].stat.numbuswaits++;
   bus_delay(busno, DISK, devno, tmp->time, tmp);
}


void disk_bus_delay_complete(devno, curr, sentbusno)
int devno;
ioreq_event *curr;
int sentbusno;
{
   intchar slotno;
   intchar busno;
   int depth;

   if ((devno < 0) || (devno > numdisks)) {
      fprintf(stderr, "Event arrive for illegal disk - devno %d, numdisks %d\n", devno, numdisks);
      exit(0);
   }
/*
fprintf (outputfile, "Entered disk_bus_delay_complete\n");
*/
   if (curr == disks[devno].buswait) {
      disks[devno].buswait = curr->next;
   } else {
      fprintf(stderr, "Must have multiple pending messages - need to fix or handle\n");
      fprintf(stderr, "%f, Completed delay %p, expected %p, next %p\n", simtime, curr, disks[devno].buswait, disks[devno].buswait->next);
      fprintf(stderr, "type1 %d, type2 %d, cause1 %d, cause2 %d, blkno1 %d, blkno2 %d\n", curr->type, disks[devno].buswait->type, curr->cause, disks[devno].buswait->cause, curr->blkno, disks[devno].buswait->blkno);
      exit(0);
   }
   busno.value = curr->busno;
   slotno.value = curr->slotno;
   depth = disks[devno].depth[0];
   slotno.byte[depth] = slotno.byte[depth] >> 4;
   curr->time = 0.0;
   if (depth == 0) {
      intr_request(curr);
   } else {
      bus_deliver_event(busno.byte[depth], slotno.byte[depth], curr);
   }
}


void disk_buffer_update_inbuffer(currdisk, seg)
disk *currdisk;
segment *seg;
{
   /* This is an out of date function from when we improved simulation   */
   /* performance by using an event that consisted of reading or writing */
   /* all blocks of interest on a given track (after any positioning).   */
   /* This function was intended to fix update the disk's buffer partway */
   /* thru the completion of the access so as to allow overlapped media  */
   /* and bus transfers. */

   if ((seg == NULL) || (seg->state == BUFFER_EMPTY) || (seg->state == BUFFER_CLEAN) || (seg->state == BUFFER_DIRTY)) {
      return;
   }
   disk_buffer_segment_wrap(seg, seg->endblkno);

/* COMMENT ALL OUT
   double tmptime;
   ioreq_event *tmp;
   double blks = 0.0;
   if (seg->access->type == DISK_BUFFER_TRACKACC_DONE) {
      tmp = seg->access;
      tmptime = tmp->time;
      currtime = currdisk->currtime;
      currcylno = currdisk->currcylno;
      currsurface = currdisk->currsurface;
      currangle = currdisk->currangle;
      tmp->time = seg->time + disklatency(currdisk, tmp->tempptr1, seg->time, tmp->cause, tmp->bcount, 0);
      if (tmp->time < simtime) {
	 blks = (double) ((band *) tmp->tempptr1)->blkspertrack;
	 blks = (blks / currdisk->rotatetime) * (simtime - tmp->time);
	 if (seg->state == BUFFER_READING) {
	    seg->endblkno = tmp->blkno + (int) blks;
         } else {
	    currdiskreq->inblkno = tmp->blkno + (int) blks;
	 }
      }
      tmp->time = tmptime;
   }
*/
}


void disk_prepare_for_data_transfer(curr)
ioreq_event *curr;
{
   disk *currdisk = &disks[(curr->devno)];
   diskreq *currdiskreq = curr->ioreq_hold_diskreq;
/*
fprintf (outputfile, "%f, Entering disk_prepare_for_data_transfer: diskno = %d\n", simtime, curr->devno);
*/

   if (currdiskreq->flags & FINAL_WRITE_RECONNECTION_1) {
      currdiskreq->flags |= FINAL_WRITE_RECONNECTION_2;
   }
   addtoextraq((event *) curr);
   disk_check_bus(currdisk,currdiskreq);
}


/* check to see if current prefetch should be aborted */

void disk_check_prefetch_swap(currdisk)
disk* currdisk;
{
   ioreq_event *nextioreq;
   diskreq     *nextdiskreq;
   int		setseg = FALSE;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f            Entering disk_check_prefetch_swap for disk %d\n", simtime, currdisk->devno);
fflush(outputfile);
}
   if (!currdisk->effectivehda) {
      fprintf(stderr, "NULL effectivehda in disk_check_prefetch_swap\n");
      exit(0);
   }

   if (!currdisk->effectivehda->seg) {
      fprintf(stderr, "NULL effectivehda->seg in disk_check_prefetch_swap\n");
      exit(0);
   }

   if (!(currdisk->effectivehda->seg->access->flags & BUFFER_BACKGROUND) ||
       !(currdisk->effectivehda->seg->access->flags & READ)) {
      fprintf(stderr, "effectivehda->seg->access is not a pure prefetch in disk_check_prefetch_swap\n");
      exit(0);
   }

   nextioreq = ioqueue_show_next_request(currdisk->queue,disk_enablement_function);
   if (nextioreq) {
      nextdiskreq = nextioreq->ioreq_hold_diskreq;
      if (!nextdiskreq->seg) {
	 setseg = TRUE;
         disk_buffer_select_segment(currdisk,nextdiskreq,FALSE);
      }
      if (nextioreq->flags & READ) {
	 disk_activate_read(currdisk,nextdiskreq,setseg,TRUE);
      } else {
	 disk_activate_write(currdisk,nextdiskreq,setseg,TRUE);
      }
   }
}


/* send completion up the line */

void disk_request_complete(currdisk, currdiskreq, curr)
disk        *currdisk;
diskreq     *currdiskreq;
ioreq_event *curr;
{
   ioreq_event *tmpioreq = currdiskreq->ioreqlist;
   diskreq     *nextdiskreq;
   double       delay;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f  %8p  Entering disk_request_complete\n", simtime, currdiskreq);
fflush(outputfile);
}

   if (currdisk->effectivebus != currdiskreq) {
      fprintf(stderr, "currdiskreq != effectivebus in disk_request_complete\n");
      exit(0);
   }

   if (curr->blkno != tmpioreq->blkno) {
      addtoextraq((event *) curr);
      curr = ioreq_copy(tmpioreq);
   }

   currdisk->lastflags = curr->flags;
   if (currdisk->outstate != DISK_TRANSFERING) {
      fprintf(stderr, "Disk not transfering in disk_request_complete\n");
      exit(0);
   }

   currdiskreq->flags |= COMPLETION_SENT;

   if (currdisk->acctime >= 0.0) {
      if (currdiskreq->flags & HDA_OWNED) {
         fprintf(stderr, "disk_request_complete:  HDA_OWNED set for fixed-access-time disk\n");
         exit(0);
      }
      if (!ioqueue_get_specific_request(currdisk->queue,tmpioreq)) {
         fprintf(stderr, "disk_request_complete:  ioreq_event not found by ioqueue_get_specific_request call\n");
         exit(0);
      }
      disk_interferestats(currdisk, tmpioreq);
      if (!ioqueue_physical_access_done(currdisk->queue,tmpioreq)) {
         fprintf(stderr, "disk_request_complete:  ioreq_event not found by ioqueue_physical_access_done call\n");
         exit(0);
      }
      currdisk->currentbus = 
	currdisk->currenthda = 
	currdisk->effectivehda = NULL;

      tmpioreq = ioqueue_show_next_request(currdisk->queue,disk_enablement_function);
      if (tmpioreq) {
	 nextdiskreq = (diskreq*) tmpioreq->ioreq_hold_diskreq;
	 if (nextdiskreq->ioreqlist->flags & READ) {
	    disk_activate_read(currdisk, nextdiskreq, FALSE, TRUE);
	 } else {
	    disk_activate_write(currdisk, nextdiskreq, FALSE, TRUE);
	 }
      }
   } else if (tmpioreq->flags & READ) {
      while (tmpioreq) {
         if (!(currdiskreq->flags & HDA_OWNED)) {
            if (!ioqueue_get_specific_request(currdisk->queue,tmpioreq)) {
               fprintf(stderr, "disk_request_complete:  ioreq_event not found by ioqueue_get_specific_request call\n");
               exit(0);
            }
            disk_interferestats(currdisk, tmpioreq);
         }
         if (!ioqueue_physical_access_done(currdisk->queue,tmpioreq)) {
            fprintf(stderr, "disk_request_complete:  ioreq_event not found by ioqueue_physical_access_done call\n");
            exit(0);
         }
	 tmpioreq = tmpioreq->next;
      }
      if ((currdisk->keeprequestdata == -1) &&
	  !currdiskreq->seg->diskreqlist->seg_next){
         if (currdiskreq->outblkno > currdiskreq->seg->endblkno) {
            fprintf(stderr, "Unable to erase request data from segment in disk_request_complete\n");
            exit(0);
	 }
	 currdiskreq->seg->startblkno = currdiskreq->outblkno;
      }

      if ((currdiskreq == currdisk->effectivehda) ||
          (currdiskreq == currdisk->currenthda)) {
         if (currdiskreq->seg->access->type == NULL_EVENT) {
            disk_release_hda(currdisk,currdiskreq);
         } else {
            if (currdisk->preseeking != NO_PRESEEK) {
	       disk_check_prefetch_swap(currdisk);
	    }
	 }
      }
   } else {  			/* WRITE */
      if ((currdiskreq == currdisk->effectivehda) ||
          (currdiskreq == currdisk->currenthda)) {
         if (currdiskreq->seg->access->type == NULL_EVENT) {
            disk_release_hda(currdisk,currdiskreq);
         }
      }
   }

   curr->time = simtime;
   curr->type = IO_INTERRUPT_ARRIVE;
   curr->cause = COMPLETION;
   delay = (curr->flags & READ) ? currdisk->overhead_complete_read : currdisk->overhead_complete_write;
   if (currdisk->acctime >= 0.0) {
      delay = 0.0;
   }
   disk_send_event_up_path(curr, (delay * currdisk->timescale));
   currdisk->outstate = DISK_WAIT_FOR_CONTROLLER;
}


void disk_reconnection_or_transfer_complete(curr)
ioreq_event *curr;
{
   disk        *currdisk = &disks[(curr->devno)];
   diskreq     *currdiskreq;
   ioreq_event *tmpioreq;
   double delay;

   currdiskreq = currdisk->effectivebus;
   if (!currdiskreq) {
      currdiskreq = currdisk->effectivebus = currdisk->currentbus;
   }
   if (!currdiskreq) {
      fprintf(stderr, "effectivebus and currentbus are NULL in disk_reconnection_or_transfer_complete\n");
      exit(0);
   }

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f  %8p  Entering disk_reconnection_or_transfer_complete for disk %d\n", simtime, currdiskreq, curr->devno);
fflush(outputfile);
}

   tmpioreq = currdiskreq->ioreqlist;
   while (tmpioreq) {
      if (tmpioreq->blkno == curr->blkno) {
	 break;
      }
      tmpioreq = tmpioreq->next;
   }
   if (!tmpioreq) {
      fprintf(stderr, "ioreq_event not found in effectivebus in disk_reconnection_or_transfer_complete\n");
      exit(0);
   }

   switch (currdisk->outstate) {
      case DISK_WAIT_FOR_CONTROLLER:
      case DISK_TRANSFERING:
	 break;
      default:
         fprintf(stderr, "Disk not waiting to transfer in disk_reconnection_or_transfer_complete - devno %d, state %d\n", curr->devno, currdisk->outstate);
         exit(0);
   }
   currdisk->effectivebus = currdiskreq;
   currdisk->outstate = DISK_TRANSFERING;
   curr->type = DISK_DATA_TRANSFER_COMPLETE;
   curr = disk_buffer_transfer_size(currdisk, currdiskreq, curr);

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "                        disk_buffer_transfer_size set bcount to %d\n", curr->bcount);
fflush(outputfile);
}

   if (curr->bcount == -2) {
      if (currdisk->outwait) {
         fprintf(stderr, "non-NULL outwait found in disk_reconnection_or_transfer_complete\n");
         exit(0);
      }
      currdisk->outwait = curr;
   } else if (curr->bcount == -1) {
      curr->type = IO_INTERRUPT_ARRIVE;
      curr->cause = DISCONNECT;
      if (tmpioreq->flags & READ) {
         delay = ((currdisk->lastflags & READ) ? currdisk->overhead_disconnect_read_afterread : currdisk->overhead_disconnect_read_afterwrite);
      } else {
         delay = ((currdiskreq->flags & EXTRA_WRITE_DISCONNECT) ? currdisk->extradisc_disconnect2 : currdisk->overhead_disconnect_write);
      }
      disk_send_event_up_path(curr, (delay * currdisk->timescale));
      currdisk->outstate = DISK_WAIT_FOR_CONTROLLER;
   } else if (curr->bcount == 0) {
      disk_request_complete(currdisk, currdiskreq, curr);
   } else if (curr->bcount > 0) {
      currdisk->starttrans = simtime;
      currdisk->blksdone = 0;
      disk_send_event_up_path(curr, (double) 0.0);
   }
}


/* If no SEG_OWNER exists, give seg to first read diskreq with HDA_OWNED but 
   not effectivehda, or first diskreq with BUFFER_APPEND, or effectivehda 
   if no such diskreqs exist (and the seg is appropriate)
*/

void disk_find_new_seg_owner(currdisk,seg)
disk    *currdisk;
segment *seg;
{
   diskreq *currdiskreq = seg->diskreqlist;
   diskreq *bestdiskreq = NULL;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f            Entering disk_find_new_seg_owner for disk %d\n", simtime, currdisk->devno);
fflush(outputfile);
}

   ASSERT(seg != NULL);
   ASSERT(seg->recyclereq == NULL);

   while (currdiskreq) {
      if (currdiskreq->flags & SEG_OWNED) {
	 return;
      }
      if (!bestdiskreq && (currdiskreq != currdisk->effectivehda) && 
	  (currdiskreq->flags & HDA_OWNED) && (currdiskreq->ioreqlist) &&
	  (currdiskreq->ioreqlist->flags & READ)) {
	 bestdiskreq = currdiskreq;
      }
      currdiskreq = currdiskreq->seg_next;
   }

   if (!bestdiskreq) {
      currdiskreq = seg->diskreqlist;
      while (currdiskreq) {
         if (currdiskreq->hittype == BUFFER_APPEND) {
	    bestdiskreq = currdiskreq;
	    break;
         }
         currdiskreq = currdiskreq->seg_next;
      }
   }

   if (bestdiskreq) {
      disk_buffer_attempt_seg_ownership(currdisk,bestdiskreq);
   } else if (currdisk->effectivehda && 
	      (currdisk->effectivehda->seg == seg)) {
      disk_buffer_attempt_seg_ownership(currdisk,currdisk->effectivehda);
   }
}


/* If pure prefetch, release the hda and free the diskreq
   If active read, release the hda if preseeking level is appropriate
   If fast write, release the hda, remove the event(s) from the ioqueue,
     and, if COMPLETION_RECEIVED, free all structures.  If
     LIMITED_FASTWRITE and an appended request exists, make it the
     next effectivehda.
   If slow write, call the completion routine and release the hda if
     preseeking level is appropriate
*/

void disk_release_hda(currdisk, currdiskreq)
disk    *currdisk;
diskreq *currdiskreq;
{
   ioreq_event *tmpioreq;
   ioreq_event *holdioreq;
   segment *seg = currdiskreq->seg;
   diskreq *tmpdiskreq;

   diskreq *nextdiskreq = NULL;
   int release_hda = FALSE;
   int free_structs = FALSE;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f  %8p  Entering disk_release_hda for disk %d\n", simtime, currdiskreq,currdisk->devno);
fflush(outputfile);
}

   ASSERT(currdiskreq != NULL);

   ASSERT2p((currdiskreq == currdisk->effectivehda), "currdiskreq", currdiskreq, "effectivehda", currdisk->effectivehda);

   ASSERT(seg != NULL);

   if (!currdiskreq->ioreqlist) {
      if (!(currdiskreq->flags & COMPLETION_RECEIVED)) {
         fprintf(stderr, "COMPLETION_RECEIVED not flagged for pure prefetch in disk_release_hda\n");
         exit(0);
      }
      release_hda = TRUE;
      free_structs = TRUE;
      seg->state = ((currdisk->enablecache || seg->diskreqlist->seg_next) ? BUFFER_CLEAN : BUFFER_EMPTY);
   } else if (currdiskreq->ioreqlist->flags & READ) {
      if ((currdisk->preseeking == PRESEEK_BEFORE_COMPLETION) ||
          ((currdisk->preseeking == PRESEEK_DURING_COMPLETION) && 
	   (currdiskreq->flags & COMPLETION_SENT))) {
         release_hda = TRUE;
         seg->state = ((currdisk->enablecache || seg->diskreqlist->seg_next || !(currdiskreq->flags & COMPLETION_SENT)) ? BUFFER_CLEAN : BUFFER_EMPTY);
      }
   } else if ((currdiskreq->flags & COMPLETION_RECEIVED) ||
	      (currdisk->preseeking == PRESEEK_BEFORE_COMPLETION) ||
	      ((currdisk->preseeking == PRESEEK_DURING_COMPLETION) && 
	       (currdiskreq->flags & COMPLETION_SENT))) {
      release_hda = TRUE;
      if (currdiskreq->flags & COMPLETION_RECEIVED) {
         free_structs = TRUE;
      }

      /* check to see if seg should be left DIRTY */

      tmpdiskreq = seg->diskreqlist;
      while (tmpdiskreq) {
         if (tmpdiskreq != currdiskreq) {
	    tmpioreq = tmpdiskreq->ioreqlist;
	    ASSERT(tmpioreq != NULL);
	    if (!(tmpioreq->flags & READ)) {
	       while (tmpioreq->next) {
	          tmpioreq = tmpioreq->next;
	       }
               if (tmpdiskreq->inblkno < (tmpioreq->blkno + tmpioreq->bcount)) {
	          seg->state = BUFFER_DIRTY;
                  if ((tmpdiskreq->hittype == BUFFER_APPEND) &&
                      (currdisk->fastwrites == LIMITED_FASTWRITE)) {
	             tmpioreq = currdiskreq->ioreqlist;
	             ASSERT(tmpioreq != NULL);
	             while (tmpioreq->next) {
	                tmpioreq = tmpioreq->next;
	             }
		     if (tmpdiskreq->ioreqlist->blkno == (tmpioreq->blkno + tmpioreq->bcount)) {
                        nextdiskreq = tmpdiskreq;
		     }
	          }
	          break;
               }
	    }
         }
         tmpdiskreq = tmpdiskreq->seg_next;
      }

      if (!tmpdiskreq) {
	 if (seg->diskreqlist->seg_next) {
	    seg->state = BUFFER_CLEAN;
	 } else {
            seg->state = ((currdisk->enablecache && currdisk->readhitsonwritedata) ? BUFFER_CLEAN : BUFFER_EMPTY);
	 }
         currdisk->numdirty--;
if ((disk_printhack > 1) && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "                        numdirty-- = %d\n",currdisk->numdirty);
fflush(outputfile);
}
         ASSERT1(((currdisk->numdirty >= 0) && (currdisk->numdirty <= currdisk->numwritesegs)),"numdirty",currdisk->numdirty);
      }
   }

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "                        free_structs = %d, release_hda = %d\n", free_structs, release_hda);
fflush(outputfile);
}

   if (free_structs) {
      disk_buffer_remove_from_seg(currdiskreq);
      tmpioreq = currdiskreq->ioreqlist;
      while (tmpioreq) {
	 holdioreq = tmpioreq->next;
         addtoextraq((event *) tmpioreq);
         tmpioreq = holdioreq;
      }
      addtoextraq((event *) currdiskreq);
      if (seg->recyclereq) {
         /* I don't think the following code ever gets used... */
         fprintf(stderr,"GOT HERE!  SURPRISE!\n");
	 if (seg->diskreqlist) {
            fprintf(stderr, "non-NULL diskreqlist found for recycled seg in disk_release_hda\n");
            exit(0);
	 }
	 nextdiskreq = seg->recyclereq;
         disk_buffer_set_segment(currdisk,seg->recyclereq);
         disk_buffer_attempt_seg_ownership(currdisk,seg->recyclereq);
         seg->recyclereq = NULL;
      } else if (seg->diskreqlist) {
         disk_find_new_seg_owner(currdisk,seg);
      }
   }

   if (release_hda) {
      if (currdisk->currenthda == currdisk->effectivehda) {
	 currdisk->currenthda = NULL;
      }
      currdisk->effectivehda = NULL;
      if (seg->access->type != NULL_EVENT) {
         fprintf(stderr, "non-NULL seg->access->type found upon releasing hda in disk_release_hda\n");
         exit(0);
      }
      disk_check_hda(currdisk, nextdiskreq, TRUE);
   }

}


/* Set up an ioreq_event for a request if it needs bus service.
   If data is ready for transfer, bcount is set to the amount.
*/

ioreq_event* disk_request_needs_bus(currdisk,currdiskreq,check_watermark)
disk* currdisk;
diskreq* currdiskreq;
int check_watermark;
{
   ioreq_event *busioreq = NULL;
   ioreq_event *tmpioreq = currdiskreq->ioreqlist;
   diskreq     *seg_owner;
   segment     *seg = currdiskreq->seg;

   if ((currdisk->outstate != DISK_IDLE) && !currdisk->outwait) {
      return(NULL);
   }

   if ((currdiskreq->flags & FINAL_WRITE_RECONNECTION_1) &&
       !(currdiskreq->flags & FINAL_WRITE_RECONNECTION_2)) {
      return(NULL);
   }

   if (currdisk->acctime >= 0.0) {
       if ((currdiskreq == currdisk->currenthda) && 
	   (currdiskreq->overhead_done <= simtime)) {
          if (currdisk->outwait) {
	     busioreq = currdisk->outwait;
	     currdisk->outwait = NULL;
             busioreq->time = simtime;
	  } else {
	     busioreq = ioreq_copy(tmpioreq);
             busioreq->time = simtime;
             busioreq->type = IO_INTERRUPT_ARRIVE;
             busioreq->cause = RECONNECT;
	  }
	  busioreq->bcount = 0;
       }
       return(busioreq);
   }

   if (seg && (currdiskreq != seg->recyclereq) && tmpioreq && 
       !(currdiskreq->flags & COMPLETION_SENT)) {
      while (tmpioreq && tmpioreq->next) {
	 if (currdiskreq->outblkno < (tmpioreq->blkno + tmpioreq->bcount)) {
	    break;
	 }
	 tmpioreq = tmpioreq->next;
      }
      if (currdiskreq->ioreqlist->flags & READ) {
         if (currdiskreq->outblkno >= (tmpioreq->blkno + tmpioreq->bcount)) {
            fprintf(stderr, "read completion detected in disk_request_needs_bus\n");
            exit(0);
	 }
         disk_buffer_update_inbuffer(currdisk, seg);
	 if ((seg->endblkno > currdiskreq->outblkno) && 
	     (seg->startblkno <= currdiskreq->outblkno) &&
	     (!check_watermark ||
	      (seg->endblkno >= (tmpioreq->blkno + tmpioreq->bcount)) ||
	      ((seg->endblkno - currdiskreq->outblkno) >= currdiskreq->watermark))) {
	    if (currdisk->outwait) {
	       busioreq = currdisk->outwait;
	       currdisk->outwait = NULL;
               busioreq->time = simtime;
	    } else {
	       busioreq = ioreq_copy(tmpioreq);
               busioreq->time = simtime;
               busioreq->type = IO_INTERRUPT_ARRIVE;
               busioreq->cause = RECONNECT;
	    }
	    busioreq->bcount = min(seg->endblkno,(tmpioreq->blkno + tmpioreq->bcount)) - currdiskreq->outblkno;
	 }
      } else {			/* WRITE */
         seg_owner = disk_buffer_seg_owner(seg,FALSE);
         if (!seg_owner) {
	    seg_owner = currdiskreq;
         }
         disk_buffer_update_inbuffer(currdisk, seg);
	 if ((currdiskreq->inblkno >= (tmpioreq->blkno + tmpioreq->bcount)) ||
	     ((currdiskreq->outblkno == currdiskreq->ioreqlist->blkno) && 
	      (currdiskreq->hittype != BUFFER_APPEND)) ||
	     ((seg->endblkno == currdiskreq->outblkno) && 
	      (seg->endblkno < (tmpioreq->blkno + tmpioreq->bcount)) &&
	      ((seg->endblkno - seg_owner->inblkno) < seg->size) &&
	      (!check_watermark || 
	       ((seg->endblkno - currdiskreq->inblkno) <= currdiskreq->watermark)))) {
	    if (currdisk->outwait) {
	       busioreq = currdisk->outwait;
	       currdisk->outwait = NULL;
               busioreq->time = simtime;
	    } else {
	       busioreq = ioreq_copy(tmpioreq);
               busioreq->time = simtime;
               busioreq->type = IO_INTERRUPT_ARRIVE;
               busioreq->cause = RECONNECT;
	    }
	    if ((currdiskreq->outblkno == currdiskreq->ioreqlist->blkno) && 
	        (currdiskreq->hittype != BUFFER_APPEND)) {
               busioreq->bcount = min(tmpioreq->bcount,seg->size);
	    } else {
               busioreq->bcount = min((tmpioreq->blkno + tmpioreq->bcount - currdiskreq->outblkno),(seg->size - seg->endblkno + seg_owner->inblkno));
	    }
	 }
      }
   }

   return(busioreq);
}


/* Queue priority list:
   10  HDA_OWNED, !currenthda & !effectivehda (completions and full read hits)
    9		  effectivehda
    8		  currenthda
    7  Appending limited fastwrite (LIMITED_FASTWRITE and BUFFER_APPEND)
    6  Full sneakyintermediateread with seg
    5  Partial sneakyintermediateread with seg
    4  Full sneakyintermediateread currently without seg
    3  Partial sneakyintermediateread currently without seg
    2  Write prebuffer to effectivehda->seg
    1  Write prebuffer to currenthda->seg
    0  Write prebuffer with other seg
    -1 Write prebuffer currently without seg

   Not usable:
      requests which don't need bus service
      requests using recycled segs
      requests with no available segs

   If a sneakyintermediateread is detected which has a seg, was marked as 
   a read hit, has not transferred any data yet, and is no longer a hit, 
   remove the diskreq from the segment.

   Possible improvements:
     un-set segment if read hit is no longer a hit and no data has been
     transferred yet.

     reverse queue order (oldest first) so that we don't have to peruse
     the entire queue if a 10 is found
*/

diskreq* disk_select_bus_request(currdisk, busioreq)
disk* currdisk;
ioreq_event** busioreq;
{
   diskreq *currdiskreq = currdisk->pendxfer;
   diskreq *bestdiskreq = NULL;
   ioreq_event *currioreq;
   ioreq_event *tmpioreq;
   int curr_value;
   int best_value = -99;
   int curr_set_segment;
   int best_set_segment = FALSE;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f         Entering disk_select_bus_request for disk %d\n", simtime, currdisk->devno);
fflush(outputfile);
}

   while (currdiskreq) {
      curr_value = -100;
      curr_set_segment = FALSE;
      if (!currdiskreq->ioreqlist) {
         fprintf(stderr, "diskreq with NULL ioreqlist found in bus queue in disk_select_bus_request\n");
         exit(0);
      } else if (currdiskreq->seg && 
		 (currdiskreq->seg->recyclereq == currdiskreq)) {
      } else if (currdiskreq->flags & HDA_OWNED) {
	 currioreq = disk_request_needs_bus(currdisk,currdiskreq,TRUE);
	 if (currioreq) {
	    if (currdiskreq == currdisk->effectivehda) {
	       curr_value = 9;
	    } else if (currdiskreq == currdisk->currenthda) {
	       curr_value = 8;
	    } else {
	       curr_value = 10;
	    }
	 }
      } else if ((best_value <= 7) && 
		 (currdisk->fastwrites == LIMITED_FASTWRITE) && 
		 (currdiskreq->hittype == BUFFER_APPEND)) {
	 currioreq = disk_request_needs_bus(currdisk,currdiskreq,TRUE);
	 if (currioreq) {
	    curr_value = 7;
	 }
      } else if ((best_value <= 6) &&
                 (currdisk->sneakyintermediatereadhits) &&
		 (currdiskreq->ioreqlist->flags & READ)) {
         if (currdiskreq->seg) {
	    currioreq = disk_request_needs_bus(currdisk,currdiskreq,TRUE);
	    if (currioreq) {
	       tmpioreq = currdiskreq->ioreqlist;
	       while (tmpioreq->next) {
		  tmpioreq = tmpioreq->next;
	       }
	       curr_value = ((currdiskreq->seg->endblkno >= (tmpioreq->blkno + tmpioreq->bcount)) ? 6 : 5);
	    } else if (currdiskreq->outblkno == currdiskreq->ioreqlist->blkno) {
               /* Not entirely sure this works or is appropriate */
	       currioreq = disk_request_needs_bus(currdisk,currdiskreq,FALSE);
	       if (currioreq) {
		  addtoextraq(currioreq);
	       } else {
if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f         sneakyintermediatereadhits removed diskreq from seg\n",simtime);
fprintf (outputfile, "                       seg = %d-%d\n", currdiskreq->seg->startblkno, currdiskreq->seg->endblkno);
fprintf (outputfile, "                       diskreq = %d, %d, %d (1==R)\n",currdiskreq->ioreqlist->blkno, currdiskreq->ioreqlist->bcount, (currdiskreq->ioreqlist->flags & READ));
fflush(outputfile);
}
	          disk_buffer_remove_from_seg(currdiskreq);
	       }
	    }
	 } 
	 if (!currdiskreq->seg && (best_value <= 4)) {
	    disk_buffer_select_segment(currdisk,currdiskreq,FALSE);
	    if (currdiskreq->seg) {
	       if (currdiskreq->hittype == BUFFER_NOMATCH) {
		  currdiskreq->seg = NULL;
	       } else {
	          if (currdisk->reqwater) {
		     currdiskreq->watermark = max(1, (int) ((double) min(currdiskreq->seg->size, currdiskreq->ioreqlist->bcount) * currdisk->readwater));
	          } else {
		     currdiskreq->watermark = (int) (((double) currdiskreq->seg->size * currdisk->readwater) + (double) 0.9999999999);
	          }
	          currioreq = disk_request_needs_bus(currdisk,currdiskreq,TRUE);
		  if (!currioreq) {
		     currdiskreq->seg = NULL;
		     currdiskreq->hittype = BUFFER_NOMATCH;
		  } else {
		     curr_value = ((currdiskreq->hittype == BUFFER_WHOLE) ? 4 : 3);
	             curr_set_segment = TRUE;
		  }
	       }
	    }
	 }
      } else if ((best_value <= 2) &&
                 (currdisk->writeprebuffering) &&
		 !(currdiskreq->ioreqlist->flags & READ)) {
         if (currdiskreq->seg) {
	    currioreq = disk_request_needs_bus(currdisk,currdiskreq,TRUE);
	    if (currioreq) {
	       tmpioreq = currdiskreq->ioreqlist;
	       while (tmpioreq->next) {
		  tmpioreq = tmpioreq->next;
	       }
	       if (currdisk->effectivehda && 
		   (currdisk->effectivehda->seg == currdiskreq->seg)) {
	          curr_value = 2;
	       } else if (currdisk->currenthda && 
		          (currdisk->currenthda->seg == currdiskreq->seg)) {
	          curr_value = 1;
	       } else {
	          curr_value = 0;
               }
            }
	 } else if (best_value <= -1) {
	    disk_buffer_select_segment(currdisk,currdiskreq,FALSE);
	    if (currdiskreq->seg) {
 	       if (currdisk->reqwater) {
                  currdiskreq->watermark = max(1, (int) ((double) min(currdiskreq->seg->size, currdiskreq->ioreqlist->bcount) * currdisk->writewater));
               } else {
                  currdiskreq->watermark = (int) (((double) currdiskreq->seg->size * currdisk->writewater) + (double) 0.9999999999);
               }
	       currioreq = disk_request_needs_bus(currdisk,currdiskreq,TRUE);
	       if (currioreq) {
	          curr_value = -1;
	          curr_set_segment = TRUE;
	       } else {
		  currdiskreq->seg = NULL;
		  currdiskreq->hittype = BUFFER_NOMATCH;
	       }
	    }
	 }
      }

      if (curr_value >= best_value) {
	 if (*busioreq) {
	    addtoextraq((event *) *busioreq);
	    if (best_set_segment) {
	       bestdiskreq->seg = NULL;
	       bestdiskreq->hittype = BUFFER_NOMATCH;
	    }
	 }
	 best_value = curr_value;
	 best_set_segment = curr_set_segment;
	 bestdiskreq = currdiskreq;
	 *busioreq = currioreq;
      } else if (curr_set_segment) {
	 currdiskreq->seg = NULL;
	 currdiskreq->hittype = BUFFER_NOMATCH;
      }

      currdiskreq = currdiskreq->bus_next;
   }

   if (best_set_segment) {
      disk_buffer_set_segment(currdisk,bestdiskreq);
   }

   return(bestdiskreq);
}


/* Attempt to start bus activity.  If currdiskreq doesn't match a non-NULL
   effectivebus, do nothing.  Otherwise, if currdiskreq doesn't match a 
   non-NULL currentbus, do nothing.  Otherwise, if there is already a 
   request outstanding (buswait), do nothing.  Otherwise, check to see if
   currdiskreq needs some bus activity.  If currdiskreq is NULL, attempt to 
   find the appropriate next request from the queue.

   Queue priority list:
      effectivebus (if non-NULL, this is the only choice possible)
      currentbus   (if non-NULL, this is the only choice possible)
      see disk_select_bus_request
*/

void disk_check_bus(currdisk,currdiskreq)
disk    *currdisk;
diskreq *currdiskreq;
{
   diskreq     *nextdiskreq = currdiskreq;
   diskreq     *tmpdiskreq;
   ioreq_event *busioreq = NULL;
   double	delay;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f  %8p  Entering disk_check_bus for disk %d\n", simtime, currdiskreq,currdisk->devno);
fflush(outputfile);
}

   if (currdisk->buswait) {
      return;
   }

   /* add checking here for acctime >= 0.0 */

   if (currdiskreq) {
      if (currdisk->effectivebus) {
	 if (currdisk->effectivebus != currdiskreq) {
	    nextdiskreq = NULL;
         }
      } else if (currdisk->currentbus) {
	 if (currdisk->currentbus != currdiskreq) {
	    nextdiskreq = NULL;
	 }
      }
      if (nextdiskreq) {

/*  Only truly affected call is the one from disk_check_hda.  It needs
    to be TRUE in order to match disk_completion's call.  Otherwise,
    preseeking is disadvantaged...
*/
         busioreq = disk_request_needs_bus(currdisk,nextdiskreq,TRUE);
         if (!busioreq) {
	    nextdiskreq = NULL;
         }
      }
   } else {
      if (currdisk->effectivebus) {
         busioreq = disk_request_needs_bus(currdisk,currdisk->effectivebus,TRUE);
         if (busioreq) {
	    nextdiskreq = currdisk->effectivebus;
         }
      } else if (currdisk->currentbus) {
         busioreq = disk_request_needs_bus(currdisk,currdisk->currentbus,TRUE);
         if (busioreq) {
	    nextdiskreq = currdisk->currentbus;
         }
      } else {
	 nextdiskreq = disk_select_bus_request(currdisk,&busioreq);
      }
   }

   if (nextdiskreq) {

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "                        nextdiskreq = %8p\n", nextdiskreq);
fflush(outputfile);
}

      if ((nextdiskreq != currdisk->currentbus) &&
	  (nextdiskreq != currdisk->effectivebus)) {
	 /* remove nextdiskreq from bus queue */
         if (currdisk->pendxfer == nextdiskreq) {
	    currdisk->pendxfer = nextdiskreq->bus_next;
	 } else {
	    tmpdiskreq = currdisk->pendxfer;
	    while (tmpdiskreq && (tmpdiskreq->bus_next != nextdiskreq)) {
	       tmpdiskreq = tmpdiskreq->bus_next;
	    }
	    if (!tmpdiskreq) {
               fprintf(stderr, "Next bus request not found in bus queue in disk_check_bus\n");
               exit(0);
	    } else {
	       tmpdiskreq->bus_next = nextdiskreq->bus_next;
	    }
	 }
	 nextdiskreq->bus_next = NULL;
      }
      currdisk->currentbus = currdisk->effectivebus = nextdiskreq;
      if (busioreq->cause == RECONNECT) {
         if (nextdiskreq->ioreqlist->flags & READ) {
	    disk_activate_read(currdisk, nextdiskreq, FALSE, FALSE);
	 } else {
	    disk_activate_write(currdisk, nextdiskreq, FALSE, FALSE);
	 }
         if (currdisk->extradisc_diskreq == nextdiskreq) {
            currdisk->extradisc_diskreq = NULL;
	    delay = currdisk->overhead_reselect_first;
         } else {
            delay = ((nextdiskreq->flags & EXTRA_WRITE_DISCONNECT) ? currdisk->overhead_reselect_other : currdisk->overhead_reselect_first);
	 }
	 if (busioreq->bcount > 0) {
            delay += currdisk->overhead_data_prep;
	 }
	 if (currdisk->acctime >= 0.0) {
	    delay = 0.0;
	 } else {
            nextdiskreq->seg->outstate = BUFFER_CONTACTING;
	 }

         currdisk->outstate = DISK_WAIT_FOR_CONTROLLER;
         disk_send_event_up_path(busioreq, (delay * currdisk->timescale));

      } else if (busioreq->cause == DISK_DATA_TRANSFER_COMPLETE) {
	 disk_reconnection_or_transfer_complete(busioreq);
      } else {
         fprintf(stderr, "unexpected busioreq->cause in disk_check_bus\n");
         exit(0);
      }
   }
}


void disk_get_remapped_sector(currdisk, curr)
disk *currdisk;
ioreq_event *curr;
{
   double acctime;
   segment *seg;
   diskreq *currdiskreq = currdisk->effectivehda;

   if (!currdiskreq) {
      fprintf(stderr, "No effectivehda in disk_get_remapped_sector\n");
      exit(0);
   }
   seg = currdiskreq->seg;
   curr->tempptr1 = disk_translate_lbn_to_pbn(currdisk, curr->blkno, MAP_FULL, &currcylno, &currsurface, &curr->cause);
   acctime = diskacctime(currdisk, curr->tempptr1, DISKACCTIME, (curr->flags & READ), simtime, currcylno, currsurface, curr->cause, 1, 0);
   curr->time = simtime + acctime;
   if (currdisk->stat.latency == (double) -1.0) {
      currdisk->stat.latency = (double) 0.0;
      currdisk->stat.xfertime = acctime - currdisk->stat.seektime;
   } else {
      currdisk->stat.xfertime += acctime;
   }
   if (((currdiskreq != seg->recyclereq) && disk_buffer_block_available(currdisk, seg, curr->blkno)) || ((curr->flags & READ) && (!currdisk->read_direct_to_buffer))) {
      curr->type = DISK_GOT_REMAPPED_SECTOR;
   } else {
      curr->time -= ((double) 1 / (double) ((band *) curr->tempptr1)->blkspertrack) * currdisk->rotatetime;
      curr->type = DISK_GOTO_REMAPPED_SECTOR;
   }
}


void disk_goto_remapped_sector(currdisk, curr)
disk *currdisk;
ioreq_event *curr;
{
   diskreq *currdiskreq = currdisk->effectivehda;
   segment *seg;

   if (!currdiskreq) {
      fprintf(stderr, "No effectivehda in disk_goto_remapped_sector\n");
      exit(0);
   }
   seg = currdiskreq->seg;
   if (seg->outstate == BUFFER_PREEMPT) {
      disk_release_hda(currdisk, currdiskreq);
      return;
   }
   if ((currdiskreq != seg->recyclereq) && disk_buffer_block_available(currdisk, seg, curr->blkno)) {
      curr->time += ((double) 1 / (double) ((band *) curr->tempptr1)->blkspertrack) * currdisk->rotatetime;
      curr->type = DISK_GOT_REMAPPED_SECTOR;
   } else {
      seg->time += currdisk->rotatetime;
      currdisk->stat.xfertime += currdisk->rotatetime;
      curr->time += currdisk->rotatetime;
      curr->type = DISK_GOTO_REMAPPED_SECTOR;
   }
   addtointq((event *) curr);
}


int disk_initiate_seek(currdisk, seg, curr, firstseek, delay, mintime)
disk *currdisk;
segment *seg;
ioreq_event *curr;
int firstseek;
double delay;
double mintime;
{
   double seektime;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f            Entering disk_initiate_seek to %d for disk %d\n", simtime, curr->blkno, curr->devno);
fflush(outputfile);
}

   curr->type = NULL_EVENT;
   seg->time = simtime + delay;
   remapsector = FALSE;
   curr->tempptr1 = disk_translate_lbn_to_pbn(currdisk, curr->blkno, MAP_FULL, &currcylno, &currsurface, &curr->cause);
   seektime = diskacctime(currdisk, curr->tempptr1, DISKSEEKTIME, (curr->flags & READ), seg->time, currcylno, currsurface, curr->cause, curr->bcount, 0);
   if (seektime < mintime) {
      seg->time += mintime - seektime;
   }
   curr->time = seg->time + seektime;
   if ((curr->flags & BUFFER_BACKGROUND) && (curr->flags & READ)) {
      if ((currdisk->contread == BUFFER_READ_UNTIL_CYL_END) && (seekdistance)) {
	 return(FALSE);
      }
      if ((currdisk->contread == BUFFER_READ_UNTIL_TRACK_END) && ((seekdistance) | (currdisk->currsurface != currsurface))) {
	 return(FALSE);
      }
   }
   if (firstseek) {
      currdisk->stat.seekdistance = seekdistance;
      currdisk->stat.seektime = seektime;
      currdisk->stat.latency = (double) -1.0;
      currdisk->stat.xfertime = (double) -1.0;
   } else if (!remapsector) {
      currdisk->stat.xfertime += seektime;
   }
   curr->type = DISK_BUFFER_SEEKDONE;
   if (remapsector) {
      disk_get_remapped_sector(currdisk, curr);
   }
   addtointq((event *) curr);
/*
fprintf (outputfile, "\nSeek from cyl %d head %d to cyl %d head %d, time %f\n", currdisk->currcylno, currdisk->currsurface, currcylno, currsurface, seektime);
*/
   return(TRUE);
}


/* Attempt to start hda activity.  If effectivehda already set, do nothing.
   If currdiskreq is NULL, attempt to find the appropriate next request from
   the queue.
*/

void disk_check_hda(currdisk,currdiskreq,ok_to_check_bus)
disk    *currdisk;
diskreq *currdiskreq;
int      ok_to_check_bus;
{
   diskreq     *nextdiskreq = currdiskreq;
   ioreq_event *nextioreq;
   segment     *seg;
   int          initiate_seek = FALSE;
   int          immediate_release = FALSE;
   double	delay = 0.0;
   double	mintime = 0.0;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f  %8p  Entering disk_check_hda for disk %d\n", simtime, currdiskreq,currdisk->devno);
fflush(outputfile);
}

   if (currdisk->acctime >= 0.0) {
      return;
   }

   if (!currdisk->effectivehda) {
      if (currdisk->currenthda) {
	 if (currdiskreq && (currdiskreq != currdisk->currenthda)) {
            fprintf(stderr, "currdiskreq != currenthda in disk_check_hda\n");
            exit(0);
	 }
	 currdisk->effectivehda = currdisk->currenthda;
      } else {
	 if (nextdiskreq && nextdiskreq->ioreqlist) {
            nextioreq = ioreq_copy(nextdiskreq->ioreqlist);
	    nextioreq->ioreq_hold_diskreq = nextdiskreq;
	    nextioreq->ioreq_hold_disk = currdisk;
	    if (!disk_enablement_function(nextioreq)) {
	       nextdiskreq = NULL;
	    }
	    addtoextraq(nextioreq);
	 }
         if (!nextdiskreq) {
	    nextioreq = ioqueue_show_next_request(currdisk->queue,disk_enablement_function);
	    if (nextioreq) {
	       nextdiskreq = (diskreq*) nextioreq->ioreq_hold_diskreq;
	    }
         }
         if (nextdiskreq) {
	    if (nextdiskreq->flags & HDA_OWNED) {
               fprintf(stderr, "disk_check_hda:  diskreq was already HDA_OWNED\n");
               exit(0);
	    }
	    if (nextdiskreq->seg && !(nextdiskreq->flags & SEG_OWNED) &&
		(nextdiskreq->seg->recyclereq != nextdiskreq)) {
	       disk_buffer_attempt_seg_ownership(currdisk, nextdiskreq);
	    }
            if (!nextdiskreq->seg) {
	       disk_buffer_select_segment(currdisk,nextdiskreq,FALSE);
               if (nextdiskreq->seg) {
		  disk_buffer_set_segment(currdisk, nextdiskreq);
		  disk_buffer_attempt_seg_ownership(currdisk, nextdiskreq);
	       } else {
	          nextdiskreq->seg = disk_buffer_recyclable_segment(currdisk,(nextdiskreq->ioreqlist->flags & READ));
	          if (nextdiskreq->seg) {
		     nextdiskreq->seg->recyclereq = nextdiskreq;
	          }
               }
            }
	    if (nextdiskreq->seg) {
	       nextioreq = nextdiskreq->ioreqlist;
	       while (nextioreq) {
		  if (!ioqueue_get_specific_request(currdisk->queue,nextioreq)) {
                     fprintf(stderr, "disk_check_hda:  ioreq_event not found by ioqueue_get_specific_request call\n");
                     exit(0);
		  }
	          nextioreq = nextioreq->next;
	       }
	       currdisk->currenthda = currdisk->effectivehda = nextdiskreq;
	       nextdiskreq->flags |= HDA_OWNED;
	    }

         } /* if diskreq found */

      } /* if currenthda else */

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "                        effectivehda = %8p\n", currdisk->effectivehda);
fflush(outputfile);
}

      if (currdisk->effectivehda) {
	 nextdiskreq = currdisk->effectivehda;
         seg = nextdiskreq->seg;
	 if (nextdiskreq->ioreqlist) {
            nextioreq = nextdiskreq->ioreqlist;
            while (nextioreq->next) {
               nextioreq = nextioreq->next;
            }
            seg->access->flags = nextioreq->flags;
	    if (nextioreq->flags & READ) {
	       if (seg->access->type != NULL_EVENT) {
                  /*
		  if ((seg->access->blkno != seg->endblkno) &&
		      (seg->access->blkno != (seg->endblkno + 1)) &&
		      (seg->access->blkno != currdisk->firstblkontrack)) {
                     fprintf(stderr, "non-NULL seg->access->type with incorrect seg->access->blkno found for read segment in disk_check_hda\n");
	             exit(0);
                  }
                  */
	       } else {
	          seg->access->blkno = seg->endblkno;
	       }
	       if (!seg->recyclereq) {
	          seg->minreadaheadblkno = max(seg->minreadaheadblkno, min((nextioreq->blkno + nextioreq->bcount + currdisk->minreadahead), currdisk->numblocks));
	          seg->maxreadaheadblkno = max(seg->maxreadaheadblkno, min(disk_buffer_get_max_readahead(currdisk,seg,nextioreq), currdisk->numblocks));
	       }
	       currdisk->immed = currdisk->immedread;
	       seg->state = BUFFER_READING;
	       if (seg->recyclereq) {
		  seg->access->blkno = nextdiskreq->outblkno;
		  initiate_seek = TRUE;
	       } else if ((seg->endblkno < (nextioreq->blkno + nextioreq->bcount)) ||
		          (seg->startblkno > nextdiskreq->outblkno)) {
	          initiate_seek = TRUE;
               } else if (seg->endblkno < max(seg->maxreadaheadblkno, disk_buffer_get_max_readahead(currdisk, seg, nextioreq))) {
                  seg->access->flags |= BUFFER_BACKGROUND;
	          if (currdisk->readaheadifidle) {
		     /* Check for type != NULL_EVENT below negates this */
	             initiate_seek = TRUE;
		  }
	       } else {
		  immediate_release = TRUE;
	       }
               if (seg->access->type != NULL_EVENT) {
		  if (immediate_release) {
		     fprintf(stderr,"CHECK:  immediate_release reset!\n");
		     fflush(stderr);
		  }
		  initiate_seek = FALSE;
		  immediate_release = FALSE;
	       }
	    } else { 			/* WRITE */
	       if (seg->access->type != NULL_EVENT) {
                  fprintf(stderr, "non-NULL seg->access->type found for write segment in disk_check_hda\n");
	          exit(0);
	       }
	       /* prepare minimum seek time for seek if not sequential writes */
               if ((nextdiskreq->hittype != BUFFER_APPEND) ||
		   (seg->access->flags & READ) ||
		   (seg->access->blkno != nextdiskreq->ioreqlist->blkno) ||
		   (seg->access->bcount != 1) ||
		   (seg->access->time != simtime)) {
                  mintime = currdisk->minimum_seek_delay;
	       } 
/*
	       else {
		  fprintf(stderr,"Sequential write stream detected at %f\n", simtime);
	       }
*/
	       seg->access->blkno = nextdiskreq->inblkno;
	       if (nextdiskreq->flags & COMPLETION_RECEIVED) {
	          seg->access->flags |= BUFFER_BACKGROUND;
	       }
	       currdisk->immed = currdisk->immedwrite;
	       if (seg->state != BUFFER_WRITING && seg->state != BUFFER_DIRTY) {
	          currdisk->numdirty++;
if ((disk_printhack > 1) && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "                        numdirty++ = %d\n",currdisk->numdirty);
fflush(outputfile);
}
		  ASSERT1(((currdisk->numdirty >= 0) && (currdisk->numdirty <= currdisk->numwritesegs)),"numdirty",currdisk->numdirty);
	       }
	       seg->state = BUFFER_WRITING;
               if (nextdiskreq->inblkno < (nextioreq->blkno + nextioreq->bcount)) {
	          initiate_seek = TRUE;
	       } else {
		  immediate_release = TRUE;
	       }
	    }
	    if (ok_to_check_bus && 
		(currdisk->extradisc_diskreq != nextdiskreq)) {
	       disk_check_bus(currdisk,nextdiskreq);
	    }
         } else {		/* Pure prefetch (NULL ioreqlist) */
	    seg->state = BUFFER_READING;
	    if (seg->endblkno < seg->maxreadaheadblkno) {
	       if (seg->access->type != NULL_EVENT) {
                  /* 
		  if ((seg->access->blkno != seg->endblkno) &&
		      (seg->access->blkno != (seg->endblkno + 1)) &&
		      (seg->access->blkno != currdisk->firstblkontrack)) {
                     fprintf(stderr, "non-NULL seg->access->type with incorrect seg->access->blkno found for pure read segment in disk_check_hda\n");
	             exit(0);
                  }
                  */
	       } else {
	          seg->access->blkno = seg->endblkno;
	       }
               seg->access->flags = READ | BUFFER_BACKGROUND;
	       currdisk->immed = currdisk->immedread;
	       if (seg->access->type == NULL_EVENT) {
	          initiate_seek = TRUE;
	       }
	    } else {
	       immediate_release = TRUE;
	    }
	 }

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "                        initiate_seek = %d, immediate_release = %d\n", initiate_seek, immediate_release);
fflush(outputfile);
}

	 if (initiate_seek) {
	    if (nextdiskreq->overhead_done > simtime) {
	       delay += nextdiskreq->overhead_done - simtime;
	    }
	    if (disk_seek_stoptime > simtime) {
	       delay += disk_seek_stoptime - simtime;
	    }
            if (!(disk_initiate_seek(currdisk, seg, seg->access, TRUE, delay, mintime ))) {
	       disk_release_hda(currdisk, nextdiskreq);
            }
	 } else if (immediate_release) {
	    disk_release_hda(currdisk, nextdiskreq);
	 }
         if (!immediate_release && (seg->state == BUFFER_READING) && 
	     (seg->access->flags & BUFFER_BACKGROUND)) {
            disk_check_prefetch_swap(currdisk);
	 }
      }
   }
}


/* prematurely stop a prefetch */

void disk_buffer_stop_access(currdisk)
disk *currdisk;
{
   diskreq* effective = currdisk->effectivehda;
   segment* seg;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f  %8p  Entering disk_buffer_stop_access\n", simtime, effective);
fflush(outputfile);
}

   if (!effective) {
      fprintf(stderr, "disk_buffer_stop_access called for disk with NULL effectivehda\n");
      exit(0);
   }
   if (effective != currdisk->currenthda) {
      fprintf(stderr, "disk_buffer_stop_access called for disk with effectivehda != currenthda\n");
      exit(0);
   }

   seg = effective->seg;

   ASSERT((seg->access->flags & BUFFER_BACKGROUND) &&
          (seg->access->flags & READ));

   if (seg->access->type != NULL_EVENT) {
      if (removefromintq(seg->access) == FALSE) {
         fprintf(stderr, "Non-null seg->access does not appear on the intq in disk_buffer_stop_access: %d\n", seg->access->type);
         exit(0);
      }
      seg->access->type = NULL_EVENT;
   }

   seg->state = ((!(effective->flags & COMPLETION_RECEIVED) || currdisk->enablecache || seg->diskreqlist->seg_next) ? BUFFER_CLEAN : BUFFER_EMPTY);

   if (effective->flags & COMPLETION_RECEIVED) {
      if (effective->ioreqlist) {
	 fprintf(stderr,"Pure prefetch with non-NULL ioreqlist detected in disk_buffer_stop_access\n");
         exit(0);
      }
      disk_buffer_remove_from_seg(effective);
      addtoextraq((event *) effective);
      if (seg->diskreqlist) {
         disk_find_new_seg_owner(currdisk,seg);
      }
   }

   currdisk->effectivehda = currdisk->currenthda = NULL;
}


/* attempt to take over control of the hda from an ongoing prefetch */

int disk_buffer_attempt_access_swap(currdisk,currdiskreq)
disk *currdisk;
diskreq *currdiskreq;
{
   diskreq     *effective = currdisk->effectivehda;
   ioreq_event *currioreq;
   segment     *seg;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f  %8p  Entering disk_buffer_attempt_access_swap\n", simtime, currdiskreq);
fprintf (outputfile, "                        trying to swap %8p\n", effective);
fflush(outputfile);
}

   if (!effective) {
      fprintf(stderr, "disk_buffer_attempt_access_swap called for disk with NULL effectivehda\n");
      exit(0);
   }

   seg = effective->seg;

   if ( !(seg->access->flags & READ)) {
      fprintf(stderr, "disk_buffer_attempt_access_swap called for write access\n");
      exit(0);
   }

   ASSERT(currdiskreq->ioreqlist != NULL);

   /* Since "free" reads are not allowed to prefetch currently, we don't
      really need the first clause.  But, better safe than sorry...
   */

   if ((effective == currdisk->currenthda) &&
       (seg->access->flags & BUFFER_BACKGROUND) &&
       (!swap_forward_only || !effective->ioreqlist ||
	(effective->ioreqlist->blkno < currdiskreq->ioreqlist->blkno))) {
      if (effective->flags & COMPLETION_RECEIVED) {
	 ASSERT(effective->ioreqlist == NULL);
         disk_buffer_remove_from_seg(effective);
         addtoextraq((event *) effective);
      } else {
	 ASSERT(effective->ioreqlist != NULL);
         currioreq = currdiskreq->ioreqlist;
	 ASSERT(currioreq != NULL);
	 while (currioreq->next) {
	    currioreq = currioreq->next;
	 }
         seg->access->flags = currioreq->flags;

         seg->minreadaheadblkno = max(seg->minreadaheadblkno, min((currioreq->blkno + currioreq->bcount + currdisk->minreadahead), currdisk->numblocks));
         seg->maxreadaheadblkno = max(seg->maxreadaheadblkno, min(disk_buffer_get_max_readahead(currdisk,seg,currioreq), currdisk->numblocks));

         if ((seg->endblkno >= (currioreq->blkno + currioreq->bcount)) &&
             (seg->endblkno < seg->maxreadaheadblkno)) {
            seg->access->flags |= BUFFER_BACKGROUND;
         }
      }
      currdisk->effectivehda = currdisk->currenthda = NULL;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "                        Swap successful\n");
fflush(outputfile);
}

      return(TRUE);
   }

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "                        Swap unsuccessful\n");
fflush(outputfile);
}

   return(FALSE);
}


/* setseg indicates that currdiskreq->seg has not been permanently "set", and 
   should be nullified if the hda cannot be obtained or set if it is obtained
*/

void disk_activate_read(currdisk,currdiskreq,setseg,ok_to_check_bus)
disk* currdisk;
diskreq* currdiskreq;
int setseg;
int ok_to_check_bus;
{
   ioreq_event *currioreq;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f  %8p  Entering disk_activate_read\n", simtime, currdiskreq);
fprintf (outputfile, "                        setseg = %d\n", setseg);
fflush(outputfile);
}

   if (currdisk->acctime >= 0.0) {
      if (!currdisk->currenthda) {
         currdisk->currenthda = 
   	   currdisk->effectivehda = 
   	   currdisk->currentbus = currdiskreq;
         currdiskreq->overhead_done = simtime + currdisk->acctime;
         currioreq = ioreq_copy(currdiskreq->ioreqlist);
         currioreq->ioreq_hold_diskreq = currdiskreq;
         currioreq->type = DISK_PREPARE_FOR_DATA_TRANSFER;
         currioreq->time = currdiskreq->overhead_done;
         addtointq((event *) currioreq);
         ioqueue_set_starttime(currdisk->queue,currdiskreq->ioreqlist);
      }
      return;
   }

   if (!(currdiskreq->flags & HDA_OWNED) &&
       currdisk->effectivehda && 
       (currdisk->effectivehda != currdiskreq) &&
       (currdiskreq->hittype != BUFFER_COLLISION) &&
       (!currdiskreq->seg || 
	(currdiskreq->seg->recyclereq == currdiskreq) || 
	(currdiskreq->seg->state != BUFFER_DIRTY)) &&
       disk_buffer_stopable_access(currdisk,currdiskreq)) {
      if ((currdiskreq->seg != currdisk->effectivehda->seg) || 
	  (currdiskreq->hittype == BUFFER_NOMATCH)) {
         disk_buffer_stop_access(currdisk);
      } else {
	 disk_buffer_attempt_access_swap(currdisk,currdiskreq);
      }
   }

   if (currdiskreq->seg && setseg) {
      if (currdisk->effectivehda) {
	 if (currdiskreq->seg->recyclereq == currdiskreq) {
            /* I don't think the following code ever gets used... */
            fprintf(stderr,"GOT HERE!  SURPRISE, SURPRISE!\n");
	    currdiskreq->seg->recyclereq = NULL;
	 }
	 currdiskreq->seg = NULL;
	 currdiskreq->hittype = BUFFER_NOMATCH;
      } else {
	 disk_buffer_set_segment(currdisk,currdiskreq);
      }
   }

   if (currdiskreq->seg && !(currdiskreq->flags & SEG_OWNED)) {
      disk_buffer_attempt_seg_ownership(currdisk,currdiskreq);
   }

   if (!(currdiskreq->flags & HDA_OWNED) &&
       (currdiskreq->hittype != BUFFER_COLLISION)) {
      disk_check_hda(currdisk,currdiskreq,ok_to_check_bus);
   }

   if (!(currdiskreq->flags & HDA_OWNED) &&
       (currdisk->currentbus == currdiskreq)) {
      currioreq = currdiskreq->ioreqlist;
      while (currioreq) {
         ioqueue_set_starttime(currdisk->queue,currioreq);
         currioreq = currioreq->next;
      }
   }

}


/* setseg indicates that currdiskreq->seg has not been permanently "set", and 
   should be nullified if the hda cannot be obtained or set if it is obtained
*/

void disk_activate_write(currdisk,currdiskreq,setseg,ok_to_check_bus)
disk* currdisk;
diskreq* currdiskreq;
int setseg;
int ok_to_check_bus;
{
   ioreq_event *currioreq;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f  %8p  Entering disk_activate_write\n", simtime, currdiskreq);
fprintf (outputfile, "                        setseg = %d\n", setseg);
fflush(outputfile);
}

   if (!currdisk->currenthda && (currdisk->acctime >= 0.0)) {
      currdisk->currenthda = 
	currdisk->effectivehda = 
	currdisk->currentbus = currdiskreq;
      currdiskreq->overhead_done = simtime + currdisk->acctime;
      currioreq = ioreq_copy(currdiskreq->ioreqlist);
      currioreq->ioreq_hold_diskreq = currdiskreq;
      currioreq->type = DISK_PREPARE_FOR_DATA_TRANSFER;
      currioreq->time = currdiskreq->overhead_done;
      addtointq((event *) currioreq);
      ioqueue_set_starttime(currdisk->queue,currdiskreq->ioreqlist);
      return;
   }

   if (!(currdiskreq->flags & HDA_OWNED) &&
       currdisk->effectivehda && 
       (currdisk->effectivehda != currdiskreq) &&
       disk_buffer_stopable_access(currdisk,currdiskreq)) {
      if ((currdiskreq->seg != currdisk->effectivehda->seg) || 
	  (currdiskreq->hittype != BUFFER_APPEND)) {
         disk_buffer_stop_access(currdisk);
      }
   }

   if (currdiskreq->seg && setseg) {
      if (currdisk->effectivehda) {
	 if (currdiskreq->seg->recyclereq == currdiskreq) {
            /* I don't think the following code ever gets used... */
            fprintf(stderr,"GOT HERE!  SURPRISE, SURPRISE, SURPRISE!\n");
	    currdiskreq->seg->recyclereq = NULL;
	 }
	 currdiskreq->seg = NULL;
	 currdiskreq->hittype = BUFFER_NOMATCH;
      } else {
	 disk_buffer_set_segment(currdisk,currdiskreq);
      }
   }

   if (currdiskreq->seg && !(currdiskreq->flags & SEG_OWNED)) {
      disk_buffer_attempt_seg_ownership(currdisk,currdiskreq);
   }

   if (!(currdiskreq->flags & HDA_OWNED) &&
       (currdiskreq->hittype != BUFFER_COLLISION)) {
      disk_check_hda(currdisk,currdiskreq,ok_to_check_bus);
   }

   if (!(currdiskreq->flags & HDA_OWNED) &&
       (currdisk->currentbus == currdiskreq)) {
      currioreq = currdiskreq->ioreqlist;
      while (currioreq) {
         ioqueue_set_starttime(currdisk->queue,currioreq);
         currioreq = currioreq->next;
      }
   }
}


void disk_request_arrive(curr)
ioreq_event *curr;
{
   ioreq_event *intrp;
   disk *currdisk;
   int flags;
   double delay;
   diskreq *new_diskreq;
   segment *seg;

   if (curr->flags & READ) {
      disk_last_read_arrival[(curr->devno)] = simtime;
   } else {
      disk_last_write_arrival[(curr->devno)] = simtime;
   }
   flags = curr->flags;
   currdisk = &disks[(curr->devno)];

   if ((curr->blkno + curr->bcount -1) > currdisk->stat.highblkno) {
      currdisk->stat.highblkno = curr->blkno + curr->bcount - 1;
   }
   if ((curr->blkno < 0) || (curr->bcount <= 0) || ((curr->blkno + curr->bcount) > currdisk->numblocks)) {
      fprintf(stderr, "Invalid set of blocks requested from disk - blkno %d, bcount %d, numblocks %d\n", curr->blkno, curr->bcount, currdisk->numblocks);
      exit(0);
   }
   currdisk->busowned = disk_get_busno(curr);
   intrp = ioreq_copy(curr);
   intrp->type = IO_INTERRUPT_ARRIVE;
   currdisk->outstate = DISK_WAIT_FOR_CONTROLLER;

   new_diskreq = (diskreq *) getfromextraq();
   new_diskreq->flags = 0;
   new_diskreq->ioreqlist = curr;
   new_diskreq->seg_next = NULL;
   new_diskreq->bus_next = NULL;
   new_diskreq->outblkno = new_diskreq->inblkno = curr->blkno;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f  %8p  Entering disk_request_arrive\n", simtime, new_diskreq);
fprintf (outputfile, "                        disk = %d, blkno = %d, bcount = %d, read = %d\n",curr->devno, curr->blkno, curr->bcount, (READ & curr->flags));
fflush(outputfile);
}

   currdisk->effectivebus = new_diskreq;

   curr->ioreq_hold_disk = currdisk;
   curr->ioreq_hold_diskreq = new_diskreq;

   /* The request must be placed in the queue before selecting a 
      segment, in order for disk_buffer_stopable_access to correctly 
      detect LIMITED_FASTWRITE possibilities.
   */

   ioqueue_add_new_request(currdisk->queue, curr);

   seg = disk_buffer_select_segment(currdisk, new_diskreq,TRUE);

   /* note:  new_diskreq->hittype and ->seg both ALWAYS set in select_seg
             (even if seg == NULL, hittype should be set to BUFFER_NOMATCH)  
   */

   if (flags & READ) {
      int enough_ready_to_go = TRUE;

      /* set up overhead_done to delay any prefetch or completion
         read activity.  readmiss overheads are used to be
         compatible with the old simulator.
      */

      new_diskreq->overhead_done = ((currdisk->lastflags & READ) ? currdisk->overhead_command_readmiss_afterread : currdisk->overhead_command_readmiss_afterwrite);
      new_diskreq->overhead_done *= currdisk->timescale;
      new_diskreq->overhead_done += simtime;

      switch (new_diskreq->hittype) {
         case BUFFER_PARTIAL:
	    if (!currdisk->immedtrans_any_readhit) {
    	       int readwater;

	       if (currdisk->reqwater) {
	          readwater = max(1, (int) ((double) min(seg->size,curr->bcount) * currdisk->readwater));
               } else {
                  readwater = (int) ((((double) seg->size * currdisk->readwater)) + (double) 0.9999999999);
               }
	       if ((seg->endblkno <= curr->blkno) || ((seg->endblkno - curr->blkno) < min(readwater,curr->bcount))) {
                  enough_ready_to_go = FALSE;
               }
	    }
	    /* continues downward */

         case BUFFER_COLLISION:
         case BUFFER_NOMATCH:
	    if (new_diskreq->hittype != BUFFER_PARTIAL) {
	       enough_ready_to_go = FALSE;
            }
	    /* continues downward */

	 case BUFFER_WHOLE:
	    /* if never_disconnect or
	       (if enough_ready_to_go and
	       (if sneaky read hits allowed or 
	       no request is active or active request is preemptable)), 
	       perform appropriate read hit overhead and prepare to 
	       transfer the data.
	       Otherwise, perform the initial command overhead portion,
	       place the request on the hda and bus queues, and disconnect.
            */
	    if (currdisk->neverdisconnect || 
		(enough_ready_to_go && !currdisk->currentbus &&
		 (currdisk->acctime < 0.0) && 
	         ((currdisk->sneakyfullreadhits && (new_diskreq->hittype == BUFFER_WHOLE)) || 
	          (currdisk->sneakypartialreadhits && (new_diskreq->hittype == BUFFER_PARTIAL)) || 
	          (!currdisk->pendxfer && (!currdisk->effectivehda || disk_buffer_stopable_access(currdisk,new_diskreq)))))) {
               intrp->cause = READY_TO_TRANSFER;
	       currdisk->currentbus = new_diskreq;
	       if (seg) {
		  disk_buffer_set_segment(currdisk,new_diskreq);
		  seg->outstate = BUFFER_CONTACTING;
	       }
	       if (new_diskreq->hittype == BUFFER_NOMATCH) {
	          delay = ((currdisk->lastflags & READ) ? currdisk->overhead_command_readmiss_afterread : currdisk->overhead_command_readmiss_afterwrite);
	       } else {
	          delay = ((currdisk->lastflags & READ) ? 
	                   currdisk->overhead_command_readhit_afterread : 
		           currdisk->overhead_command_readhit_afterwrite);
	       }
               delay += currdisk->overhead_data_prep;
	       if (currdisk->acctime >= 0.0) {
		  delay = 0.0;
	       }
               disk_send_event_up_path(intrp, (delay * currdisk->timescale));
	       disk_activate_read(currdisk, new_diskreq, FALSE, TRUE);
            } else {
               intrp->cause = DISCONNECT;
	       delay = ((currdisk->lastflags & READ) ? currdisk->overhead_command_readmiss_afterread : currdisk->overhead_command_readmiss_afterwrite);
               delay += ((currdisk->lastflags & READ) ? currdisk->overhead_disconnect_read_afterread : currdisk->overhead_disconnect_read_afterwrite);
	       if (currdisk->acctime >= 0.0) {
		  delay = 0.0;
	       }
               disk_send_event_up_path(intrp, (delay * currdisk->timescale));
	       disk_activate_read(currdisk, new_diskreq, TRUE, TRUE);
            }
            break;

         default:
            fprintf(stderr, "Invalid read hittype in disk_request_arrive - blkno %d, bcount %d, hittype %d\n", curr->blkno, curr->bcount, new_diskreq->hittype);
            exit(0);
      }
   } else { 				/* WRITE */

      /* set up overhead_done to delay any mechanical activity */
      new_diskreq->overhead_done = ((currdisk->lastflags & READ) ? currdisk->overhead_command_write_afterread : currdisk->overhead_command_write_afterwrite);
      new_diskreq->overhead_done *= currdisk->timescale;
      new_diskreq->overhead_done += simtime;

      /* LIMITED_FASTWRITE checking is done for "free", as there will 
	 be no currentbus or pendxfer and the effectivehda access will 
	 be "stopable" in appropriate cases.
      */

      if (currdisk->neverdisconnect || 
	  (seg && !currdisk->currentbus && (currdisk->acctime < 0.0) &&
	   (currdisk->writeprebuffering || 
	    (!currdisk->pendxfer && (!currdisk->effectivehda || disk_buffer_stopable_access(currdisk,new_diskreq)))))) {
	 if ((new_diskreq->flags & EXTRA_WRITE_DISCONNECT) &&
	     (currdisk->acctime < 0.0)) {
            /* re-set up overhead_done to delay any mechanical activity */
            new_diskreq->overhead_done = currdisk->extradisc_command + currdisk->extradisc_seekdelta;
            new_diskreq->overhead_done *= currdisk->timescale;
            new_diskreq->overhead_done += simtime;
	    currdisk->extradisc_diskreq = new_diskreq;
	    intrp->cause = DISCONNECT;
	    delay = currdisk->extradisc_command + currdisk->extradisc_disconnect1;
	    extra_write_disconnects++;
	    if (seg) {
	       seg->outstate = BUFFER_IDLE;
            }
	 } else {
            intrp->cause = READY_TO_TRANSFER;
	    delay = ((currdisk->lastflags & READ) ? 
	             currdisk->overhead_command_write_afterread : 
	             currdisk->overhead_command_write_afterwrite);
            delay += currdisk->overhead_data_prep;
	    if (seg) {
	       seg->outstate = BUFFER_CONTACTING;
            }
         }
	 currdisk->currentbus = new_diskreq;
	 if (seg) {
	    disk_buffer_set_segment(currdisk,new_diskreq);
	 }
	 if (currdisk->acctime >= 0.0) {
	    delay = 0.0;
	 }
         disk_send_event_up_path(intrp, (delay * currdisk->timescale));
	 disk_activate_write(currdisk, new_diskreq, FALSE, TRUE);
      } else {
	 new_diskreq->flags &= ~EXTRA_WRITE_DISCONNECT;
         intrp->cause = DISCONNECT;
         delay = ((currdisk->lastflags & READ) ? currdisk->overhead_command_write_afterread : currdisk->overhead_command_write_afterwrite);
         delay += currdisk->overhead_disconnect_write;
	 if (currdisk->acctime >= 0.0) {
	    delay = 0.0;
	 }
         disk_send_event_up_path(intrp, (delay * currdisk->timescale));
         disk_activate_write(currdisk, new_diskreq, TRUE, TRUE);
      }
   }
}


/* intermediate disconnect */
/* add acctime >= processing */

void disk_disconnect(curr)
ioreq_event *curr;
{
   disk        *currdisk = &disks[(curr->devno)];
   diskreq     *currdiskreq;
   ioreq_event *tmpioreq;

   currdiskreq = currdisk->effectivebus;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f  %8p  Entering disk_disconnect for disk %d\n", simtime, currdiskreq, currdisk->devno);
fflush(outputfile);
}

   tmpioreq = currdiskreq->ioreqlist;
   while (tmpioreq) {
      if (tmpioreq->blkno == curr->blkno) {
	 break;
      }
      tmpioreq = tmpioreq->next;
   }
   if (!tmpioreq) {
      fprintf(stderr, "ioreq_event not found in effectivebus in disk_disconnect\n");
      exit(0);
   }
   addtoextraq((event *) curr);

   currdisk->effectivebus = NULL;

   if (currdisk->busowned != -1) {
      bus_ownership_release(currdisk->busowned);
      currdisk->busowned = -1;
      currdisk->outstate = ((currdisk->buswait) ? DISK_WAIT_FOR_CONTROLLER : DISK_IDLE);
   }

   if (currdiskreq == currdisk->currentbus) {
      if ((currdiskreq->flags & EXTRA_WRITE_DISCONNECT) &&
	  (currdisk->extradisc_diskreq == currdiskreq)) {
         tmpioreq = ioreq_copy(tmpioreq);
	 tmpioreq->ioreq_hold_diskreq = currdiskreq;
         tmpioreq->type = DISK_PREPARE_FOR_DATA_TRANSFER;
         tmpioreq->time = simtime + currdisk->extradisc_inter_disconnect;
         addtointq((event *) tmpioreq);
      } else {
         currdisk->currentbus = NULL;
         currdiskreq->bus_next = currdisk->pendxfer;
         currdisk->pendxfer = currdiskreq;
         disk_check_bus(currdisk, NULL);
      }
   } else {
      currdiskreq->bus_next = currdisk->pendxfer;
      currdisk->pendxfer = currdiskreq;
      disk_check_bus(currdisk, NULL);
   }
}


/* completion disconnect */
/* add acctime >= 0.0 processing */

void disk_completion(curr)
ioreq_event *curr;
{
   disk        *currdisk = &disks[(curr->devno)];
   diskreq     *currdiskreq;
   ioreq_event *tmpioreq;
   segment     *seg;

   currdiskreq = currdisk->effectivebus;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f  %8p  Entering disk_completion for disk %d\n", simtime, currdiskreq,currdisk->devno);
fflush(outputfile);
}

   if (!currdiskreq) {
      fprintf(stderr, "effectivebus is NULL in disk_completion\n");
      exit(0);
   }
   if (!(currdiskreq->flags & COMPLETION_SENT)) {
      fprintf(stderr, "effectivebus isn't marked COMPLETION_SENT in disk_completion\n");
      exit(0);
   }

   seg = currdiskreq->seg;
   if (!seg && currdisk->acctime < 0.0) {
      fprintf(stderr, "NULL seg found in effectivebus in disk_completion\n");
      exit(0);
   }

   tmpioreq = currdiskreq->ioreqlist;
   while (tmpioreq) {
      if (tmpioreq->blkno == curr->blkno) {
	 break;
      }
      tmpioreq = tmpioreq->next;
   }
   if (!tmpioreq) {
      fprintf(stderr, "ioreq_event not found in effectivebus in disk_completion\n");
      exit(0);
   }
   while (tmpioreq->next) {
      tmpioreq = tmpioreq->next;
   }

   if (currdiskreq->ioreqlist->flags & READ) {
      disk_last_read_completion[(curr->devno)] = simtime;
   } else {
      disk_last_write_completion[(curr->devno)] = simtime;
   }

   currdiskreq->flags |= COMPLETION_RECEIVED;
   addtoextraq((event *) curr);
   currdisk->effectivebus = NULL;
   if (currdiskreq == currdisk->currentbus) {
         currdisk->currentbus = NULL;
   }

   if (currdisk->busowned != -1) {
      bus_ownership_release(currdisk->busowned);
      currdisk->busowned = -1;
      currdisk->outstate = DISK_IDLE;
   }

   if (currdisk->acctime >= 0.0) {
      addtoextraq((event *) currdiskreq->ioreqlist);
      addtoextraq((event *) currdiskreq);
      return;
   } else if (currdiskreq->ioreqlist->flags & READ) {
      while (currdiskreq->ioreqlist) {
         tmpioreq = currdiskreq->ioreqlist;
         currdiskreq->ioreqlist = tmpioreq->next;
         addtoextraq((event *) tmpioreq);
      }
      if ((currdiskreq == currdisk->effectivehda) ||
          (currdiskreq == currdisk->currenthda)) {
         if (seg->access->type == NULL_EVENT) {
            disk_release_hda(currdisk,currdiskreq);
         } else {
	    disk_check_prefetch_swap(currdisk);
	 }
      } else {
         disk_buffer_remove_from_seg(currdiskreq);
         addtoextraq((event *) currdiskreq);
         if (seg->recyclereq) {
	    if (seg->diskreqlist) {
               fprintf(stderr, "non-NULL diskreqlist found for recycled seg in disk_completion\n");
               exit(0);
	    }
            disk_buffer_set_segment(currdisk,seg->recyclereq);
            disk_buffer_attempt_seg_ownership(currdisk,seg->recyclereq);
            seg->recyclereq = NULL;
         } else if (seg->diskreqlist) {
            disk_find_new_seg_owner(currdisk,seg);
         } else if (!currdisk->enablecache) {
               seg->state = BUFFER_EMPTY;
         }
      }
   } else { 			/* WRITE */
      if (currdiskreq->inblkno >= (tmpioreq->blkno + tmpioreq->bcount)) {
	 if ((currdiskreq == currdisk->effectivehda) ||
	     (currdiskreq == currdisk->currenthda)) {
	    disk_release_hda(currdisk,currdiskreq);
	 } else {
            while (currdiskreq->ioreqlist) {
               tmpioreq = currdiskreq->ioreqlist;
               currdiskreq->ioreqlist = tmpioreq->next;
               addtoextraq((event *) tmpioreq);
            }
            disk_buffer_remove_from_seg(currdiskreq);
            addtoextraq((event *) currdiskreq);
            if (seg->recyclereq) {
	       if (seg->diskreqlist) {
                  fprintf(stderr, "non-NULL diskreqlist found for recycled seg in disk_completion\n");
                  exit(0);
	       }
               disk_buffer_set_segment(currdisk,seg->recyclereq);
               disk_buffer_attempt_seg_ownership(currdisk,seg->recyclereq);
               seg->recyclereq = NULL;
            } else if (seg->diskreqlist) {
               disk_find_new_seg_owner(currdisk,seg);
	    }
	 }
      } else if (currdiskreq->flags & SEG_OWNED) {
	 seg->access->flags |= BUFFER_BACKGROUND;
      }
   }

   disk_check_hda(currdisk, NULL, TRUE);
   disk_check_bus(currdisk, NULL);
}


void disk_interrupt_complete(curr)
ioreq_event *curr;
{
   switch (curr->cause) {

      case RECONNECT:
         disk_reconnection_or_transfer_complete(curr);
	 break;

      case DISCONNECT:
	 disk_disconnect(curr);
	 break;

      case COMPLETION:
	 disk_completion(curr);
	 break;

      default:
         fprintf(stderr, "Unrecognized event type at disk_interrupt_complete\n");
         exit(0);
   }
}


void disk_event_arrive(curr)
ioreq_event *curr;
{
/*
fprintf (outputfile, "Entered disk_event_arrive - devno %d, blkno %d, type %d, cause %d, time %f\n", curr->devno, curr->blkno, curr->type, curr->cause, curr->time);
*/

   if ((curr->devno < 0) || (curr->devno > numdisks)) {
      fprintf(stderr, "Event arrive for illegal disk - devno %d, numdisks %d\n", curr->devno, numdisks);
      exit(0);
   }
   switch (curr->type) {

      case IO_ACCESS_ARRIVE:
         if (disks[(curr->devno)].overhead > 0.0) {
            curr->time = simtime + (disks[(curr->devno)].overhead * disks[(curr->devno)].timescale);
            curr->type = DISK_OVERHEAD_COMPLETE;
            addtointq((event *) curr);
         } else {
            disk_request_arrive(curr);
         }
         break;

      case DISK_OVERHEAD_COMPLETE:
         disk_request_arrive(curr);
         break;

      case DISK_BUFFER_SEEKDONE:
         disk_buffer_seekdone(&disks[curr->devno], curr);
         break;

/*
      case DISK_BUFFER_TRACKACC_DONE:
         disk_buffer_trackacc_done(&disks[curr->devno], curr);
         break;
*/

      case DISK_BUFFER_SECTOR_DONE:
         disk_buffer_sector_done(&disks[curr->devno], curr);
         break;

      case DISK_GOTO_REMAPPED_SECTOR:
         disk_goto_remapped_sector(&disks[curr->devno], curr);
         break;

      case DISK_GOT_REMAPPED_SECTOR:
         disk_got_remapped_sector(&disks[curr->devno], curr);
         break;

      case DISK_PREPARE_FOR_DATA_TRANSFER:
         disk_prepare_for_data_transfer(curr);
         break;

      case DISK_DATA_TRANSFER_COMPLETE:
         disk_reconnection_or_transfer_complete(curr);
         break;

      case IO_INTERRUPT_COMPLETE:
         disk_interrupt_complete(curr);
         break;

      case IO_QLEN_MAXCHECK:
         /* Used only at initialization time to set up queue stuff */
         curr->tempint1 = -1;
         curr->tempint2 = disk_get_maxoutstanding(curr->devno);
         curr->bcount = 0;
         break;

      default:
         fprintf(stderr, "Unrecognized event type at disk_event_arrive\n");
         exit(0);
   }
/*
fprintf (outputfile, "Exiting disk_event arrive\n");
*/
}


/* Possible improvements:

   As write sectors complete, appending writes may be able to put more data
   into the cache.  However, need to find a way to keep from having to check
   everything on the bus queue every time a write sector completes.  Obviously
   can check for appending writes in the same seg, but need to be conscious of
   watermarks for the appending write as well.  (Don't reconnect just to xfer
   one more sector after each sector completes -- unless watermark says so.)

   As read sectors complete, sneakyintermediatereads could become available.
   However, need to find a way to keep from having to check everything on the
   bus queue every time a read sector completes.
*/

int disk_buffer_request_complete(currdisk, currdiskreq)
disk    *currdisk;
diskreq *currdiskreq;
{
   int background;
   int reading;
   int reqdone = FALSE;
   int hold_type;
   double acctime;
   int watermark = FALSE;
   ioreq_event *tmpioreq = currdiskreq->ioreqlist;
   segment *seg = currdiskreq->seg;

if ((disk_printhack > 1) && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f  %8p  Entering disk_buffer_request_complete\n", simtime, currdiskreq);
fflush(outputfile);
}

   while (tmpioreq && tmpioreq->next) {
      tmpioreq = tmpioreq->next;
   }

   background = seg->access->flags & BUFFER_BACKGROUND;
   reading = (seg->state == BUFFER_READING);
   if (currdisk->outwait) {
      currdisk->outwait->time = simtime;
      currdisk->outwait->bcount = 0;
      addtointq((event *) currdisk->outwait);
      currdisk->outwait = NULL;
   }
   if (reading) {
      if (background) {
	 if (seg->endblkno >= seg->maxreadaheadblkno) {
	    reqdone = TRUE;
         }
      } else {
         if ((seg->endblkno - currdiskreq->outblkno) >= currdiskreq->watermark) {
	    watermark = TRUE;
         }
         if (seg->endblkno >= (tmpioreq->blkno + tmpioreq->bcount)) {
	    reqdone = TRUE;
         }
      }
   } else {                     /* WRITE */
      if (background) {
	 if (seg->endblkno == currdiskreq->inblkno) {
	    reqdone = TRUE;
	 }
      } else {
         if ((seg->endblkno - currdiskreq->inblkno) <= currdiskreq->watermark) {
	    if (seg->endblkno < (tmpioreq->blkno + tmpioreq->bcount)) {
	       watermark = TRUE;
	    }
         }
	 if (currdiskreq->inblkno >= (tmpioreq->blkno + tmpioreq->bcount)) {
	    reqdone = TRUE;
	 }
      }
   }

   if ((background && reqdone && reading) || (seg->outstate == BUFFER_PREEMPT)) {
      seg->access->type = NULL_EVENT;
      disk_release_hda(currdisk, currdiskreq);
      return(TRUE);
   } 

   if (!background && 
       (seg->outstate == BUFFER_IDLE) && 
       (reqdone | watermark)) {
      disk_check_bus(currdisk,currdiskreq);
   }

   if (background && reading && (seg->endblkno == seg->minreadaheadblkno) &&
       ((currdisk->preseeking == PRESEEK_BEFORE_COMPLETION) || 
	(currdiskreq->flags & COMPLETION_RECEIVED) ||
	((currdisk->preseeking == PRESEEK_DURING_COMPLETION) && 
	 (currdiskreq->flags & COMPLETION_SENT)))) {
      hold_type = seg->access->type;
      seg->access->type = NULL_EVENT;
      disk_check_prefetch_swap(currdisk);
      if ((seg->outstate == BUFFER_PREEMPT) ||
	  (currdisk->effectivehda != currdiskreq)) {
         return(TRUE);
      }
      seg->access->type = hold_type;
   } else if ((!background || !reading) && reqdone) {
      if (!reading || currdisk->contread == BUFFER_NO_READ_AHEAD || (seg->endblkno >= seg->maxreadaheadblkno)) {
         seg->access->type = NULL_EVENT;
      } else {
         seg->access->flags |= BUFFER_BACKGROUND;
         reqdone = FALSE;
      }
      if (currdisk->stat.seekdistance != -1) {
         acctime = currdisk->stat.seektime + currdisk->stat.latency + currdisk->stat.xfertime;
         disk_acctimestats(currdisk, currdisk->stat.seekdistance, currdisk->stat.seektime, currdisk->stat.latency, currdisk->stat.xfertime, acctime);
      }
      tmpioreq = currdiskreq->ioreqlist;
      while (tmpioreq) {
         disk_interferestats(currdisk, tmpioreq);
	 tmpioreq = tmpioreq->next;
      }
      currdisk->stat.seekdistance = -1;
      currdisk->stat.xfertime = (double) 0.0;
      currdisk->stat.latency = (double) 0.0;
      if (!reqdone &&
          ((currdisk->preseeking == PRESEEK_BEFORE_COMPLETION) || 
	   (currdiskreq->flags & COMPLETION_RECEIVED) ||
	   ((currdisk->preseeking == PRESEEK_DURING_COMPLETION) && 
	    (currdiskreq->flags & COMPLETION_SENT)))) {
         hold_type = seg->access->type;
         seg->access->type = NULL_EVENT;
         disk_check_prefetch_swap(currdisk);
         if ((seg->outstate == BUFFER_PREEMPT) ||
	     (currdisk->effectivehda != currdiskreq)) {
            return(TRUE);
         }
         seg->access->type = hold_type;
      }
   }

   if (reqdone) {
      if (!reading) {
         tmpioreq = currdiskreq->ioreqlist;
         while (tmpioreq) {
            if (!ioqueue_physical_access_done(currdisk->queue,tmpioreq)) {
               fprintf(stderr, "disk_buffer_request_complete:  ioreq_event not found by ioqueue_physical_access_done call\n");
               exit(0);
            }
	    tmpioreq = tmpioreq->next;
         }
         if (!(currdiskreq->flags & COMPLETION_SENT) &&
	     (seg->outstate == BUFFER_IDLE)) {
	    disk_check_bus(currdisk, currdiskreq);
         }
      }
      seg->access->type = NULL_EVENT;
      disk_release_hda(currdisk,currdiskreq);
   }

   return(reqdone);
}


void disk_got_remapped_sector(currdisk, curr)
disk *currdisk;
ioreq_event *curr;
{
   diskreq *currdiskreq = currdisk->effectivehda;
   segment *seg;

   if (!currdiskreq) {
      fprintf(stderr, "No effectivehda in disk_got_remapped_sector\n");
      exit(0);
   }
   seg = currdiskreq->seg;
   if (seg->outstate == BUFFER_PREEMPT) {
      seg->access->type = NULL_EVENT;
      disk_release_hda(currdisk, currdiskreq);
      return;
   }

   if ((currdiskreq == seg->recyclereq) || !disk_buffer_block_available(currdisk, seg, curr->blkno)) {
      seg->time += currdisk->rotatetime;
      currdisk->stat.xfertime += currdisk->rotatetime;
      curr->time += currdisk->rotatetime;
      curr->type = DISK_GOT_REMAPPED_SECTOR;
      addtointq((event *) curr);
   } else {
      if (seg->state == BUFFER_READING) {
         if (curr->blkno != seg->endblkno) {
	    fprintf(stderr, "Logical address of remapped sector not next to read\n");
	    exit(0);
         }
         seg->endblkno++;
      } else {
         if (curr->blkno != currdiskreq->inblkno) {
	    fprintf(stderr, "Logical address of remapped sector not next to read\n");
	    exit(0);
         }
         currdiskreq->inblkno++;
      }
      if (!(disk_buffer_request_complete(currdisk, seg))) {
         curr->blkno++;
         if (!(disk_initiate_seek(currdisk, seg, curr, 0, 0.0, 0.0))) {
	    disk_release_hda(currdisk,currdiskreq);
         }
      }
   }
}


void disk_buffer_update_outbuffer(currdisk, seg)
disk *currdisk;
segment *seg;
{
   int blks;
   diskreq* currdiskreq = currdisk->effectivebus;
   double tmptime;
   ioreq_event *tmpioreq;
   diskreq *seg_owner;

   if (!currdiskreq) {
      return;
   }

   if (currdiskreq->seg != seg) {
      fprintf(stderr, "Segments don't match in disk_buffer_update_outbuffer\n");
      exit(0);
   }

   seg_owner = disk_buffer_seg_owner(seg,FALSE);
   if (!seg_owner) {
      seg_owner = currdiskreq;
   }

   tmpioreq = currdiskreq->ioreqlist;
   while (tmpioreq) {
      if ((currdiskreq->outblkno >= tmpioreq->blkno) &&
          (currdiskreq->outblkno < (tmpioreq->blkno + tmpioreq->bcount))) {
         break;
      }
      tmpioreq = tmpioreq->next;
   }
   if (!tmpioreq) {
      fprintf(stderr, "Cannot find ioreq in disk_buffer_update_outbuffer\n");
      exit(0);
   }

/*
fprintf (outputfile, "Entered disk_buffer_update_ongoing - devno %d\n", seg->reqlist->devno);
*/
   if (currdisk->outwait) {
      return;
   }
   blks = bus_get_data_transfered(tmpioreq, disk_get_depth(tmpioreq->devno));
   if (blks == -1) {
      tmptime = disk_get_blktranstime(tmpioreq);
      if (tmptime != (double) 0.0) {
         blks = (int) ((simtime - currdisk->starttrans) / tmptime);
      } else {
	 blks = seg->outbcount;
      }
   }
   blks = min(seg->outbcount, (blks - currdisk->blksdone));
   currdisk->blksdone += blks;
   currdiskreq->outblkno += blks;
   seg->outbcount -= blks;
   if (currdiskreq->outblkno > seg->endblkno) {
      seg->endblkno = currdiskreq->outblkno;
   }
   disk_buffer_segment_wrap(seg, seg->endblkno);
   if ((seg->state == BUFFER_WRITING) && ((seg_owner->inblkno < seg->startblkno) || (seg_owner->inblkno > seg->endblkno))) {
      fprintf(stderr, "Error with inblkno at disk_buffer_update_outbuffer: %d, %d, %d, %d\n", seg_owner->inblkno, seg->startblkno, seg->endblkno, blks);
      exit(0);
   }
/*
fprintf (outputfile, "Exiting disk_buffer_update_ongoing\n");
*/
}


int disk_buffer_use_trackacc(currdisk, curr, seg)
disk *currdisk;
ioreq_event *curr;
segment *seg;
{
   return(FALSE);

   /* This function, which has not been resurrected since the last major   */
   /* organizational change in the simulator, was designed to support the  */
   /* use of a single event for reading or writing all sectors of interest */
   /* on a track (after any positioning).  The purpose was to reduce time  */
   /* to complete the simulation.  It worked well.  It should be simpler   */
   /* instead use a "group of sectors" type of approach, given all of the  */
   /* various complexities of modern disks. */
}


/* returns the following values in returned_ioreq->bcount:
      -2 if no disconnect and no valid transfer size exists
      -1 if disconnect requested
       0 if completed
      >0 if valid transfer size exists
*/

ioreq_event* disk_buffer_transfer_size(currdisk, currdiskreq, curr)
disk        *currdisk;
diskreq     *currdiskreq;
ioreq_event *curr;
{
   segment *seg = currdiskreq->seg;
   diskreq *seg_owner;
   ioreq_event *tmpioreq;

/*
fprintf (outputfile, "Entered disk_buffer_transfer_size - devno %d\n", curr->devno);
fprintf (outputfile, "outstate %d, outblkno %d, outbcount %d, endblkno %d, inblkno %d\n", seg->outstate, currdiskreq->outblkno, seg->outbcount, seg->endblkno, seg->inblkno);
*/

   if (currdisk->acctime >= 0.0) {
      if (currdiskreq->overhead_done <= simtime) {
         curr->bcount = 0;
      } else {
         curr->bcount = -2;
      }
      return(curr);
   }

   if (!seg) {
      if (!currdisk->neverdisconnect) {
	 fprintf(stderr, "NULL seg found at disk_buffer_transfer_size without neverdisconnect set\n");
	 exit(0);
      }
      curr->bcount = -2;
      return(curr);
   }

   tmpioreq = currdiskreq->ioreqlist;
   while (tmpioreq) {
      if (tmpioreq->blkno == curr->blkno) {
	 break;
      }
      tmpioreq = tmpioreq->next;
   }
   if (!tmpioreq) {
      fprintf(stderr, "ioreq_event not found in effectivebus in disk_buffer_transfer_size\n");
      exit(0);
   }

   disk_buffer_update_inbuffer(currdisk, seg);
   if (tmpioreq->flags & READ) {
      if (seg->outstate == BUFFER_TRANSFERING) {
         currdiskreq->outblkno += seg->outbcount;
      }
      if (currdiskreq->outblkno >= (tmpioreq->blkno + tmpioreq->bcount)) {
	 tmpioreq = tmpioreq->next;
         if (!tmpioreq) {
            seg->outstate = BUFFER_IDLE;
            curr->bcount = 0;
            return(curr);
         } else {
	    if (currdiskreq->outblkno >= (tmpioreq->blkno + tmpioreq->bcount)) {
               fprintf(stderr, "read ioreq_event skipped in disk_reconnection_or_transfer_complete\n");
               exit(0);
	    }
	    addtoextraq((event *) curr);
	    curr = ioreq_copy(tmpioreq);
	 }
      }
      if (seg->endblkno > currdiskreq->outblkno) {
         seg->outstate = BUFFER_TRANSFERING;
	 curr->bcount = seg->outbcount = min(seg->endblkno,(tmpioreq->blkno + tmpioreq->bcount)) - currdiskreq->outblkno;
      } else if (currdisk->hold_bus_for_whole_read_xfer || 
		 currdisk->neverdisconnect) {
	 seg->outstate = BUFFER_TRANSFERING;
	 seg->outbcount = 0;
	 curr->bcount = -2;
      } else {
	 seg->outstate = BUFFER_IDLE;
	 curr->bcount = -1;
      }
   } else {  			/* WRITE */
      if ((currdiskreq->outblkno == (tmpioreq->blkno + tmpioreq->bcount)) &&
	  (currdiskreq->inblkno >= currdiskreq->outblkno)) {
	 if (tmpioreq->next) {
            fprintf(stderr, "write ioreq_event skipped (A) in disk_reconnection_or_transfer_complete\n");
            exit(0);
	 }
	 seg->outstate = BUFFER_IDLE;
	 curr->bcount = 0;
         return(curr);
      }

      seg_owner = disk_buffer_seg_owner(seg,FALSE);
      if (!seg_owner) {
	 seg_owner = currdiskreq;
      }

      if (seg->outstate == BUFFER_TRANSFERING) {
         currdiskreq->outblkno += seg->outbcount;
	 if (tmpioreq->blkno < seg->startblkno) {
	    seg->startblkno = tmpioreq->blkno;
	 }
	 if (currdiskreq->outblkno > seg->endblkno) {
	    seg->endblkno = currdiskreq->outblkno;
	 }
	 if (seg->hold_bcount && 
	     (seg->endblkno == seg->hold_blkno)) {
            if (tmpioreq->next || 
		(currdiskreq->outblkno != (tmpioreq->blkno + tmpioreq->bcount))) {
	       fprintf(stderr, "Error with hold fields at disk_buffer_transfer_size\n");
	       exit(0);
            }
            seg->endblkno += seg->hold_bcount;
	    seg->hold_bcount = 0;
	 }
	 disk_buffer_segment_wrap(seg, seg->endblkno);
	 ASSERT((seg_owner->inblkno >= seg->startblkno) && 
		(seg_owner->inblkno <= seg->endblkno));
         if (currdiskreq->outblkno >= (tmpioreq->blkno + tmpioreq->bcount)) {
	    tmpioreq = tmpioreq->next;
	    if (!tmpioreq) {
	       seg->outstate = BUFFER_IDLE;
	       if (currdisk->fastwrites == NO_FASTWRITE) {
	          curr->bcount = -1;
	       } else {
		  curr->bcount = 0;
	       }
	       return(curr);
	    } else {
	       if (currdiskreq->outblkno >= (tmpioreq->blkno + tmpioreq->bcount)) {
                  fprintf(stderr, "write ioreq_event skipped (B) in disk_reconnection_or_transfer_complete\n");
                  exit(0);
	       }
	       addtoextraq((event *) curr);
	       curr = ioreq_copy(tmpioreq);
	    }
         }
	 if ((currdiskreq->outblkno - seg_owner->inblkno) > seg->size) {
	    fprintf(stderr, "Buffer needs to wrap around\n");
	    exit(0);
	 }

      }
      seg->outstate = BUFFER_TRANSFERING;
      curr->bcount = seg->outbcount = 
	min((seg->size - (currdiskreq->outblkno - seg_owner->inblkno)), 
	    (tmpioreq->blkno + tmpioreq->bcount - currdiskreq->outblkno));
/*
fprintf (outputfile, "%f Start transfer of %d sectors of %d (from %d)\n", simtime, seg->outbcount, seg->reqlist->bcount, seg->reqlist->blkno);
*/
      if ((!(currdiskreq->flags & SEG_OWNED) && 
	   (seg->endblkno < currdiskreq->outblkno)) || 
	  (seg->outbcount <= 0)) {
         if (currdisk->hold_bus_for_whole_write_xfer || 
      	     currdisk->neverdisconnect) {
if ((disk_printhack > 1) && (simtime >= disk_printhacktime)) {
   fprintf(stderr,"Holding bus...\n");
}
	    seg->outstate = BUFFER_TRANSFERING;
	    seg->outbcount = 0;
	    curr->bcount = -2;
         } else {
	    seg->outstate = BUFFER_IDLE;
	    curr->bcount = -1;
         }
      }
   }
   return(curr);
}


/* Enablement function for ioqueue which selects the next hda access to
   perform

   If all segs are dirty and none are recyclable, choose a write with a segment

   Don't choose sneaky reads on write segments unless they are recyclable

   If no seg, call select_segment (and possibly recyclable_segment) to
     make sure that a seg is/will be available.  However, reset the seg and
     hittype afterwards.

   Don't choose BUFFER_COLLISION requests

   Note that writeseg environments cannot use recycled segments
*/

int disk_enablement_function(currioreq)
ioreq_event* currioreq;
{
   diskreq     *currdiskreq = (diskreq*) currioreq->ioreq_hold_diskreq;
   disk        *currdisk = (disk*) currioreq->ioreq_hold_disk;
   ioreq_event *tmpioreq;
   diskreq     *seg_owner;

   if (!currdisk) {
      fprintf(stderr, "Disk ioqueue iobuf has NULL ioreq_hold_disk (disk*)\n");
      exit(0);
   }

   if (!currdiskreq) {
      fprintf(stderr, "Disk ioqueue iobuf has NULL ioreq_hold_diskreq (diskreq*)\n");
      exit(0);
   }

   if (currdisk->acctime >= 0.0) {
      return(TRUE);
   }

   if (currioreq->flags & READ) {
/*
      if ((currdisk->numdirty == currdisk->numsegs) && !currdiskreq->seg)
	 return(FALSE);
      } else 
*/
      if (currdiskreq->seg) {
	 ASSERT(currdiskreq->seg->recyclereq != currdiskreq);
	 if ((currdiskreq->seg->state == BUFFER_WRITING) ||
	     (currdiskreq->seg->state == BUFFER_DIRTY)) {
            if (!disk_buffer_reusable_segment_check(currdisk,currdiskreq->seg)) {
	       return(FALSE);
	    }
         }
      } else {
         disk_buffer_select_segment(currdisk,currdiskreq,FALSE);
         if (currdiskreq->seg) {
	    if ((currdiskreq->seg->state == BUFFER_WRITING) ||
	        (currdiskreq->seg->state == BUFFER_DIRTY)) {
               if (!disk_buffer_reusable_segment_check(currdisk,currdiskreq->seg)) {
	          currdiskreq->hittype = BUFFER_NOMATCH;
	          currdiskreq->seg = NULL;
	          return(FALSE);
	       }
            }
	    currdiskreq->hittype = BUFFER_NOMATCH;
	    currdiskreq->seg = NULL;
         } else {
	    if ((currdiskreq->hittype == BUFFER_COLLISION) ||
		!disk_buffer_recyclable_segment(currdisk,TRUE)) {
               return(FALSE);
	    }
	 }
      }
   } else {		/* WRITE */
      if (currdiskreq->seg) {
/*
      if ((currdisk->numdirty == currdisk->numsegs) &&
*/
	 ASSERT(currdiskreq->seg->recyclereq != currdiskreq);
         if (!(currdiskreq->flags & SEG_OWNED)) {
	    seg_owner = disk_buffer_seg_owner(currdiskreq->seg,FALSE);
	    if (!seg_owner) {
	       seg_owner = currdiskreq->seg->diskreqlist;
	       ASSERT(seg_owner->ioreqlist != NULL);
               while (seg_owner->ioreqlist->flags & READ) {
		  seg_owner = seg_owner->seg_next;
		  ASSERT(seg_owner != NULL);
		  ASSERT(seg_owner->ioreqlist != NULL);
	       }
	    }
	    if (seg_owner != currdiskreq) {
	       tmpioreq = currdiskreq->ioreqlist;
	       ASSERT(tmpioreq != NULL);
	       while (tmpioreq->next) {
	          tmpioreq = tmpioreq->next;
	       }
	       if (((tmpioreq->blkno + tmpioreq->bcount - seg_owner->inblkno) > currdiskreq->seg->size) && 
		   (seg_owner->inblkno != currdiskreq->ioreqlist->blkno)) {
	          return(FALSE);
	       }
	    }
	 }
      } else {
         disk_buffer_select_segment(currdisk,currdiskreq,FALSE);
         if (currdiskreq->seg) {
	    if (currdiskreq->hittype == BUFFER_APPEND) {
	       seg_owner = disk_buffer_seg_owner(currdiskreq->seg,FALSE);
	       ASSERT(seg_owner != NULL);
	       tmpioreq = currdiskreq->ioreqlist;
	       ASSERT(tmpioreq != NULL);
	       while (tmpioreq->next) {
	          tmpioreq = tmpioreq->next;
	       }
	       if (((tmpioreq->blkno + tmpioreq->bcount - seg_owner->inblkno) > currdiskreq->seg->size) && 
	           (seg_owner->inblkno != currdiskreq->ioreqlist->blkno)) {
	          currdiskreq->hittype = BUFFER_NOMATCH;
	          currdiskreq->seg = NULL;
	          return(FALSE);
	       }
	    }
	    currdiskreq->hittype = BUFFER_NOMATCH;
	    currdiskreq->seg = NULL;
         } else {
	    if ((currdiskreq->hittype == BUFFER_COLLISION) ||
		!disk_buffer_recyclable_segment(currdisk,FALSE)) {
               return(FALSE);
	    }
	 }
      }
   }

   return(TRUE);
}


/* "stopable" means either that the current effectivehda request can,
   in fact, be stopped instantly, or that the current request can be
   "merged" in some way with the effectivehda request.  Examples of the
   latter case include read hits on prefetched data and sequential
   writes.

   Note that a request is not stopable if the preseeking level is
   not appropriate, or if its seg is marked as recycled, or if there is 
   a "better" request in the queue than the passed in request.  
   "Better" is indicated via the preset scheduling algorithm and 
   ioqueue_show_next_request.
*/

int disk_buffer_stopable_access(currdisk, currdiskreq)
disk *currdisk;
diskreq *currdiskreq;
{
   segment *seg;
   diskreq *effective = currdisk->effectivehda;
   ioreq_event *currioreq;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f  %8p  Entering disk_buffer_stopable_access\n", simtime, currdiskreq);
fprintf (outputfile, "                        checking if %8p is stoppable\n", effective);
fflush(outputfile);
}

   if (!effective) {
      fprintf(stderr, "Trying to stop a non-existent access\n");
      exit(0);
   }

   seg = effective->seg;

   if (!seg) {
      fprintf(stderr, "effectivehda has NULL seg in disk_buffer_stopable_access\n");
      exit(0);
   }

   if (seg->recyclereq) {
      if (seg->recyclereq != effective) {
         fprintf(stderr, "effectivehda != recyclereq in disk_buffer_stopable_access\n");
         exit(0);
      }
      return(FALSE);
   }

   if ((seg->state != BUFFER_READING) && (seg->state != BUFFER_WRITING)) {
      fprintf(stderr, "Trying to stop a non-active access\n");
      exit(0);
   }

   if (seg->state == BUFFER_READING) {
/*
fprintf (outputfile, "%f, Inside background read portion of stopable_access - read %d\n", simtime, (curr->flags & READ));
*/
      currioreq = currdiskreq->ioreqlist;

      /* if active request, return FALSE if minreadaheadblkno hasn't been
	 reached and the current request isn't a read hit on this segment
      */

      if ((effective->ioreqlist != NULL) && 
	  (seg->endblkno < seg->minreadaheadblkno) && 
	  (!(currioreq->flags & READ) || 
	   (currdiskreq->seg != seg) ||
	   (currioreq->blkno < seg->startblkno) || 
	   (currioreq->blkno > seg->endblkno))) {
         return(FALSE);
      }

      if (!(currioreq->flags & READ) && (currdisk->write_hit_stop_readahead)) {
         while (currioreq) {
            if (disk_buffer_overlap(seg, currioreq)) {
	       seg->maxreadaheadblkno = seg->minreadaheadblkno = seg->endblkno;
	       break;
            }
	    currioreq = currioreq->next;
         }
      }

      currioreq = currdiskreq->ioreqlist;

      /* Check for read hit or "almost read hit" (first blkno == block 
	 currently being prefetched.  If hit is indicated, first check to
         make sure that the passed-in request is the "best" request to
	 receive the hda.  (In some cases, it may be better to let the
	 prefetch reach the minimum and then go to another request 
	 rather than take the hit immediately.)
      */

      if ((currioreq->flags & READ) && 
	  currdisk->enablecache &&
	  (currdiskreq->seg == seg) &&
	  (currioreq->blkno >= seg->startblkno) && 
	  ((currioreq->blkno < seg->endblkno) || 
	   (currdisk->almostreadhits && (currioreq->blkno == seg->endblkno)))) {
         ioreq_event *bestioreq = ioqueue_show_next_request(currdisk->queue,disk_enablement_function);

	 if (!bestioreq || 
	     (bestioreq->ioreq_hold_diskreq == currdiskreq)) {
	    return(TRUE);
	 } else {
	    /* This is a temporary kludge to fix the problem with SSTF/VSCAN
	       alternating between up and down for equal diff's         */
            bestioreq = ioqueue_show_next_request(currdisk->queue,disk_enablement_function);
	    if (bestioreq->ioreq_hold_diskreq == currdiskreq) {
	       return(TRUE);
	    } else {
	       return(FALSE);
	    }
	 }
      } 
      
      if ((!(effective->flags & COMPLETION_SENT) && 
	   (currdisk->preseeking != PRESEEK_BEFORE_COMPLETION)) || 
	  (!(effective->flags & COMPLETION_RECEIVED) && 
	   (currdisk->preseeking == NO_PRESEEK))) {
         return(FALSE);
      }

      if (seg->endblkno < seg->minreadaheadblkno) {
	 seg->maxreadaheadblkno = seg->minreadaheadblkno;
	 return(FALSE);
      }

      if ((seg->access->type == DISK_GOT_REMAPPED_SECTOR) ||
          (seg->access->type == DISK_GOTO_REMAPPED_SECTOR)) {
         seg->outstate = BUFFER_PREEMPT;
         return(FALSE);
      } else if (seg->access->type == NULL_EVENT) {
	 return(TRUE);
      }

      if ((!currdisk->disconnectinseek) && ((seg->access->type == DISK_BUFFER_SEEKDONE) || (!currdisk->stopinsector))) {
         if (removefromintq(seg->access) == FALSE) {
            fprintf(stderr, "Non-null seg->access does not appear on the intq in stopable: %d\n", seg->access->type);
            exit(0);
         }
	 if (seg->access->type == DISK_BUFFER_SEEKDONE) {
            seg->access->tempptr1 = disk_translate_lbn_to_pbn(currdisk, seg->access->blkno, MAP_FULL, &currcylno, &currsurface, &seg->access->cause);
            seg->access->time = seg->time + diskacctime(currdisk, seg->access->tempptr1, DISKSEEK, (seg->access->flags & READ), seg->time, currcylno, currsurface, seg->access->cause, seg->access->bcount, 0);
	 }
	 disk_seek_stoptime = seg->access->time;
         seg->access->type = NULL_EVENT;
	 return(TRUE);
      }
      if ((seg->access->type == DISK_BUFFER_SEEKDONE) || (!currdisk->stopinsector)) {                               /* Must be SEEKDONE or SECTOR_DONE */
         seg->outstate = BUFFER_PREEMPT;
         return(FALSE);
      }
      return(TRUE);
   } else if (seg->state == BUFFER_WRITING) {
      if ((currdiskreq->hittype == BUFFER_APPEND) &&
	  (currdiskreq->seg == seg) &&
	  (seg->endblkno == currdiskreq->ioreqlist->blkno) &&
	  ((currdisk->fastwrites != LIMITED_FASTWRITE) || 
	   (ioqueue_get_number_pending(currdisk->queue) == 1))) {
         ioreq_event *bestioreq = ioqueue_show_next_request(currdisk->queue,disk_enablement_function);

	 if (!bestioreq || (bestioreq->ioreq_hold_diskreq == currdiskreq)) {
	    return(TRUE);
	 }
      }
   }

/* Must deal with write overlap with ongoing writes here, if care to */

   return(FALSE);
}


void disk_buffer_seekdone(currdisk, curr)
disk *currdisk;
ioreq_event *curr;
{
   int immed;
   diskreq *currdiskreq = currdisk->effectivehda;
   segment *seg;
   double mydiff;

   seg = currdiskreq->seg;

if (disk_printhack && (simtime >= disk_printhacktime)) {
fprintf (outputfile, "%12.6f  %8p  Entering disk_buffer_seekdone for disk %d\n", simtime, currdiskreq,currdisk->devno);
fflush(outputfile);
}

   curr->tempptr1 = disk_translate_lbn_to_pbn(currdisk, curr->blkno, MAP_FULL, &currcylno, &currsurface, &curr->cause);
   curr->time = seg->time + diskacctime(currdisk, curr->tempptr1, DISKSEEK, (curr->flags & READ), seg->time, currcylno, currsurface, curr->cause, curr->bcount, 0);
   mydiff = fabs(curr->time - simtime);
   /* This is purely for self-checking.  Can be removed. */
   if ((mydiff > 0.000000001) && (mydiff > (0.00000000001 * simtime))) {
      fprintf(stderr, "Access times don't match (1) - exp. %f  actual %f  mydiff %f\n", simtime, curr->time, mydiff);
      exit(0);
   }
   if (seg->outstate == BUFFER_PREEMPT) {
      curr->type = NULL_EVENT;
      disk_release_hda(currdisk,currdiskreq);
      return;
   }
   if (seg->recyclereq != currdiskreq) {

/*  NOT NEEDED WHILE TRACKACC IS OUT OF SERVICE.... (PERHAPS MOVE THERE?)
      if (seg->outstate == BUFFER_TRANSFERING) {
         disk_buffer_update_outbuffer(currdisk, seg);
      }
*/

      if ((!currdisk->sectbysect) && (disk_buffer_use_trackacc(currdisk, curr, seg))) {
         return;
      }
   }
   if (currdisk->stat.xfertime == (double) -1) {
      currdisk->stat.latency = simtime;
   }

   immed = currdisk->immed;
   curr->bcount = ((band *) curr->tempptr1)->blkspertrack;
   remapsector = FALSE;
   disk_get_lbn_boundaries_for_track(currdisk, curr->tempptr1, currdisk->currcylno, currdisk->currsurface, &currdisk->firstblkontrack, &currdisk->endoftrack);
   currdisk->translatesectbysect = FALSE;
   if (remapsector || ((currdisk->endoftrack - currdisk->firstblkontrack) != curr->bcount)) {
      currdisk->translatesectbysect = TRUE;
   }
/*
fprintf (outputfile, "remapsector %d, endoftrack %d, firstblkontrack %d, blkspertrack %d\n", remapsector, currdisk->endoftrack, currdisk->firstblkontrack, curr->bcount);
*/

   if ((currdisk->readanyfreeblocks) && (curr->flags & READ) && (curr->blkno != currdisk->firstblkontrack)) {
      curr->blkno = currdisk->firstblkontrack;
      immed = TRUE;
   }
   seg->time = simtime;
   curr->time = seg->time + disklatency(currdisk, curr->tempptr1, seg->time, curr->cause, curr->bcount, immed);
   currdisk->immedstart = currdisk->endoftrack;
   currdisk->immedend = currdisk->immedstart;
   curr->blkno = trackstart + currdisk->firstblkontrack;
   curr->cause = trackstart;
   curr->type = DISK_BUFFER_SECTOR_DONE;
   curr->bcount = 0;
   addtointq((event *) curr);

/*
fprintf (outputfile, "startblkno %d, endblkno %d, outblkno %d, reqstart %d, reqend %d\n", seg->startblkno, seg->endblkno, currdiskreq->outblkno, seg->reqlist->blkno, (seg->reqlist->blkno + seg->reqlist->bcount));
fprintf (outputfile, "Leaving disk_buffer_seekdone\n");
*/
}


void disk_buffer_sector_done(currdisk, curr)
disk *currdisk;
ioreq_event *curr;
{
   segment *seg;
   diskreq *currdiskreq = currdisk->effectivehda;
   ioreq_event *tmpioreq;
   int blks_on_track;
   int start_of_track;
   int end_of_track;
   int firstblkno;
   int lastblkno;
   int endlat = 0;
   int currblkno;
   int currcause;
   double mydiff;

   if (!currdiskreq) {
      fprintf(stderr, "No effectivehda in disk_buffer_sector_done\n");
      exit(0);
   }
   seg = currdiskreq->seg;
/*
fprintf (outputfile, "\nEntering disk_buffer_sector_done - devno %d, bcount %d, simtime %f\n", curr->devno, curr->bcount, simtime);
*/
   blks_on_track = ((band *) curr->tempptr1)->blkspertrack;
   start_of_track = currdisk->firstblkontrack;
   end_of_track = currdisk->endoftrack;

   if (!currdisk->translatesectbysect) {
      currblkno = (curr->blkno == start_of_track) ? (end_of_track-1) : (curr->blkno-1);
   } else {
      currcause = (curr->cause) ? (curr->cause-1) : (blks_on_track-1);
      remapsector = FALSE;
      currblkno = disk_translate_pbn_to_lbn(currdisk, curr->tempptr1, currdisk->currcylno, currdisk->currsurface, currcause);
      if (remapsector | (currblkno < 0)) {
	 currblkno = -1;
	 if (curr->bcount) {
	    curr->bcount = 2;
	 }
      }
   }
   currangle = currdisk->currangle;
/*
fprintf (outputfile, "More state: blkno %d, cause %d, currangle %f, currblkno %f\n", curr->blkno, curr->cause, currdisk->currangle, disk_get_blkno_from_currangle(currdisk, curr->tempptr1));
fprintf (outputfile, "Cylno %d, Surface %d\n", currdisk->currcylno, currdisk->currsurface);
*/

   if (curr->bcount) {
      currdisk->currangle += (simtime - seg->time) / currdisk->rotatetime;
      currdisk->currangle -= (double) ((int) currdisk->currangle);
      currdisk->currtime = simtime;
      if ((!currdisk->read_direct_to_buffer) && (curr->flags & READ) && (curr->bcount == 1) && (!(disk_buffer_block_available(currdisk, seg, currblkno)))) {
	 curr->bcount = 2;
      }
   } else {
      curr->time = seg->time + diskacctime(currdisk, curr->tempptr1, DISKPOS, (curr->flags & READ), seg->time, currdisk->currcylno, currdisk->currsurface, curr->cause, 1, 0);
      mydiff = fabs(curr->time - simtime);
/*
fprintf (outputfile, "trackstart %d, blkno %d, cause %d\n", trackstart, curr->blkno, curr->cause);
*/
      /* This is purely for self-checking.  Can be removed. */
      if ((mydiff > 0.000000001) && (mydiff > (0.00000000001 * simtime))) {
         fprintf(stderr, "Times don't match in disk_buffer_sector_done - exp %f real %f\n", simtime, curr->time);
	 fprintf(stderr, "devno %d, blkno %d, bcount %d, bandno %p, blkinband %d\n", curr->devno, curr->blkno, curr->bcount, curr->tempptr1, (curr->blkno-bandstart));
         exit(0);
      }
   }
/*
fprintf (outputfile, "More state: blkno %d, cause %d, currangle %f, currblkno %f\n", curr->blkno, curr->cause, currdisk->currangle, disk_get_blkno_from_currangle(currdisk, curr->tempptr1));
fprintf (outputfile, "Cylno %d, Surface %d\n", currdisk->currcylno, currdisk->currsurface);
*/
   if (seg->recyclereq == currdiskreq) {
      curr->bcount = 2;
      firstblkno = currdiskreq->inblkno;
      tmpioreq = currdiskreq->ioreqlist;
      while (tmpioreq->next) {
	 tmpioreq = tmpioreq->next;
      }
      lastblkno = min((tmpioreq->blkno + tmpioreq->bcount),end_of_track);
   } else if (seg->state == BUFFER_READING) {
      firstblkno = seg->endblkno;
      if (curr->flags & BUFFER_BACKGROUND) {
	 lastblkno = min(seg->maxreadaheadblkno, end_of_track);
      } else {
         tmpioreq = currdiskreq->ioreqlist;
         while (tmpioreq->next) {
	    tmpioreq = tmpioreq->next;
         }
	 lastblkno = min((tmpioreq->blkno + tmpioreq->bcount), end_of_track);
      }
   } else {
      firstblkno = currdiskreq->inblkno;
      lastblkno = min(seg->endblkno, end_of_track);
   }

   if (currdisk->stat.xfertime == (double) -1) {
      endlat++;
   }
/*
fprintf (outputfile, "State check: firstblkno %d, lastblkno %d, curr->blkno %d, immed %d\n", firstblkno, lastblkno, curr->blkno, currdisk->immed);
fprintf (outputfile, "More info: read %d, immedstart %d, end_of_track %d, first_on_track %d\n", (seg->state == BUFFER_READING), currdisk->immedstart, end_of_track, start_of_track);
*/
   if ((curr->bcount > 2) || (curr->bcount < 0)) {
      fprintf(stderr, "Unacceptable bcount value in ioreq at disk_buffer_sector_done\n");
      exit(0);
   } else if (curr->bcount == 1) {
      if (currblkno == firstblkno) {
         firstblkno++;
         if (firstblkno == currdisk->immedstart) {
            firstblkno = currdisk->immedend;
            currdisk->immedstart = end_of_track + 1;
            currdisk->immedend = currdisk->immedstart;
         }
	 endlat++;
      } else if (currblkno < seg->startblkno) {
	 ASSERT2(((seg->state == BUFFER_READING) && currdisk->readanyfreeblocks),"seg->state",seg->state,"readanyfreeblocks",currdisk->readanyfreeblocks);
	 seg->startblkno = currblkno;
	 firstblkno = currblkno + 1;
      } else if ((currdisk->immed) && (currdisk->immedstart >= end_of_track)) {
         if (currblkno >= lastblkno) {
            if ((seg->state == BUFFER_READING) && (currdisk->contread != BUFFER_NO_READ_AHEAD) && (currblkno < currdiskreq->inblkno)) {
	       currdisk->immedstart = currblkno;
	       currdisk->immedend = currblkno + 1;
            }
         } else if (currblkno > firstblkno) {
            currdisk->immedstart = currblkno;
            currdisk->immedend = currblkno + 1;
	    endlat++;
         }
      } else if ((currblkno == currdisk->immedend) && ((currblkno < lastblkno) || (currblkno < currdiskreq->inblkno))) {
         currdisk->immedend++;
	 disk_buffer_segment_wrap(seg, currdisk->immedend);
      } else if ((currblkno > currdisk->immedend) && (currblkno < lastblkno)) {
	 currdisk->immedstart = currblkno;
	 currdisk->immedend = currblkno + 1;
      }
   }

   if ((currdiskreq != seg->recyclereq) && 
       (seg->outstate == BUFFER_TRANSFERING)) {
      disk_buffer_update_outbuffer(currdisk, seg);
   }
/*
fprintf (outputfile, "State check: firstblkno %d, lastblkno %d, currblkno %d, read %d\n", firstblkno, lastblkno, currblkno, (curr->flags & READ));
*/

   if (endlat == 2) {
      currdisk->stat.latency = seg->time - currdisk->stat.latency;
      currdisk->stat.xfertime = (double) 0.0;
   }
   if (currdisk->stat.xfertime != (double) -1) {
      currdisk->stat.xfertime += simtime - seg->time;
   }

   if (currdiskreq != seg->recyclereq) {
      if (seg->state == BUFFER_READING) {
         seg->endblkno = firstblkno;
      } else {
         currdiskreq->inblkno = firstblkno;
      }
      disk_buffer_segment_wrap(seg, seg->endblkno);

      if (disk_buffer_request_complete(currdisk, currdiskreq)) {
/*
fprintf (outputfile, "Leaving disk_buffer_sector_done\n");
*/
         return;
      }
   }

   if (firstblkno == end_of_track) {
      curr->blkno = end_of_track;
      if (!(disk_initiate_seek(currdisk, seg, curr, 0, 0.0, 0.0))) {
         disk_release_hda(currdisk,currdiskreq);
	 return;
      }
   } else {
      curr->blkno -= curr->cause;
      curr->cause = ((curr->cause+1) == blks_on_track) ? 0 : (curr->cause+1);
      curr->blkno += curr->cause;
      seg->time = simtime;
      curr->time = seg->time + (currdisk->rotatetime / (double) blks_on_track);
      if (!currdisk->translatesectbysect) {
         currcause = (curr->blkno == start_of_track) ? (end_of_track-1) : (curr->blkno-1);
      } else {
         currcause = (curr->cause) ? (curr->cause-1) : (blks_on_track-1);
	 remapsector = FALSE;
         currcause = disk_translate_pbn_to_lbn(currdisk, curr->tempptr1, currdisk->currcylno, currdisk->currsurface, currcause);
	 if (remapsector) {
	    currcause = -1;
	 }
      }
      if ((currdiskreq != seg->recyclereq) && (currcause == -2) && (seg->state == BUFFER_READING) && (currdiskreq->ioreqlist) && (currdisk->readanyfreeblocks) && (currblkno >= 0) && (currblkno == (seg->endblkno-1)) && (seg->endblkno < currdiskreq->ioreqlist->blkno) && (curr->cause > 0)) {
	 seg->startblkno = currdiskreq->ioreqlist->blkno;
	 seg->endblkno = seg->startblkno;
	 currcause = -1;
      }
      if ((currcause == -2) && (currblkno >= 0) && (currblkno == (firstblkno-1))) {
	 curr->blkno--;
	 if (curr->cause == 0) {
	    curr->blkno += blks_on_track;
         }
	 disk_get_remapped_sector(currdisk, curr);
      } else {
         curr->bcount = 1;
         if (((currdisk->read_direct_to_buffer || !(curr->flags & READ)) && 
	      ((currdiskreq == seg->recyclereq) || 
	       !(disk_buffer_block_available(currdisk, seg, currcause)))) || 
	     (currcause < 0)) {
	    curr->bcount = 2;
         }
      }
      addtointq((event *) curr);
   }
/*
fprintf (outputfile, "Leaving disk_buffer_sector_done\n");
*/
}


/* THIS ROUTINE IS UNDOUBTABLY OUT-OF-DATE!!! */

double disk_estimate_acctime(currdisk, curr)
disk *currdisk;
ioreq_event *curr;
{
   int tmpblkno;
   int tmpbcount;
   double runtime = 0.0;
   double starttime;
   int firstontrack;
   int lastontrack;
   int immed;
   int cylno;
   int surfaceno;

   immed = (curr->flags & READ) ? currdisk->immedread : currdisk->immedwrite;
   tmpblkno = curr->blkno;
   tmpbcount = curr->bcount;
   starttime = curr->time;
   while (curr->blkno < (tmpblkno + tmpbcount)) {
      curr->tempptr1 = disk_translate_lbn_to_pbn(currdisk, curr->blkno, MAP_FULL, &cylno, &surfaceno, &curr->cause);
      runtime += diskseektime(currdisk, (cylno - currcylno), (surfaceno - currsurface), (curr->flags & READ));
      currcylno = cylno;
      currsurface = surfaceno;
      disk_get_lbn_boundaries_for_track(currdisk, curr->tempptr1, currcylno, currsurface, &firstontrack, &lastontrack);
      curr->bcount = min((tmpblkno + tmpbcount - curr->blkno), (lastontrack - curr->blkno));
      curr->bcount = (remapsector) ? 1 : curr->bcount;
      curr->time = starttime + runtime;
      runtime += disklatency(currdisk, curr->tempptr1, curr->time, curr->cause, curr->bcount, immed);
      curr->time = starttime + runtime;
      runtime += diskxfertime(currdisk, curr);
      runtime += addtolatency;
      curr->blkno += curr->bcount;
   }
   curr->blkno = tmpblkno;
   curr->bcount = tmpbcount;
   return(runtime);
}


/* THIS ROUTINE IS UNDOUBTABLY OUT-OF-DATE!!! */

double disk_buffer_estimate_acctime(currdisk, curr, maxtime)
disk *currdisk;
ioreq_event *curr;
double maxtime;
{
   double tpass = 0.0;

   /* acctime estimation is not currently supported */
   ASSERT(FALSE);

   return(tpass);
}


/* Possible improvements:  rundelay is not used currently */

double disk_buffer_estimate_servtime(currdisk, curr, checkcache, maxtime)
disk *currdisk;
ioreq_event *curr;
int checkcache;
double maxtime;
{
   double tmptime;
   int tmpblkno;
   int lastontrack;
   int hittype = BUFFER_NOMATCH;

   if (currdisk->acctime >= 0.0) {
      return(currdisk->acctime);
   }

   if (checkcache) {
      int buffer_reading = FALSE;

      hittype = disk_buffer_check_segments(currdisk,curr,&buffer_reading);
      if (hittype == BUFFER_APPEND) {
	 return(0.0);
      } else if (hittype == BUFFER_WHOLE) {
	 return(buffer_reading ? reading_buffer_whole_servtime :
				 buffer_whole_servtime);
      } else if (hittype == BUFFER_PARTIAL) {
	 return(buffer_reading ? reading_buffer_partial_servtime :
				 buffer_partial_servtime);
      }
   }

   tmpblkno = curr->bcount;
   tmptime = curr->time;
   curr->tempptr1 = disk_translate_lbn_to_pbn(currdisk, curr->blkno, MAP_FULL, &currcylno, &currsurface, &curr->cause);
   curr->time = diskacctime(currdisk, curr->tempptr1, DISKSEEKTIME, (curr->flags & READ), curr->time, currcylno, currsurface, curr->cause, curr->bcount, 0);
   if (curr->time < maxtime) {
      curr->time = tmptime;
      disk_get_lbn_boundaries_for_track(currdisk, curr->tempptr1, currcylno, currsurface, NULL, &lastontrack);
      curr->bcount = min(curr->bcount, (lastontrack - curr->blkno));
      curr->tempptr1 = disk_translate_lbn_to_pbn(currdisk, curr->blkno, MAP_FULL, &currcylno, &currsurface, &curr->cause);
      currdisk->immed = (curr->flags & READ) ? currdisk->immedread : currdisk->immedwrite;
      tmptime = diskacctime(currdisk, curr->tempptr1, DISKSERVTIME, (curr->flags & READ), curr->time, currcylno, currsurface, curr->cause, curr->bcount, currdisk->immed);
      curr->bcount = tmpblkno;
      if ((!(curr->flags & READ)) && (tmptime < currdisk->minimum_seek_delay)) {
         tmptime = currdisk->minimum_seek_delay;
      }
   } else {
      tmptime = maxtime + 1.0;
   }
   return(tmptime);
}

