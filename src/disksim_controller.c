
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
#include "disksim_bus.h"

int numctlorgs = 0;
struct logorg *ctlorgs = NULL;

int numcontrollers = 0;
controller *controllers = NULL;

int ctl_printcachestats;
int ctl_printsizestats;
int ctl_printlocalitystats;
int ctl_printblockingstats;
int ctl_printinterferestats;
int ctl_printqueuestats;
int ctl_printcritstats;
int ctl_printidlestats;
int ctl_printintarrstats;
int ctl_printstreakstats;
int ctl_printstampstats;
int ctl_printperdiskstats;


/* Currently, controllers can not communicate via ownership-type buses */


int controller_C700_based(ctlno)
int ctlno;
{
   if (controllers[ctlno].type == CTLR_BASED_ON_53C700) {
      return(TRUE);
   }
   return(FALSE);
}


int controller_get_downward_busno(currctlr, curr, slotnoptr)
controller *currctlr;
ioreq_event *curr;
int *slotnoptr;
{
   intchar busno;
   intchar slotno;
   int depth;

   busno.value = curr->busno;
   slotno.value = curr->slotno;
   depth = currctlr->depth[0];
   ASSERT1(depth != MAXDEPTH, "depth", depth);
   depth++;
   if (slotnoptr != NULL) {
      *slotnoptr = slotno.byte[depth] & 0x0F;
   }
   return(busno.byte[depth]);
}


int controller_get_upward_busno(currctlr, curr, slotnoptr)
controller *currctlr;
ioreq_event *curr;
int *slotnoptr;
{
   intchar busno;
   intchar slotno;
   int depth;

   busno.value = curr->busno;
   slotno.value = curr->slotno;
   depth = currctlr->depth[0];
   if (slotnoptr != NULL) {
      *slotnoptr = slotno.byte[depth] >> 4;
   }
   if (depth != 0) {
      return(busno.byte[depth]);
   } else {
      return(-1);
   }
}


void controller_send_event_down_path(currctlr, curr, delay)
controller *currctlr;
ioreq_event *curr;
double delay;
{
   int busno;
   int slotno;

   busno = controller_get_downward_busno(currctlr, curr, NULL);
   slotno = controller_get_outslot(currctlr->ctlno, busno);
   curr->next = currctlr->buswait;
   currctlr->buswait = curr;
   curr->tempint1 = busno;
/*
fprintf (outputfile, "Inside send_event_down_path - ctlno %d, busno %d, slotno %d, blkno %d, addr %p\n", currctlr->ctlno, busno, slotno, curr->blkno, curr);
*/
   curr->time = delay * currctlr->timescale;
   if (currctlr->outbusowned == -1) {
      if (bus_ownership_get(busno, slotno, curr) == FALSE) {

/*
fprintf (outputfile, "Must get ownership of bus\n");
*/
      } else {
         bus_delay(busno, CONTROLLER, currctlr->ctlno, curr->time, curr);
         curr->time = (double) -1;
      }
   } else if (currctlr->outbusowned == busno) {
      bus_delay(busno, CONTROLLER, currctlr->ctlno, curr->time, curr);
      curr->time = (double) -1.0;
   } else {
      fprintf(stderr, "Wrong bus owned for transfer desired\n");
      exit(0);
   }
}


void controller_send_event_up_path(currctlr, curr, delay)
controller *currctlr;
ioreq_event *curr;
double delay;
{
   int busno;
   int slotno;

   busno = controller_get_upward_busno(currctlr, curr, NULL);
   slotno = currctlr->slotno[0];
/*
fprintf (outputfile, "Inside send_event_down_path - ctlno %d, busno %d, slotno %d, blkno %d, addr %x\n", currctlr->ctlno, busno, slotno, curr->blkno, curr);
*/
   curr->next = currctlr->buswait;
   currctlr->buswait = curr;
   curr->tempint1 = busno;
   curr->time = delay * currctlr->timescale;
   if (busno == -1) {
      bus_delay(busno, CONTROLLER, currctlr->ctlno, curr->time, curr);
      curr->time = (double) -1.0;
   } else if (currctlr->inbusowned == -1) {
      if (bus_ownership_get(busno, slotno, curr) == FALSE) {
      } else {
         bus_delay(busno, CONTROLLER, currctlr->ctlno, curr->time, curr);
         curr->time = (double) -1.0;
      }
   } else if (currctlr->inbusowned == busno) {
      bus_delay(busno, CONTROLLER, currctlr->ctlno, curr->time, curr);
      curr->time = (double) -1.0;
   } else {
      fprintf(stderr, "Wrong bus owned for transfer desired\n");
      exit(0);
   }
}


int controller_get_data_transfered(ctlno, devno)
int ctlno;
int devno;
{
   double tmptime;
   int tmpblks;
   controller *currctlr = &controllers[ctlno];
   ioreq_event *tmp = currctlr->datatransfers;
/*
fprintf (outputfile, "Entered controller_get_data_transfered - ctlno %d, devno %d\n", currctlr->ctlno, devno);
*/
   while (tmp) {
      if (tmp->devno == devno) {
         tmptime = ((ioreq_event *) tmp->buf)->time;
         tmptime = (tmptime - simtime) / tmp->time;
         tmpblks = ((ioreq_event *) tmp->buf)->blkno - tmp->blkno;
         tmpblks += ((ioreq_event *) tmp->buf)->bcount;
         return((int) ((double) tmpblks - tmptime));
      }
      tmp = tmp->next;
   }
   return(-1);
}


void controller_disown_busno(currctlr, busno)
controller *currctlr;
int busno;
{
   ASSERT1((busno == currctlr->inbusowned) || (busno == currctlr->outbusowned), "busno", busno);
   if (currctlr->inbusowned == busno) {
      bus_ownership_release(currctlr->inbusowned);
      currctlr->inbusowned = -1;
   } else {
      currctlr->outbusowned = -1;
   }
}


void controller_bus_ownership_grant(ctlno, curr, busno, arbdelay)
int ctlno;
ioreq_event *curr;
int busno;
double arbdelay;
{
   controller *currctlr = &controllers[ctlno];
   ioreq_event *tmp;
   ASSERT1((ctlno >= 0) && (ctlno < numcontrollers), "ctlno", ctlno);
/*
fprintf (outputfile, "Ownership granted for bus %d - ctlno %d\n", busno, ctlno);
*/
   tmp = currctlr->buswait;
   while ((tmp) && (tmp != curr)) {
      tmp = tmp->next;
   }
       /* Bus ownership granted to unknown controller request */
   ASSERT(tmp != NULL);
   switch (tmp->type) {
      case IO_ACCESS_ARRIVE:
      case IO_INTERRUPT_COMPLETE:
                                  currctlr->outbusowned = busno;
                                  break;
      case IO_INTERRUPT_ARRIVE:
                                  currctlr->inbusowned = busno;
                                  break;
      default:
               fprintf(stderr, "Unknown event type at controller_bus_ownership_grant - %d\n", tmp->type);
               exit(0);
   }
   currctlr->waitingforbus += arbdelay;
   bus_delay(busno, CONTROLLER, currctlr->ctlno, tmp->time, tmp);
   tmp->time = (double) -1.0;
   controller_disown_busno(&controllers[ctlno], busno);
}


void controller_remove_type_from_buswait(currctlr, type)
controller *currctlr;
int type;
{
   ioreq_event *tmp;
   ioreq_event *trv;
   int busno;

   ASSERT(currctlr->buswait != NULL);
   if (currctlr->buswait->type == type) {
      tmp = currctlr->buswait;
      currctlr->buswait = tmp->next;
   } else {
      trv = currctlr->buswait;
      while ((trv->next) && (trv->next->type != type)) {
         trv = trv->next;
      }
      ASSERT1(trv->next != NULL, "type", type);
      tmp = trv->next;
      trv->next = tmp->next;
   }
   if (tmp->time == -1.0) {
      fprintf(stderr, "In remove_type_from_buswait, the event is in transit\n");
      exit(0);
   } else {
      busno = controller_get_downward_busno(currctlr, tmp, NULL);
      bus_remove_from_arbitration(busno, tmp);
      addtoextraq((event *) tmp);
   }
}


void controller_bus_delay_complete(ctlno, curr, busno)
int ctlno;
ioreq_event *curr;
int busno;
{
   controller *currctlr = &controllers[ctlno];
   ioreq_event *trv;
   int slotno;
   int buswaitdir;

   if (curr == currctlr->buswait) {
      currctlr->buswait = curr->next;
   } else {
      trv = currctlr->buswait;
      while ((trv->next != NULL) && (trv->next != curr)) {
         trv = trv->next;
      }
      ASSERT(trv->next == curr);
      trv->next = curr->next;
   }
/*
fprintf (outputfile, "Controller_bus_delay_complete - ctlno %d, busno %d, blkno %d, type %d, cause %d\n", currctlr->ctlno, curr->tempint1, curr->blkno, curr->type, curr->cause);
*/
   curr->next = NULL;
   curr->time = 0.0;
   if (busno == controller_get_upward_busno(currctlr, curr, &slotno)) {
      buswaitdir = UP;
      if (busno == -1) {
         intr_request(curr);
      } else {
         bus_deliver_event(busno, slotno, curr);
      }
      if (controllers[ctlno].inbusowned != -1) {
         bus_ownership_release(controllers[ctlno].inbusowned);
         controllers[ctlno].inbusowned = -1;
      }
   } else if (busno == controller_get_downward_busno(currctlr, curr, &slotno)) {
      buswaitdir = DOWN;
      bus_deliver_event(busno, slotno, curr);
      if (controllers[ctlno].outbusowned != -1) {
         controllers[ctlno].outbusowned = -1;
      }
   } else {
      fprintf(stderr, "Non-matching busno for request in controller_bus_delay_complete: busno %d\n", busno);
      exit(0);
   }
}


void controller_event_arrive(ctlno, curr)
int ctlno;
ioreq_event *curr;
{
   ASSERT1((ctlno >= 0) && (ctlno < numcontrollers), "ctlno", ctlno);
   ASSERT(curr != NULL);

/*
fprintf (outputfile, "Inside controller_event_arrive: ctlno = %d, type = %d, cause = %d\n", ctlno, curr->type, curr->cause);
fprintf (outputfile, "Info continued: devno %d, blkno %d\n", curr->devno, curr->blkno);
*/

   switch(controllers[ctlno].type) {
      
      case CTLR_PASSTHRU:
               controller_passthru_event_arrive(&controllers[ctlno], curr);
               break;
      
      case CTLR_BASED_ON_53C700:
               controller_53c700_event_arrive(&controllers[ctlno], curr);
               break;
      
      case CTLR_SMART:
               controller_smart_event_arrive(&controllers[ctlno], curr);
               break;

      default:
               fprintf(stderr, "Unknown controller type in controller_interrupt_arrive: %d\n", controllers[ctlno].type);
               exit(0);
   }
}


void controller_read_toprints(parfile)
FILE *parfile;
{
   getparam_bool(parfile, "Print controller cache stats?", &ctl_printcachestats);
   getparam_bool(parfile, "Print controller size stats?", &ctl_printsizestats);
   getparam_bool(parfile, "Print controller locality stats?", &ctl_printlocalitystats);
   getparam_bool(parfile, "Print controller blocking stats?", &ctl_printblockingstats);
   getparam_bool(parfile, "Print controller interference stats?", &ctl_printinterferestats);
   getparam_bool(parfile, "Print controller queue stats?", &ctl_printqueuestats);
   getparam_bool(parfile, "Print controller crit stats?", &ctl_printcritstats);
   getparam_bool(parfile, "Print controller idle stats?", &ctl_printidlestats);
   getparam_bool(parfile, "Print controller intarr stats?", &ctl_printintarrstats);
   getparam_bool(parfile, "Print controller streak stats?", &ctl_printstreakstats);
   getparam_bool(parfile, "Print controller stamp stats?", &ctl_printstampstats);
   getparam_bool(parfile, "Print controller per-device stats?", &ctl_printperdiskstats);
}


void controller_read_specs(parfile)
FILE *parfile;
{
   int specno_expected = 1;
   int specno;
   int ctlno = 0;
   int copies;
   int maxqlen;
   int i;

   getparam_int(parfile, "\nNumber of controllers", &numcontrollers, 1, 0, 0);

   if (numcontrollers == 0) {
      return;
   }

   controllers = (controller *) malloc(numcontrollers * (sizeof(controller)));
   ASSERT(controllers != NULL);

   while (ctlno < numcontrollers) {
      if (fscanf(parfile, "\nController Spec #%d\n", &specno) != 1) {
         fprintf(stderr, "Error reading controller spec number\n");
         exit(0);
      }
      if (specno != specno_expected) {
         fprintf(stderr, "Unexpected value for controller spec number: specno %d, expected %d\n", specno, specno_expected);
         exit(0);
      }
      fprintf (outputfile, "\nController Spec #%d\n", specno);
      specno_expected++;

      getparam_int(parfile, "# controllers with Spec", &copies, 1, 0, 0);

      if (copies == 0) {
         copies = numcontrollers - ctlno;
      }

      if ((ctlno + copies) > numcontrollers) {
         fprintf(stderr, "Too many controller specifications provided\n");
         exit(0);
      }

      getparam_int(parfile, "Controller Type", &controllers[ctlno].type, 3, MINCTLTYPE, MAXCTLTYPE);
      getparam_double(parfile, "Scale for delays", &controllers[ctlno].timescale, 1, (double) 0.0, (double) 0.0);
      getparam_double(parfile, "Bulk sector transfer time", &controllers[ctlno].blktranstime, 1, (double) 0.0, (double) 0.0);

	/* Overhead for propogating request to lower-level component */
      controllers[ctlno].ovrhd_disk_request = 0.5;
	/* Overhead for propogating ready-to-transfer to upper-level component */
      controllers[ctlno].ovrhd_ready = 0.5;
	/* Overhead for propogating ready-to-transfer to lower-level component */
      controllers[ctlno].ovrhd_disk_ready = 0.5;
	/* Overhead for propogating disconnect to upper-level component */
      controllers[ctlno].ovrhd_disconnect = 0.5;
	/* Overhead for propogating disconnect to lower-level component */
      controllers[ctlno].ovrhd_disk_disconnect = 0.5;
	/* Overhead for propogating reconnect to upper-level component */
      controllers[ctlno].ovrhd_reconnect = 0.5;
	/* Overhead for propogating reconnect to lower-level component */
      controllers[ctlno].ovrhd_disk_reconnect = 0.5;
	/* Overhead for propogating complete to upper-level component */
      controllers[ctlno].ovrhd_complete = 0.5;
	/* Overhead for propogating complete to lower-level component */
      controllers[ctlno].ovrhd_disk_complete = 0.5;
	/* Overhead for propogating disconnect/complete-complete to upper-level component */
      controllers[ctlno].ovrhd_reset = 0.5;

      getparam_int(parfile, "Maximum queue length", &maxqlen, 1, 0, 0);
      controllers[ctlno].maxoutstanding = maxqlen + 1;
      getparam_int(parfile, "Print stats for controller", &controllers[ctlno].printstats, 3, 0, 1);

      for (i=ctlno; i<(ctlno+copies); i++) {
	 controllers[i].type = controllers[ctlno].type;
	 controllers[i].timescale = controllers[ctlno].timescale;
	 controllers[i].blktranstime = controllers[ctlno].blktranstime;
         controllers[i].ovrhd_disk_request = controllers[ctlno].ovrhd_disk_request;
         controllers[i].ovrhd_ready = controllers[ctlno].ovrhd_ready;
         controllers[i].ovrhd_disk_ready = controllers[ctlno].ovrhd_disk_ready;
         controllers[i].ovrhd_disconnect = controllers[ctlno].ovrhd_disconnect;
         controllers[i].ovrhd_disk_disconnect = controllers[ctlno].ovrhd_disk_disconnect;
         controllers[i].ovrhd_reconnect = controllers[ctlno].ovrhd_reconnect;
         controllers[i].ovrhd_disk_reconnect = controllers[ctlno].ovrhd_disk_reconnect;
         controllers[i].ovrhd_complete = controllers[ctlno].ovrhd_complete;
         controllers[i].ovrhd_disk_complete = controllers[ctlno].ovrhd_disk_complete;
         controllers[i].ovrhd_reset = controllers[ctlno].ovrhd_reset;
	 controllers[i].maxoutstanding = controllers[ctlno].maxoutstanding;
	 controllers[i].printstats = controllers[ctlno].printstats;
	 controllers[i].buswait = NULL;
	 controllers[i].connections = NULL;
	 controllers[i].datatransfers = NULL;
	 controllers[i].hostwaiters = NULL;
	 controllers[i].numinbuses = 0;
      }
      if (controllers[ctlno].type == CTLR_SMART) {
	 controller_smart_read_specs(parfile, controllers, ctlno, copies);
      }
      ctlno += copies;
   }
}


void controller_read_physorg(parfile)
FILE *parfile;
{
   int i = 0;
   int usectlnum = 0;
   int j;
   int ctlno;
   char tmp[20];
   char tmpvals[20];
   int numrets = 0;
   int outbuscnt;
   int outbusadd;
   char outbuses[20];
   char op;
   int val;

   while (i < numcontrollers) {

      if (fscanf(parfile, "\nController #%s\n", tmpvals) != 1) {
         fprintf(stderr, "Error reading controller portion of physical organization\n");
         exit(0);
      }
      if ((numrets = sscanf(tmpvals, "%d%s", &ctlno, tmp)) < 1) {
         fprintf(stderr, "Error parsing controller portion of physical organization\n");
         exit(0);
      } else if (numrets == 1) {
         /* sprintf(tmp, ""); */
         tmp[0] = (char) 0;
      }
      if (ctlno != (i + 1)) {
         fprintf(stderr, "Controller numbers in physical organization appear out of order\n");
         exit(0);
      }
      fprintf (outputfile, "\nController #%d%s\n", ctlno, tmp);

      j = ctlno;
      if (strcmp(tmp, "") != 0) {
         if (sscanf(tmp, "%c%d", &op, &val) != 2) {
            fprintf(stderr, "Can't parse value for 'Controller #' %d in physical organization\n", ctlno);
            exit(0);
         }
         if ((op != '-') || (val <= ctlno) || (val > numcontrollers)) {
            fprintf(stderr, "Illegal value for 'Controller #' %d in physical organization - %d%c%d\n", ctlno, ctlno, op, val);
            exit(0);
         }
         j += val - ctlno;
      }

      getparam_int(parfile, "# of connected buses", &outbuscnt, 3, 1, MAXOUTBUSES);

      if (fscanf(parfile, "Connected buses: %s\n", outbuses) != 1) {
         fprintf(stderr, "Error reading 'Connected buses' for controller %d\n", ctlno);
         exit(0);
      }
      if ((outbuses[0] != '#') && ((outbuses[0] > '9') || (outbuses[0] < '1'))) {
         fprintf(stderr, "Invalid value for 'Connected buses' for controller %d", ctlno);
         exit(0);
      }
      fprintf (outputfile, "Connected buses:   %s\n", outbuses);

      usectlnum = 0;
      if (outbuses[0] == '#') {
         usectlnum = 1;
         outbusadd = 0;
         if (strcmp(outbuses, "#") != 0) {
            if (sscanf(outbuses, "#%c%d", &op, &val) != 2) {
               fprintf(stderr, "Invalid value for 'Connected buses' for controller %d", ctlno);
               exit(0);
            }
            if (((op != '+') && (op != '-')) || (val < 0)) {
               fprintf(stderr, "Invalid value for 'Connected buses' for controller %d", ctlno);
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
	 controllers[i].numoutbuses = outbuscnt;
         if (usectlnum) {
            controllers[i].outbuses[0] = i + outbusadd;
         } else {
            controllers[i].outbuses[0] = val - 1;
         }
      }
   }
}


void controller_read_logorg(parfile)
FILE *parfile;
{
   getparam_int(parfile, "\n# of controller-level organizations", &numctlorgs, 3, 0, MAXLOGORGS);
   fscanf(parfile, "\n");

   if (numctlorgs > 0) {
      fprintf (stderr, "Controller-level logical organizations are not currently supported\n");
      exit (0);
      ctlorgs = logorg_readparams(parfile, numctlorgs, ctl_printlocalitystats, ctl_printblockingstats, ctl_printinterferestats, ctl_printstreakstats, ctl_printstampstats, ctl_printintarrstats, ctl_printidlestats, ctl_printsizestats);
   }
}


int controller_set_depth(ctlno, inbusno, depth, slotno)
int ctlno;
int inbusno;
int depth;
int slotno;
{
   intchar ret;
   int i;
   int cnt;
/*
fprintf (outputfile, "controller_set_depth %d, inbusno %d, depth %d, slotno %d\n", ctlno, inbusno, depth, slotno);
*/
   ASSERT1((ctlno >= 0) && (ctlno < numcontrollers), "ctlno", ctlno);
   cnt = controllers[ctlno].numinbuses;
   if ((cnt > 0) && (controllers[ctlno].depth[(cnt-1)] < depth)) {
      return(0);
   }
   controllers[ctlno].numinbuses++;
   if ((cnt + 1) > MAXINBUSES) {
      fprintf(stderr, "Too many inbuses specified for controller %d : %d\n", ctlno, (cnt+1));
      exit(0);
   }
   controllers[ctlno].inbuses[cnt] = inbusno;
   controllers[ctlno].depth[cnt] = depth;
   controllers[ctlno].slotno[cnt] = slotno;
   ret.value = 0;
   for (i = 0; i < controllers[ctlno].numoutbuses; i++) {
      ret.byte[i] = (char) controllers[ctlno].outbuses[i];
      controllers[ctlno].outslot[i] = bus_get_controller_slot(ret.byte[i], ctlno);
   }
   return(ret.value);
}


int controller_get_bus_master(busno)
int busno;
{
   int i;
   int j;

   for (i = 0; i < numcontrollers; i++) {
      for (j = 0; j < controllers[i].numoutbuses; j++) {
         if (controllers[i].outbuses[j] == busno) {
            return(i);
         }
      }
   }
   return(-1);
}


int controller_get_outslot(ctlno, busno)
int ctlno;
int busno;
{
   int i;
   int slotno = 0;

   ASSERT1((ctlno >= 0) && (ctlno < numcontrollers), "ctlno", ctlno);
   for (i = 0; i < controllers[ctlno].numoutbuses; i++) {
      if (controllers[ctlno].outbuses[i] == busno) {
         slotno = controllers[ctlno].outslot[i];
         break;
      }
   }
   ASSERT(slotno != 0);
   return(slotno);
}


int controller_get_numcontrollers()
{
   return(numcontrollers);
}


int controller_get_depth(ctlno)
int ctlno;
{
   ASSERT1((ctlno >= 0) && (ctlno < numcontrollers), "ctlno", ctlno);
   return(controllers[ctlno].depth[0]);
}


int controller_get_inbus(ctlno)
int ctlno;
{
   ASSERT1((ctlno >= 0) && (ctlno < numcontrollers), "ctlno", ctlno);
   return(controllers[ctlno].inbuses[0]);
}


int controller_get_slotno(ctlno)
int ctlno;
{
   ASSERT1((ctlno >= 0) && (ctlno < numcontrollers), "ctlno", ctlno);
   return(controllers[ctlno].slotno[0]);
}


int controller_get_maxoutstanding(ctlno)
int ctlno;
{
   ASSERT1((ctlno >= 0) && (ctlno < numcontrollers), "ctlno", ctlno);
   return(controllers[ctlno].maxoutstanding);
}


void controller_param_override(paramname, paramval, first, last)
char *paramname;
char *paramval;
int first;
int last;
{
   int i;

   if (first == -1) {
      first = 0;
      last = numcontrollers - 1;
   } else if ((first < 0) || (last >= numcontrollers) || (last < first)) {
      fprintf(stderr, "Invalid range at controller_param_override: %d - %d\n", first, last);
      exit(0);
   }
   for (i=first; i<=last; i++) {
      if ((strcmp(paramname, "ioqueue_schedalg") == 0) || (strcmp(paramname, "ioqueue_seqscheme") == 0) || (strcmp(paramname, "ioqueue_cylmaptype") == 0) || (strcmp(paramname, "ioqueue_to_time") == 0) || (strcmp(paramname, "ioqueue_timeout_schedalg") == 0) || (strcmp(paramname, "ioqueue_priority_schedalg") == 0) || (strcmp(paramname, "ioqueue_priority_mix") == 0)) {
         ioqueue_param_override(controllers[i].queue, paramname, paramval);
      } else if ((strcmp(paramname, "cache_size") == 0) || (strcmp(paramname, "cache_segcount") == 0) || (strcmp(paramname, "cache_linesize") == 0) || (strcmp(paramname, "cache_bitgran") == 0) || (strcmp(paramname, "cache_lockgran") == 0) || (strcmp(paramname, "cache_readshare") == 0) || (strcmp(paramname, "cache_maxreqsize") == 0) || (strcmp(paramname, "cache_replace") == 0) || (strcmp(paramname, "cache_writescheme") == 0) || (strcmp(paramname, "cache_readprefetch") == 0) || (strcmp(paramname, "cache_writeprefetch") == 0) || (strcmp(paramname, "cache_linebyline") == 0)) {
         cache_param_override(controllers[i].cache, paramname, paramval);
      } else if (strcmp(paramname, "maxqlen") == 0) {
         int maxqlen;
         if (sscanf(paramval, "%d\n", &maxqlen) != 1) {
            fprintf(stderr, "Error reading usequeue in iodriver_param_override\n");
            exit(0);
         }
         if (maxqlen < 0) {
            fprintf(stderr, "Invalid value for maxqlen in controller_param_override: %d\n", maxqlen);
            exit(0);
         }
         controllers[i].maxoutstanding = maxqlen + 1;
      } else {
         fprintf(stderr, "Unsupported param to override at controller_param_override: %s\n", paramname);
         exit(0);
      }
   }
}


void controller_initialize()
{
   int i;

   for (i = 0; i < numcontrollers; i++) {
      controllers[i].ctlno = i;
      controllers[i].state = FREE;
      addlisttoextraq((event *) &controllers[i].connections);
      addlisttoextraq((event *) &controllers[i].buswait);
      addlisttoextraq((event *) &controllers[i].datatransfers);
      addlisttoextraq((event *) &controllers[i].hostwaiters);
      controllers[i].hosttransfer = FALSE;
      controllers[i].inbusowned = -1;
      controllers[i].outbusowned = -1;
      controllers[i].waitingforbus = 0.0;
      if (controllers[i].type == CTLR_SMART) {
         controller_smart_initialize(&controllers[i]);
      }
   }
}


void controller_resetstats()
{
   int i;

   for (i=0; i<numcontrollers; i++) {
      controllers[i].waitingforbus = 0.0;
      if (controllers[i].type == CTLR_SMART) {
         controller_smart_resetstats(&controllers[i]);
      }
   }
}


void controller_printstats()
{
   int ctlno;
   char prefix[81];
   double waitingforbus = 0.0;

   fprintf (outputfile, "\nCONTROLLER STATISTICS\n");
   fprintf (outputfile, "---------------------\n\n");

   for (ctlno=0; ctlno < numcontrollers; ctlno++) {
      waitingforbus += controllers[ctlno].waitingforbus;
      if (controllers[ctlno].printstats == 0) {
         continue;
      }
      sprintf(prefix, "Controller #%d ", ctlno);
      fprintf (outputfile, "%s\n\n", prefix);
      switch(controllers[ctlno].type) {

         case CTLR_PASSTHRU:
            controller_passthru_printstats(&controllers[ctlno], prefix);
            break;

         case CTLR_BASED_ON_53C700:
            controller_53c700_printstats(&controllers[ctlno], prefix);
            break;

         case CTLR_SMART:
            controller_smart_printstats(&controllers[ctlno], prefix);
            break;

         default:
            fprintf(stderr, "Unknown controller type in controller_printstats: %d\n", controllers[ctlno].type);
            exit(0);
       }
   }
   fprintf (outputfile, "Total controller bus wait time: %f\n",waitingforbus);
}


void controller_cleanstats()
{
}

