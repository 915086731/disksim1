
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
#include "disksim_controller.h"
#include "disksim_orgface.h"
#include "disksim_ioqueue.h"
#include "disksim_cache.h"

extern int ctl_printcachestats;
extern int ctl_printsizestats;
extern int ctl_printlocalitystats;
extern int ctl_printblockingstats;
extern int ctl_printinterferestats;
extern int ctl_printqueuestats;
extern int ctl_printcritstats;
extern int ctl_printidlestats;
extern int ctl_printintarrstats;
extern int ctl_printstreakstats;
extern int ctl_printstampstats;
extern int ctl_printperdiskstats;


struct ioq * controller_smart_queuefind(currctlr, devno)
controller *currctlr;
int devno;
{
   ASSERT1((devno >= 0) && (devno < disk_get_numdisks()), "devno", devno);
   return(currctlr->devices[devno].queue);
}


void controller_smart_wakeup(currctlr, cacheevent)
controller *currctlr;
struct cacheevent *cacheevent;
{
   if (cacheevent) {
      cache_wakeup_complete(currctlr->cache, cacheevent);
   }
}


void controller_smart_issue_access(currctlr, curr)
controller *currctlr;
ioreq_event *curr;
{
   struct ioq *queue = currctlr->devices[curr->devno].queue;
   int numout = ioqueue_get_reqoutstanding(queue);

   ioqueue_add_new_request(queue, curr);
   if (numout < currctlr->devices[curr->devno].maxoutstanding) {
      ioreq_event *sched = ioqueue_get_next_request(queue, NULL);
      controller_send_event_down_path(currctlr, sched, currctlr->ovrhd_disk_request);
   }
}


void controller_smart_disk_data_transfer(currctlr, curr)
controller *currctlr;
ioreq_event *curr;
{
   ioreq_event *tmp = (ioreq_event *) getfromextraq();
/*
fprintf (outputfile, "%f: controller_smart_disk_data_transfer: devno %d, bcount %d\n", simtime, curr->devno, curr->bcount);
*/
   curr->time = max(disk_get_blktranstime(curr), currctlr->blktranstime);
   tmp->time = simtime + ((double) curr->bcount * curr->time);
   tmp->type = CONTROLLER_DATA_TRANSFER_COMPLETE;
   tmp->devno = curr->devno;
   tmp->blkno = curr->blkno;
   tmp->bcount = curr->bcount;
   tmp->tempint2 = currctlr->ctlno;
   tmp->tempptr1 = curr;

   /* want to use the buf value for something else! */
   curr->buf = tmp;
   curr->next = currctlr->datatransfers;
   curr->prev = NULL;
   if (curr->next) {
      curr->next->prev = curr;
   }
   currctlr->datatransfers = curr;

   addtointq((event *) tmp);
}


void controller_smart_disk_data_transfer_complete(currctlr, curr)
controller *currctlr;
ioreq_event *curr;
{
   ioreq_event *tmp = (ioreq_event *) curr->tempptr1;

   tmp->bcount -= curr->bcount;
   addtoextraq((event *) curr);
   ASSERT(tmp->bcount >= 0);
   if (tmp->bcount == 0) {
      if (tmp->next) {
         tmp->next->prev = tmp->prev;
      }
      if (tmp->prev) {
         tmp->prev->next = tmp->next;
      } else {
         currctlr->datatransfers = tmp->next;
      }
      tmp->time = simtime;
      addtointq((event *) tmp);
   } else {
      fprintf(stderr, "Haven't required less than all out transfer at controller_smart_disk_data_transfer_complete\n");
      exit(0);
   }
}


void controller_smart_request_complete(currctlr, curr)
controller *currctlr;
ioreq_event *curr;
{
/*
fprintf (outputfile, "Request completed at smart controller: devno %d, blkno %d\n", curr->devno, curr->blkno);
*/

   curr->type = IO_INTERRUPT_ARRIVE;
   curr->cause = COMPLETION;
   controller_send_event_up_path(currctlr, curr, currctlr->ovrhd_complete);
}


void controller_smart_host_data_transfer_complete(currctlr, curr)
controller *currctlr;
ioreq_event *curr;
{
   /* DMA to/from host complete */

   if (curr->flags & READ) {
      cache_free_block_clean(currctlr->cache, curr);
      controller_smart_request_complete(currctlr, curr);
   } else {
      /* cache will call "done" function if request doesn't block */
      cache_free_block_dirty(currctlr->cache, curr, controller_smart_request_complete, currctlr);
   }
   if (currctlr->hostwaiters) {
      curr = currctlr->hostwaiters->next;
      if (curr->next == curr) {
	 currctlr->hostwaiters = NULL;
      } else {
	 currctlr->hostwaiters->next = curr->next;
      }
      curr->next = NULL;
                                       /* Time for DMA */
      curr->time = simtime + ((double) curr->bcount * currctlr->blktranstime);
      addtointq((event *) curr);
   } else {
      currctlr->hosttransfer = FALSE;
   }
}


void controller_smart_host_data_transfer(currctlr, curr)
controller *currctlr;
ioreq_event *curr;
{
   /* DMA data to/from host */

   curr->type = CONTROLLER_DATA_TRANSFER_COMPLETE;
   curr->tempint2 = currctlr->ctlno;
   curr->tempptr1 = NULL;

   if (currctlr->hosttransfer) {
      if (currctlr->hostwaiters) {
         curr->next = currctlr->hostwaiters->next;
         currctlr->hostwaiters->next = curr;
      } else {
         curr->next = curr;
      }
      currctlr->hostwaiters = curr;
      return;
   }
                                       /* Time for DMA */
   curr->time = simtime + ((double) curr->bcount * currctlr->blktranstime);
   currctlr->hosttransfer = TRUE;
   addtointq((event *) curr);
}


void controller_smart_request_arrive(currctlr, curr)
controller *currctlr;
ioreq_event *curr;
{
/*
fprintf (outputfile, "Request arrived at smart controller: devno %d, blkno %d, flags %x\n", curr->devno, curr->blkno, curr->flags);
*/

   /* Cache will call "done" function if cache access doesn't block */

   cache_get_block(currctlr->cache, curr, controller_smart_host_data_transfer, currctlr);
}


void controller_smart_access_complete(currctlr, curr)
controller *currctlr;
ioreq_event *curr;
{
   struct ioq *queue = currctlr->devices[curr->devno].queue;
   ioreq_event *done = ioreq_copy(curr);
   int devno = curr->devno;
   int numout;

   /* Responds to completion interrupt */

   done->type = IO_INTERRUPT_COMPLETE;
   currctlr->outbusowned = controller_get_downward_busno(currctlr, done, NULL);
   controller_send_event_down_path(currctlr, done, currctlr->ovrhd_disk_complete);
   currctlr->outbusowned = -1;

   /* Handles request completion, including call-backs into cache */

   curr = ioqueue_physical_access_done(queue, curr);
   while ((done = curr)) {
      curr = curr->next;
      /* call back into cache with completion -- let it do request_complete */
      controller_smart_wakeup(currctlr, cache_disk_access_complete(currctlr->cache, done));
   }

   /* Initiate another request, if any pending */

   numout = ioqueue_get_reqoutstanding(queue);
   if ((numout < currctlr->devices[devno].maxoutstanding) && (curr = ioqueue_get_next_request(queue, NULL))) {
      controller_send_event_down_path(currctlr, curr, currctlr->ovrhd_disk_request);
   }
}


void controller_smart_ready_to_transfer(currctlr, curr)
controller *currctlr;
ioreq_event *curr;
{
   curr->type = IO_INTERRUPT_COMPLETE;
   curr->cause = RECONNECT;
   currctlr->outbusowned = controller_get_downward_busno(currctlr, curr, NULL);
   controller_send_event_down_path(currctlr, curr, currctlr->ovrhd_disk_ready);
   currctlr->outbusowned = -1;
}


void controller_smart_disconnect(currctlr, curr)
controller *currctlr;
ioreq_event *curr;
{
   curr->type = IO_INTERRUPT_COMPLETE;
   currctlr->outbusowned = controller_get_downward_busno(currctlr, curr, NULL);
   controller_send_event_down_path(currctlr, curr, currctlr->ovrhd_disk_disconnect);
   currctlr->outbusowned = -1;
}


void controller_smart_reconnect_to_transfer(currctlr, curr)
controller *currctlr;
ioreq_event *curr;
{
   curr->type = IO_INTERRUPT_COMPLETE;
   currctlr->outbusowned = controller_get_downward_busno(currctlr, curr, NULL);
   controller_send_event_down_path(currctlr, curr, currctlr->ovrhd_disk_reconnect);
   currctlr->outbusowned = -1;
}


void controller_smart_interrupt_arrive(currctlr, curr)
controller *currctlr;
ioreq_event *curr;
{
   switch (curr->cause) {

      case COMPLETION:
         controller_smart_access_complete(currctlr, curr);
         break;

      case RECONNECT:
         controller_smart_reconnect_to_transfer(currctlr, curr);
         break;

      case DISCONNECT:
         controller_smart_disconnect(currctlr, curr);
         break;

      case READY_TO_TRANSFER:
         controller_smart_ready_to_transfer(currctlr, curr);
         break;

      default:
         fprintf(stderr, "Unknown interrupt cause in smart_interrupt_arrive: %d\n", curr->cause);
         exit(0);
   }
}


void controller_smart_interrupt_complete(currctlr, curr)
controller *currctlr;
ioreq_event *curr;
{
   switch (curr->cause) {

       case COMPLETION:
          addtoextraq((event *) curr);
          break;

      default:
         fprintf(stderr, "Unknown interrupt cause in smart_interrupt_complete: %d\n", curr->cause);
         exit(0);
   }
}


void controller_smart_event_arrive(currctlr, curr)
controller *currctlr;
ioreq_event *curr;
{
   switch (curr->type) {

      case IO_ACCESS_ARRIVE:
         controller_smart_request_arrive(currctlr, curr);
         break;

      case IO_INTERRUPT_ARRIVE:
         controller_smart_interrupt_arrive(currctlr, curr);
         break;

      case IO_INTERRUPT_COMPLETE:
         controller_smart_interrupt_complete(currctlr, curr);
         break;

      case DISK_DATA_TRANSFER_COMPLETE:
         controller_smart_disk_data_transfer(currctlr, curr);
         break;

      case CONTROLLER_DATA_TRANSFER_COMPLETE:
         if (curr->tempptr1) {
            controller_smart_disk_data_transfer_complete(currctlr, curr);
         } else {
            controller_smart_host_data_transfer_complete(currctlr, curr);
         }
         break;

      case IO_QLEN_MAXCHECK:
         curr->tempint1 = currctlr->ctlno;
         curr->tempint2 = currctlr->maxoutstanding;
         curr->bcount = cache_get_maxreqsize(currctlr->cache);
         break;

      default:
         fprintf(stderr, "Unknown event type arriving at 53c700 controller: %d\n", curr->type);
         exit(0);
   }
}


void controller_smart_initialize(currctlr)
controller *currctlr;
{
   int numdevs = disk_get_numdisks();
   int i;

   currctlr->numdevices = numdevs;
   currctlr->devices = (device *) malloc(numdevs * sizeof(device));
   ASSERT(currctlr->devices != NULL);
   for (i=0; i<numdevs; i++) {
      currctlr->devices[i].devno = i;
      currctlr->devices[i].busy = FALSE;
      currctlr->devices[i].flag = 0;
      currctlr->devices[i].queue = ioqueue_copy(currctlr->queue);
      ioqueue_initialize(currctlr->devices[i].queue, i);
      currctlr->devices[i].maxoutstanding = currctlr->maxdiskqsize;
   }
   cache_initialize(currctlr->cache, controller_smart_issue_access, currctlr, controller_smart_queuefind, currctlr, controller_smart_wakeup, currctlr, numdevs);
}


void controller_smart_resetstats(currctlr)
controller *currctlr;
{
   int numdevs = disk_get_numdisks();
   int i;

   for (i=0; i<numdevs; i++) {
      ioqueue_resetstats(currctlr->devices[i].queue);
   }
   cache_resetstats(currctlr->cache);
}


void controller_smart_read_specs(parfile, controllers, start, copies)
FILE *parfile;
controller *controllers;
int start;
int copies;
{
    int maxdiskqsize;
    int i;

    controllers[start].queue = ioqueue_readparams(parfile, ctl_printqueuestats, ctl_printcritstats, ctl_printidlestats, ctl_printintarrstats, ctl_printsizestats);
    controllers[start].cache = cache_readparams(parfile);

    getparam_int(parfile, "Max per-disk pending count", &maxdiskqsize, 1, 0, 0);
    controllers[start].maxdiskqsize = maxdiskqsize;

    for (i=(start+1); i<(start+copies); i++) {
       controllers[i].queue = ioqueue_copy(controllers[start].queue);
       controllers[i].cache = cache_copy(controllers[start].cache);
       controllers[i].maxdiskqsize = maxdiskqsize;
    }
}


void controller_smart_printstats(currctlr, prefix)
controller *currctlr;
char *prefix;
{
   int devno;
   char devprefix[181];
   struct ioq **queueset = malloc (currctlr->numdevices * sizeof(void *));

   if (ctl_printcachestats) {
      cache_printstats(currctlr->cache, prefix);
   }

   for (devno=0; devno<currctlr->numdevices; devno++) {
      queueset[devno] = currctlr->devices[devno].queue;
   }
   sprintf (devprefix, "%sdevices ", prefix);
   ioqueue_printstats (queueset, currctlr->numdevices, devprefix);

   if ((ctl_printperdiskstats) && (currctlr->numdevices > 1)) {
      for (devno=0; devno<currctlr->numdevices; devno++) {
         struct ioq *queue = currctlr->devices[devno].queue;
         if (ioqueue_get_number_of_requests(queue)) {
            sprintf(devprefix, "%sdevice #%d ", prefix, devno);
            ioqueue_printstats(&queue, 1, devprefix);
         }
      }
   }
}

