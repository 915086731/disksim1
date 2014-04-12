
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

/* disk_printhack = 0, 1, or 2 */
int disk_printhack = 0;
double disk_printhacktime = 0.0;

int  numdisks = 0;
disk *disks = NULL;

/* Used simply for tracking things for later debugging purposes (if nec.) */
double disk_last_read_arrival[MAXDISKS];
double disk_last_read_completion[MAXDISKS];
double disk_last_write_arrival[MAXDISKS];
double disk_last_write_completion[MAXDISKS];

int numsyncsets = 0;
int extra_write_disconnects = 0;

int disk_printqueuestats;
int disk_printcritstats;
int disk_printidlestats;
int disk_printintarrstats;
int disk_printsizestats;
int disk_printseekstats;
int disk_printlatencystats;
int disk_printxferstats;
int disk_printacctimestats;
int disk_printinterferestats;
int disk_printbufferstats;

char *statdesc_seekdiststats	=	"Seek distance";
char *statdesc_seektimestats	=	"Seek time";
char *statdesc_rotlatstats	=	"Rotational latency";
char *statdesc_xfertimestats	=	"Transfer time";
char *statdesc_acctimestats	=	"Access time";

extern double buffer_partial_servtime;
extern double reading_buffer_partial_servtime;
extern double buffer_whole_servtime;
extern double reading_buffer_whole_servtime;


int disk_get_numdisks()
{
   return(numdisks);
}


void disk_cleanstats()
{
   int i;

   for (i=0; i<numdisks; i++) {
      ioqueue_cleanstats(disks[i].queue);
   }
}


void disk_param_override(paramname, paramval, first, last)
char *paramname;
char *paramval;
int first;
int last;
{
   int i;

   if (first == -1) {
      first = 0;
      last = numdisks - 1;
   } else if ((first < 0) || (last >= numdisks) || (last < first)) {
      fprintf(stderr, "Invalid range at disk_param_override: %d - %d\n", first, last);
      exit(0);
   }
   i = first;
   for (i=first; i<=last; i++) {

      if ((strcmp(paramname, "ioqueue_schedalg") == 0) || (strcmp(paramname, "ioqueue_seqscheme") == 0) || (strcmp(paramname, "ioqueue_cylmaptype") == 0) || (strcmp(paramname, "ioqueue_to_time") == 0) || (strcmp(paramname, "ioqueue_timeout_schedalg") == 0) || (strcmp(paramname, "ioqueue_priority_schedalg") == 0) || (strcmp(paramname, "ioqueue_priority_mix") == 0)) {
         ioqueue_param_override(disks[i].queue, paramname, paramval);

      } else if (strcmp(paramname, "reading_buffer_whole_servtime") == 0) {
         if (sscanf(paramval, "%lf\n", &reading_buffer_whole_servtime) != 1) {
            fprintf(stderr, "Error reading reading_buffer_whole_servtime in disk_param_override\n");
            exit(0);
         }
         if (reading_buffer_whole_servtime < 0.0) {
            fprintf(stderr, "Invalid value for reading_buffer_whole_servtime in disk_param_override: %f\n", reading_buffer_whole_servtime);
            exit(0);
         }
      } else if (strcmp(paramname, "buffer_whole_servtime") == 0) {
         if (sscanf(paramval, "%lf\n", &buffer_whole_servtime) != 1) {
            fprintf(stderr, "Error reading buffer_whole_servtime in disk_param_override\n");
            exit(0);
         }
         if (buffer_whole_servtime < 0.0) {
            fprintf(stderr, "Invalid value for buffer_whole_servtime in disk_param_override: %f\n", buffer_whole_servtime);
            exit(0);
         }
      } else if (strcmp(paramname, "reading_buffer_partial_servtime") == 0) {
         if (sscanf(paramval, "%lf\n", &reading_buffer_partial_servtime) != 1) {
            fprintf(stderr, "Error reading reading_buffer_partial_servtime in disk_param_override\n");
            exit(0);
         }
         if (reading_buffer_partial_servtime < 0.0) {
            fprintf(stderr, "Invalid value for reading_buffer_partial_servtime in disk_param_override: %f\n", reading_buffer_partial_servtime);
            exit(0);
         }
      } else if (strcmp(paramname, "buffer_partial_servtime") == 0) {
         if (sscanf(paramval, "%lf\n", &buffer_partial_servtime) != 1) {
            fprintf(stderr, "Error reading buffer_partial_servtime in disk_param_override\n");
            exit(0);
         }
         if (buffer_partial_servtime < 0.0) {
            fprintf(stderr, "Invalid value for buffer_partial_servtime in disk_param_override: %f\n", buffer_partial_servtime);
            exit(0);
         }
      } else if (strcmp(paramname, "readaheadifidle") == 0) {
         if (sscanf(paramval, "%d\n", &disks[i].readaheadifidle) != 1) {
            fprintf(stderr, "Error reading readaheadifidle in disk_param_override\n");
            exit(0);
         }
         if ((disks[i].readaheadifidle != 0) && (disks[i].readaheadifidle != 1)) {
            fprintf(stderr, "Invalid value for readaheadifidle in disk_param_override: %d\n", disks[i].readaheadifidle);
            exit(0);
         }
      } else if (strcmp(paramname, "writecomb") == 0) {
         if (sscanf(paramval, "%d\n", &disks[i].writecomb) != 1) {
            fprintf(stderr, "Error reading writecomb in disk_param_override\n");
            exit(0);
         }
         if ((disks[i].writecomb != 0) && (disks[i].writecomb != 1)) {
            fprintf(stderr, "Invalid value for writecomb in disk_param_override: %d\n", disks[i].writecomb);
            exit(0);
         }
      } else if (strcmp(paramname, "maxqlen") == 0) {
         if (sscanf(paramval, "%d\n", &disks[i].maxqlen) != 1) {
            fprintf(stderr, "Error reading maxqlen in disk_param_override\n");
            exit(0);
         }
         if (disks[i].maxqlen < 0) {
            fprintf(stderr, "Invalid value for maxqlen in disk_param_override: %d\n", disks[i].maxqlen);
            exit(0);
         }
      } else if (strcmp(paramname, "hold_bus_for_whole_read_xfer") == 0) {
         if (sscanf(paramval, "%d", &disks[i].hold_bus_for_whole_read_xfer) != 1) {
            fprintf(stderr, "Error reading hold_bus_for_whole_read_xfer in disk_param_override\n");
            exit(0);
         }
         if ((disks[i].hold_bus_for_whole_read_xfer != 0) && (disks[i].hold_bus_for_whole_read_xfer != 1)) {
            fprintf(stderr, "Invalid value for hold_bus_for_whole_read_xfer in disk_param_override: %d\n", disks[i].hold_bus_for_whole_read_xfer);
            exit(0);
         }
      } else if (strcmp(paramname, "hold_bus_for_whole_write_xfer") == 0) {
         if (sscanf(paramval, "%d", &disks[i].hold_bus_for_whole_write_xfer) != 1) {
            fprintf(stderr, "Error reading hold_bus_for_whole_write_xfer in disk_param_override\n");
            exit(0);
         }
         if ((disks[i].hold_bus_for_whole_write_xfer != 0) && (disks[i].hold_bus_for_whole_write_xfer != 1)) {
            fprintf(stderr, "Invalid value for hold_bus_for_whole_write_xfer in disk_param_override: %d\n", disks[i].hold_bus_for_whole_write_xfer);
            exit(0);
         }
         if ((disks[i].hold_bus_for_whole_read_xfer == 1) &&
             ((disks[i].sneakyfullreadhits == 1) ||
              (disks[i].sneakypartialreadhits == 1) ||
              (disks[i].sneakyintermediatereadhits == 1))) {
            fprintf(stderr, "hold_bus_for_whole_read_xfer and one or more sneakyreadhits detected disk_param_override\n");
            exit(0);
         }
      } else if (strcmp(paramname, "almostreadhits") == 0) {
         if (sscanf(paramval, "%d", &disks[i].almostreadhits) != 1) {
            fprintf(stderr, "Error reading almostreadhits in disk_param_override\n");
            exit(0);
         }
         if ((disks[i].almostreadhits != 0) && (disks[i].almostreadhits != 1)) {
            fprintf(stderr, "Invalid value for almostreadhits in disk_param_override: %d\n", disks[i].almostreadhits);
            exit(0);
         }
      } else if (strcmp(paramname, "sneakyfullreadhits") == 0) {
         if (sscanf(paramval, "%d", &disks[i].sneakyfullreadhits) != 1) {
            fprintf(stderr, "Error reading sneakyfullreadhits in disk_param_override\n");
            exit(0);
         }
         if ((disks[i].sneakyfullreadhits != 0) && (disks[i].sneakyfullreadhits != 1)) {
            fprintf(stderr, "Invalid value for sneakyfullreadhits in disk_param_override: %d\n", disks[i].sneakyfullreadhits);
            exit(0);
         }
         if ((disks[i].sneakyfullreadhits == 1) && (disks[i].hold_bus_for_whole_read_xfer == 1)) {
            fprintf(stderr, "hold_bus_for_whole_read_xfer and sneakyfullreadhits detected disk_param_override\n");
            exit(0);
         }
      } else if (strcmp(paramname, "sneakypartialreadhits") == 0) {
         if (sscanf(paramval, "%d", &disks[i].sneakypartialreadhits) != 1) {
            fprintf(stderr, "Error reading sneakypartialreadhits in disk_param_override\n");
            exit(0);
         }
         if ((disks[i].sneakypartialreadhits != 0) && (disks[i].sneakypartialreadhits != 1)) {
            fprintf(stderr, "Invalid value for sneakypartialreadhits in disk_param_override: %d\n", disks[i].sneakypartialreadhits);
            exit(0);
         }
         if ((disks[i].sneakypartialreadhits == 1) && (disks[i].hold_bus_for_whole_read_xfer == 1)) {
            fprintf(stderr, "hold_bus_for_whole_read_xfer and sneakypartialreadhits detected disk_param_override\n");
            exit(0);
         }
      } else if (strcmp(paramname, "sneakyintermediatereadhits") == 0) {
         if (sscanf(paramval, "%d", &disks[i].sneakyintermediatereadhits) != 1) {
            fprintf(stderr, "Error reading sneakyintermediatereadhits in disk_param_override\n");
            exit(0);
         }
         if ((disks[i].sneakyintermediatereadhits != 0) && (disks[i].sneakyintermediatereadhits != 1)) {
            fprintf(stderr, "Invalid value for sneakyintermediatereadhits in disk_param_override: %d\n", disks[i].sneakyintermediatereadhits);
            exit(0);
         }
         if ((disks[i].sneakyintermediatereadhits == 1) &&
             (disks[i].hold_bus_for_whole_read_xfer == 1)) {
            fprintf(stderr, "hold_bus_for_whole_read_xfer and sneakyintermediatereadhits detected disk_param_override\n");
            exit(0);
         }
      } else if (strcmp(paramname, "readhitsonwritedata") == 0) {
         if (sscanf(paramval, "%d", &disks[i].readhitsonwritedata) != 1) {
            fprintf(stderr, "Error reading readhitsonwritedata in disk_param_override\n");
            exit(0);
         }
         if ((disks[i].readhitsonwritedata != 0) && (disks[i].readhitsonwritedata != 1)) {
            fprintf(stderr, "Invalid value for readhitsonwritedata in disk_param_override: %d\n", disks[i].readhitsonwritedata);
            exit(0);
         }
      } else if (strcmp(paramname, "writeprebuffering") == 0) {
         if (sscanf(paramval, "%d", &disks[i].writeprebuffering) != 1) {
            fprintf(stderr, "Error reading writeprebuffering in disk_param_override\n");
            exit(0);
         }
         if ((disks[i].writeprebuffering != 0) && (disks[i].writeprebuffering != 1)) {
            fprintf(stderr, "Invalid value for writeprebuffering in disk_param_override: %d\n", disks[i].writeprebuffering);
            exit(0);
         }
      } else if (strcmp(paramname, "preseeking") == 0) {
         if (sscanf(paramval, "%d", &disks[i].preseeking) != 1) {
            fprintf(stderr, "Error reading preseeking in disk_param_override\n");
            exit(0);
         }
         if ((disks[i].preseeking != 0) && (disks[i].preseeking != 1) && (disks[i].preseeking != 2)) {
            fprintf(stderr, "Invalid value for preseeking in disk_param_override: %d\n", disks[i].preseeking);
            exit(0);
         }
      } else if (strcmp(paramname, "neverdisconnect") == 0) {
         if (sscanf(paramval, "%d", &disks[i].neverdisconnect) != 1) {
            fprintf(stderr, "Error reading neverdisconnect in disk_param_override\n");
            exit(0);
         }
         if ((disks[i].neverdisconnect != 0) && (disks[i].neverdisconnect != 1)) {
            fprintf(stderr, "Invalid value for neverdisconnect in disk_param_override: %d\n", disks[i].neverdisconnect);
            exit(0);
         }
      } else if (strcmp(paramname, "numsegs") == 0) {
         if (sscanf(paramval, "%d", &disks[i].numsegs) != 1) {
            fprintf(stderr, "Error reading numsegs in disk_param_override\n");
            exit(0);
         }
         if (disks[i].numsegs < 1) {
            fprintf(stderr, "Invalid value for numsegs in disk_param_override: %d\n", disks[i].numsegs);
            exit(0);
         }
      } else if (strcmp(paramname, "segsize") == 0) {
         if (sscanf(paramval, "%d", &disks[i].segsize) != 1) {
            fprintf(stderr, "Error reading segsize in disk_param_override\n");
            exit(0);
         }
         if ((disks[i].segsize < 1) ||
	     (disks[i].segsize > disks[i].numblocks)) {
            fprintf(stderr, "Invalid value for segsize in disk_param_override: %d\n", disks[i].segsize);
            exit(0);
         }
      } else if (strcmp(paramname, "numwritesegs") == 0) {
         if (sscanf(paramval, "%d", &disks[i].numwritesegs) != 1) {
            fprintf(stderr, "Error reading numwritesegs in disk_param_override\n");
            exit(0);
         }
         if ((disks[i].numwritesegs > disks[i].numsegs) || (disks[i].numwritesegs < 1)) {
            fprintf(stderr, "Invalid value for numwritesegs in disk_param_override: %d\n", disks[i].numwritesegs);
            exit(0);
         }
      } else if (strcmp(paramname, "dedicatedwriteseg") == 0) {
         int useded;
         if (sscanf(paramval, "%d", &useded) != 1) {
            fprintf(stderr, "Error reading dedicatedwriteseg in disk_param_override\n");
            exit(0);
         }
         disks[i].dedicatedwriteseg = NULL;;
         if ((disks[i].numsegs > 1) && (useded)) {
            disks[i].dedicatedwriteseg++;
         }
         if ((useded != 0) && (useded != 1)) {
            fprintf(stderr, "Invalid value for dedicatedwriteseg in disk_param_override: %d\n", useded);
            exit(0);
         }
      } else if (strcmp(paramname, "fastwrites") == 0) {
         if (sscanf(paramval, "%d", &disks[i].fastwrites) != 1) {
            fprintf(stderr, "Error reading fastwrites in disk_param_override\n");
            exit(0);
         }
         if ((disks[i].fastwrites != 0) && (disks[i].fastwrites != 1) && (disks[i].fastwrites != 2)) {
            fprintf(stderr, "Invalid value for fastwrites in disk_param_override: %d\n", disks[i].fastwrites);
            exit(0);
         }
         if ((disks[i].fastwrites != 0) && (disks[i].enablecache == FALSE)) {
            fprintf(stderr, "disk_param_override:  Can't use fast write if not employing caching\n");
            exit(0);
         }
      } else {
	 fprintf(stderr, "Unsupported param to override at disk_param_override: %s\n", paramname);
	 exit(0);
      }
   }
}


void diskstatinit(diskno, firsttime)
int diskno;
int firsttime;
{
   diskstat *stat = &disks[diskno].stat;

   if (firsttime) {
      stat_initialize(statdeffile, statdesc_seekdiststats, &stat->seekdiststats);
      stat_initialize(statdeffile, statdesc_seektimestats, &stat->seektimestats);
      stat_initialize(statdeffile, statdesc_rotlatstats, &stat->rotlatstats);
      stat_initialize(statdeffile, statdesc_xfertimestats, &stat->xfertimestats);
      stat_initialize(statdeffile, statdesc_acctimestats, &stat->acctimestats);
   } else {
      stat_reset(&stat->seekdiststats);
      stat_reset(&stat->seektimestats);
      stat_reset(&stat->rotlatstats);
      stat_reset(&stat->xfertimestats);
      stat_reset(&stat->acctimestats);
   }

   stat->highblkno = 0;
   stat->zeroseeks = 0;
   stat->zerolatency = 0;
   stat->writecombs = 0;
   stat->readmisses = 0;
   stat->writemisses = 0;
   stat->fullreadhits = 0;
   stat->appendhits = 0;
   stat->prependhits = 0;
   stat->readinghits = 0;
   stat->runreadingsize = 0.0;
   stat->remreadingsize = 0.0;
   stat->parthits = 0;
   stat->runpartsize = 0.0;
   stat->rempartsize = 0.0;
   stat->interfere[0] = 0;
   stat->interfere[1] = 0;
   stat->requestedbus = 0.0;
   stat->waitingforbus = 0.0;
   stat->numbuswaits = 0;
}


void disk_syncset_init()
{
   int i, j;
   int synced[128];

   for (i=1; i<=numsyncsets; i++) {
      synced[i] = 0;
   }
   for (i=0; i<numdisks; i++) {
      if (disks[i].syncset == 0) {
         disks[i].rpm -= (disks[i].rpm * disks[i].rpmerr * (double) 0.01 *
			 ((double) 1 - ((double) 2 * drand48())));
         disks[i].currangle = drand48();
      } else if ((disks[i].syncset > 0) && (synced[(disks[i].syncset)] == 0)) {
         disks[i].rpm -= (disks[i].rpm * disks[i].rpmerr * (double) 0.01 *
			 ((double) 1 - ((double) 2 * drand48())));
         disks[i].currangle = drand48();
         for (j=i; j<numdisks; j++) {
	    if (disks[j].syncset == disks[i].syncset) {
	       disks[j].rpm = disks[i].rpm;
	       disks[j].currangle = disks[i].currangle;
	    }
	 }
	 synced[(disks[i].syncset)] = 1;
      }
   }
}


void disk_initialize()
{
   int i, j;
   diskreq *tmpdiskreq;
   segment *tmpseg;
   double tmptime;
   double rotblks;
   double tmpfull;
   double tmpavg;
/*
fprintf (outputfile, "Entered disk_initialize - numdisks %d\n", numdisks);
*/
   StaticAssert (sizeof(segment) <= DISKSIM_EVENT_SIZE);
   StaticAssert (sizeof(diskreq) <= DISKSIM_EVENT_SIZE);

   disk_syncset_init();
   for (i = 0; i < numdisks; i++) {
      ioqueue_initialize(disks[i].queue, i);
      addlisttoextraq((event *) &disks[i].outwait);
      addlisttoextraq((event *) &disks[i].buswait);
      if (disks[i].currentbus) {
	 if (disks[i].currentbus == disks[i].effectivebus) {
	    disks[i].effectivebus = NULL;
	 }
	 tmpdiskreq = disks[i].currentbus;
	 if (tmpdiskreq->seg) {
	    disk_buffer_remove_from_seg(tmpdiskreq);
	 }
         addlisttoextraq((event *) &tmpdiskreq->ioreqlist);
	 disks[i].currentbus = NULL;
	 addtoextraq((event *) tmpdiskreq);
      }
      if (disks[i].effectivebus) {
	 tmpdiskreq = disks[i].effectivebus;
	 if (tmpdiskreq->seg) {
	    disk_buffer_remove_from_seg(tmpdiskreq);
	 }
         addlisttoextraq((event *) &tmpdiskreq->ioreqlist);
	 disks[i].effectivebus = NULL;
	 addtoextraq((event *) tmpdiskreq);
      }
      if (disks[i].currenthda) {
	 if (disks[i].currenthda == disks[i].effectivehda) {
	    disks[i].effectivehda = NULL;
	 }
	 tmpdiskreq = disks[i].currenthda;
	 if (tmpdiskreq->seg) {
	    disk_buffer_remove_from_seg(tmpdiskreq);
	 }
         addlisttoextraq((event *) &tmpdiskreq->ioreqlist);
	 disks[i].currenthda = NULL;
	 addtoextraq((event *) tmpdiskreq);
      }
      if (disks[i].effectivehda != NULL) {
	 tmpdiskreq = disks[i].effectivehda;
	 if (tmpdiskreq->seg) {
	    disk_buffer_remove_from_seg(tmpdiskreq);
	 }
         addlisttoextraq((event *) &tmpdiskreq->ioreqlist);
	 disks[i].effectivehda = NULL;
	 addtoextraq((event *) tmpdiskreq);
      }
      while (disks[i].pendxfer) {
	 tmpdiskreq = disks[i].pendxfer;
	 if (tmpdiskreq->seg) {
	    disk_buffer_remove_from_seg(tmpdiskreq);
	 }
         addlisttoextraq((event *) &tmpdiskreq->ioreqlist);
	 disks[i].pendxfer = tmpdiskreq->bus_next;
	 addtoextraq((event *) tmpdiskreq);
      }
      disks[i].numinbuses = 0;
      disks[i].outstate = DISK_IDLE;
      disks[i].busy = FALSE;
      disks[i].prev_readahead_min = -1;
      disks[i].translatesectbysect = FALSE;
      disks[i].extradisc_diskreq = NULL;
      disks[i].currcylno = 0;
      disks[i].currsurface = 0;
      disks[i].currtime = 0.0;
      disks[i].lastflags = READ;
      disks[i].rotatetime = (double) MSECS_PER_MIN / disks[i].rpm;
      disks[i].lastgen = -1;
      disks[i].busowned = -1;
      disks[i].numdirty = 0;
      if (disks[i].seglist == NULL) {
	 disks[i].seglist = (segment *) malloc(sizeof(segment));
	 disks[i].seglist->next = NULL;
	 disks[i].seglist->prev = NULL;
	 disks[i].seglist->diskreqlist = NULL;
	 disks[i].seglist->recyclereq = NULL;
	 disks[i].seglist->access = NULL;
	 for (j = 1; j < disks[i].numsegs; j++) {
	    tmpseg = (segment *) malloc(sizeof(segment));
	    tmpseg->next = disks[i].seglist;
	    disks[i].seglist = tmpseg;
            tmpseg->next->prev = tmpseg;
	    tmpseg->prev = NULL;
	    tmpseg->diskreqlist = NULL;
	    tmpseg->recyclereq = NULL;
	    tmpseg->access = NULL;
	 }
	 if (disks[i].dedicatedwriteseg) {
	    disks[i].dedicatedwriteseg = disks[i].seglist;
	 }
      }
      tmpseg = disks[i].seglist;
      for (j = 0; j < disks[i].numsegs; j++) {
	 tmpseg->outstate = BUFFER_IDLE;
	 tmpseg->state = BUFFER_EMPTY;
	 tmpseg->size = disks[i].segsize;
	 while (tmpseg->diskreqlist) {
            addlisttoextraq((event *) &tmpseg->diskreqlist->ioreqlist);
	    tmpdiskreq = tmpseg->diskreqlist;
	    tmpseg->diskreqlist = tmpdiskreq->seg_next;
	    addtoextraq((event *) tmpdiskreq);
	 }
	 /* recyclereq should have already been "recycled" :) by the
	    effectivehda or currenthda recycling above */
	 tmpseg->recyclereq = NULL;
         addlisttoextraq((event *) &tmpseg->access);
	 tmpseg = tmpseg->next;
      }
      for (j=0; j<disks[i].numbands; j++) {
	 tmptime = max(diskseektime(&disks[i], 0, 1, 0), diskseektime(&disks[i], 0, 1, 1));
	 rotblks = (double) disks[i].bands[j].blkspertrack / disks[i].rotatetime;
         disks[i].bands[j].firstblkno = disks[i].bands[j].firstblkno / rotblks;
	 if (disks[i].bands[j].trackskew == -1.0) {
	    disks[i].bands[j].trackskew = tmptime;
	 } else if (disks[i].bands[j].trackskew == -2.0) {
	    tmptime = (double) ((int) (tmptime * rotblks + 0.999999999));
	    disks[i].bands[j].trackskew = tmptime / rotblks;
	 } else {
	    disks[i].bands[j].trackskew = disks[i].bands[j].trackskew / rotblks;
	 }
	 tmptime = max(diskseektime(&disks[i], 1, 0, 0), diskseektime(&disks[i], 1, 0, 1));
	 if (disks[i].bands[j].cylskew == -1.0) {
	    disks[i].bands[j].cylskew = tmptime;
	 } else if (disks[i].bands[j].cylskew == -2.0) {
	    tmptime = (double) ((int) (tmptime * rotblks + 0.99999999));
	    disks[i].bands[j].cylskew = tmptime / rotblks;
	 } else {
	    disks[i].bands[j].cylskew = disks[i].bands[j].cylskew / rotblks;
	 }
      }
      if ((disks[i].seektime == THREEPOINT_CURVE) && (disks[i].seekavg > disks[i].seekone)) {
fprintf (outputfile, "seekone %f, seekavg %f, seekfull %f\n", disks[i].seekone, disks[i].seekavg, disks[i].seekfull);
	 tmpfull = disks[i].seekfull;
	 tmpavg = disks[i].seekavg;
	 tmptime = (double) -10.0 * disks[i].seekone;
	 tmptime += (double) 15.0 * disks[i].seekavg;
	 tmptime += (double) -5.0 * disks[i].seekfull;
	 tmptime = tmptime / ((double) 3 * sqrt((double) (disks[i].numcyls)));
	 disks[i].seekavg *= (double) -15.0;
	 disks[i].seekavg += (double) 7.0 * disks[i].seekone;
	 disks[i].seekavg += (double) 8.0 * disks[i].seekfull;
	 disks[i].seekavg = disks[i].seekavg / (double) (3 * disks[i].numcyls);
	 disks[i].seekfull = tmptime;
fprintf (outputfile, "seekone %f, seekavg %f, seekfull %f\n", disks[i].seekone, disks[i].seekavg, disks[i].seekfull);
fprintf (outputfile, "seekone %f, seekavg %f, seekfull %f\n", diskseektime(&disks[i], 1, 0, 1), diskseektime(&disks[i], (disks[i].numcyls / 3), 0, 1), diskseektime(&disks[i], (disks[i].numcyls - 1), 0, 1));
	 if ((disks[i].seekavg < 0.0) || (disks[i].seekfull < 0.0)) {
	    disks[i].seektime = THREEPOINT_CURVE;
	    disks[i].seekfull = tmpfull;
	    disks[i].seekavg = tmpavg;
	 }
      }
      diskstatinit(i, TRUE);
      disk_check_numblocks(&disks[i]);
   }
}


void disk_resetstats()
{
   int i;

   for (i=0; i<numdisks; i++) {
      ioqueue_resetstats(disks[i].queue);
      diskstatinit(i, 0);
   }
}


void disk_read_toprints(parfile)
FILE *parfile;
{
   getparam_bool(parfile, "Print disk queue stats?", &disk_printqueuestats);
   getparam_bool(parfile, "Print disk crit stats?", &disk_printcritstats);
   getparam_bool(parfile, "Print disk idle stats?", &disk_printidlestats);
   getparam_bool(parfile, "Print disk intarr stats?", &disk_printintarrstats);
   getparam_bool(parfile, "Print disk size stats?", &disk_printsizestats);
   getparam_bool(parfile, "Print disk seek stats?", &disk_printseekstats);
   getparam_bool(parfile, "Print disk latency stats?", &disk_printlatencystats);
   getparam_bool(parfile, "Print disk xfer stats?", &disk_printxferstats);
   getparam_bool(parfile, "Print disk acctime stats?", &disk_printacctimestats);
   getparam_bool(parfile, "Print disk interfere stats?", &disk_printinterferestats);
   getparam_bool(parfile, "Print disk buffer stats?", &disk_printbufferstats);
}


void bandcopy(destbands, srcbands, numbands)
band *destbands;
band *srcbands;
int numbands;
{
   int i, j;

   for (i=0; i<numbands; i++) {
      destbands[i].startcyl = srcbands[i].startcyl;
      destbands[i].endcyl = srcbands[i].endcyl;
      destbands[i].firstblkno = srcbands[i].firstblkno;
      destbands[i].blkspertrack = srcbands[i].blkspertrack;
      destbands[i].deadspace = srcbands[i].deadspace;
      destbands[i].trackskew = srcbands[i].trackskew;
      destbands[i].cylskew = srcbands[i].cylskew;
      destbands[i].blksinband = srcbands[i].blksinband;
      destbands[i].sparecnt = srcbands[i].sparecnt;
      destbands[i].numslips = srcbands[i].numslips;
      destbands[i].numdefects = srcbands[i].numdefects;
      for (j=0; j<srcbands[i].numslips; j++) {
         destbands[i].slip[j] = srcbands[i].slip[j];
      }
      for (j=0; j<srcbands[i].numdefects; j++) {
         destbands[i].defect[j] = srcbands[i].defect[j];
         destbands[i].remap[j] = srcbands[i].remap[j];
      }
   }
}


void band_read_specs(bands, numbands, parfile, numsurfaces, sparescheme)
band *bands;
int numbands;
FILE *parfile;
int numsurfaces;
int sparescheme;
{
   int i;
   int bandno;
   int bandno_expected;
   int startcyl;
   int endcyl;
   int blkspertrack;
   int deadspace;
   int sparecnt;
   int numslips;
   int numdefects;
   int defect;
   int remap;

   for (bandno_expected=1; bandno_expected<=numbands; bandno_expected++) {
      if (fscanf(parfile, "Band #%d\n", &bandno) != 1) {
	 fprintf(stderr, "Error reading band number\n");
	 exit(0);
      }
      if (bandno != bandno_expected) {
	 fprintf(stderr, "Invalid value for band number - %d\n", bandno);
	 exit(0);
      }
      fprintf (outputfile, "Band #%d\n", bandno);
      bandno--;

      getparam_int(parfile, "First cylinder number", &startcyl, 1, 0, 0);
      bands[bandno].startcyl = startcyl;

      getparam_int(parfile, "Last cylinder number", &endcyl, 1, 0, 0);
      bands[bandno].endcyl = endcyl;

      getparam_int(parfile, "Blocks per track", &blkspertrack, 1, 1, 0);
      bands[bandno].blkspertrack = blkspertrack;

      getparam_double(parfile, "Offset of first block", &bands[bandno].firstblkno, 1, (double) 0.0, (double) 0.0);

      getparam_int(parfile, "Empty space at zone front", &deadspace, 1, 0, 0);
      bands[bandno].deadspace = deadspace;

      getparam_double(parfile, "Skew for track switch", &bands[bandno].trackskew, 1, (double) -2.0, (double) 0.0);
      getparam_double(parfile, "Skew for cylinder switch", &bands[bandno].cylskew, 1, (double) -2.0, (double) 0.0);

      getparam_int(parfile, "Number of spares", &sparecnt, 1, 0, 0);
      bands[bandno].sparecnt = sparecnt;

      getparam_int(parfile, "Number of slips", &numslips, 3, 0, MAXSLIPS);
      bands[bandno].numslips = numslips;
      for (i=0; i<numslips; i++) {
         getparam_int(parfile, "Slip", &bands[bandno].slip[i], 1, 0, 0);
      }

      getparam_int(parfile, "Number of defects", &numdefects, 3, 0, MAXDEFECTS);
      bands[bandno].numdefects = numdefects;
      for (i=0; i<numdefects; i++) {
         if (fscanf(parfile, "Defect: %d %d\n", &defect, &remap) != 2) {
	    fprintf(stderr, "Error reading defect #%d\n", i);
	    exit(0);
         }
         if ((defect < 0) || (remap < 0)) {
	    fprintf(stderr, "Invalid value(s) for defect - %d %d\n", defect, remap);
	    exit(0);
         }
         bands[bandno].defect[i] = defect;
         bands[bandno].remap[i] = remap;
         fprintf (outputfile, "Defect: %d %d\n", defect, remap);
      }

      if ((sparescheme == TRACK_SPARING) && ((numslips + numdefects) > sparecnt)) {
	 fprintf(stderr, "Defects and slips outnumber the available spares: %d < %d + %d\n", sparecnt, numdefects, numslips);
	 exit(0);
      }

      bands[bandno].blksinband = (endcyl-startcyl+1)*blkspertrack*numsurfaces;
      bands[bandno].blksinband -= deadspace;
      if (sparescheme == TRACK_SPARING) {
	 bands[bandno].blksinband -= sparecnt * blkspertrack;
      } else if (sparescheme == SECTPERCYL_SPARING) {
	 bands[bandno].blksinband -= sparecnt * (endcyl - startcyl + 1);
      } else if (sparescheme == SECTPERTRACK_SPARING) {
	 bands[bandno].blksinband -= sparecnt * (endcyl-startcyl+1) * numsurfaces;
      }
fprintf (outputfile, "blksinband %d, sparecnt %d, blkspertrack %d, numtracks %d\n", bands[bandno].blksinband, sparecnt, blkspertrack, ((endcyl-startcyl+1)*numsurfaces));
   }
}


FILE * disk_locate_spec_in_specfile(parfile, brandname, line)
FILE *parfile;
char *brandname;
char *line;
{
   FILE *specfile;
   char specname[200];
   char specfilename[200];

   if (!fgets (line, 200, parfile)) {
      fprintf (stderr, "Unable to read disk specification file name from parfile\n");
      exit (0);
   }
   if (sscanf (line, "Disk specification file: %s", specfilename) != 1) {
      fprintf (stderr, "Bad format for `Disk specification file:'");
      exit (0);
   }
   if ((specfile = fopen(specfilename, "r")) == NULL) {
      fprintf(stderr, "Disk specifcation file %s cannot be opened for read access\n", specfilename);
      exit(0);
   }
   sprintf(specname, "Disk brand name: %s\n", brandname);
   while (fgets(line, 200, specfile)) {
      if (strcmp(line, specname) == 0) {
	 return(specfile);
      }
   }
   fprintf(stderr, "Disk specification for %s not found in file %s\n", brandname, specfilename);
   exit(0);
}


void disk_read_specs(parfile)
FILE *parfile;
{
   char line[201];
   FILE *specfile = parfile;
   char brandname[81];
   int i, j;
   int copies;
   int specno;
   int specno_expected = 1;
   int diskno = 0;
   double acctime;
   double seektime;
   char seekinfo[81];
   int extractseekcnt = 0;
   int *extractseekdists = NULL;
   double *extractseektimes = NULL;
   double seekavg;
   int numbands;
   int enablecache;
   int contread;
   double hp1, hp2, hp3, hp4, hp5, hp6;
   double first10seeks[10];
   disk *currdisk;
   int useded;

   getparam_int(parfile, "\nNumber of disks", &numdisks, 3, 0, MAXDISKS);
   if (!numdisks) {
      return;
   }

   disks = (disk*) malloc(numdisks * (sizeof(disk)));
   ASSERT(disks != NULL);

   while (diskno < numdisks) {
      currdisk = &disks[diskno];
      if (fscanf(parfile, "\nDisk Spec #%d\n", &specno) != 1) {
	 fprintf(stderr, "Error reading disk spec number\n");
	 exit(0);
      }
      if (specno != specno_expected) {
	 fprintf(stderr, "Unexpected value for disk spec number: specno %d, expected %d\n", specno, specno_expected);
	 exit(0);
      }
      fprintf (outputfile, "\nDisk Spec #%d\n", specno);
      specno_expected++;

      getparam_int(parfile, "# disks with Spec", &copies, 1, 0, 0);
      if (copies == 0) {
	 copies = numdisks - diskno;
      }

      if ((diskno+copies) > numdisks) {
	 fprintf(stderr, "Too many disk specifications provided\n");
	 exit(0);
      }

      if (fgets(line, 200, parfile) == NULL) {
	 fprintf(stderr, "End of file before disk specifications\n");
	 exit(0);
      }
      specfile = parfile;
      if (sscanf(line, "Disk brand name: %s\n", brandname) == 1) {
	 specfile = disk_locate_spec_in_specfile(parfile, brandname, line);
         if (fgets(line, 200, specfile) == NULL) {
            fprintf(stderr, "End of file before disk specifications\n");
            exit(0);
         }
      }

      if (sscanf(line, "Access time (in msecs): %lf\n", &acctime) != 1) {
         fprintf(stderr, "Error reading access time\n");
         exit(0);
      }
      if ((acctime < 0.0) && (!diskspecialaccesstime(acctime))) {
	 fprintf(stderr, "Invalid value for access time - %f\n", acctime);
	 exit(0);
      }
      fprintf (outputfile, "Access time (in msecs): %f\n", acctime);

      getparam_double(specfile, "Seek time (in msecs)", &seektime, 0, (double) 0.0, (double) 0.0);
      if ((seektime < 0.0) && (!diskspecialseektime(seektime))) {
         fprintf(stderr, "Invalid value for seek time - %f\n", seektime);
         exit(0);
      }

      getparam_double(specfile, "Single cylinder seek time", &currdisk->seekone, 1, (double) 0.0, (double) 0.0);

      if (fscanf(specfile, "Average seek time: %s\n", seekinfo) != 1) {
	 fprintf(stderr, "Error reading average seek time\n");
	 exit(0);
      }
      if (seektime == EXTRACTION_SEEK) {
	 seekavg = 0.0;
         disk_read_extracted_seek_curve(seekinfo, &extractseekcnt, &extractseekdists, &extractseektimes);
      } else {
	 if (sscanf(seekinfo, "%lf", &seekavg) != 1) {
	    fprintf(stderr, "Invalid value for average seek time: %s\n", seekinfo);
	    exit(0);
	 }
      }
      if (seekavg < 0.0) {
         fprintf(stderr, "Invalid value for average seek time - %f\n", seekavg);
         exit(0);
      }
      fprintf (outputfile, "Average seek time: %s\n", seekinfo);

      getparam_double(specfile, "Full strobe seek time", &currdisk->seekfull, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Add. write settling delay", &currdisk->seekwritedelta, 0, (double) 0.0, (double) 0.0);

      if (fscanf(specfile, "HPL seek equation values: %lf %lf %lf %lf %lf %lf\n", &hp1, &hp2, &hp3, &hp4, &hp5, &hp6) != 6) {
	 fprintf(stderr, "Error reading HPL seek equation values\n");
	 exit(0);
      }
      if ((hp1 < 0) | (hp2 < 0) | (hp3 < 0) | (hp4 < 0) | (hp5 < 0) | (hp6 < -1)) {
         fprintf(stderr, "Invalid value for an HPL seek equation values - %f %f %f %f %f %f\n", hp1, hp2, hp3, hp4, hp5, hp6);
         exit(0);
      }
      fprintf (outputfile, "HPL seek equation values: %f %f %f %f %f %f\n", hp1, hp2, hp3, hp4, hp5, hp6);
      if (hp6 != -1) {
	 currdisk->seekone = hp6;
      }

      if ((j = fscanf(specfile, "First 10 seek times: %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf\n", &first10seeks[0], &first10seeks[1], &first10seeks[2], &first10seeks[3], &first10seeks[4], &first10seeks[5], &first10seeks[6], &first10seeks[7], &first10seeks[8], &first10seeks[9])) == 0) {
	 fprintf(stderr, "Error reading first 10 seek times\n");
	 exit(0);
      }
      if (seektime == FIRST10_PLUS_HPL_SEEK_EQUATION) {
         if ((j < 10) |(first10seeks[0] < 0) | (first10seeks[1] < 0) | (first10seeks[2] < 0) | (first10seeks[3] < 0) | (first10seeks[4] < 0) | (first10seeks[5] < 0) | (first10seeks[6] < 0) | (first10seeks[7] < 0) | (first10seeks[8] < 0) | (first10seeks[9] < 0)) {
            fprintf(stderr, "Invalid value for a First 10 seek times: %d - %f %f %f %f %f %f %f %f %f %f\n", j, first10seeks[0], first10seeks[1], first10seeks[2], first10seeks[3], first10seeks[4], first10seeks[5], first10seeks[6], first10seeks[7], first10seeks[8], first10seeks[9]);
            exit(0);
         }
         fprintf (outputfile, "First 10 seek times:    %f %f %f %f %f %f %f %f %f %f\n", first10seeks[0], first10seeks[1], first10seeks[2], first10seeks[3], first10seeks[4], first10seeks[5], first10seeks[6], first10seeks[7], first10seeks[8], first10seeks[9]);
      } else {
         fprintf (outputfile, "First 10 seek times:    %f\n", first10seeks[0]);
      }

      getparam_double(specfile, "Head switch time", &currdisk->headswitch, 1, (double) 0.0, (double) 0.0);

      getparam_double(specfile, "Rotation speed (in rpms)", &currdisk->rpm, 4, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Percent error in rpms", &currdisk->rpmerr, 3, (double) 0.0, (double) 100.0);

      getparam_int(specfile, "Number of data surfaces", &currdisk->numsurfaces, 1, 1, 0);
      getparam_int(specfile, "Number of cylinders", &currdisk->numcyls, 1, 1, 0);
      getparam_int(specfile, "Blocks per disk", &currdisk->numblocks, 1, 1, 0);

      getparam_int(specfile, "Sparing scheme used", &currdisk->sparescheme, 3, NO_SPARING, MAXSPARESCHEME);

      getparam_double(specfile, "Per-request overhead time", &currdisk->overhead, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Time scale for overheads", &currdisk->timescale, 1, (double) 0.0, (double) 0.0);

      getparam_double(specfile, "Bulk sector transfer time", &currdisk->blktranstime, 1, (double) 0.0, (double) 0.0);
      getparam_bool(specfile, "Hold bus entire read xfer:", &currdisk->hold_bus_for_whole_read_xfer);
      getparam_bool(specfile, "Hold bus entire write xfer:", &currdisk->hold_bus_for_whole_write_xfer);

      getparam_bool(specfile, "Allow almost read hits:", &currdisk->almostreadhits);
      getparam_bool(specfile, "Allow sneaky full read hits:", &currdisk->sneakyfullreadhits);
      getparam_bool(specfile, "Allow sneaky partial read hits:", &currdisk->sneakypartialreadhits);
      getparam_bool(specfile, "Allow sneaky intermediate read hits:", &currdisk->sneakyintermediatereadhits);
      getparam_bool(specfile, "Allow read hits on write data:", &currdisk->readhitsonwritedata);
      getparam_bool(specfile, "Allow write prebuffering:", &currdisk->writeprebuffering);
      getparam_int(specfile, "Preseeking level", &currdisk->preseeking, 3, 0, 2);
      getparam_bool(specfile, "Never disconnect:", &currdisk->neverdisconnect);
      getparam_bool(specfile, "Print stats for disk:", &currdisk->printstats);
      getparam_int(specfile, "Avg sectors per cylinder", &currdisk->sectpercyl, 3, 1, currdisk->numblocks);

      getparam_int(specfile, "Max queue length at disk", &currdisk->maxqlen, 1, 0, 0);
      currdisk->queue = ioqueue_readparams(specfile, disk_printqueuestats, disk_printcritstats, disk_printidlestats, disk_printintarrstats, disk_printsizestats);

      getparam_int(specfile, "Number of buffer segments", &currdisk->numsegs, 1, 1, 0);
      getparam_int(specfile, "Maximum number of write segments", &currdisk->numwritesegs, 3, 1, currdisk->numsegs);
      getparam_int(specfile, "Segment size (in blks)", &currdisk->segsize, 3, 1, currdisk->numblocks);
      getparam_bool(specfile, "Use separate write segment:", &useded);
      currdisk->dedicatedwriteseg = NULL;
      if ((currdisk->numsegs > 1) && (useded)) {
          currdisk->dedicatedwriteseg++;
      }

      getparam_double(specfile, "Low (write) water mark", &currdisk->writewater, 3, (double) 0.0, (double) 1.0);
      getparam_double(specfile, "High (read) water mark", &currdisk->readwater, 3, (double) 0.0, (double) 1.0);
      getparam_bool(specfile, "Set watermark by reqsize:", &currdisk->reqwater);

      getparam_bool(specfile, "Calc sector by sector:", &currdisk->sectbysect);

      getparam_int(specfile, "Enable caching in buffer", &enablecache, 3, 0, 2);
      getparam_int(specfile, "Buffer continuous read", &contread, 3, 0, 4);
      if ((contread) && (enablecache == FALSE)) {
	 fprintf(stderr, "Makes no sense to use read-ahead but not use caching\n");
	 exit(0);
      }

      getparam_int(specfile, "Minimum read-ahead (blks)", &currdisk->minreadahead, 3, 0, currdisk->segsize);
      if ((contread == BUFFER_NO_READ_AHEAD) && (currdisk->minreadahead > 0)) {
         fprintf(stderr, "'Minimum read-ahead (blks)' forced to zero due to 'Buffer continuous read' value\n");
	 currdisk->minreadahead = 0;
      }

      getparam_int(specfile, "Maximum read-ahead (blks)", &currdisk->maxreadahead, 3, 0, currdisk->segsize);
      if ((currdisk->maxreadahead < currdisk->minreadahead) || ((contread == BUFFER_NO_READ_AHEAD) && (currdisk->maxreadahead > 0))) {
         fprintf(stderr, "'Maximum read-ahead (blks)' forced to zero due to 'Buffer continuous read' value\n");
	 currdisk->maxreadahead = currdisk->minreadahead;
      }

      getparam_int(specfile, "Read-ahead over requested", &currdisk->keeprequestdata, 3, -1, 1);
      if (currdisk->keeprequestdata >= 0) {
         currdisk->keeprequestdata = abs(currdisk->keeprequestdata - 1);
      }

      getparam_bool(specfile, "Read-ahead on idle hit:", &currdisk->readaheadifidle);
      getparam_bool(specfile, "Read any free blocks:", &currdisk->readanyfreeblocks);
      if ((currdisk->readanyfreeblocks != FALSE) && (enablecache == FALSE)) {
	 fprintf(stderr, "Makes no sense to read blocks with caching disabled\n");
	 exit(0);
      }

      getparam_int(specfile, "Fast write level", &currdisk->fastwrites, 3, 0, 2);
      if ((currdisk->fastwrites) && (enablecache == FALSE)) {
	 fprintf(stderr, "Can't use fast write if not employing caching\n");
	 exit(0);
      }

      getparam_int(specfile, "Immediate buffer read", &currdisk->immedread, 3, 0, 2);
      getparam_int(specfile, "Immediate buffer write", &currdisk->immedwrite, 3, 0, 2);
      getparam_bool(specfile, "Combine seq writes:", &currdisk->writecomb);
      getparam_bool(specfile, "Stop prefetch in sector:", &currdisk->stopinsector);
      getparam_bool(specfile, "Disconnect write if seek:", &currdisk->disconnectinseek);
      getparam_bool(specfile, "Write hit stop prefetch:", &currdisk->write_hit_stop_readahead);
      getparam_bool(specfile, "Read directly to buffer:", &currdisk->read_direct_to_buffer);
      getparam_bool(specfile, "Immed transfer partial hit:", &currdisk->immedtrans_any_readhit);

      getparam_double(specfile, "Read hit over. after read", &currdisk->overhead_command_readhit_afterread, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Read hit over. after write", &currdisk->overhead_command_readhit_afterwrite, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Read miss over. after read", &currdisk->overhead_command_readmiss_afterread, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Read miss over. after write", &currdisk->overhead_command_readmiss_afterwrite, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Write over. after read", &currdisk->overhead_command_write_afterread, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Write over. after write", &currdisk->overhead_command_write_afterwrite, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Read completion overhead", &currdisk->overhead_complete_read, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Write completion overhead", &currdisk->overhead_complete_write, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Data preparation overhead", &currdisk->overhead_data_prep, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "First reselect overhead", &currdisk->overhead_reselect_first, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Other reselect overhead", &currdisk->overhead_reselect_other, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Read disconnect afterread", &currdisk->overhead_disconnect_read_afterread, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Read disconnect afterwrite", &currdisk->overhead_disconnect_read_afterwrite, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Write disconnect overhead", &currdisk->overhead_disconnect_write, 1, (double) 0.0, (double) 0.0);

      getparam_bool(specfile, "Extra write disconnect:", &currdisk->extra_write_disconnect);
      getparam_double(specfile, "Extradisc command overhead", &currdisk->extradisc_command, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Extradisc disconnect overhead", &currdisk->extradisc_disconnect1, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Extradisc inter-disconnect delay", &currdisk->extradisc_inter_disconnect, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Extradisc 2nd disconnect overhead", &currdisk->extradisc_disconnect2, 1, (double) 0.0, (double) 0.0);
      getparam_double(specfile, "Extradisc seek delta", &currdisk->extradisc_seekdelta, 1, (double) 0.0, (double) 0.0);

      getparam_double(specfile, "Minimum seek delay", &currdisk->minimum_seek_delay, 1, (double) 0.0, (double) 0.0);

      getparam_int(specfile, "Number of bands", &numbands, 1, 1, 0);

      for (i=diskno; i<(diskno+copies); i++) {
         disks[i].acctime = acctime;
         disks[i].seektime = seektime;
         disks[i].seekone = currdisk->seekone;
         disks[i].seekavg = seekavg;
         disks[i].seekfull = currdisk->seekfull;
	 disks[i].seekwritedelta = currdisk->seekwritedelta;
         disks[i].hpseek[0] = hp1;
         disks[i].hpseek[1] = hp2;
         disks[i].hpseek[2] = hp3;
         disks[i].hpseek[3] = hp4;
         disks[i].hpseek[4] = hp5;
         disks[i].hpseek[5] = hp6;
	 for (j=0; j<10; j++) {
	    disks[i].first10seeks[j] = first10seeks[j];
	 }
	 disks[i].extractseekcnt = extractseekcnt;
	 disks[i].extractseekdists = extractseekdists;
	 disks[i].extractseektimes = extractseektimes;
         disks[i].headswitch = currdisk->headswitch;
         disks[i].rpm = currdisk->rpm;
         disks[i].rpmerr = currdisk->rpmerr;
         disks[i].numsurfaces = currdisk->numsurfaces;
         disks[i].numcyls = currdisk->numcyls;
         disks[i].numblocks = currdisk->numblocks;
         disks[i].sparescheme = currdisk->sparescheme;
         disks[i].printstats = currdisk->printstats;
         disks[i].sectpercyl = currdisk->sectpercyl;
	 disks[i].devno = i;
         disks[i].maxqlen = currdisk->maxqlen;
         disks[i].write_hit_stop_readahead = currdisk->write_hit_stop_readahead;
         disks[i].read_direct_to_buffer = currdisk->read_direct_to_buffer;
         disks[i].immedtrans_any_readhit = currdisk->immedtrans_any_readhit;
         disks[i].extra_write_disconnect = currdisk->extra_write_disconnect;
         disks[i].overhead_command_readhit_afterread = currdisk->overhead_command_readhit_afterread;
         disks[i].overhead_command_readhit_afterwrite = currdisk->overhead_command_readhit_afterwrite;
         disks[i].overhead_command_readmiss_afterread = currdisk->overhead_command_readmiss_afterread;
         disks[i].overhead_command_readmiss_afterwrite = currdisk->overhead_command_readmiss_afterwrite;
         disks[i].overhead_command_write_afterread = currdisk->overhead_command_write_afterread;
         disks[i].overhead_command_write_afterwrite = currdisk->overhead_command_write_afterwrite;
         disks[i].overhead_complete_read = currdisk->overhead_complete_read;
         disks[i].overhead_complete_write = currdisk->overhead_complete_write;
         disks[i].overhead_data_prep = currdisk->overhead_data_prep;
         disks[i].overhead_reselect_first = currdisk->overhead_reselect_first;
         disks[i].overhead_reselect_other = currdisk->overhead_reselect_other;
         disks[i].overhead_disconnect_read_afterread = currdisk->overhead_disconnect_read_afterread;
         disks[i].overhead_disconnect_read_afterwrite = currdisk->overhead_disconnect_read_afterwrite;
         disks[i].overhead_disconnect_write = currdisk->overhead_disconnect_write;
         disks[i].extradisc_command = currdisk->extradisc_command;
         disks[i].extradisc_disconnect1 = currdisk->extradisc_disconnect1;
         disks[i].extradisc_inter_disconnect = currdisk->extradisc_inter_disconnect;
         disks[i].extradisc_disconnect2 = currdisk->extradisc_disconnect2;
         disks[i].extradisc_seekdelta = currdisk->extradisc_seekdelta;
         disks[i].minimum_seek_delay = currdisk->minimum_seek_delay;
         disks[i].readanyfreeblocks = currdisk->readanyfreeblocks;
         disks[i].overhead = currdisk->overhead;
         disks[i].timescale = currdisk->timescale;
         disks[i].blktranstime = currdisk->blktranstime;
         disks[i].hold_bus_for_whole_read_xfer = currdisk->hold_bus_for_whole_read_xfer;
         disks[i].hold_bus_for_whole_write_xfer = currdisk->hold_bus_for_whole_write_xfer;
         disks[i].almostreadhits = currdisk->almostreadhits;
         disks[i].sneakyfullreadhits = currdisk->sneakyfullreadhits;
         disks[i].sneakypartialreadhits = currdisk->sneakypartialreadhits;
         disks[i].sneakyintermediatereadhits = currdisk->sneakyintermediatereadhits;
         disks[i].readhitsonwritedata = currdisk->readhitsonwritedata;
         disks[i].writeprebuffering = currdisk->writeprebuffering;
         disks[i].preseeking = currdisk->preseeking;
         disks[i].neverdisconnect = currdisk->neverdisconnect;
	 disks[i].numsegs = currdisk->numsegs;
	 disks[i].numwritesegs = currdisk->numwritesegs;
	 disks[i].segsize = currdisk->segsize;
	 disks[i].dedicatedwriteseg = currdisk->dedicatedwriteseg;
	 disks[i].writewater = currdisk->writewater;
	 disks[i].readwater = currdisk->readwater;
	 disks[i].reqwater = currdisk->reqwater;
	 disks[i].sectbysect = currdisk->sectbysect;
	 disks[i].enablecache = enablecache;
	 disks[i].contread = contread;
	 disks[i].minreadahead = currdisk->minreadahead;
	 disks[i].maxreadahead = currdisk->maxreadahead;
	 disks[i].keeprequestdata = currdisk->keeprequestdata;
	 disks[i].readaheadifidle = currdisk->readaheadifidle;
	 disks[i].fastwrites = currdisk->fastwrites;
	 disks[i].immedread = currdisk->immedread;
	 disks[i].immedwrite = currdisk->immedwrite;
	 disks[i].writecomb = currdisk->writecomb;
	 disks[i].stopinsector = currdisk->stopinsector;
	 disks[i].disconnectinseek = currdisk->disconnectinseek;
	 disks[i].seglist = NULL;
         disks[i].buswait = NULL;
	 disks[i].currenthda = NULL;
	 disks[i].effectivehda = NULL;
	 disks[i].currentbus = NULL;
	 disks[i].effectivebus = NULL;
	 disks[i].pendxfer = NULL;
	 disks[i].outwait = NULL;
         disks[i].numbands = numbands;
	 if (i != diskno) {
	    disks[i].queue = ioqueue_copy(currdisk->queue);
	 }
         disks[i].bands = (band *) malloc(numbands*(sizeof(band)));
	 ASSERT(disks[i].bands != NULL);
	 if (i == diskno) {
            band_read_specs(currdisk->bands, numbands, specfile, currdisk->numsurfaces, currdisk->sparescheme);
         } else {
	    bandcopy(disks[i].bands, currdisk->bands, numbands);
	 }
      }
      diskno += copies;
   }
}


void disk_read_syncsets(parfile)
FILE *parfile;
{
   int i;
   int setno;
   int size;
   int setstart;
   int setend;

   if (fscanf(parfile,"\nNumber of synchronized sets: %d\n\n", &numsyncsets) != 1) {
      fprintf(stderr, "Error reading number of synchronized sets\n");
      exit(0);
   }
   if (numsyncsets < 0) {
      fprintf(stderr, "Invalid value for number of synchronized sets - %d\n", numsyncsets);
      exit(0);
   }
   fprintf (outputfile, "\nNumber of synchronized sets: %d\n\n", numsyncsets);

   for (i=0; i< numdisks; i++) {
      disks[i].syncset = 0;
   }

   for (i=1; i<=numsyncsets; i++) {
      if (fscanf(parfile, "Number of disks in set #%d: %d\n", &setno, &size) != 2) {
         fprintf(stderr, "Error reading number of disks in synchronized set\n");
         exit(0);
      }
      if (setno != i) {
	 fprintf(stderr, "Synchronized sets not appearing in correct order\n");
	 exit(0);
      }
      if ((size < 1) || (size > 31)) {
	 fprintf(stderr, "Invalid value for size of synchronized set #%d\n", setno);
	 exit(0);
      }
      fprintf (outputfile, "Number of disks in set #%d: %d\n", setno, size);

      if (fscanf(parfile, "Synchronized disks: %d-%d\n", &setstart, &setend) != 2) {
	 fprintf(stderr, "Error reading 'Synchronized disks: ' for set #%d\n", setno);
	 exit(0);
      }
      if ((setstart >= setend) || (setstart < 1) || (setend > numdisks) || (setstart + size - setend - 1)) {
	 fprintf(stderr, "Invalid set of synchronized disks in set #%d\n", setno);
	 exit(0);
      }
      fprintf (outputfile, "Synchronized disks: %d-%d\n", setstart, setend);
      for (setstart--; setstart < setend; setstart++) {
         if (disks[setstart].syncset != 0) {
            fprintf (stderr, "Overlapping synchronized disk sets (%d and %d)\n", setno, disks[setstart].syncset);
            exit (0);
         }
	 disks[setstart].syncset = i;
      }
   }
}


void disk_interfere_printstats(set, setsize, prefix)
int *set;
int setsize;
char *prefix;
{
   int seq = 0;
   int loc = 0;
   int i;

   if (disk_printinterferestats == FALSE)
      return;

   for(i=0; i<setsize; i++) {
      seq += disks[(set[i])].stat.interfere[0];
      loc += disks[(set[i])].stat.interfere[1];
   }
   fprintf (outputfile, "%sSequential interference: %d\n", prefix, seq);
   fprintf (outputfile, "%sLocal interference:      %d\n", prefix, loc);
}


void disk_buffer_printstats(set, setsize, prefix)
int *set;
int setsize;
char *prefix;
{
   int writecombs = 0;
   int readmisses = 0;
   int writemisses = 0;
   int fullreadhits = 0;
   int prependhits = 0;
   int appendhits = 0;
   int readinghits = 0;
   double runreadingsize = 0.0;
   double remreadingsize = 0.0;
   int parthits = 0;
   double runpartsize = 0.0;
   double rempartsize = 0.0;
   double waitingforbus = 0.0;
   int numbuswaits = 0;
   int hits;
   int misses;
   int total;
   int reads;
   int writes;
   int i;

   if (disk_printbufferstats == FALSE)
      return;

   for (i=0; i<setsize; i++) {
      writecombs += disks[(set[i])].stat.writecombs;
      readmisses += disks[(set[i])].stat.readmisses;
      writemisses += disks[(set[i])].stat.writemisses;
      fullreadhits += disks[(set[i])].stat.fullreadhits;
      prependhits += disks[(set[i])].stat.prependhits;
      appendhits += disks[(set[i])].stat.appendhits;
      readinghits += disks[(set[i])].stat.readinghits;
      runreadingsize += disks[(set[i])].stat.runreadingsize;
      remreadingsize += disks[(set[i])].stat.remreadingsize;
      parthits += disks[(set[i])].stat.parthits;
      runpartsize += disks[(set[i])].stat.runpartsize;
      rempartsize += disks[(set[i])].stat.rempartsize;
      waitingforbus += disks[(set[i])].stat.waitingforbus;
      numbuswaits += disks[(set[i])].stat.numbuswaits;
   }
   hits = fullreadhits + appendhits + prependhits + parthits + readinghits;
   misses = readmisses + writemisses;
   reads = fullreadhits + readmisses + readinghits + parthits;
   total = hits + misses;
   writes = total - reads;
   if (total == 0) {
      return;
   }
   fprintf(outputfile, "%sNumber of buffer accesses:    %d\n", prefix, total);
   fprintf(outputfile, "%sBuffer hit ratio:        %6d \t%f\n", prefix, hits, ((double) hits / (double) total));
   fprintf(outputfile, "%sBuffer miss ratio:            %6d \t%f\n", prefix, misses, ((double) misses / (double) total));
   fprintf(outputfile, "%sBuffer read hit ratio:        %6d \t%f \t%f\n", prefix, fullreadhits, ((double) fullreadhits / (double) max(1,reads)), ((double) fullreadhits / (double) total));
   fprintf(outputfile, "%sBuffer prepend hit ratio:       %6d \t%f\n", prefix, prependhits, ((double) prependhits / (double) max(1,writes)));
   fprintf(outputfile, "%sBuffer append hit ratio:       %6d \t%f\n", prefix, appendhits, ((double) appendhits / (double) max(1,writes)));
   fprintf(outputfile, "%sWrite combinations:           %6d \t%f\n", prefix, writecombs, ((double) writecombs / (double) max(1,writes)));
   fprintf(outputfile, "%sOngoing read-ahead hit ratio: %6d \t%f \t%f\n", prefix, readinghits, ((double) readinghits / (double) max(1,reads)), ((double) readinghits / (double) total));
   fprintf(outputfile, "%sAverage read-ahead hit size:  %f\n", prefix, (runreadingsize / (double) max(1,readinghits)));
   fprintf(outputfile, "%sAverage remaining read-ahead: %f\n", prefix, (remreadingsize / (double) max(1,readinghits)));
   fprintf(outputfile, "%sPartial read hit ratio: %6d \t%f \t%f\n", prefix, parthits, ((double) parthits / (double) max(1,reads)), ((double) parthits / (double) total));
   fprintf(outputfile, "%sAverage partial hit size:     %f\n", prefix, (runpartsize / (double) max(1,parthits)));
   fprintf(outputfile, "%sAverage remaining partial:    %f\n", prefix, (rempartsize / (double) max(1,parthits)));
   fprintf(outputfile, "%sTotal disk bus wait time: %f\n", prefix, waitingforbus);
   fprintf(outputfile, "%sNumber of disk bus waits: %d\n", prefix, numbuswaits);
}


void disk_seek_printstats(set, setsize, prefix)
int *set;
int setsize;
char *prefix;
{
   int i;
   int zeros = 0;
   statgen * statset[MAXDISKS];
   double zerofrac;

   if (disk_printseekstats == FALSE) {
      return;
   }

   for (i=0; i<setsize; i++) {
      zeros += disks[(set[i])].stat.zeroseeks;
      statset[i] = &disks[(set[i])].stat.seekdiststats;
   }
   zerofrac = (double) zeros / (double) stat_get_count_set(statset, setsize);
   fprintf (outputfile, "%sSeeks of zero distance:\t%d\t%f\n", prefix, zeros, zerofrac);
   stat_print_set(statset, setsize, prefix);

   for (i=0; i<setsize; i++) {
      statset[i] = &disks[(set[i])].stat.seektimestats;
   }
   stat_print_set(statset, setsize, prefix);
}


void disk_latency_printstats(set, setsize, prefix)
int *set;
int setsize;
char *prefix;
{
   int i;
   int zeros = 0;
   statgen * statset[MAXDISKS];
   double zerofrac;

   if (disk_printlatencystats == FALSE) {
      return;
   }

   for (i=0; i<setsize; i++) {
      zeros += disks[(set[i])].stat.zerolatency;
      statset[i] = &disks[(set[i])].stat.rotlatstats;
   }
   if (setsize == 1) {
      fprintf (outputfile, "%sFull rotation time:      %f\n", prefix, disks[(set[0])].rotatetime);
   }
   zerofrac = (double) zeros / (double) stat_get_count_set(statset, setsize);
   fprintf (outputfile, "%sZero rotate latency:\t%d\t%f\n", prefix, zeros, zerofrac);
   stat_print_set(statset, setsize, prefix);
}


void disk_transfer_printstats(set, setsize, prefix)
int *set;
int setsize;
char *prefix;
{
   int i;
   statgen * statset[MAXDISKS];

   if (disk_printxferstats) {
      for (i=0; i<setsize; i++) {
         statset[i] = &disks[(set[i])].stat.xfertimestats;
      }
      stat_print_set(statset, setsize, prefix);
   }
}


void disk_acctime_printstats(set, setsize, prefix)
int *set;
int setsize;
char *prefix;
{
   int i;
   statgen * statset[MAXDISKS];

   if (disk_printacctimestats) {
      for (i=0; i<setsize; i++) {
         statset[i] = &disks[(set[i])].stat.acctimestats;
      }
      stat_print_set(statset, setsize, prefix);
   }
}


void disk_printsetstats(set, setsize, sourcestr)
int *set;
int setsize;
char *sourcestr;
{
   int i;
   struct ioq * queueset[MAXDISKS];
   int reqcnt = 0;
   char prefix[80];

   sprintf(prefix, "%sdisk ", sourcestr);
   for (i=0; i<setsize; i++) {
      queueset[i] = disks[(set[i])].queue;
      reqcnt += ioqueue_get_number_of_requests(disks[(set[i])].queue);
   }
   if (reqcnt == 0) {
      fprintf (outputfile, "\nNo disk requests for members of this set\n\n");
      return;
   }
   ioqueue_printstats(queueset, setsize, prefix);

   disk_seek_printstats(set, setsize, prefix);
   disk_latency_printstats(set, setsize, prefix);
   disk_transfer_printstats(set, setsize, prefix);
   disk_acctime_printstats(set, setsize, prefix);
   disk_interfere_printstats(set, setsize, prefix);
   disk_buffer_printstats(set, setsize, prefix);
}


void disk_printstats()
{
   struct ioq * queueset[MAXDISKS];
   int set[MAXDISKS];
   int i;
   int reqcnt = 0;
   char prefix[80];

   fprintf(outputfile, "\nDISK STATISTICS\n");
   fprintf(outputfile, "---------------\n\n");

   sprintf(prefix, "Disk ");
   for (i=0; i<numdisks; i++) {
      queueset[i] = disks[i].queue;
      reqcnt += ioqueue_get_number_of_requests(disks[i].queue);
   }
   if (reqcnt == 0) {
      fprintf(outputfile, "No disk requests encountered\n");
      return;
   }
/*
   fprintf(outputfile, "Number of extra write disconnects:   %5d  \t%f\n", extra_write_disconnects, ((double) extra_write_disconnects / (double) reqcnt));
*/
   ioqueue_printstats(queueset, numdisks, prefix);

   for (i=0; i<numdisks; i++) {
      set[i] = i;
   }
   disk_seek_printstats(set, numdisks, prefix);
   disk_latency_printstats(set, numdisks, prefix);
   disk_transfer_printstats(set, numdisks, prefix);
   disk_acctime_printstats(set, numdisks, prefix);
   disk_interfere_printstats(set, numdisks, prefix);
   disk_buffer_printstats(set, numdisks, prefix);
   fprintf (outputfile, "\n\n");

   if (numdisks <= 1) {
      return;
   }

   for (i=0; i<numdisks; i++) {
      if (disks[i].printstats == FALSE) {
	 continue;
      }
      if (ioqueue_get_number_of_requests(disks[i].queue) == 0) {
	 fprintf(outputfile, "No requests for disk #%d\n\n\n", i);
	 continue;
      }
      fprintf(outputfile, "Disk #%d:\n\n", i);
      fprintf(outputfile, "Disk #%d highest block number requested: %d\n", i, disks[i].stat.highblkno);
      sprintf(prefix, "Disk #%d ", i);
      ioqueue_printstats(&disks[i].queue, 1, prefix);
      disk_seek_printstats(&set[i], 1, prefix);
      disk_latency_printstats(&set[i], 1, prefix);
      disk_transfer_printstats(&set[i], 1, prefix);
      disk_acctime_printstats(&set[i], 1, prefix);
      disk_interfere_printstats(&set[i], 1, prefix);
      disk_buffer_printstats(&set[i], 1, prefix);
      fprintf (outputfile, "\n\n");
   }
}

