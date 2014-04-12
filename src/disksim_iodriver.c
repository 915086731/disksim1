
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
#include "disksim_stat.h"
#include "disksim_iosim.h"
#include "disksim_iodriver.h"
#include "disksim_orgface.h"
#include "disksim_ioqueue.h"

int numiodrivers = 0;
iodriver *iodrivers = NULL;

int numsysorgs = 0;
struct logorg *sysorgs = NULL;

int drv_printsizestats;
int drv_printlocalitystats;
int drv_printblockingstats;
int drv_printinterferestats;
int drv_printqueuestats;
int drv_printcritstats;
int drv_printidlestats;
int drv_printintarrstats;
int drv_printstreakstats;
int drv_printstampstats;
int drv_printperdiskstats;

statgen emptyqueuestats;
statgen initiatenextstats;

char *statdesc_emptyqueue	=  "Empty queue delay";
char *statdesc_initiatenext	=  "Initiate next delay";

extern FILE *outios;

int totalreqs = 0;
extern void resetstats();

extern int closedios;
extern double closedthinktime;


void iodriver_send_event_down_path(curr)
ioreq_event *curr;
{
   intchar busno;
   intchar slotno;

   busno.value = curr->busno;
   slotno.value = curr->slotno;
   slotno.byte[0] = slotno.byte[0] & 0x0F;
/*
fprintf (outputfile, "iodriver_send_event_down_path: busno %d, slotno %d\n", busno.byte[0], slotno.byte[0]);
*/
   bus_deliver_event(busno.byte[0], slotno.byte[0], curr);
}


double iodriver_raise_priority(iodriverno, opid, devno, blkno, chan)
int iodriverno;
int opid;
int devno;
int blkno;
void *chan;
{
   logorg_raise_priority(sysorgs, numsysorgs, opid, devno, blkno, chan);
   return(0.0);
}


void schedule_disk_access(curriodriver, curr)
iodriver *curriodriver;
ioreq_event *curr;
{
   int queuectlr = curriodriver->devices[(curr->devno)].queuectlr;
   if (queuectlr != -1) {
      curriodriver->ctlrs[queuectlr].numoutstanding++;
   }
   curriodriver->devices[(curr->devno)].busy = TRUE;
}


int check_send_out_request(curriodriver, devno)
iodriver *curriodriver;
int devno;
{
   int numout;

   if ((curriodriver->consttime == IODRIVER_TRACED_QUEUE_TIMES) || (curriodriver->consttime == IODRIVER_TRACED_BOTH_TIMES)) {
      return(FALSE);
   }
   numout = ioqueue_get_reqoutstanding(curriodriver->devices[devno].queue);
   if (curriodriver->usequeue == TRUE) {
      int queuectlr = curriodriver->devices[devno].queuectlr;
      if (queuectlr != -1) {
         numout = curriodriver->ctlrs[queuectlr].numoutstanding;
/*
fprintf (outputfile, "Check send_out_req: queuectlr %d, numout %d, maxout %d, send %d\n", queuectlr, numout, curriodriver->ctlrs[queuectlr].maxoutstanding, (numout < curriodriver->ctlrs[queuectlr].maxoutstanding));
*/
         return(numout < curriodriver->ctlrs[queuectlr].maxoutstanding);
      } else {
         return(numout < curriodriver->devices[devno].maxoutstanding);
      }
   } else {
      return(!numout);
   }
}


ioreq_event * handle_new_request(curriodriver, curr)
iodriver *curriodriver;
ioreq_event *curr;
{
   struct ioq *queue = curriodriver->devices[(curr->devno)].queue;
   ioreq_event *ret = NULL;

   ioqueue_add_new_request(queue, curr);
   if (check_send_out_request(curriodriver, curr->devno)) {
      ret = ioqueue_get_next_request(queue, NULL);
      schedule_disk_access(curriodriver, ret);
      ret->time = IODRIVER_IMMEDSCHED_TIME * curriodriver->scale;
   }
   return(ret);
}


void iodriver_schedule(iodriverno, curr)
int iodriverno;
ioreq_event *curr;
{
   ctlr *ctl;
/*
fprintf (outputfile, "Entered iodriver_schedule - devno %d, blkno %d, bcount %d, read %d\n", curr->devno, curr->blkno, curr->bcount, (curr->flags & READ));
*/
   ASSERT1(curr->type == IO_ACCESS_ARRIVE, "curr->type", curr->type);

   if ((iodrivers[iodriverno].consttime != 0.0) && (iodrivers[iodriverno].consttime != IODRIVER_TRACED_QUEUE_TIMES)) {
      curr->type = IO_INTERRUPT;
      if (iodrivers[iodriverno].consttime > 0.0) {
         curr->time = iodrivers[iodriverno].consttime;
      } else {
         curr->time = ((double) curr->tempint2 / (double) 1000);
      }
      curr->cause = COMPLETION;
      intr_request((event *) curr);
      return;
   }
   ctl = iodrivers[iodriverno].devices[(curr->devno)].ctl;
   if ((ctl) && (ctl->flags & DRIVER_C700)) {
      if ((ctl->pendio) && ((curr->devno != ctl->pendio->next->devno) || (curr->opid != ctl->pendio->next->opid) || (curr->blkno != ctl->pendio->next->blkno))) {
         curr->next = ctl->pendio->next;
         ctl->pendio->next = curr;
         ctl->pendio = curr;
         return;
      } else if (ctl->pendio == NULL) {
         ctl->pendio = ioreq_copy(curr);
         ctl->pendio->next = ctl->pendio;
      }
      if (ctl->flags & DRIVER_CTLR_BUSY) {
         addtoextraq((event *) curr);
         return;
      }
   }
   curr->busno = iodrivers[iodriverno].devices[(curr->devno)].buspath.value;
   curr->slotno = iodrivers[iodriverno].devices[(curr->devno)].slotpath.value;
   if (iodrivers[iodriverno].devices[(curr->devno)].queuectlr != -1) {
      int ctlrno = iodrivers[iodriverno].devices[(curr->devno)].queuectlr;
      ctl = &iodrivers[iodriverno].ctlrs[ctlrno];
      if ((ctl->maxreqsize) && (curr->bcount > ctl->maxreqsize)) {
         ioreq_event *totalreq = ioreq_copy(curr);
/*
fprintf (outputfile, "%f, oversized request: opid %d, blkno %d, bcount %d, maxreqsize %d\n", simtime, curr->opid, curr->blkno, curr->bcount, ctl->maxreqsize);
*/
         curr->bcount = ctl->maxreqsize;
         if (ctl->oversized) {
            totalreq->next = ctl->oversized->next;
            ctl->oversized->next = totalreq;
         } else {
            totalreq->next = totalreq;
            ctl->oversized = totalreq;
         }
      }
   }
   iodriver_send_event_down_path(curr);
/*
fprintf (outputfile, "Leaving iodriver_schedule\n");
*/
}


void update_iodriver_statistics()
{
}


void iodriver_check_c700_based_status(curriodriver, devno, cause, type, blkno)
iodriver *curriodriver;
int devno;
int cause;
int type;
int blkno;
{
   ctlr *ctl;
   ioreq_event *tmp;

   ctl = curriodriver->devices[devno].ctl;
   if ((ctl == NULL) || (!(ctl->flags & DRIVER_C700))) {
      return;
   }
   if (type == IO_INTERRUPT_ARRIVE) {
      if (ctl->pendio != NULL) {
         tmp = ctl->pendio->next;
         if ((tmp->devno == devno) && (tmp->blkno == blkno)) {
            if (ctl->pendio == tmp) {
               ctl->pendio = NULL;
            } else {
               ctl->pendio->next = tmp->next;
            }
            addtoextraq((event *) tmp);
         }
      }
      switch (cause) {
         case COMPLETION:
         case DISCONNECT:
         case RECONNECT:
         case READY_TO_TRANSFER:
                 ctl->flags |= DRIVER_CTLR_BUSY;
                 break;
         default:
                 fprintf(stderr, "Unknown interrupt cause at iodriver_check_c700_based_status - cause %d\n", cause);
                 exit(0);
      }
   } else {
      switch (cause) {
         case COMPLETION:
         case DISCONNECT:
                           ctl->flags &= ~DRIVER_CTLR_BUSY;
                           if (ctl->pendio != NULL) {
                              tmp = ioreq_copy(ctl->pendio->next);
                              tmp->time = simtime;
                              addtointq((event *) tmp);
                           }
                           break;
         case RECONNECT:
         case READY_TO_TRANSFER:
                           ctl->flags |= DRIVER_CTLR_BUSY;
                           break;
         default:
                           fprintf(stderr, "Unknown interrupt cause at iodriver_check_c700_based_status - cause %d\n", cause);
                           exit(0);
      }
   }
}


/* Should only be called for completion interrupts!!!!! */

void iodriver_add_to_intrp_eventlist(intrp, curr, scale)
intr_event *intrp;
event *curr;
double scale;
{
   if (curr) {
      event *tmp = intrp->eventlist;
      int lasttype = curr->type;
      intrp->eventlist = curr;
      if (curr->type == IO_ACCESS_ARRIVE) {
         curr->time = IODRIVER_COMPLETION_RESPTOSCHED_TIME;
      } else if (curr->type == IO_REQUEST_ARRIVE) {
         curr->time = IODRIVER_COMPLETION_RESPTOREQ_TIME;
      } else {       /* Assume wakeup */
         curr->time = IODRIVER_COMPLETION_RESPTOWAKEUP_TIME;
      }
      curr->time *= scale;
      while (curr->next) {
         curr = curr->next;
	 curr->time = 0.0;
      }
      curr->next = tmp;
      if (lasttype != IO_REQUEST_ARRIVE) {
         while (tmp) {
            if (tmp->type == INTEND_EVENT) {
               ASSERT(tmp->next == NULL);
               tmp->time = (lasttype == IO_ACCESS_ARRIVE) ? IODRIVER_COMPLETION_SCHEDTOEND_TIME : IODRIVER_COMPLETION_WAKEUPTOEND_TIME;
               tmp->time *= scale;
            } else if (tmp->type != IO_REQUEST_ARRIVE) {    /* assume wakeup */
               tmp->time = (lasttype == IO_ACCESS_ARRIVE) ? IODRIVER_COMPLETION_SCHEDTOWAKEUP_TIME : 0.0;
               lasttype = tmp->type;
               tmp->time *= scale;
            }
            tmp = tmp->next;
         }
      }
   }
}


void iodriver_access_complete(iodriverno, intrp)
int iodriverno;
intr_event *intrp;
{
   int i;
   int numreqs;
   ioreq_event *tmp;
   ioreq_event *del;
   ioreq_event *req;
   int devno;
   int skip = 0;
   ctlr *ctl = NULL;

   if (iodrivers[iodriverno].type == STANDALONE) {
      req = ioreq_copy((ioreq_event *) intrp->infoptr);
   } else {
      req = (ioreq_event *) intrp->infoptr;
   }
/*
fprintf (outputfile, "Request completing - time %f, devno %d, blkno %d\n", simtime, req->devno, req->blkno);
*/

   if (iodrivers[iodriverno].devices[(req->devno)].queuectlr != -1) {
      int ctlrno = iodrivers[iodriverno].devices[(req->devno)].queuectlr;
      ctl = &iodrivers[iodriverno].ctlrs[ctlrno];
      tmp = ctl->oversized;
      numreqs = 1;
      while (((numreqs) || (tmp != ctl->oversized)) && (tmp) && (tmp->next) && ((tmp->next->devno != req->devno) || (tmp->next->opid != req->opid) || (req->blkno < tmp->next->blkno) || (req->blkno >= (tmp->next->blkno + tmp->next->bcount)))) {
/*
fprintf (outputfile, "oversized request in list: opid %d, blkno %d, bcount %d\n", tmp->opid, tmp->blkno, tmp->bcount);
*/
         numreqs = 0;
         tmp = tmp->next;
      }
      if ((tmp) && (tmp->next->devno == req->devno) && (tmp->next->opid == req->opid) && (req->blkno >= tmp->next->blkno) && (req->blkno < (tmp->next->blkno + tmp->next->bcount))) {
fprintf (outputfile, "%f, part of oversized request completed: opid %d, blkno %d, bcount %d, maxreqsize %d\n", simtime, req->opid, req->blkno, req->bcount, ctl->maxreqsize);
         if ((req->blkno + ctl->maxreqsize) < (tmp->next->blkno + tmp->next->bcount)) {
fprintf (outputfile, "more to go\n");
            req->blkno += ctl->maxreqsize;
            req->bcount = min(ctl->maxreqsize, (tmp->next->blkno + tmp->next->bcount - req->blkno));
            goto schedule_next;
         } else {
fprintf (outputfile, "done for real\n");
            addtoextraq((event *) req);
            req = tmp->next;
            tmp->next = tmp->next->next;
            if (ctl->oversized == req) {
               ctl->oversized = (req != req->next) ? req->next : NULL;
            }
            req->next = NULL;
         }
      }
   }
   devno = req->devno;
   req = ioqueue_physical_access_done(iodrivers[iodriverno].devices[devno].queue, req);
   if (ctl) {
      ctl->numoutstanding--;
   }
   if (traceformat == VALIDATE) {
      extern FILE *iotracefile;
      extern ioreq_event * iotrace_validate_get_ioreq_event();
      extern void io_validate_do_stats1();
      extern void io_validate_do_stats2();

      tmp = (ioreq_event *) getfromextraq();
      io_validate_do_stats1();
      tmp = iotrace_validate_get_ioreq_event(iotracefile, tmp);
      if (tmp) {
         io_validate_do_stats2(tmp);
         tmp->type = IO_REQUEST_ARRIVE;
         addtointq((event *) tmp);
      } else {
         simstop();
      }
   } else if (closedios) {
      extern FILE *iotracefile;
      extern int io_using_external_event();

      tmp = (ioreq_event *) io_get_next_external_event(iotracefile);
      if (tmp) {
         io_using_external_event(tmp);
         tmp->time = simtime + closedthinktime;
         tmp->type = IO_REQUEST_ARRIVE;
         addtointq((event *) tmp);
      } else {
         simstop();
      }
   }
   while (req) {
      tmp = req;
      req = req->next;
      tmp->next = NULL;
      update_iodriver_statistics();
      if ((numreqs = logorg_mapcomplete(sysorgs, numsysorgs, tmp)) == COMPLETE) {
         if (iodrivers[iodriverno].type != STANDALONE) {
            iodriver_add_to_intrp_eventlist(intrp, io_done_notify(tmp), iodrivers[iodriverno].scale);
         } else {
            io_done_notify (tmp);
         }
      } else if (numreqs > 0) {
         for (i=0; i<numreqs; i++) {
            del = tmp->next;
            tmp->next = del->next;
            del->next = NULL;
            del->type = IO_REQUEST_ARRIVE;
            del->flags |= MAPPED;
            skip |= (del->devno == devno);
            if (iodrivers[iodriverno].type == STANDALONE) {
               del->time += simtime + 0.0000000001; /* to affect an ordering */
               addtointq((event *) del);
            } else {
               iodriver_add_to_intrp_eventlist(intrp, (event *) del, iodrivers[iodriverno].scale);
            }
         }
      }
      addtoextraq((event *) tmp);
   }
   if ((iodrivers[iodriverno].consttime == IODRIVER_TRACED_QUEUE_TIMES) || (iodrivers[iodriverno].consttime == IODRIVER_TRACED_BOTH_TIMES)) {
      if (ioqueue_get_number_in_queue(iodrivers[iodriverno].devices[devno].queue) > 0) {
         iodrivers[iodriverno].devices[devno].flag = 1;
         iodrivers[iodriverno].devices[devno].lastevent = simtime;
      }
      return;
   }
   if (skip) {
      return;
   }
   req = ioqueue_get_next_request(iodrivers[iodriverno].devices[devno].queue, NULL);
/*
fprintf (outputfile, "next scheduled: req %p, req->blkno %d, req->flags %x\n", req, ((req) ? req->blkno : 0), ((req) ? req->flags : 0));
*/

schedule_next:
   if (req) {
      req->type = IO_ACCESS_ARRIVE;
      req->next = NULL;
      if (ctl) {
         ctl->numoutstanding++;
      }
      if (iodrivers[iodriverno].type == STANDALONE) {
         req->time = simtime;
         addtointq((event *) req);
      } else {
         iodriver_add_to_intrp_eventlist(intrp, (event *) req, iodrivers[iodriverno].scale);
      }
   }
}


void iodriver_respond_to_device(iodriverno, intrp)
int iodriverno;
intr_event *intrp;
{
   ioreq_event *req = NULL;
   int devno;
   int cause;

   if ((iodrivers[iodriverno].consttime != 0.0) && (iodrivers[iodriverno].consttime != IODRIVER_TRACED_QUEUE_TIMES)) {
      if (iodrivers[iodriverno].type == STANDALONE) {
         addtoextraq((event *) intrp->infoptr);
      }
      return;
   }
   req = (ioreq_event *) intrp->infoptr;
/*
fprintf (outputfile, "%f, Responding to device - cause = %d, blkno %d\n", simtime, req->cause, req->blkno);
*/
   req->type = IO_INTERRUPT_COMPLETE;
   devno = req->devno;
   cause = req->cause;
   switch (cause) {
      case COMPLETION:
                           if (iodrivers[iodriverno].type != STANDALONE) {
                              req = ioreq_copy((ioreq_event *) intrp->infoptr);
                           }
      case DISCONNECT:
      case RECONNECT:
                           iodriver_send_event_down_path(req);
                           break;
      case READY_TO_TRANSFER:
                           addtoextraq((event *) req);
                           break;
      default:
               fprintf(stderr, "Unknown io_interrupt cause - %d\n", req->cause);
               exit(0);
   }
   iodriver_check_c700_based_status(&iodrivers[iodriverno], devno, cause, IO_INTERRUPT_COMPLETE, 0);
}


void iodriver_interrupt_complete(iodriverno, intrp)
int iodriverno;
intr_event *intrp;
{
/*
fprintf (outputfile, "%f, Interrupt completing - cause = %d, blkno %d\n", simtime, ((ioreq_event *) intrp->infoptr)->cause, ((ioreq_event *) intrp->infoptr)->blkno);
*/
   if (iodrivers[iodriverno].type == STANDALONE) {
      if (((ioreq_event *) intrp->infoptr)->cause == COMPLETION) {
         iodriver_access_complete(iodriverno, intrp);
      }
      iodriver_respond_to_device(iodriverno, intrp);
   }
   addtoextraq((event *) intrp);
}


double iodriver_get_time_to_handle_interrupt(curriodriver, cause, read)
iodriver *curriodriver;
int cause;
int read;
{
   double retval = 0.0;
   if (cause == DISCONNECT) {
      retval = (read) ? IODRIVER_READ_DISCONNECT_TIME : IODRIVER_WRITE_DISCONNECT_TIME;
   } else if (cause == RECONNECT) {
      retval = IODRIVER_RECONNECT_TIME;
   } else if (cause == COMPLETION) {
      retval = (IODRIVER_BASE_COMPLETION_TIME - IODRIVER_COMPLETION_RESPTODEV_TIME) * (double) 5;   /* because it gets divided by five later */
   } else if (cause != READY_TO_TRANSFER) {
      fprintf(stderr, "Unknown interrupt at time_to_handle_interrupt: %d\n", cause);
      exit(0);
   }
   return(curriodriver->scale * retval);
}


double iodriver_get_time_to_respond_to_device(curriodriver, cause, fifthoftotal)
iodriver *curriodriver;
int cause;
double fifthoftotal;
{
   double retval;
   if (cause == COMPLETION) {
      retval = IODRIVER_COMPLETION_RESPTODEV_TIME;
   } else {
			/* arbitrarily chosen based on some observations */
      retval = fifthoftotal * (double) 4;
   }
   return(curriodriver->scale * retval);
}


void iodriver_interrupt_arrive(iodriverno, intrp)
int iodriverno;
intr_event *intrp;
{
   event *tmp;
   ioreq_event *infoptr = (ioreq_event *) intrp->infoptr;
/*
fprintf (outputfile, "%f, Interrupt arriving - cause = %d, blkno %d\n", simtime, infoptr->cause, infoptr->blkno);
*/
   if ((iodrivers[iodriverno].consttime == 0.0) || (iodrivers[iodriverno].consttime == IODRIVER_TRACED_QUEUE_TIMES)) {
      intrp->time = iodriver_get_time_to_handle_interrupt(&iodrivers[iodriverno], infoptr->cause, (infoptr->flags & READ));
      iodriver_check_c700_based_status(&iodrivers[iodriverno], infoptr->devno, infoptr->cause, IO_INTERRUPT_ARRIVE, infoptr->blkno);
   } else {
      intrp->time = 0.0;
   }
   if (iodrivers[iodriverno].type == STANDALONE) {
      intrp->time += simtime;
      intrp->type = IO_INTERRUPT_COMPLETE;
      addtointq((event *) intrp);
   } else {
      tmp = getfromextraq();
      intrp->time /= (double) 5;
      tmp->time = intrp->time;
      tmp->type = INTEND_EVENT;
      tmp->next = NULL;
      ((intr_event *)tmp)->vector = IO_INTERRUPT;
      intrp->eventlist = tmp;
      if (infoptr->cause == COMPLETION) {
         tmp = getfromextraq();
         tmp->time = 0.0;
         tmp->type = IO_ACCESS_COMPLETE;
         tmp->next = intrp->eventlist;
         ((ioreq_event *)tmp)->tempptr1 = intrp;
         intrp->eventlist = tmp;
      }
      tmp = getfromextraq();
      tmp->time = iodriver_get_time_to_respond_to_device(&iodrivers[iodriverno], infoptr->cause, intrp->time);
      tmp->type = IO_RESPOND_TO_DEVICE;
      tmp->next = intrp->eventlist;
      ((ioreq_event *)tmp)->tempptr1 = intrp;
      intrp->eventlist = tmp;
   }
}


double iodriver_tick()
{
   int i, j;
   double ret = 0.0;

   for (i = 0; i < numiodrivers; i++) {
      for (j = 0; j < iodrivers[i].numdevices; j++) {
         ret += ioqueue_tick(iodrivers[i].devices[j].queue);
      }
   }
   return(0.0);
}


event * iodriver_request(iodriverno, curr)
int iodriverno;
ioreq_event *curr;
{
   ioreq_event *temp = NULL;
   ioreq_event *ret = NULL;
   ioreq_event *retlist = NULL;
   int numreqs;
   extern int warmup_iocnt;
   extern double warmuptime;
/*
fprintf (outputfile, "Entered iodriver_request - simtime %f, devno %d, blkno %d, cause %d\n", simtime, curr->devno, curr->blkno, curr->cause);
*/
   if (outios) {
      fprintf(outios, "%.6f\t%d\t%d\t%d\t%x\n", curr->time, curr->devno, curr->blkno, curr->bcount, curr->flags);
   }
   totalreqs++;
   if (totalreqs == warmup_iocnt) {
      warmuptime = simtime;
      resetstats();
   }
   numreqs = logorg_maprequest(sysorgs, numsysorgs, curr);
   temp = curr->next;
   for (; numreqs>0; numreqs--) {
          /* Request list size must match numreqs */
      ASSERT(curr != NULL);
      curr->next = NULL;
      if ((iodrivers[iodriverno].consttime == IODRIVER_TRACED_QUEUE_TIMES) || (iodrivers[iodriverno].consttime == IODRIVER_TRACED_BOTH_TIMES)) {
         ret = ioreq_copy(curr);
         ret->time = simtime + (double) ret->tempint1 / (double) 1000;
         ret->type = IO_TRACE_REQUEST_START;
         addtointq((event *) ret);
         ret = NULL;
         if ((curr->slotno == 1) && (ioqueue_get_number_in_queue(iodrivers[iodriverno].devices[(curr->devno)].queue) == 0)) {
            iodrivers[(iodriverno)].devices[(curr->devno)].flag = 2;
            iodrivers[(iodriverno)].devices[(curr->devno)].lastevent = simtime;
         }
      }
      ret = handle_new_request(&iodrivers[iodriverno], curr);

      if ((ret) && (iodrivers[iodriverno].type == STANDALONE) && (ret->time == 0.0)) {
         ret->type = IO_ACCESS_ARRIVE;
         ret->time = simtime;
         iodriver_schedule(iodriverno, ret);
      } else if (ret) {
         ret->type = IO_ACCESS_ARRIVE;
         ret->next = retlist;
         ret->prev = NULL;
         retlist = ret;
      }
      curr = temp;
      temp = (temp) ? temp->next : NULL;
   }
   if (iodrivers[iodriverno].type == STANDALONE) {
      while (retlist) {
         ret = retlist;
         retlist = ret->next;
         ret->next = NULL;
         ret->time += simtime;
         addtointq((event *) ret);
      }
   }
/*
fprintf (outputfile, "leaving iodriver_request: retlist %p\n", retlist);
*/
   return((event *) retlist);
}


void iodriver_trace_request_start(iodriverno, curr)
int iodriverno;
ioreq_event *curr;
{
   ioreq_event *tmp;
   device *currdev = &iodrivers[iodriverno].devices[(curr->devno)];
   double tdiff = simtime - currdev->lastevent;

   if (currdev->flag == 1) {
      stat_update(&initiatenextstats, tdiff);
   } else if (currdev->flag == 2) {
      stat_update(&emptyqueuestats, tdiff);
   }
   currdev->flag = 0;

   tmp = ioqueue_get_specific_request(currdev->queue, curr);
   addtoextraq((event *) curr);
   ASSERT(tmp != NULL);

   schedule_disk_access(&iodrivers[iodriverno], tmp);
   tmp->time = simtime;
   tmp->type = IO_ACCESS_ARRIVE;
   tmp->slotno = 0;
   if (tmp->time == simtime) {
      iodriver_schedule(iodriverno, tmp);
   } else {
      addtointq((event *) tmp);
   }
}


void iodriver_get_path_to_controller(iodriverno, ctl)
int iodriverno;
ctlr * ctl;
{
   int depth;
   int currbus;
   char inslotno;
   char outslotno;
   int master;

   depth = controller_get_depth(ctl->ctlno);
   currbus = controller_get_inbus(ctl->ctlno);
   inslotno = (char) controller_get_slotno(ctl->ctlno);
   master = controller_get_bus_master(currbus);
   while (depth > 0) {
/*
fprintf (outputfile, "ctlno %d, depth %d, currbus %d, inslotno %d, master %d\n", ctl->ctlno, depth, currbus, inslotno, master);
*/
      outslotno = (char) controller_get_outslot(master, currbus);
      ctl->buspath.byte[depth] = (char) currbus;
      ctl->slotpath.byte[depth] = (inslotno & 0x0F) | (outslotno << 4);
      depth--;
      currbus = controller_get_inbus(master);
      inslotno = (char) controller_get_slotno(master);
      master = controller_get_bus_master(currbus);
          /* Bus must have at least one controller */
      ASSERT1((master != -1) || (depth <= 0), "currbus", currbus);
   }
   outslotno = (char) iodriverno;
   ctl->buspath.byte[depth] = currbus;
   ctl->slotpath.byte[depth] = (inslotno & 0x0F) | (outslotno << 4);
}


void iodriver_set_ctl_to_device(iodriverno, dev)
int iodriverno;
device * dev;
{
   int i;
   ctlr *tmp;

   dev->ctl = NULL;
   for (i=0; i < iodrivers[iodriverno].numctlrs; i++) {
      tmp = &iodrivers[iodriverno].ctlrs[i];
      if ((tmp->slotpath.value & dev->slotpath.value) == tmp->slotpath.value) {
         if ((tmp->buspath.value & dev->buspath.value) == tmp->buspath.value) {
            if (tmp->flags & DRIVER_C700) {
               dev->ctl = tmp;
               return;
            } else {
               dev->ctl = tmp;
            }
         }
      }
   }
}


void iodriver_get_path_to_device(iodriverno, dev)
int iodriverno;
device * dev;
{
   int depth;
   int currbus;
   char inslotno;
   char outslotno;
   int master;

   depth = disk_get_depth(dev->devno);
   currbus = disk_get_inbus(dev->devno);
   inslotno = (char) disk_get_slotno(dev->devno);
   master = controller_get_bus_master(currbus);
   while (depth > 0) {
/*
fprintf (outputfile, "devno %d, depth %d, currbus %d, inslotno %d, master %d\n", dev->devno, depth, currbus, inslotno, master);
*/
      outslotno = (char) controller_get_outslot(master, currbus);
      dev->buspath.byte[depth] = (char) currbus;
      dev->slotpath.byte[depth] = (inslotno & 0x0F) | (outslotno << 4);
      depth--;
      currbus = controller_get_inbus(master);
      inslotno = (char) controller_get_slotno(master);
      master = controller_get_bus_master(currbus);
          /* Bus must have at least one controller */
      ASSERT1((master != -1) || (depth <= 0), "currbus", currbus);
   }
   outslotno = (char) iodriverno;
   dev->buspath.byte[depth] = currbus;
   dev->slotpath.byte[depth] = (inslotno & 0x0F) | (outslotno << 4);
}


void get_device_maxoutstanding(curriodriver, dev)
iodriver *curriodriver;
device * dev;
{
   ioreq_event *chk = (ioreq_event *) getfromextraq();

   chk->busno = dev->buspath.value;
   chk->slotno = dev->slotpath.value;
   chk->devno = dev->devno;
   chk->type = IO_QLEN_MAXCHECK;
   iodriver_send_event_down_path(chk);
   dev->queuectlr = chk->tempint1;
   dev->maxoutstanding = (chk->tempint1 == -1) ? chk->tempint2 : -1;
   if (chk->tempint1 != -1) {
      curriodriver->ctlrs[chk->tempint1].maxoutstanding = chk->tempint2;
      curriodriver->ctlrs[chk->tempint1].maxreqsize = chk->bcount;
   }
/*
   fprintf (outputfile, "Maxoutstanding: tempint1 %d, tempint2 %d, bcount %d\n", chk->tempint1, chk->tempint2, chk->bcount);
*/
   addtoextraq((event *) chk);
}


void print_paths_to_devices()
{
   int i,j;

   for (i = 0; i < numiodrivers; i++) {
      fprintf (outputfile, "I/O driver #%d\n", i);
      for (j = 0; j < iodrivers[i].numdevices; j++) {
         fprintf (outputfile, "Device #%d: buspath = %x, slotpath = %x\n", j, iodrivers[i].devices[j].buspath.value, iodrivers[i].devices[j].slotpath.value);
      }
   }
}


void print_paths_to_ctlrs()
{
   int i,j;

   for (i = 0; i < numiodrivers; i++) {
      fprintf (outputfile, "I/O driver #%d\n", i);
      for (j = 0; j < iodrivers[i].numctlrs; j++) {
         fprintf (outputfile, "Controller #%d: buspath = %x, slotpath = %x\n", j, iodrivers[i].ctlrs[j].buspath.value, iodrivers[i].ctlrs[j].slotpath.value);
      }
   }
}


void iodriver_param_override(paramname, paramval, first, last)
char *paramname;
char *paramval;
int first;
int last;
{
   int i;

   if (first == -1) {
      first = 0;
      last = numiodrivers - 1;
   } else if ((first < 0) || (last >= numiodrivers) || (last < first)) {
      fprintf(stderr, "Invalid range at iodriver_param_override: %d - %d\n", first, last);
      exit(0);
   }
   i = first;
   for (i=first; i<=last; i++) {
      if ((strcmp(paramname, "ioqueue_schedalg") == 0) || (strcmp(paramname, "ioqueue_seqscheme") == 0) || (strcmp(paramname, "ioqueue_cylmaptype") == 0) || (strcmp(paramname, "ioqueue_to_time") == 0) || (strcmp(paramname, "ioqueue_timeout_schedalg") == 0) || (strcmp(paramname, "ioqueue_priority_schedalg") == 0) || (strcmp(paramname, "ioqueue_priority_mix") == 0)) {
         ioqueue_param_override(iodrivers[i].queue, paramname, paramval);
      } else if (strcmp(paramname, "usequeue") == 0) {
         if (sscanf(paramval, "%d\n", &iodrivers[i].usequeue) != 1) {
            fprintf(stderr, "Error reading usequeue in iodriver_param_override\n");
            exit(0);
         }
         if ((iodrivers[i].usequeue != TRUE) && (iodrivers[i].usequeue != FALSE)) {
            fprintf(stderr, "Invalid value for usequeue in iodriver_param_override: %d\n", iodrivers[i].usequeue);
            exit(0);
         }
      } else {
         fprintf(stderr, "Unsupported param to override at iodriver_param_override: %s\n", paramname);
         exit(0);
      }
   }
}


void iodriver_sysorg_param_override(paramname, paramval, first, last)
char *paramname;
char *paramval;
int first;
int last;
{
   logorg_param_override(sysorgs, numsysorgs, paramname, paramval, first, last);
}


void iodriver_initialize(standalone)
int standalone;
{
   int numdevs;
   struct ioq * queueset[MAXDISKS];
   int i, j;
   iodriver *curriodriver;

      /* Code will be broken by multiple iodrivers */
   ASSERT1(numiodrivers == 1, "numiodrivers", numiodrivers);

   bus_set_depths();
/*
fprintf (outputfile, "Back from bus_set_depths\n");
*/
   for (i = 0; i < numiodrivers; i++) {
      curriodriver = &iodrivers[i];
      curriodriver->type = standalone;
      if (standalone != STANDALONE) {
         curriodriver->scale = 1.0;
      }
      numdevs = controller_get_numcontrollers();
      curriodriver->numctlrs = numdevs;
      curriodriver->ctlrs = (ctlr*) malloc(numdevs * (sizeof(ctlr)));
      ASSERT(curriodriver->ctlrs != NULL);
      for (j=0; j < numdevs; j++) {
         ctlr *currctlr = &curriodriver->ctlrs[j];
         currctlr->ctlno = j;
         currctlr->flags = 0;
         if (controller_C700_based(j) == TRUE) {
/*
fprintf (outputfile, "This one is c700_based - %d\n", j);
*/
            currctlr->flags |= DRIVER_C700;
         }
         currctlr->buspath.value = 0;
         currctlr->slotpath.value = 0;
         iodriver_get_path_to_controller(i, currctlr);
         currctlr->numoutstanding = 0;
         currctlr->pendio = NULL;
         currctlr->oversized = NULL;
      }

      numdevs = disk_get_numdisks();
      curriodriver->numdevices = numdevs;
      curriodriver->devices = (device*) malloc(numdevs * (sizeof(device)));
      ASSERT(curriodriver->devices != NULL);
      for (j = 0; j < numdevs; j++) {
         device *currdev = &curriodriver->devices[j];
         currdev->devno = j;
         currdev->busy = FALSE;
         currdev->flag = 0;
         currdev->queue = ioqueue_copy(curriodriver->queue);
         ioqueue_initialize(currdev->queue, j);
         queueset[j] = currdev->queue;
         currdev->buspath.value = 0;
         currdev->slotpath.value = 0;
         iodriver_get_path_to_device(i, currdev);
         iodriver_set_ctl_to_device(i, currdev);
         get_device_maxoutstanding(curriodriver, currdev);
      }
      logorg_initialize(sysorgs, numsysorgs, queueset);
/*
fprintf (outputfile, "Back from logorg_initialize\n");
*/
   }
   stat_initialize(statdeffile, statdesc_emptyqueue, &emptyqueuestats);
   stat_initialize(statdeffile, statdesc_initiatenext, &initiatenextstats);
/*
print_paths_to_devices();
print_paths_to_ctlrs();
*/
}


void iodriver_resetstats()
{
   int i, j;
   int numdevs = disk_get_numdisks();

   for (i=0; i<numiodrivers; i++) {
      for (j=0; j<numdevs; j++) {
         ioqueue_resetstats(iodrivers[i].devices[j].queue);
      }
   }
   logorg_resetstats(sysorgs, numsysorgs);
   stat_reset(&emptyqueuestats);
   stat_reset(&initiatenextstats);
}


void iodriver_read_toprints(parfile)
FILE *parfile;
{
   getparam_bool(parfile, "Print driver size stats?", &drv_printsizestats);
   getparam_bool(parfile, "Print driver locality stats?", &drv_printlocalitystats);
   getparam_bool(parfile, "Print driver blocking stats?", &drv_printblockingstats);
   getparam_bool(parfile, "Print driver interference stats?", &drv_printinterferestats);
   getparam_bool(parfile, "Print driver queue stats?", &drv_printqueuestats);
   getparam_bool(parfile, "Print driver crit stats?", &drv_printcritstats);
   getparam_bool(parfile, "Print driver idle stats?", &drv_printidlestats);
   getparam_bool(parfile, "Print driver intarr stats?", &drv_printintarrstats);
   getparam_bool(parfile, "Print driver streak stats?", &drv_printstreakstats);
   getparam_bool(parfile, "Print driver stamp stats?", &drv_printstampstats);
   getparam_bool(parfile, "Print driver per-device stats?", &drv_printperdiskstats);
}


void iodriver_read_specs(parfile)
FILE *parfile;
{
   int specno_expected = 1;
   int iodriverno = 0;
   double consttime;
   int copies;
   int i;

   getparam_int(parfile, "\nNumber of device drivers", &numiodrivers, 1, 0, 0);

   if (numiodrivers == 0) {
      return;
   }

   iodrivers = (iodriver *) malloc(numiodrivers * (sizeof(iodriver)));
   ASSERT(iodrivers != NULL);

   while (iodriverno < numiodrivers) {
      if (fscanf(parfile, "\nDevice Driver Spec #%d\n", &i) != 1) {
         fprintf(stderr, "Error reading device driver spec number\n");
         exit(0);
      }
      if (i != specno_expected) {
	 fprintf(stderr, "Unexpected value for device driver spec number: specno %d, expected %d\n", i, specno_expected);
	 exit(0);
      }
      fprintf (outputfile, "\nDevice Driver Spec #%d\n", i);
      specno_expected++;

      getparam_int(parfile, "# drivers with Spec", &copies, 1, 0, 0);

      if (copies == 0) {
         copies = numiodrivers - iodriverno;
      }

      if ((iodriverno + copies) > numiodrivers) {
         fprintf(stderr, "Too many device driver specifications provided\n");
         exit(0);
      }

      getparam_int(parfile, "Device driver type", &iodrivers[iodriverno].type, 3, STANDALONE, STANDALONE);

      getparam_double(parfile, "Constant access time", &consttime, 0, (double) 0.0, (double) 0.0);
      iodrivers[iodriverno].consttime = consttime;
      if ((consttime < 0.0) && (consttime != IODRIVER_TRACED_ACCESS_TIMES) && (consttime != IODRIVER_TRACED_QUEUE_TIMES) && (consttime != IODRIVER_TRACED_BOTH_TIMES)) {
         fprintf(stderr, "Invalid value for 'Constant access time' - %f\n", consttime);
         exit(0);
      }

      iodrivers[iodriverno].scale = 0.0;

      iodrivers[iodriverno].queue = ioqueue_readparams(parfile, drv_printqueuestats, drv_printcritstats, drv_printidlestats, drv_printintarrstats, drv_printsizestats);

      getparam_int(parfile, "Use queueing in subsystem", &iodrivers[iodriverno].usequeue, 3, 0, 1);

      for (i=(iodriverno+1); i<(iodriverno+copies); i++) {
         iodrivers[i].type = iodrivers[iodriverno].type;
         iodrivers[i].consttime = iodrivers[iodriverno].consttime;
         iodrivers[i].scale = iodrivers[iodriverno].scale;
         iodrivers[i].usequeue = iodrivers[iodriverno].usequeue;
         iodrivers[i].queue = ioqueue_copy(iodrivers[iodriverno].queue);
      }
      iodriverno += copies;
   }
}


void iodriver_read_physorg(parfile)
FILE *parfile;
{
   int i = 0;
   int usedrvnum = 0;
   int j;
   int iodriverno;
   char tmpnums[20];
   int numrets = 0;
   char tmp[20];
   int outbuscnt;
   int outbusadd;
   char outbuses[20];
   char op;
   int val;

   while (i < numiodrivers) {

      if (fscanf(parfile, "\nDriver #%s\n", tmpnums) != 1) {
         fprintf(stderr, "Error reading driver portion of physical organization\n");
         exit(0);
      }
      if ((numrets = sscanf(tmpnums, "%d%s", &iodriverno, tmp)) < 1) {
         fprintf(stderr, "Error parsing driver portion of physical organization\n");
         exit(0);
      } else if (numrets == 1) {
         /* sprintf(tmp, ""); */
         tmp[0] = (char) 0;
      }
      if (iodriverno != (i + 1)) {
         fprintf(stderr, "Driver numbers in physical organization appear out of order\n");
         exit(0);
      }
      fprintf (outputfile, "\nDriver #%d%s\n", iodriverno, tmp);

      j = i + 1;
      if (strcmp(tmp, "") != 0) {
         if (sscanf(tmp, "%c%d", &op, &val) != 2) {
            fprintf(stderr, "Illegal value for 'Driver #' %d in physical organization\n", iodriverno);
            exit(0);
         }
         if ((op != '-') || (val <= iodriverno) || (val > numiodrivers)) {
            fprintf(stderr, "Illegal value for 'Driver #' %d in physical organization\n", iodriverno);
            exit(0);
         }
         j += val - iodriverno;
      }

      getparam_int(parfile, "# of connected buses", &outbuscnt, 3, 1, MAXOUTBUSES);

      if (fscanf(parfile, "Connected buses: %s\n", outbuses) != 1) {
         fprintf(stderr, "Error reading 'Connected buses' for driver %d\n", iodriverno);
         exit(0);
      }
      if ((outbuses[0] != '#') && ((outbuses[0] > '9') || (outbuses[0] < '1'))) {
         fprintf(stderr, "Invalid value for 'Connected buses' for driver %d", iodriverno);
         exit(0);
      }
      fprintf (outputfile, "Connected buses:   %s\n", outbuses);

      usedrvnum = 0;
      if (outbuses[0] == '#') {
         usedrvnum = 1;
         outbusadd = 0;
         if (strcmp(outbuses, "#") != 0) {
            if (sscanf(outbuses, "#%c%d", &op, &val) != 2) {
               fprintf(stderr, "Invalid value for 'Connected buses' for driver %d", iodriverno);
               exit(0);
            }
            if (((op != '+') && (op != '-')) || (val < 0)) {
               fprintf(stderr, "Invalid value for 'Connected buses' for driver %d", iodriverno);
               exit(0);
            }
            if (op == '+') {
               outbusadd = val;
            } else {
               outbusadd = -val;
            }
         }
      } else {
         sscanf(outbuses, "%d", &val);
      }

      for (; i < j; i++) {
         iodrivers[i].numoutbuses = outbuscnt;
         if (usedrvnum) {
            iodrivers[i].outbus = i + outbusadd;
         } else {
            iodrivers[i].outbus = val - 1;
         }
      }
      bus_set_to_zero_depth(iodrivers[(j-1)].outbus);
   }
}


void iodriver_read_logorg(parfile)
FILE *parfile;
{
   getparam_int(parfile, "\n# of system-level organizations", &numsysorgs, 3, 0, MAXLOGORGS);

   if (numsysorgs > 0) {
      sysorgs = logorg_readparams(parfile, numsysorgs, drv_printlocalitystats, drv_printblockingstats, drv_printinterferestats, drv_printstreakstats, drv_printstampstats, drv_printintarrstats, drv_printidlestats, drv_printsizestats);
   } else {
      fprintf (stderr, "Must have at least one system-level organization\n");
      exit (0);
   }
}


void iodriver_printstats()
{
   int i;
   int j;
   struct ioq **queueset;
   int setsize = 0;
   char prefix[80];

   fprintf (outputfile, "\nSYSTEM-LEVEL LOGORG STATISTICS\n");
   fprintf (outputfile, "------------------------------\n\n");
   sprintf(prefix, "System ");
   logorg_printstats(sysorgs, numsysorgs, prefix);

   fprintf (outputfile, "\nIODRIVER STATISTICS\n");
   fprintf (outputfile, "-------------------\n\n");
   for (i = 0; i < numiodrivers; i++) {
      setsize += iodrivers[i].numdevices;
   }
   queueset = (struct ioq **)malloc(setsize*sizeof(int));
   ASSERT(queueset != NULL);
   setsize = 0;
   for (i = 0; i < numiodrivers; i++) {
      for (j = 0; j < iodrivers[i].numdevices; j++) {
         queueset[setsize] = iodrivers[i].devices[j].queue;
         setsize++;
      }
   }
   sprintf(prefix, "IOdriver ");
   if (stat_get_count(&emptyqueuestats) > 0) {
      stat_print(&emptyqueuestats, prefix);
   }
   if (stat_get_count(&initiatenextstats) > 0) {
      stat_print(&initiatenextstats, prefix);
   }
   ioqueue_printstats(queueset, setsize, prefix);
   if ((drv_printperdiskstats == TRUE) && ((numiodrivers > 1) || (iodrivers[0].numdevices > 1))) {
      for (i = 0; i < numiodrivers; i++) {
         for (j = 0; j < iodrivers[i].numdevices; j++) {
            fprintf (outputfile, "\nI/O Driver #%d - Device #%d\n", i, j);
            sprintf(prefix, "IOdriver #%d device #%d ", i, j);
            ioqueue_printstats(&iodrivers[i].devices[j].queue, 1, prefix);
         }
      }
   }
   free(queueset);
}


void iodriver_cleanstats()
{
   int i;
   int j;

   logorg_cleanstats(sysorgs, numsysorgs);
   for (i = 0; i < numiodrivers; i++) {
      for (j = 0; j < iodrivers[i].numdevices; j++) {
         ioqueue_cleanstats(iodrivers[i].devices[j].queue);
      }
   }
}

