
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
#include "disksim_ioface.h"
#include "disksim_orgface.h"
#include "disksim_iosim.h"
#include "disksim_stat.h"

#define TRACEMAPPINGS	MAXDISKS

extern event * iotrace_get_ioreq_event();

int closedios = 0;
double closedthinktime = 0.0;
double ioscale = 1.0;
double last_request_arrive = 0.0;
double constintarrtime = 0.0;

int tracemappings = 0;
int tracemap[TRACEMAPPINGS];
int tracemap1[TRACEMAPPINGS];
int tracemap2[TRACEMAPPINGS];
int tracemap3[TRACEMAPPINGS];
int tracemap4[TRACEMAPPINGS];
statgen *tracestats = NULL;
statgen *tracestats1 = NULL;
statgen *tracestats2 = NULL;
statgen *tracestats3 = NULL;
statgen *tracestats4 = NULL;
statgen *tracestats5 = NULL;
int printtracestats = TRUE;
int validatebuf[10];

char *statdesc_tracequeuestats =	"Trace queue time";
char *statdesc_tracerespstats =		"Trace response time";
char *statdesc_traceaccstats =		"Trace access time";
char *statdesc_traceqlenstats =		"Trace queue length";
char *statdesc_tracenoqstats =		"Trace non-queue time";
char *statdesc_traceaccdiffstats =	"Trace access diff time";
char *statdesc_traceaccwritestats =	"Trace write access time";
char *statdesc_traceaccdiffwritestats =	"Trace write access diff time";
char *statdesc_tracerotmissstats =	"Trace rotation miss time";

event *io_extq = NULL;
int io_extqlen = 0;
int io_extq_type;


ioreq_event * ioreq_copy(old)
ioreq_event *old;
{
   ioreq_event *new = (ioreq_event *) getfromextraq();
   bcopy ((char *)old, (char *)new, sizeof (ioreq_event));
   return(new);
}


void io_catch_stray_events(curr)
ioreq_event *curr;
{
   switch (curr->type) {

      case DISK_DATA_TRANSFER_COMPLETE:
                    curr->time = disk_get_blktranstime(curr);
		    curr->time = simtime + (curr->time * (double) curr->bcount);
	            addtointq((event *) curr);
		    break;

      default:
	    fprintf(stderr, "Unknown event type at io_catch_stray_events\n");
       	    exit(0);
   }
}


event * io_request(curr)
ioreq_event *curr;
{
   curr->type = IO_REQUEST_ARRIVE;
   return (iodriver_request(0, curr));
}


void io_schedule(curr)
ioreq_event *curr;
{
   curr->type = IO_ACCESS_ARRIVE;
   iodriver_schedule(0, curr);
}


double io_tick()
{
   return (iodriver_tick());
}


double io_raise_priority(opid, devno, blkno, chan)
int opid;
int devno;
int blkno;
void *chan;
{
   return (iodriver_raise_priority(0, opid, devno, blkno, chan));
}


void io_interrupt_arrive(intrp)
ioreq_event *intrp;
{
   intrp->type = IO_INTERRUPT_ARRIVE;
   iodriver_interrupt_arrive (0, intrp);
}


void io_interrupt_complete(intrp)
ioreq_event *intrp;
{
   intrp->type = IO_INTERRUPT_COMPLETE;
   iodriver_interrupt_complete (0, intrp);
}


void io_internal_event(curr)
ioreq_event *curr;
{
   ASSERT(curr != NULL);
/*
fprintf (outputfile, "%f: io_internal_event entered with event type %d, %f\n", curr->time, curr->type, simtime);
*/
   switch (curr->type) {
      case IO_REQUEST_ARRIVE:
	 iodriver_request(0, curr);
         break;

      case IO_ACCESS_ARRIVE:
	 iodriver_schedule(0, curr);
         break;

      case IO_INTERRUPT_ARRIVE:
	 iodriver_interrupt_arrive(0, (intr_event *) curr);
         break;

      case IO_RESPOND_TO_DEVICE:
	 iodriver_respond_to_device(0, (intr_event *) curr->tempptr1);
	 addtoextraq((event *) curr);
         break;

      case IO_ACCESS_COMPLETE:
	 iodriver_access_complete(0, (intr_event *) curr->tempptr1);
	 addtoextraq((event *) curr);
         break;

      case IO_INTERRUPT_COMPLETE:
	 iodriver_interrupt_complete(0, (intr_event *) curr);
         break;

      case DISK_OVERHEAD_COMPLETE:
      case DISK_DATA_TRANSFER_COMPLETE:
      case DISK_PREPARE_FOR_DATA_TRANSFER:
      case DISK_BUFFER_SEEKDONE:
      case DISK_BUFFER_TRACKACC_DONE:
      case DISK_BUFFER_SECTOR_DONE:
      case DISK_GOT_REMAPPED_SECTOR:
      case DISK_GOTO_REMAPPED_SECTOR:
	 disk_event_arrive(curr);
	 break;

      case BUS_OWNERSHIP_GRANTED:
      case BUS_DELAY_COMPLETE:
	 bus_event_arrive(curr);
         break;

      case CONTROLLER_DATA_TRANSFER_COMPLETE:
	 controller_event_arrive(curr->tempint2, curr);
         break;

      case TIMESTAMP_LOGORG:
	 logorg_timestamp(curr);
         break;

      case IO_TRACE_REQUEST_START:
	 iodriver_trace_request_start(0, curr);
         break;

      default:
	 fprintf(stderr, "Unknown event type passed to io_internal_events\n");
         exit(0);
   }
/*
fprintf (outputfile, "Leaving io_internal_event\n");
*/
}


void io_read_generalparms(parfile)
FILE *parfile;
{
   int i;

   getparam_double(parfile, "I/O Trace Time Scale", &ioscale, 0, (double) 0.0, (double) 0.0);
   if (ioscale < 0.0) {
      if (ioscale > -1.0) {
         constintarrtime = 1/(-ioscale);
         ioscale = 1.0;
      } else {
         closedios = (int) -ioscale;
         ioscale = 1.0;
      }
   } else if (ioscale == 0.0) {
      fprintf(stderr, "Invalid value for I/O trace time scale - %f\n", ioscale);
      exit(0);
   }

   getparam_int(parfile, "Number of I/O Mappings", &tracemappings, 3, 0, TRACEMAPPINGS);

   for (i=0; i<tracemappings; i++) {
      if (fscanf(parfile,"Mapping: %x %x %d %d %d\n", &tracemap[i], &tracemap1[i], &tracemap2[i], &tracemap3[i], &tracemap4[i]) != 5) {
         fprintf(stderr, "Error reading I/O mappings #%d\n", i);
         exit(0);
      }
      if ((tracemap1[i] < 0) || (tracemap2[i] <= 0) || ((tracemap2[i] % 512) && (512 % tracemap2[i])) || (tracemap3[i] <= 0) || (tracemap4[i] < 0)) {
         fprintf(stderr, "Invalid value for I/O mapping #%d - %x %d\n", i, tracemap1[i], tracemap2[i]);
         exit(0);
      }
      fprintf (outputfile, "Mapping: %x %x %d %d %d\n", tracemap[i], tracemap1[i], tracemap2[i], tracemap3[i], tracemap4[i]);
      if (tracemap2[i] == 512) {
	 tracemap2[i] = 0;
      } else if (tracemap2[i] > 512) {
	 tracemap2[i] = -(tracemap2[i] / 512);
      } else {
	 tracemap2[i] = 512 / tracemap2[i];
      }
   }
}


void io_readparams(parfile)
FILE *parfile;
{
   fscanf(parfile, "\nI/O Subsystem Input Parameters\n");
   fprintf(outputfile, "\nI/O Subsystem Input Parameters\n");
   fscanf(parfile, "------------------------------\n");
   fprintf(outputfile, "------------------------------\n");

   fscanf(parfile, "\nPRINTED I/O SUBSYSTEM STATISTICS\n\n");
   fprintf(outputfile, "\nPRINTED I/O SUBSYSTEM STATISTICS\n\n");

   iodriver_read_toprints(parfile);
   bus_read_toprints(parfile);
   controller_read_toprints(parfile);
   disk_read_toprints(parfile);

   fscanf(parfile, "\nGENERAL I/O SUBSYSTEM PARAMETERS\n\n");
   fprintf(outputfile, "\nGENERAL I/O SUBSYSTEM PARAMETERS\n\n");

   io_read_generalparms(parfile);

   fscanf(parfile, "\nCOMPONENT SPECIFICATIONS\n");
   fprintf(outputfile, "\nCOMPONENT SPECIFICATIONS\n");

   iodriver_read_specs(parfile);
   bus_read_specs(parfile);
   controller_read_specs(parfile);
   disk_read_specs(parfile);

   fscanf(parfile, "\nPHYSICAL ORGANIZATION\n");
   fprintf(outputfile, "\nPHYSICAL ORGANIZATION\n");

   iodriver_read_physorg(parfile);
   controller_read_physorg(parfile);
   bus_read_physorg(parfile);

   fscanf(parfile, "\nSYNCHRONIZATION\n");
   fprintf(outputfile, "\nSYNCHRONIZATION\n");

   disk_read_syncsets(parfile);

   fscanf(parfile, "\nLOGICAL ORGANIZATION\n");
   fprintf(outputfile, "\nLOGICAL ORGANIZATION\n");

   iodriver_read_logorg(parfile);
   controller_read_logorg(parfile);
/*
   bus_print_phys_config();
*/
}


void io_validate_do_stats1()
{
   extern double lastphystime;
   extern double validate_lastserv;
   extern int validate_lastread;
   int i;

   if (tracestats2 == NULL) {
      if (!(tracestats2 = (statgen *)malloc(sizeof(statgen)))) {
	 fprintf(stderr, "Can't malloc tracestats2 in io_validate_get_ioreq_event\n");
	 exit(0);
      }
      if (!(tracestats3 = (statgen *)malloc(sizeof(statgen)))) {
	 fprintf(stderr, "Can't malloc tracestats3 in io_validate_get_ioreq_event\n");
	 exit(0);
      }
      if (!(tracestats4 = (statgen *)malloc(sizeof(statgen)))) {
	 fprintf(stderr, "Can't malloc tracestats4 in io_validate_get_ioreq_event\n");
	 exit(0);
      }
      if (!(tracestats5 = (statgen *)malloc(sizeof(statgen)))) {
	 fprintf(stderr, "Can't malloc tracestats5 in io_validate_get_ioreq_event\n");
	 exit(0);
      }
      stat_initialize(statdeffile, statdesc_traceaccstats, tracestats2);
      stat_initialize(statdeffile, statdesc_traceaccdiffstats, tracestats3);
      stat_initialize(statdeffile, statdesc_traceaccwritestats, tracestats4);
      stat_initialize(statdeffile, statdesc_traceaccdiffwritestats, tracestats5);
      for (i=0; i<10; i++) {
	 validatebuf[i] = 0;
      }
   } else {
      stat_update(tracestats3, (validate_lastserv - lastphystime));
      if (!validate_lastread) {
         stat_update(tracestats5, (validate_lastserv - lastphystime));
      }
   }
}


void io_validate_do_stats2(new)
ioreq_event *new;
{
   extern double validate_lastserv;
   extern char validate_buffaction[];

   stat_update(tracestats2, validate_lastserv);
   if (new->flags == WRITE) {
      stat_update(tracestats4, validate_lastserv);
   }
   if (strcmp(validate_buffaction, "Doub") == 0) {
      validatebuf[0]++;
   } else if (strcmp(validate_buffaction, "Trip") == 0) {
      validatebuf[1]++;
   } else if (strcmp(validate_buffaction, "Miss") == 0) {
      validatebuf[2]++;
   } else if (strcmp(validate_buffaction, "Hit") == 0) {
      validatebuf[3]++;
   } else {
      fprintf(stderr, "Unrecognized buffaction in validate trace: %s\n", validate_buffaction);
      exit(0);
   }
}


void io_hpl_do_stats1()
{
   int i;

   if (tracestats == NULL) {
      if (!(tracestats = (statgen *)malloc(tracemappings * sizeof(statgen)))) {
         fprintf(stderr, "Can't malloc tracestats in io_hpl_srt_get_ioreq_event\n");
         exit(0);
      }
      if (!(tracestats1 = (statgen *)malloc(tracemappings * sizeof(statgen)))) {
         fprintf(stderr, "Can't malloc tracestats1 in io_hpl_srt_get_ioreq_event\n");
         exit(0);
      }
      if (!(tracestats2 = (statgen *)malloc(tracemappings * sizeof(statgen)))) {
         fprintf(stderr, "Can't malloc tracestats2 in io_hpl_srt_get_ioreq_event\n");
         exit(0);
      }
      if (!(tracestats3 = (statgen *)malloc(tracemappings * sizeof(statgen)))) {
         fprintf(stderr, "Can't malloc tracestats3 in io_hpl_srt_get_ioreq_event\n");
         exit(0);
      }
      if (!(tracestats4 = (statgen *)malloc(tracemappings * sizeof(statgen)))) {
         fprintf(stderr, "Can't malloc tracestats4 in io_hpl_srt_get_ioreq_event\n");
         exit(0);
      }
      for (i=0; i<tracemappings; i++) {
         stat_initialize(statdeffile, statdesc_tracequeuestats, &tracestats[i]);
         stat_initialize(statdeffile, statdesc_tracerespstats, &tracestats1[i]);
         stat_initialize(statdeffile, statdesc_traceaccstats, &tracestats2[i]);
         stat_initialize(statdeffile, statdesc_traceqlenstats, &tracestats3[i]);
         stat_initialize(statdeffile, statdesc_tracenoqstats, &tracestats4[i]);
      }
   }
}


void io_map_trace_request(temp)
ioreq_event *temp;
{
   int i;

   for (i=0; i<tracemappings; i++) {
      if (temp->devno == tracemap[i]) {
	 temp->devno = tracemap1[i];
	 if (tracemap2[i]) {
	    if (tracemap2[i] < 1) {
	       temp->blkno *= -tracemap2[i];
	    } else {
	       if (temp->blkno % tracemap2[i]) {
	          fprintf(stderr, "Small sector size disk using odd sector number: %d\n", temp->blkno);
	          exit(0);
	       }
/*
	       fprintf (outputfile, "mapping block number %d to %d\n", temp->blkno, (temp->blkno / tracemap2[i]));
*/
	       temp->blkno /= tracemap2[i];
	    }
	 }
	 temp->bcount *= tracemap3[i];
	 temp->blkno += tracemap4[i];
	 if (tracestats) {
	    stat_update(&tracestats[i], ((double) temp->tempint1 / (double) 1000));
	    stat_update(&tracestats1[i],((double) (temp->tempint1 + temp->tempint2) / (double) 1000));
	    stat_update(&tracestats2[i],((double) temp->tempint2 / (double) 1000));
	    stat_update(&tracestats3[i], (double) temp->slotno);
	    if (temp->slotno == 1) {
	       stat_update(&tracestats4[i], ((double) temp->tempint1 / (double) 1000));
	    }
	 }
	 return;
      }
   }
/*
   fprintf(stderr, "Requested device not mapped - %x\n", temp->devno);
   exit(0);
*/
}


event * io_get_next_external_event(iotracefile)
FILE *iotracefile;
{
   event *temp;
   extern double tracebasetime;

   ASSERT(io_extq == NULL);
/*
fprintf (outputfile, "Near beginning of io_get_next_external_event\n");
*/
   temp = getfromextraq();
   switch (traceformat) {
      case VALIDATE: io_validate_do_stats1();
		     break;
      case HPL: io_hpl_do_stats1();
                break;
   }
   temp = iotrace_get_ioreq_event(iotracefile, traceformat, temp);
   if (temp) {
      switch (traceformat) {
         case VALIDATE: io_validate_do_stats2(temp);
		        break;
      }
      temp->type = IO_REQUEST_ARRIVE;
      if (constintarrtime > 0.0) {
	 temp->time = last_request_arrive + constintarrtime;
	 last_request_arrive = temp->time;
      }
      temp->time = (temp->time * ioscale) + tracebasetime;
      if ((temp->time < simtime) && (!closedios)) {
         fprintf(stderr, "Trace event appears out of time order in trace - simtime %f, time %f\n", simtime, temp->time);
	 fprintf(stderr, "ioscale %f, tracebasetime %f\n", ioscale, tracebasetime);
	 fprintf(stderr, "devno %d, blkno %d, bcount %d, flags %d\n", ((ioreq_event *)temp)->devno, ((ioreq_event *)temp)->blkno, ((ioreq_event *)temp)->bcount, ((ioreq_event *)temp)->flags);
         exit(0);
      }
      if (tracemappings) {
         io_map_trace_request(temp);
      }
      io_extq = temp;
      io_extq_type = temp->type;
   }
/*
fprintf (outputfile, "leaving io_get_next_external_event\n");
*/
   return(temp);
}


int io_using_external_event(curr)
event *curr;
{
   if (io_extq == curr) {
      curr->type = io_extq_type;
      io_extq = NULL;
      return(1);
   }
   return(0);
}


void io_printstats()
{
   extern int hpreads;
   extern int hpwrites;
   extern int syncreads;
   extern int syncwrites;
   extern int asyncreads;
   extern int asyncwrites;
   int i;
   int cnt = 0;
   char prefix[80];

   fprintf (outputfile, "\nSTORAGE SUBSYSTEM STATISTICS\n");
   fprintf (outputfile, "----------------------------\n");
   if (hpreads | hpwrites) {
      fprintf (outputfile, "\n");
      fprintf(outputfile, "Total reads:    \t%d\t%5.2f\n", hpreads, ((double) hpreads / (double) (hpreads + hpwrites)));
      fprintf(outputfile, "Total writes:   \t%d\t%5.2f\n", hpwrites, ((double) hpwrites / (double) (hpreads + hpwrites)));
      fprintf(outputfile, "Sync Reads:  \t%d\t%5.2f\t%5.2f\n", syncreads, ((double) syncreads / (double) (hpreads + hpwrites)), ((double) syncreads / (double) hpreads));
      fprintf(outputfile, "Sync Writes: \t%d\t%5.2f\t%5.2f\n", syncwrites, ((double) syncwrites / (double) (hpreads + hpwrites)), ((double) syncwrites / (double) hpwrites));
      fprintf(outputfile, "Async Reads: \t%d\t%5.2f\t%5.2f\n", asyncreads, ((double) asyncreads / (double) (hpreads + hpwrites)), ((double) asyncreads / (double) hpreads));
      fprintf(outputfile, "Async Writes:\t%d\t%5.2f\t%5.2f\n", asyncwrites, ((double) asyncwrites / (double) (hpreads + hpwrites)), ((double) asyncwrites / (double) hpwrites));
   }
   if ((tracestats) && (printtracestats)) {
      /* info relevant to HPL traces */
      for (i=0; i<tracemappings; i++) {
         sprintf(prefix, "Mapped disk #%d ", i);
         stat_print(&tracestats[i], prefix);
         stat_print(&tracestats1[i], prefix);
         stat_print(&tracestats2[i], prefix);
         stat_print(&tracestats3[i], prefix);
         stat_print(&tracestats4[i], prefix);
      }
   } else if ((tracestats2) && (printtracestats)) {
      /* info to help with validation */
      fprintf (outputfile, "\n");
      stat_print(tracestats2, "VALIDATE ");
      stat_print(tracestats3, "VALIDATE ");
      stat_print(tracestats4, "VALIDATE ");
      stat_print(tracestats5, "VALIDATE ");
      for (i=0; i<10; i++) {
	 cnt += validatebuf[i];
      }
      fprintf (outputfile, "VALIDATE double disconnects:  %5d  \t%f\n", validatebuf[0], ((double) validatebuf[0] / (double) cnt));
      fprintf (outputfile, "VALIDATE triple disconnects:  %5d  \t%f\n", validatebuf[1], ((double) validatebuf[1] / (double) cnt));
      fprintf (outputfile, "VALIDATE read buffer hits:    %5d  \t%f\n", validatebuf[3], ((double) validatebuf[3] / (double) cnt));
      fprintf (outputfile, "VALIDATE buffer misses:       %5d  \t%f\n", validatebuf[2], ((double) validatebuf[2] / (double) cnt));
   }

   iodriver_printstats();
   disk_printstats();
   controller_printstats();
   bus_printstats();
}


void io_param_override(structtype, paramname, paramval, first, last)
int structtype;
char *paramname;
char *paramval;
int first;
int last;
{
   switch (structtype) {

      case IOSIM:     if (strcmp(paramname, "ioscale") == 0) {
			 if (sscanf(paramval, "%lf", &ioscale) != 1) {
			    fprintf(stderr, "Error reading ioscale in io_param_override\n");
			    exit(0);
			 }
			 if (ioscale == 0.0) {
			    fprintf(stderr, "Invalid value for ioscale in io_param_override: %f\n", ioscale);
			    exit(0);
			 }
		      } else {
			 fprintf(stderr, "Upsupported IOSIM name at io_param_override: %s\n", paramname);
			 exit(0);
		      }
		      break;

      case IODRIVER:  iodriver_param_override(paramname, paramval, first, last);
		      break;

      case SYSORG:    iodriver_sysorg_param_override(paramname, paramval, first, last);
		      break;

      case DISK:      disk_param_override(paramname, paramval, first, last);
		      break;

      case CONTROLLER: controller_param_override(paramname, paramval, first, last);
		       break;

      case BUS:       bus_param_override(paramname, paramval, first, last);
		      break;

      default:   fprintf(stderr, "Unsupported structure type at io_param_override: %d\n", structtype);
		 exit(0);
   }
}


void io_initialize(standalone)
int standalone;
{
   StaticAssert (sizeof(ioreq_event) <= DISKSIM_EVENT_SIZE);
   disk_initialize();
   bus_initialize();
   controller_initialize();
   iodriver_initialize(standalone);
/*
   bus_print_phys_config();
*/
}


void io_resetstats()
{
   iodriver_resetstats();
   disk_resetstats();
   bus_resetstats();
   controller_resetstats();
}


void io_cleanstats()
{
   iodriver_cleanstats();
   disk_cleanstats();
   bus_cleanstats();
   controller_cleanstats();
}

