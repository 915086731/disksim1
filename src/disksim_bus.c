
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
#include "disksim_bus.h"

int numbuses = 0;
bus *buses = NULL;

char *statdesc_busidlestats =	"Bus idle period length";
char *statdesc_arbwaitstats =	"Arbitration wait time";

static int bus_printidlestats;
static int bus_printarbwaitstats;


void bus_print_phys_config()
{
   int i;
   int j;

   for (i = 0; i < numbuses; i++) {
      fprintf (outputfile, "Bus #%d\n", i);
      for (j = 0; j <= buses[i].numslots; j++) {
         fprintf (outputfile, "Slot #%d: %d %d\n", j, buses[i].slots[j].devtype, buses[i].slots[j].devno);
      }
   }
}


void bus_set_to_zero_depth(busno)
int busno;
{
   ASSERT1((busno >= 0) && (busno < numbuses), "busno", busno);
   buses[busno].depth = 0;
}


int bus_get_controller_slot(busno, ctlno)
int busno;
int ctlno;
{
   int i;
   int slotno = 0;

   ASSERT1((busno >= 0) && (busno < numbuses), "busno", busno);
   for (i = 1; i <= buses[busno].numslots; i++) {
      if ((buses[busno].slots[i].devno == ctlno) && (buses[busno].slots[i].devtype == CONTROLLER)) {
         slotno = i;
         break;
      }
   }
   if (slotno == 0) {
      fprintf(stderr, "Controller not connected to bus but called bus_get_controller_slot: %d %d\n", ctlno, busno);
      exit(0);
   }
   return(slotno);
}


void bus_set_depths()
{
   int depth;
   int busno;
   int slotno;
   intchar ret;
   int i;
   u_int devno;

   for (depth = 0; depth < MAXDEPTH; depth++) {
/*
fprintf (outputfile, "At depth %d\n", depth);
*/
      for (busno = 0; busno < numbuses; busno++) {
/*
fprintf (outputfile, "At busno %d\n", busno);
*/
         if (buses[busno].depth == depth) {
/*
fprintf (outputfile, "Busno %d is at this depth - %d\n", busno, depth);
*/
            for (slotno = 1; slotno <= buses[busno].numslots; slotno++) {
/*
fprintf (outputfile, "Deal with slotno %d of busno %d\n", slotno, busno);
*/
               devno = buses[busno].slots[slotno].devno;
               switch (buses[busno].slots[slotno].devtype) {
                  case CONTROLLER:
/*
fprintf (outputfile, "Slotno %d contains controller number %d\n", slotno, devno);
*/
				   ret.value = controller_set_depth(devno, busno, depth, slotno);
				   break;
                  case DISK:
/*
fprintf (outputfile, "Slotno %d contains disk number %d\n", slotno, devno);
*/
				   ret.value = disk_set_depth(devno, busno, depth, slotno);
				   break;
                  default:         fprintf(stderr, "Invalid device type in bus slot\n");
                                   exit(0);
               }
/*
fprintf (outputfile, "Back from setting device depth\n");
*/
	       if (ret.value != 0) {
/*
fprintf (outputfile, "Non-zero return - %x\n", ret.value);
*/
		  for (i = 0; i < MAXDEPTH; i++) {
		     if (ret.byte[i] != 0) {
/*
fprintf (outputfile, "Set busno %x to have depth %d\n", ret.byte[i], (depth+1));
*/
		        buses[ret.byte[i]].depth = depth + 1;
		     }
		  }
	       }
	    }
         }
      }
   }
}


double bus_get_transfer_time(busno, bcount, read)
int busno;
int bcount;
int read;
{
   double blktranstime;

   ASSERT1((busno >= 0) && (busno < numbuses), "busno", busno);
   blktranstime = (read) ? buses[busno].readblktranstime : buses[busno].writeblktranstime;
   return((double) bcount * blktranstime);
}


void bus_delay(busno, devtype, devno, delay, curr)
int busno;
int devtype;
int devno;
double delay;
ioreq_event *curr;
{
   bus_event *tmp = (bus_event *) getfromextraq();
   tmp->type = BUS_DELAY_COMPLETE;
   tmp->time = simtime + delay;
   tmp->devno = devno;
   tmp->busno = busno;
   tmp->devtype = devtype;
   tmp->delayed_event = curr;
   addtointq((event *) tmp);
}


void bus_event_arrive(curr)
bus_event *curr;
{
   if (curr->type == BUS_OWNERSHIP_GRANTED) {
      ASSERT(curr == buses[curr->busno].arbwinner);
      buses[curr->busno].arbwinner = NULL;
      stat_update (&buses[curr->busno].arbwaitstats, (simtime - curr->wait_start));
   }
   switch (curr->devtype) {

      case CONTROLLER:
         if (curr->type == BUS_DELAY_COMPLETE) {
	    controller_bus_delay_complete (curr->devno, curr->delayed_event, curr->busno);
         } else {
	    controller_bus_ownership_grant (curr->devno, curr->delayed_event, curr->busno, (simtime - curr->wait_start));
         }
	 break;

      case DISK:
         if (curr->type == BUS_DELAY_COMPLETE) {
	    disk_bus_delay_complete (curr->devno, curr->delayed_event, curr->busno);
         } else {
	    disk_bus_ownership_grant (curr->devno, curr->delayed_event, curr->busno, (simtime - curr->wait_start));
         }
	 break;

      default:
	 fprintf(stderr, "Unknown device type at bus_event_arrive: %d, type %d\n", curr->devtype, curr->type);
	 exit(0);
   }
   addtoextraq((event *)curr);
}


bus_event * bus_fifo_arbitration(currbus)
bus *currbus;
{
   bus_event *ret = currbus->owners;
   ASSERT(currbus->owners != NULL);
   currbus->owners = ret->next;
   return(ret);
}


bus_event * bus_slotpriority_arbitration(currbus)
bus *currbus;
{
   bus_event *ret = currbus->owners;

   ASSERT(currbus->owners != NULL);
   if (currbus->owners->next == NULL) {
      currbus->owners = NULL;
      return(ret);
   } else {
      bus_event *trv = ret;
      while (trv->next) {
         if (trv->next->slotno > ret->slotno) {
            ret = trv->next;
         }
         trv = trv->next;
      }
      if (ret == currbus->owners) {
         currbus->owners = ret->next;
         ret->next = NULL;
      } else {
	 trv = currbus->owners;
         while ((trv->next != NULL) && (trv->next != ret)) {
            trv = trv->next;
         }
	 ASSERT(trv->next == ret);
         trv->next = ret->next;
         ret->next = NULL;
      }
   }
   return(ret);
}


bus_event * bus_arbitrate_for_ownership(currbus)
bus *currbus;
{
   switch (currbus->arbtype) {

      case SLOTPRIORITY_ARB:
          return(bus_slotpriority_arbitration(currbus));

      case FIFO_ARB:
          return(bus_fifo_arbitration(currbus));

      default:
          fprintf(stderr, "Unrecognized bus arbitration type in bus_arbitrate_for_ownership\n");
          exit(0);
   }
}


int bus_ownership_get(busno, slotno, curr)
int busno;
int slotno;
ioreq_event *curr;
{
   bus_event *tmp;

   if (buses[busno].type == INTERLEAVED) {
      return(TRUE);
   }
   tmp = (bus_event *) getfromextraq();
   tmp->type = BUS_OWNERSHIP_GRANTED;
   tmp->devno = buses[busno].slots[slotno].devno;
   tmp->busno = busno;
   tmp->delayed_event = curr;
   tmp->wait_start = simtime;
   if (buses[busno].state == BUS_FREE) {
      tmp->devtype = buses[busno].slots[slotno].devtype;
      tmp->time = simtime + buses[busno].arbtime;
/*
fprintf (outputfile, "Granting ownership immediately - devno %d, devtype %d, busno %d\n", tmp->devno, tmp->devtype, tmp->busno);
*/
      buses[busno].arbwinner = tmp;
      addtointq((event *) tmp);
      buses[busno].state = BUS_OWNED;
      buses[busno].runidletime += simtime - buses[busno].lastowned;
      stat_update (&buses[busno].busidlestats, (simtime - buses[busno].lastowned));
   } else {
      tmp->slotno = slotno;
/*
fprintf (outputfile, "Must wait for bus to become free - devno %d, slotno %d, busno %d\n", tmp->devno, tmp->slotno, tmp->busno);
*/
      tmp->next = NULL;
      if (buses[busno].owners) {
         bus_event *tail = buses[busno].owners;
         while (tail->next) {
            tail = tail->next;
         }
         tail->next = tmp;
      } else {
         buses[busno].owners = tmp;
      }
   }
   return(FALSE);
}


void bus_ownership_release(busno)
int busno;
{
   bus_event *tmp;
/*
fprintf (outputfile, "Bus ownership being released - %d\n", busno);
*/
   ASSERT(buses[busno].arbwinner == NULL);
   if (buses[busno].owners == NULL) {
/*
fprintf (outputfile, "Bus has become free - %d\n", busno);
*/
      buses[busno].state = BUS_FREE;
      buses[busno].lastowned = simtime;
   } else {
      tmp = bus_arbitrate_for_ownership(&buses[busno]);
      tmp->devtype = buses[busno].slots[tmp->slotno].devtype;
      tmp->time = simtime + buses[busno].arbtime;
/*
fprintf (outputfile, "Bus ownership transfered - devno %d, devtype %d, busno %d\n", tmp->devno, tmp->devtype, tmp->busno);
*/
      buses[busno].arbwinner = tmp;
      addtointq((event *) tmp);
   }
}


void bus_remove_from_arbitration(busno, curr)
int busno;
ioreq_event *curr;
{
   bus_event *tmp;
   bus_event *trv;

   if (buses[busno].arbwinner) {
      if (curr == buses[busno].arbwinner->delayed_event) {
         if (removefromintq(buses[busno].arbwinner) != TRUE) {
            fprintf(stderr, "Ongoing arbwinner not in internal queue, at bus_remove_from_arbitration\n");
            exit(0);
         }
         addtoextraq((event *) buses[busno].arbwinner);
         buses[busno].arbwinner = NULL;
         bus_ownership_release(busno);
         return;
      }
   }
   if (curr == buses[busno].owners->delayed_event) {
      tmp = buses[busno].owners;
      buses[busno].owners = tmp->next;
   } else {
      trv = buses[busno].owners;
      while ((trv->next) && (curr != trv->next->delayed_event)) {
         trv = trv->next;
      }
      ASSERT(trv->next != NULL);
      tmp = trv->next;
      trv->next = tmp->next;
   }
   addtoextraq((event *) tmp);
}


void bus_deliver_event(busno, slotno, curr)
int busno;
int slotno;
ioreq_event *curr;
{
   int devno;

   ASSERT1((busno >= 0) && (busno < numbuses), "busno", busno);
   ASSERT1((slotno > 0) && (slotno <= buses[busno].numslots), "slotno", slotno);

/*
fprintf (outputfile, "In middle of bus_deliver_event\n");
*/
   devno = buses[busno].slots[slotno].devno;
   switch (buses[busno].slots[slotno].devtype) {

      case CONTROLLER:  controller_event_arrive(devno, curr);
                        break;

      case DISK:        ASSERT(devno == curr->devno);
                        disk_event_arrive(curr);
                        break;

      default:          fprintf(stderr, "Invalid device type in bus slot\n");
                        exit(0);
   }
}


int bus_get_data_transfered(curr, depth)
ioreq_event *curr;
int depth;
{
   intchar slotno;
   intchar busno;
   int checkdevno;
   int ret;
/*
fprintf (outputfile, "Entered bus_get_data_transfered - devno %d, depth %d, busno %x, slotno %x\n", curr->devno, depth, curr->busno, curr->slotno);
*/
   busno.value = curr->busno;
   slotno.value = curr->slotno;
   while (depth) {
      checkdevno = buses[(busno.byte[depth])].slots[(slotno.byte[depth] >> 4)].devno;
      switch (buses[(busno.byte[depth])].slots[(slotno.byte[depth] >> 4)].devtype) {
   
         case CONTROLLER:       ret = controller_get_data_transfered(checkdevno, curr->devno);
                                break;
   
         default:       fprintf(stderr, "Invalid device type in bus_get_data_transfered\n");
                        exit(0);
      }
      if (ret != -1) {
         return(ret);
      }
      depth--;
   }
   return(-1);
}


void bus_read_toprints(parfile)
FILE *parfile;
{
   getparam_bool(parfile, "Print bus idle stats?", &bus_printidlestats);
   getparam_bool(parfile, "Print bus arbwait stats?", &bus_printarbwaitstats);
}


void bus_read_specs(parfile)
FILE *parfile;
{
   int specno_expected = 1;
   int specno;
   int busno = 0;
   int copies;
   int i;

   getparam_int(parfile, "\nNumber of buses", &numbuses, 1, 0, 0);
   if (numbuses == 0) {
      return;
   }

   buses = (bus *) malloc(numbuses * (sizeof(bus)));
   ASSERT(buses != NULL);

   while (busno < numbuses) {
      if (fscanf(parfile, "\nBus Spec #%d\n", &specno) != 1) {
         fprintf(stderr, "Error reading spec number\n");
         exit(0);
      }
      if (specno != specno_expected) {
         fprintf(stderr, "Unexpected value for bus spec number: specno %d, expected %d\n", specno, specno_expected);
         exit(0);
      }
      fprintf (outputfile, "\nBus Spec #%d\n", specno);
      specno_expected++;

      getparam_int(parfile, "# buses with Spec", &copies, 1, 0, 0);
      if (copies == 0) {
         copies = numbuses - busno;
      }

      if ((busno + copies) > numbuses) {
         fprintf(stderr, "Too many bus specifications provided\n");
         exit(0);
      }

      getparam_int(parfile, "Bus Type", &buses[busno].type, 3, BUS_TYPE_MIN, BUS_TYPE_MAX);
      getparam_int(parfile, "Arbitration type", &buses[busno].arbtype, 3, BUS_ARB_TYPE_MIN, BUS_ARB_TYPE_MAX);
      getparam_double(parfile, "Arbitration time", &buses[busno].arbtime, 1, (double) 0.0, (double) 0.0);
      getparam_double(parfile, "Read block transfer time", &buses[busno].readblktranstime, 1, (double) 0.0, (double) 0.0);
      getparam_double(parfile, "Write block transfer time", &buses[busno].writeblktranstime, 1, (double) 0.0, (double) 0.0);
      getparam_int(parfile, "Print stats for bus", &buses[busno].printstats, 3, 0, 1);

      for (i=busno; i<(busno+copies); i++) {
         buses[i].type = buses[busno].type;
         buses[i].arbtype = buses[busno].arbtype;
         buses[i].arbtime = buses[busno].arbtime;
         buses[i].readblktranstime = buses[busno].readblktranstime;
         buses[i].writeblktranstime = buses[busno].writeblktranstime;
         buses[i].printstats = buses[busno].printstats;
         buses[i].owners = NULL;
         buses[i].depth = -1;
      }
      busno += copies;
   }
}


void bus_read_physorg(parfile)
FILE *parfile;
{
   int i = 0;
   int j;
   int k;
   int l;
   int cnt;
   int busno;
   char tmp[20];
   char tmpvals[20];
   int numrets;
   int slotcnt;
   char slots[20];
   char devices[40];
   char op;
   int val;
   int endval;
   int devtype;
   int slotadd;

   while (i < numbuses) {

      if (fscanf(parfile, "\nBus #%s\n", tmpvals) != 1) {
         fprintf(stderr, "Error reading bus portion of physical organization\n");
         exit(0);
      }
      if ((numrets = sscanf(tmpvals, "%d%s", &busno, tmp)) < 1) {
         fprintf(stderr, "Error parsing bus portion of physical organization\n");
         exit(0);
      } else if (numrets == 1) {
	 /* sprintf(tmp, ""); */
         tmp[0] = (char) 0;
      }
      if (busno != (i + 1)) {
         fprintf(stderr, "Bus numbers in physical organization appear out of order\n");
         exit(0);
      }
      fprintf (outputfile, "\nBus #%d%s\n", busno, tmp);

      j = i + 1;
      if (strcmp(tmp, "") != 0) {
         if (sscanf(tmp, "%c%d", &op, &val) != 2) {
            fprintf(stderr, "Illegal value for 'Bus #' %d in physical organization\n", busno);
            exit(0);
         }
         if ((op != '-') || (val <= busno) || (val > numbuses)) {
            fprintf(stderr, "Illegal value for 'Bus #' %d in physical organization: op %c, val %d, numbuses %d\n", busno, op, val, numbuses);
            exit(0);
         }
         j += val - busno;
      }

      getparam_int(parfile, "# of utilized slots", &slotcnt, 3, 1, MAXSLOTS);

      for (k = i; k < j; k++) {
         buses[k].numslots = slotcnt;
         if (!(buses[k].slots = (slot *) malloc((slotcnt+1) * (sizeof(slot))))) {
            fprintf(stderr, "Problem allocating slots for bus #%d\n", busno);
            exit(0);
         }
      }
      if (buses[i].numslots == 0)
	 continue;
      cnt = 1;
      while (cnt <= slotcnt) {
         if (fscanf(parfile, "Slots: %s %s\n", devices, slots) != 2) {
            fprintf(stderr, "Error reading 'Slots' for bus %d\n", busno);
            exit(0);
         }
	 if (strcmp(devices, "Controllers") == 0) {
	    devtype = CONTROLLER;
	 } else if (strcmp(devices, "Disks") == 0) {
	    devtype = DISK;
	 } else {
            fprintf(stderr, "Invalid value for 'Slots' for bus %d\n", busno);
            exit(0);
	 }
         if ((slots[0] != '#') && ((slots[0] > '9') || (slots[0] < '1'))) {
            fprintf(stderr, "Invalid value for 'Slots' for bus %d\n", busno);
            exit(0);
         }

         if (slots[0] == '#') {
            slotadd = 0;
            if (strcmp(slots, "#") != 0) {
               if (sscanf(slots, "#%d", &val) != 1) {
                  fprintf(stderr, "Invalid value for 'Slots' for bus %d\n", busno);
                  exit(0);
               }
               slotadd = val;
            }
	    for (k=i; k < j; k++) {
	       buses[k].slots[cnt].devtype = devtype;
               buses[k].slots[cnt].devno = k + slotadd;
	    }
	    cnt++;
         } else {
	    /* sprintf(tmp, ""); */
            tmp[0] = (char) 0;
            sscanf(slots, "%d%s", &val, tmp);
	    if (strcmp(tmp, "") == 0) {
	       for (k = i; k < j; k++) {
                  buses[k].slots[cnt].devtype = devtype;
	          buses[k].slots[cnt].devno = val - 1;
               }
	       cnt++;
	    } else if (tmp[0] == '-') {
	       if (sscanf(tmp, "-%d", &endval) != 1) {
		  fprintf(stderr, "A problem with parsing 'Slots' for bus %d\n", busno);
		  exit(0);
	       }
	       if ((endval - val + 1) > slotcnt) {
		  fprintf(stderr, "Too many slot descriptions provided for bus %d\n", busno);
		  exit(0);
	       }
	       for (k = i; k < j; k++) {
		  for (l = cnt; l <= (cnt + endval - val); l++) {
                     buses[k].slots[l].devtype = devtype;
	             buses[k].slots[l].devno = val + (l - cnt) - 1;
/*
fprintf (outputfile, "Slot %d contains devtype %d number %d\n", l, devtype, (val + l - cnt - 1));
*/
		  }
               }
	       cnt += endval - val + 1;
	    } else {
               fprintf(stderr, "Invalid value for 'Slots' for bus %d\n", busno);
               exit(0);
	    }
         }
	 fprintf (outputfile, "Slots: %s %s\n", devices, slots);
      }

      i = j;
   }
}


void bus_param_override(paramname, paramval, first, last)
char *paramname;
char *paramval;
int first;
int last;
{
   fprintf(stderr, "No param overrides currently supported at bus_param_override\n");
   exit(0);
}


void bus_resetstats()
{
   int i;

   for (i=0; i<numbuses; i++) {
      buses[i].lastowned = simtime;
      buses[i].runidletime = 0.0;
      stat_reset (&buses[i].busidlestats);
      stat_reset (&buses[i].arbwaitstats);
   }
}


void bus_initialize()
{
   int i;

   StaticAssert (sizeof(bus_event) <= DISKSIM_EVENT_SIZE);
   for (i=0; i<numbuses; i++) {
      buses[i].state = BUS_FREE;
      addlisttoextraq((event *) &buses[i].owners);
      stat_initialize (statdeffile, statdesc_arbwaitstats, &buses[i].arbwaitstats);
      stat_initialize (statdeffile, statdesc_busidlestats, &buses[i].busidlestats);
   }
   bus_resetstats();
}


void bus_printstats()
{
   int i;
   char prefix[81];

   fprintf (outputfile, "\nBUS STATISTICS\n");
   fprintf (outputfile, "--------------\n\n");

   for (i=0; i<numbuses; i++) {
      if (buses[i].printstats) {
         sprintf (prefix, "Bus #%d ", i);
         fprintf (outputfile, "Bus #%d\n", i);
         fprintf (outputfile, "Bus #%d Total utilization time: \t%.2f   \t%6.5f\n", i, (simtime - warmuptime - buses[i].runidletime), ((simtime - warmuptime - buses[i].runidletime) / (simtime - warmuptime)));
         if (bus_printidlestats) {
            stat_print (&buses[i].busidlestats, prefix);
         }
         if (bus_printarbwaitstats) {
            fprintf (outputfile, "Bus #%d Number of arbitrations: \t%d\n", i, stat_get_count (&buses[i].arbwaitstats));
            stat_print (&buses[i].arbwaitstats, prefix);
         }
         fprintf (outputfile, "\n");
      }
   }
}


void bus_cleanstats()
{
   int i;

   for (i=0; i<numbuses; i++) {
      if (buses[i].state == BUS_FREE) {
         buses[i].runidletime += simtime - buses[i].lastowned;
         stat_update (&buses[i].busidlestats, (simtime - buses[i].lastowned));
      }
   }
}

