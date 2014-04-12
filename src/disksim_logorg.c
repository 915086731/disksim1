
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
#include "disksim_orgface.h"
#include "disksim_logorg.h"
#include "disksim_ioqueue.h"

#define MAX_QUEUE_LENGTH 10000

extern double disk_last_read_completion[];
extern double disk_last_read_arrival[];
extern double disk_last_write_completion[];
extern double disk_last_write_arrival[];

char *statdesc_intarr =      "Inter-arrival time";
char *statdesc_readintarr =  "Read inter-arrival";
char *statdesc_writeintarr = "Write inter-arrival";
char *statdesc_reqsize =     "Request size";
char *statdesc_readsize =    "Read request size";
char *statdesc_writesize =   "Write request size";
char *statdesc_idles =       "Idle period length";
char *statdesc_resptime =    "Response time";
char *statdesc_streak =      "Streak length";
char *statdesc_locality =    "Inter-request distance";


int logorg_overlap(currlogorg, devno, curr, blksperpart)
logorg *currlogorg;
int devno;
ioreq_event *curr;
int blksperpart;
{
   int calc;

   calc = curr->blkno - currlogorg->devs[devno].startblkno;
   if ((calc >= 0) && (calc < blksperpart)) {
      curr->devno = devno;
      curr->blkno = calc;
      return(TRUE);
   }
   return(FALSE);
}


void logorg_streakstat(currlogorg, diskno)
logorg *currlogorg;
int diskno;
{
   int last = currlogorg->lastdiskaccessed;
   logorgdev *devs = currlogorg->devs;

   if (currlogorg->printstreakstats == FALSE) {
      return;
   }

   ASSERT1((diskno >= 0) && (diskno <= currlogorg->actualnumdisks), "diskno", diskno);
   if (diskno == last) {
      if (diskno != currlogorg->actualnumdisks) {
         devs[diskno].curstreak++;
      }
      return;
   }
   if (last != currlogorg->actualnumdisks) {
      stat_update(&devs[last].streakstats, (double) devs[last].curstreak);
      devs[last].curstreak = 0;
   }
   currlogorg->lastdiskaccessed = diskno;
   if (diskno != currlogorg->actualnumdisks) {
      devs[diskno].curstreak = 1;
   }
}


void logorg_update_intarr_stats(currlogorg, curr)
logorg *currlogorg;
ioreq_event *curr;
{
   double tdiff;

   if (currlogorg->printintarrstats == FALSE) {
      return;
   }

   tdiff = simtime - currlogorg->stat.lastarr;
   stat_update(&currlogorg->stat.intarrstats, tdiff);
   currlogorg->stat.lastarr = simtime;
   if (curr->flags & READ) {
      tdiff = simtime - currlogorg->stat.lastread;
      stat_update(&currlogorg->stat.readintarrstats, tdiff);
      currlogorg->stat.lastread = simtime;
   } else {
      tdiff = simtime - currlogorg->stat.lastwrite;
      stat_update(&currlogorg->stat.writeintarrstats, tdiff);
      currlogorg->stat.lastwrite = simtime;
   }
}


void logorg_update_interfere_stats(currlogorg, curr)
logorg *currlogorg;
ioreq_event *curr;
{
   int i;
   int first;
   int *lastreq = currlogorg->stat.lastreq;
   int diffblkno;

   if (currlogorg->printinterferestats == FALSE) {
      return;
   }

   first = stat_get_count(&currlogorg->stat.intarrstats) % INTERFEREMAX;
   first = (INTERFEREMAX - first) % INTERFEREMAX;
   for (i=0; i<INTERFEREMAX; i++) {
      if (curr->devno == lastreq[(((i+first)%INTERFEREMAX) << 1)]) {
         diffblkno = curr->blkno - lastreq[((((i+first)%INTERFEREMAX) << 1) + 1)];
         if (!diffblkno) {
            currlogorg->stat.intdist[(i*INTDISTMAX)]++;
            i = -1;
            break;
         }
      }
   }
   if (i != -1) {
      for (i=0; i<INTERFEREMAX; i++) {
         if (curr->devno == lastreq[(((i+first)%INTERFEREMAX) << 1)]) {
            diffblkno = curr->blkno - lastreq[((((i+first)%INTERFEREMAX) << 1) + 1)];
            if ((diffblkno <= 64) && (diffblkno >= -64)) {
               if (diffblkno < 0) {
                  if (diffblkno >= -4) {
                     currlogorg->stat.intdist[((i*INTDISTMAX)+1)]++;
                  } else if (diffblkno >= -8) {
                     currlogorg->stat.intdist[((i*INTDISTMAX)+2)]++;
                  } else if (diffblkno >= -16) {
                     currlogorg->stat.intdist[((i*INTDISTMAX)+3)]++;
                  } else if (diffblkno >= -64) {
                     currlogorg->stat.intdist[((i*INTDISTMAX)+4)]++;
                  }
               } else {
                  if (diffblkno <= 8) {
                     currlogorg->stat.intdist[((i*INTDISTMAX)+5)]++;
                  } else if (diffblkno <= 16) {
                     currlogorg->stat.intdist[((i*INTDISTMAX)+6)]++;
                  } else if (diffblkno <= 64) {
                     currlogorg->stat.intdist[((i*INTDISTMAX)+7)]++;
                  }
               }
               break;
            }
         }
      }
   }
   first = ((first - 1) % INTERFEREMAX) << 1;
   lastreq[first] = curr->devno;
   lastreq[(first + 1)] = curr->blkno + curr->bcount;
}


void logorg_update_blocking_stats(currlogorg, curr)
logorg *currlogorg;
ioreq_event *curr;
{
   int i;
   int maxval;

   if (currlogorg->printblockingstats == FALSE) {
      return;
   }

   maxval = min(BLOCKINGMAX, curr->bcount);
   for (i=0; i<maxval; i++) {
      if ((curr->bcount % (i+1)) == 0) {
         currlogorg->stat.blocked[i]++;
      }
   }
   maxval = min(BLOCKINGMAX, curr->blkno);
   for (i=0; i<maxval; i++) {
      if ((curr->blkno % (i+1)) == 0) {
         currlogorg->stat.aligned[i]++;
      }
   }
}


void logorg_numoutdist(numout, dist)
int numout;
int *dist;
{
   int i = min(numout, 9);
   ASSERT(numout >= 0);
   dist[i]++;
}


void logorg_diffdist(mydiff, dist)
double mydiff;
int *dist;
{
   int i;

   if (mydiff < (double) 0.5) {
      i = 0;
   } else if (mydiff < (double) 1.0) {
      i = 1;
   } else if (mydiff < (double) 1.5) {
      i = 2;
   } else if (mydiff < (double) 2.0) {
      i = 3;
   } else if (mydiff < (double) 2.5) {
      i = 4;
   } else if (mydiff < (double) 3.0) {
      i = 5;
   } else if (mydiff < (double) 4.0) {
      i = 6;
   } else if (mydiff < (double) 5.0) {
      i = 7;
   } else if (mydiff < (double) 6.0) {
      i = 8;
   } else {
      i = 9;
   }
   dist[i]++;
}


void logorg_maprequest_update_stats(currlogorg, curr, req, numreqs)
logorg *currlogorg;
ioreq_event *curr;
outstand *req;
int numreqs;
{
   int i, j;
   ioreq_event *temp;
   int gen;
   double tpass;
   logorgdev *currdev;
   int orgdevno = curr->devno - currlogorg->devs[0].devno;
/*
fprintf (outputfile, "Entering logorg_maprequest_update_stats - numreqs %d, orgdevno %d\n", numreqs, orgdevno);
*/
   currdev = &currlogorg->devs[orgdevno];
   logorg_update_interfere_stats(currlogorg, curr);
   logorg_update_intarr_stats(currlogorg, curr);
   logorg_update_blocking_stats(currlogorg, curr);
   
   if (currlogorg->printsizestats) {
      stat_update(&currlogorg->stat.sizestats, (double) curr->bcount);
      if (curr->flags & READ) {
         stat_update(&currlogorg->stat.readsizestats, (double) curr->bcount);
      } else {
         stat_update(&currlogorg->stat.writesizestats, (double) curr->bcount);
      }
   }
   if (curr->flags & TIME_CRITICAL) {
      if (curr->flags & READ) {
         currlogorg->stat.critreads++;
      } else {
         currlogorg->stat.critwrites++;
      }
   }
   stat_update(&currdev->localitystats, (double) (currdev->lastblkno - curr->blkno));
   if ((curr->blkno > currdev->lastblkno) && (curr->blkno <= currdev->lastblkno2)) {
      if (curr->flags & READ) {
         currdev->intreads++;
      } else {
         currdev->intwrites++;
      }
   }
   currdev->lastblkno2 = curr->blkno + curr->bcount + 16;
   if (curr->blkno == currdev->lastblkno) {
      if (curr->flags & READ) {
         currdev->seqreads++;
      } else {
         currdev->seqwrites++;
      }
   }
   currdev->lastblkno = curr->blkno + curr->bcount;
   gen = curr->cause;
   if (curr->flags & SEQ) {
      if (curr->flags & READ) {
         currlogorg->stat.seqreads++;
      } else {
         currlogorg->stat.seqwrites++;
      }
      if (currlogorg->stat.gens[gen] != curr->devno) {
         currlogorg->stat.seqdiskswitches++;
         curr->flags &= ~SEQ;
      }
   }
   if (curr->flags & LOCAL) {
      currlogorg->stat.numlocal++;
      if (currlogorg->stat.gens[gen] != curr->devno) {
         currlogorg->stat.locdiskswitches++;
         curr->flags &= ~LOCAL;
      }
   }
   currlogorg->stat.gens[gen] = curr->devno;
   logorg_streakstat(currlogorg, orgdevno);
/*
fprintf (outputfile, "Midway through stats\n");
*/
   temp = curr;
   for (i=0; i<numreqs; i++) {
      currlogorg->devs[(temp->devno - currlogorg->devs[0].devno)].numout++;
      temp = temp->next;
   }

   if (currlogorg->stat.outstanding > 0) {
      currlogorg->stat.nonzeroouttime += simtime - currlogorg->stat.outtime;
   } else {
      tpass = simtime - currlogorg->stat.idlestart;
      stat_update(&currlogorg->stat.idlestats, tpass);
   }
   currlogorg->stat.idlestart = simtime;
   currlogorg->stat.runouttime += (double) currlogorg->stat.outstanding * (simtime - currlogorg->stat.outtime);
   currlogorg->stat.outstanding++;
   if (currlogorg->stat.outstanding >= MAX_QUEUE_LENGTH) {
      fprintf(stderr,"Stopping simulation because of saturation: simtime %f, totalreqs %d\n", simtime, totalreqs);
      fflush(stderr);
      fprintf (outputfile, "Stopping simulation because of saturation: simtime %f, totalreqs %d\n", simtime, totalreqs);
      fprintf (outputfile, "last request:  dev=%d, blk=%d, cnt=%d, %d (R==1)\n",curr->devno, curr->blkno, curr->bcount, (curr->flags & READ));
      for (j=0;j<MAXDISKS;j++) {
         fprintf(outputfile, "last read arrival for dev %d =    %12.6f\n",j,disk_last_read_arrival[j]);
         fprintf(outputfile, "last read completion for dev %d = %12.6f\n",j,disk_last_read_completion[j]);
         fprintf(outputfile, "last write arrival for dev %d =    %12.6f\n",j,disk_last_write_arrival[j]);
         fprintf(outputfile, "last write completion for dev %d = %12.6f\n",j,disk_last_write_completion[j]);
      }
      simstop();
   }
   if (req->flags & READ) {
      currlogorg->stat.readoutstanding++;
   }
   if (currlogorg->stat.outstanding > currlogorg->stat.maxoutstanding) {
      currlogorg->stat.maxoutstanding = currlogorg->stat.outstanding;
   }
   currlogorg->stat.outtime = simtime;
/*
fprintf (outputfile, "Leaving maprequest_stats\n");
*/
}


void logorg_mapcomplete_update_stats(currlogorg, curr, req)
logorg *currlogorg;
ioreq_event *curr;
outstand *req;
{
   stat_update(&currlogorg->stat.resptimestats, (simtime - req->arrtime));
   currlogorg->stat.nonzeroouttime += simtime - currlogorg->stat.outtime;
   currlogorg->stat.runouttime += currlogorg->stat.outstanding * (simtime - currlogorg->stat.outtime);
   currlogorg->stat.outstanding--;
   if (req->flags & READ) {
      currlogorg->stat.readoutstanding--;
   }
   currlogorg->stat.outtime = simtime;
}


void logorg_timestamp(curr)
ioreq_event *curr;
{
   logorg *currlogorg = (logorg *) curr->tempptr1;
   double avgqlen = 0.0;
   double avgdiff = 0.0;
   int maxqlen = 0;
   double maxdiff = 0.0;
   int i;
   double calc;

   curr->time += currlogorg->stampinterval;
   if (curr->time > currlogorg->stampstop) {
      addtoextraq((event *) curr);
   } else {
      addtointq((event *) curr);
   }
   calc = (double) currlogorg->actualnumdisks;
   for (i=0; i<currlogorg->actualnumdisks; i++) {
      logorg_numoutdist(currlogorg->devs[i].numout, currlogorg->devs[i].distnumout);
      if (currlogorg->stampfile) {
         fprintf(currlogorg->stampfile, "%d ", currlogorg->devs[i].numout);
      }
      avgqlen += (double) currlogorg->devs[i].numout;
      if (currlogorg->devs[i].numout > maxqlen) {
         maxqlen = currlogorg->devs[i].numout;
      }
   }
   if (currlogorg->stampfile) {
      fprintf(currlogorg->stampfile, "\n");
   }
   avgqlen = avgqlen / calc;
   for (i=0; i<currlogorg->actualnumdisks; i++) {
      if (avgqlen > (double) currlogorg->devs[i].numout) {
         avgdiff += (avgqlen - (double) currlogorg->devs[i].numout);
      } else {
         avgdiff += ((double) currlogorg->devs[i].numout - avgqlen);
      }
      maxdiff += (double) (maxqlen - currlogorg->devs[i].numout);
   }
   logorg_diffdist((avgdiff / calc), currlogorg->stat.distavgdiff);
   logorg_diffdist((maxdiff / calc), currlogorg->stat.distmaxdiff);
}


void logorg_addnewtooutstandq(currlogorg, temp)
logorg *currlogorg;
outstand *temp;
{
   outstand *run = NULL;

   temp->next = NULL;
   run = currlogorg->hashoutstand[(temp->opid % HASH_OUTSTAND)];
   if (run == NULL) {
      currlogorg->hashoutstand[(temp->opid % HASH_OUTSTAND)] = temp;
   } else {
      while (run->next != NULL)
         run = run->next;
      run->next = temp;
   }
   currlogorg->outstandqlen++;
}


void logorg_addtooutstandq(currlogorg, temp)
logorg *currlogorg;
outstand *temp;
{
   temp->next = currlogorg->hashoutstand[(temp->opid % HASH_OUTSTAND)];
   currlogorg->hashoutstand[(temp->opid % HASH_OUTSTAND)] = temp;
   currlogorg->outstandqlen++;
}


outstand *logorg_show_buf_from_outstandq(currlogorg, buf, opid)
logorg *currlogorg;
void *buf;
int opid;
{
   int i;
   outstand *tmp;

   for(i=0; i<HASH_OUTSTAND; i++) {
      tmp = currlogorg->hashoutstand[i];
      while ((tmp) && ((tmp->buf != buf) || ((opid != -1) && (tmp->reqopid != opid)))) {
         tmp = tmp->next;
      }
      if (tmp) {
         return(tmp);
      }
   }
   return(NULL);
}


outstand * logorg_getfromoutstandq(currlogorg, requestno)
logorg *currlogorg;
int requestno;
{
   outstand *temp = NULL;
   outstand *hold = NULL;
   int i;

   if (requestno == -1) {
      for(i=0; i<HASH_OUTSTAND; i++) {
         if (currlogorg->hashoutstand[i] != NULL) {
            temp = currlogorg->hashoutstand[i];
            currlogorg->hashoutstand[i] = temp->next;
            currlogorg->outstandqlen--;
            return(temp);
         }
      }
   }

   temp = currlogorg->hashoutstand[(requestno % HASH_OUTSTAND)];
   if (temp == NULL)
      return(NULL);
   if (temp->opid == requestno) {
      currlogorg->outstandqlen--;
      currlogorg->hashoutstand[(requestno % HASH_OUTSTAND)] = temp->next;
   } else {
      while (temp->next != NULL) {
         if (temp->next->opid == requestno) {
            hold = temp->next;
            temp->next = temp->next->next;
            currlogorg->outstandqlen--;
            return(hold);
         }
         temp = temp->next;
      }
      temp = NULL;
   }
   return(temp);
}


void logorg_mapideal(currlogorg, curr)
logorg *currlogorg;
ioreq_event *curr;
{
   curr->devno = currlogorg->idealno;
   curr->blkno = -1;
   currlogorg->idealno = (currlogorg->idealno + 1) % currlogorg->numdisks;
   curr->next = curr;
}


void logorg_maprandom(currlogorg, curr)
logorg *currlogorg;
ioreq_event *curr;
{
   curr->devno = (int)((double)currlogorg->numdisks * drand48());
   curr->blkno = -1;
   curr->next = curr;
}


int logorg_mapinterleaved(currlogorg, curr)
logorg *currlogorg;
ioreq_event *curr;
{
   int numdisks;
   int blkno;
   int i;
   ioreq_event *temp;

   numdisks = currlogorg->numdisks;
   blkno = curr->blkno / numdisks;
   curr->bcount += curr->blkno - (blkno * numdisks);
   curr->bcount = (curr->bcount + numdisks - 1) / numdisks;
   curr->blkno = blkno;
   curr->devno = 0;
   temp = curr;
   for (i = 1; i < numdisks; i++) {
      temp->next = ioreq_copy(curr);
      temp->next->devno = i;
      temp = temp->next;
   }
   temp->next = curr;
   return(numdisks);
}


int logorg_mapstriped(currlogorg, curr)
logorg *currlogorg;
ioreq_event *curr;
{
   int numdisks;
   int stripeunit;
   int unitno;
   int stripeno;
   int reqsize;
   int numstripes = 2;
   int numreqs = 1;
   int startslop;
   int devno;
   int startdevno;
   ioreq_event *newreq;
   ioreq_event *temp;
   int blkno;
   int i;
   int last = FALSE;

   numdisks = currlogorg->numdisks;
   stripeunit = currlogorg->stripeunit;

   temp = curr;
   if (currlogorg->addrbyparts) {
      curr->blkno = (curr->devno * currlogorg->blksperpart) + curr->blkno;
   }
   if (stripeunit == 0) {
      return(logorg_mapinterleaved(currlogorg, curr));
   }
   unitno = curr->blkno / stripeunit;
   stripeno = unitno / numdisks;
   if (stripeno == currlogorg->numfull) {
      last = TRUE;
      stripeunit = currlogorg->blksperpart - (currlogorg->numfull * stripeunit);
      unitno = stripeno * numdisks;
      unitno += (curr->blkno - (unitno * currlogorg->stripeunit)) / stripeunit;
      curr->blkno -= stripeno * numdisks * currlogorg->stripeunit;
   }
   devno = unitno % numdisks;
   curr->devno = devno;
   curr->blkno = curr->blkno % stripeunit;
   startslop = stripeunit - curr->blkno;
   curr->blkno += stripeno * currlogorg->stripeunit;
   if (startslop < curr->bcount) {
      blkno = stripeno * currlogorg->stripeunit;
      for (i=0; i<numdisks; i++) {
         currlogorg->sizes[i] = 0;
      }
      currlogorg->sizes[devno] = startslop;
      reqsize = curr->bcount - startslop;
/*
fprintf (outputfile, "Have a multi-disk request - bcount %d, startslop %d, stripeno %d, numfull %d\n", curr->bcount, startslop, stripeno, currlogorg->numfull);
*/
      startdevno = devno;
      devno++;
      if (devno == numdisks) {
	 devno = 0;
	 stripeno++;
	 if (stripeno == currlogorg->numfull) {
	    stripeunit = currlogorg->blksperpart - (stripeno * stripeunit);
	 }
      }
      while (reqsize > stripeunit) {
	 currlogorg->sizes[devno] += stripeunit;
	 numstripes++;
	 devno++;
	 reqsize -= stripeunit;
	 if (devno == numdisks) {
	    devno = 0;
	    stripeno++;
	    if (stripeno == currlogorg->numfull) {
	       stripeunit = currlogorg->blksperpart - (stripeno * stripeunit);
	    }
	 }
      }
      currlogorg->sizes[devno] += reqsize;
      curr->bcount = currlogorg->sizes[startdevno];
      numreqs = min(numstripes, numdisks);
      for (i = (startdevno + 1); i < (startdevno + numreqs); i++) {
         newreq = (ioreq_event *) getfromextraq();
	 newreq->devno = i % numdisks;
         newreq->time = curr->time;
         newreq->blkno = blkno + (wrap(startdevno, newreq->devno) * currlogorg->stripeunit);
         newreq->busno = curr->busno & 0x0000FFFF;
         newreq->bcount = currlogorg->sizes[(i % numdisks)];
         newreq->flags = curr->flags;
	 newreq->next = NULL;
	 newreq->prev = NULL;
	 newreq->type = curr->type;
	 newreq->opid = curr->opid;
	 newreq->buf = curr->buf;
         temp->next = newreq;
	 temp = newreq;
      }
   }
   temp->next = curr;
   return(numreqs);
}


int logorg_maprequest(logorgs, numlogorgs, curr)
logorg *logorgs;
int numlogorgs;
ioreq_event *curr;
{
   int numreqs = 1;
   int maptype;
   int reduntype;
   int logorgno = -1;
   int i, j;
   outstand *req = NULL;
   ioreq_event *temp;
   int orgdevno;
/*
fprintf (outputfile, "Entered logorg_maprequest - numlogorgs %d, devno %d, blkno %d, time %f\n", numlogorgs, curr->devno, curr->blkno, simtime);
fprintf (outputfile, "flags %x\n", curr->flags);
*/
   ASSERT1((numlogorgs >= 1) && (numlogorgs < MAXLOGORGS), "numlogorgs", numlogorgs);
   if (curr->flags & MAPPED) {
      curr->flags &= ~MAPPED;
      curr->next = NULL;
      return(1);
   }
   req = (outstand *) getfromextraq();
   ASSERT (req != NULL);
   req->arrtime = simtime;
   req->bcount = curr->bcount;
   req->blkno = curr->blkno;
   req->devno = curr->devno;
   req->flags = curr->flags;
   req->busno = curr->busno;
   req->buf = curr->buf;
   req->reqopid = curr->opid;
   req->depend = NULL;
   for (i = 0; i < numlogorgs; i++) {
      if (!logorgs[i].addrbyparts) {
	 if ((curr->devno == logorgs[i].arraydisk) && ((curr->blkno + curr->bcount) < (logorgs[i].blksperpart * logorgs[i].numdisks)) && (curr->blkno >= 0)) {
	    logorgno = i;
	    break;
	 }
      } else {
         for (j = 0; j < logorgs[i].numdisks; j++) {
	    if (curr->devno == logorgs[i].devs[j].devno) {
	       if (logorg_overlap(&logorgs[i], j, curr, logorgs[i].blksperpart) == TRUE) {
	          logorgno = i;
	          break;
	       }
	    }
         }
	 if (logorgno != -1) {
	    break;
         }
      }
   }

   /* Every request must be covered by a logorg */
   if (logorgno == -1) {
      fprintf (stderr, "unexpected request location: devno %x, blkno %d, bcount %d\n", curr->devno, curr->blkno, curr->bcount);
   }
   ASSERT(logorgno != -1);
   maptype = logorgs[logorgno].maptype;
   reduntype = logorgs[logorgno].reduntype;

   curr->opid = logorgs[logorgno].opid;
   req->opid = logorgs[logorgno].opid;
   curr->next = NULL;
   curr->prev = NULL;

   if (maptype == ASIS) {
      curr->next = curr;
   } else if (maptype == IDEAL) {
      logorg_mapideal(&logorgs[logorgno], curr);
   } else if (maptype == RANDOM) {
      logorg_maprandom(&logorgs[logorgno], curr);
   } else if (maptype == STRIPED) {
      numreqs = logorg_mapstriped(&logorgs[logorgno], curr);
   } else {
      fprintf(stderr, "Unknown maptype in use at logorg_maprequest - %d\n", maptype);
      exit(0);
   }

   if (reduntype == NO_REDUN) {
   } else if (reduntype == SHADOWED) {
      numreqs = logorg_shadowed(&logorgs[logorgno], curr, numreqs);
   } else if (reduntype == PARITY_DISK) {
      numreqs = logorg_parity_disk(&logorgs[logorgno], curr, numreqs);
   } else if (reduntype == PARITY_ROTATED) {
      numreqs = logorg_parity_rotate(&logorgs[logorgno], curr, numreqs);
   } else if (reduntype == PARITY_TABLE) {
      numreqs = logorg_parity_table(&logorgs[logorgno], curr, numreqs);
   } else {
      fprintf(stderr, "Unknown reduntype in use at logorg_maprequest - %d\n", reduntype);
      exit(0);
   }
/*
fprintf (outputfile, "back from logorging: numreqs %d\n", numreqs);
*/
   orgdevno = curr->devno;
   req->depend = (depends *) curr->prev;
   curr->blkno += logorgs[logorgno].devs[orgdevno].startblkno;
   curr->devno = logorgs[logorgno].devs[orgdevno].devno;
   temp = curr->next;
   i = 1;
   while (temp != curr) {
      temp->blkno += logorgs[logorgno].devs[(temp->devno - logorgs[logorgno].devs[0].devno)].startblkno;
      temp->devno = logorgs[logorgno].devs[(temp->devno - logorgs[logorgno].devs[0].devno)].devno;
      temp->opid = curr->opid;
      temp = temp->next;
      i++;
   }
   req->numreqs = numreqs;
   logorg_addnewtooutstandq(&logorgs[logorgno], req);
   logorgs[logorgno].opid++;
   logorg_maprequest_update_stats(&logorgs[logorgno], curr, req, i);
/*
fprintf (outputfile, "Leaving logorg_maprequest: cnt %d, devno %d, blkno %d\n", i, curr->devno, curr->blkno);
*/
   return(i);
}


int logorg_mapcomplete(logorgs, numlogorgs, curr)
logorg *logorgs;
int numlogorgs;
ioreq_event *curr;
{
   outstand *req;
   int i, j;
   int logorgno = -1;
   int ret = NOT_COMPLETE;
   ioreq_event *temp;
/*
fprintf (outputfile, "Entered logorg_mapcomplete: %f, devno %d, blkno %d\n", simtime, curr->devno, curr->blkno);
*/
   if ((numlogorgs < 1) || (numlogorgs >= MAXLOGORGS)) {
      fprintf(stderr, "Bad number of logorgs at logorg_mapcomplete: %d\n", numlogorgs);
      exit(0);
   }
   for (i = 0; i < numlogorgs; i++) {
      for (j = 0; j < logorgs[i].actualnumdisks; j++) {
         if (curr->devno == logorgs[i].devs[j].devno) {
            if (logorg_overlap(&logorgs[i], j, curr, logorgs[i].actualblksperpart) == TRUE) {
               logorgno = i;
               break;
            }
         }
      }
      if (logorgno != -1)
         break;
   }
   if (logorgno == -1) {
      fprintf(stderr, "logorg_mapcomplete entered for non-logorged request\n");
      exit(0);
   }

   if (curr->flags & READ) {
      logorgs[logorgno].stat.reads++;
   }
   logorgs[logorgno].stat.idlestart = simtime;
   logorgs[logorgno].devs[curr->devno].numout--;
   req = logorg_getfromoutstandq(&logorgs[logorgno], curr->opid);
   ASSERT(req != NULL);
   req->numreqs--;
   if (req->depend) {
      ret = logorg_check_dependencies(&logorgs[logorgno], req, curr);
      temp = curr->next;
      for (i = 0; i < ret; i++) {
         temp->blkno += logorgs[logorgno].devs[(temp->devno)].startblkno;
         temp->devno = logorgs[logorgno].devs[(temp->devno)].devno;
         temp = temp->next;
      }
      if (ret == 0) {
	 ret = NOT_COMPLETE;
      }
   }

   curr->blkno += logorgs[logorgno].devs[(curr->devno)].startblkno;
   curr->devno = logorgs[logorgno].devs[(curr->devno)].devno;
   if (req->numreqs) {
      logorg_addtooutstandq(&logorgs[logorgno], req);
      return(ret);
   } else {
/*
fprintf (outputfile, "Request completion:  %2d %7d %4d %c %f\n", req->devno, req->blkno, req->bcount, ((req->flags & READ) ? 'R' : 'W'), (simtime - req->arrtime));
*/

      logorg_mapcomplete_update_stats(&logorgs[logorgno], curr, req);
      curr->bcount = req->bcount;
      curr->blkno = req->blkno;
      curr->flags = req->flags;
      curr->busno = req->busno;
      curr->devno = req->devno;
      curr->opid = req->reqopid;
      curr->buf = req->buf;
      addtoextraq((event *) req);
      return(COMPLETE);
   }
/*
fprintf (outputfile, "Leaving logorg_mapcomplete\n");
*/
}


void logorg_raise_priority(logorgs, numlogorgs, opid, devno, blkno, buf)
logorg *logorgs;
int numlogorgs;
int opid;
int devno;
int blkno;
void *buf;
{
   int i, j;
   int logorgno = -1;
   int calc;
   outstand *tmp;
   int found = 0;
/*
fprintf (outputfile, "Entered logorg_raise_priority: numlogorgs %d, devno %d, blkno %d, buf %p\n", numlogorgs, devno, blkno, buf);
*/
   ASSERT((numlogorgs >= 1) && (numlogorgs < MAXLOGORGS));
   for (i = 0; i < numlogorgs; i++) {
      if (!logorgs[i].addrbyparts) {
	 if (devno == logorgs[i].arraydisk) {
	    logorgno = i;
	    break;
	 }
      } else {
         for (j = 0; j < logorgs[i].numdisks; j++) {
	    if (devno == logorgs[i].devs[j].devno) {
               calc = blkno - logorgs[i].devs[devno].startblkno;
               if ((calc >= 0) && (calc < logorgs[i].blksperpart)) {
	          logorgno = i;
	          break;
	       }
	    }
         }
	 if (logorgno != -1) {
	    break;
         }
      }
   }
   ASSERT(logorgno != -1);
   tmp = logorg_show_buf_from_outstandq(&logorgs[logorgno], buf, opid);
   if (tmp) {
      for (i=0; i<logorgs[logorgno].actualnumdisks; i++) {
         found = ioqueue_raise_priority(logorgs[logorgno].devs[i].queue, tmp->opid);
      }
   }
/*
fprintf (outputfile, "Leaving logorg_raise_priority\n");
*/
}


logorg * logorg_readparams(parfile, numlogorgs, printlocalitystats, printblockingstats, printinterferestats, printstreakstats, printstampstats, printintarrstats, printidlestats, printsizestats)
FILE *parfile;
int numlogorgs;
int printlocalitystats;
int printblockingstats;
int printinterferestats;
int printstreakstats;
int printstampstats;
int printintarrstats;
int printidlestats;
int printsizestats;
{
   logorg *logorgs = NULL;
   logorgdev *devs = NULL;
   int orgno;
   char byparts[10];
   char type[20];
   char redun[30];
   char comp[10];
   int devno;
   int start;
   int end;
   double rmwpoint;
   char filename[40];
   FILE * stampfile = NULL;
   int i = 0;
   int j;

   if (numlogorgs == 0)
      return(NULL);

   if (!(logorgs = (logorg *) malloc(numlogorgs * (sizeof(logorg))))) {
      fprintf(stderr, "Problem allocating logorgs in logorg_readparams\n");
      exit(0);
   }

   while (i < numlogorgs) {

      for (j=0; j<HASH_OUTSTAND; j++) {
         logorgs[i].hashoutstand[j] = NULL;
      }
      logorgs[i].outstandqlen = 0;

      if (fscanf(parfile, "Organization #%d: %s %s %s %s\n", &orgno, byparts, type, redun, comp) != 5) {
         fprintf(stderr, "Error reading 'Organization #' for logorg\n");
         exit(0);
      }
      if (orgno != (i + 1)) {
         fprintf(stderr, "Logical organizations appear out of order\n");
         exit(0);
      }
      fprintf (outputfile, "Organization #%d: %s %s %s %s\n", orgno, byparts, type, redun, comp);

      if (strcmp(comp, "Whole") == 0) {
         getparam_int(parfile, "Number of disks", &devno, 3, 1, MAXDISKS);
         logorgs[i].numdisks = devno;
         logorgs[i].actualnumdisks = devno;
         if (fscanf(parfile, "Disks: %d-%d\n", &start, &end) != 2) {
            fprintf(stderr, "Error reading disks in logical organization #%d\n", orgno);
            exit(0);
         }
         if (((end - start + 1) != devno) || (start < 1)) {
            fprintf(stderr, "Invalid set of disks in logical organization #%d\n", orgno);
            exit(0);
         }
	 fprintf (outputfile, "Disks: %d-%d\n", start, end);
         logorgs[i].blksperpart = disk_get_number_of_blocks(start - 1);
	 logorgs[i].actualblksperpart = logorgs[i].blksperpart;
         devs = (logorgdev *) malloc(devno * (sizeof(logorgdev)));
         ASSERT(devs != NULL);
	 for (j=0; j<devno; j++) {
	    devs[j].devno = j + start - 1;
	    devs[j].startblkno = 0;
	 }
	 logorgs[i].devs = &devs[0];
      } else if (strcmp(comp, "Partial") == 0) {
	 fprintf(stderr, "Haven't done partial disk organizations yet\n");
	 exit(0);
      } else {
         fprintf(stderr, "Unsupported component type for logical organization #%d\n", orgno);
         exit(0);
      }

      if (strcmp(byparts, "Parts") == 0) {
	 logorgs[i].addrbyparts = TRUE;
      } else if (strcmp(byparts, "Array") == 0) {
	 logorgs[i].addrbyparts = FALSE;
      } else {
	 fprintf(stderr, "Unknown logical organization addressing specified - %s\n", byparts);
	 exit(0);
      }
      getparam_int(parfile, "High-level device number", &logorgs[i].arraydisk, 1, 0, 0);

      if (strcmp(type, "Asis") == 0) {
	 logorgs[i].maptype = ASIS;
	 if (!logorgs[i].addrbyparts) {
	    fprintf(stderr, "Can't have ASIS organization with array addressing\n");
	    exit(0);
	 }
      } else if (strcmp(type, "Striped") == 0) {
	 logorgs[i].maptype = STRIPED;
      } else if (strcmp(type, "Random") == 0) {
	 logorgs[i].maptype = RANDOM;
      } else if (strcmp(type, "Ideal") == 0) {
	 logorgs[i].maptype = IDEAL;
      } else {
	 fprintf(stderr, "Unknown logical organization type specified - %s\n", type);
	 exit(0);
      }

      getparam_int(parfile, "Stripe unit (in sectors)", &logorgs[i].stripeunit, 3, 0, logorgs[i].blksperpart);

      if (strcmp(redun, "Noredun") == 0) {
	 logorgs[i].reduntype = NO_REDUN;
      } else if (strcmp(redun, "Shadowed") == 0) {
	 logorgs[i].reduntype = SHADOWED;
      } else if (strcmp(redun, "Parity_disk") == 0) {
	 logorgs[i].reduntype = PARITY_DISK;
      } else if (strcmp(redun, "Parity_rotated") == 0) {
	 logorgs[i].reduntype = PARITY_ROTATED;
      } else {
	 fprintf(stderr, "Unknown logical organization redun specified - %s\n", redun);
	 exit(0);
      }

      getparam_int(parfile, "Synch writes for safety", &logorgs[i].writesync, 3, 0, 1);
      getparam_int(parfile, "Number of copies", &logorgs[i].copies, 1, 2, 0);
      getparam_int(parfile, "Copy choice on read", &logorgs[i].copychoice, 3, 1, 6);

      getparam_double(parfile, "RMW vs. reconstruct", &rmwpoint, 3, (double) 0.0, (double) 1.0);
      logorgs[i].rmwpoint = (int) (rmwpoint * (double) logorgs[i].actualnumdisks);

      getparam_int(parfile, "Parity stripe unit", &logorgs[i].parityunit, 3, 1, logorgs[i].blksperpart);
      getparam_int(parfile, "Parity rotation type", &logorgs[i].rottype, 3, 1, 4);

      getparam_double(parfile, "Time stamp interval", &logorgs[i].stampinterval, 1, (double) 0.0, (double) 0.0);
      getparam_double(parfile, "Time stamp start time", &logorgs[i].stampstart, 1, (double) 0.0, (double) 0.0);
      getparam_double(parfile, "Time stamp stop time", &logorgs[i].stampstop, 1, (double) (logorgs[i].stampstart + logorgs[i].stampinterval), (double) 0.0);

      if (fscanf(parfile,"Time stamp file name :  %s\n", filename) != 1) {
         fprintf(stderr, "Error reading time stamp file name\n");
         exit(0);
      }
      if ((filename) && (logorgs[i].stampinterval != 0.0) && (strcmp(filename,"0")) && (strcmp(filename,"null")) && !(stampfile = fopen(filename, "w"))) {
         fprintf(stderr, "Invalid value for time stamp file name - %s\n", filename);
         exit(0);
      }
      fprintf (outputfile, "Time stamp file name :        %s\n", filename);
      logorgs[i].stampfile = stampfile;

      logorgs[i].printlocalitystats = printlocalitystats;
      logorgs[i].printblockingstats = printblockingstats;
      logorgs[i].printinterferestats = printinterferestats;
      logorgs[i].printidlestats = printidlestats;
      logorgs[i].printintarrstats = printintarrstats;
      logorgs[i].printstreakstats = printstreakstats;
      logorgs[i].printstampstats = printstampstats;
      logorgs[i].printsizestats = printsizestats;
      logorgs[i].sizes = NULL;
      logorgs[i].redunsizes = NULL;
      logorgs[i].stat.blocked = NULL;
      logorgs[i].stat.aligned = NULL;
      logorgs[i].stat.lastreq = NULL;
      logorgs[i].stat.intdist = NULL;
      i++;
   }
   return(logorgs);
}


void logorg_param_override(logorgs, numlogorgs, paramname, paramval, first, last)
logorg *logorgs;
int numlogorgs;
char *paramname;
char *paramval;
int first;
int last;
{
   int i;

   if (first == -1) {
      first = 0;
      last = numlogorgs - 1;
   } else if ((first < 0) || (last >= numlogorgs) || (first > last)) {
      fprintf(stderr, "Invalid range passed to logorg_param_override: %d-%d\n", first, last);
      exit(0);
   }
   for (i=first; i<=last; i++) {
      if (strcmp(paramname, "stripeunit") == 0) {
	 if (sscanf(paramval, "%d\n", &logorgs[i].stripeunit) != 1) {
	    fprintf(stderr, "Error reading stripeunit in logorg_param_override\n");
	    exit(0);
	 }
         if ((logorgs[i].stripeunit < 0) || (logorgs[i].stripeunit > logorgs[i].blksperpart)) {
            fprintf(stderr, "Invalid value for stripeunit at logorg_param_override: %d\n", logorgs[i].stripeunit);
            exit(0);
         }
      } else if (strcmp(paramname, "parityunit") == 0) {
         if (sscanf(paramval,"%d\n", &logorgs[i].parityunit) != 1) {
            fprintf(stderr, "Error reading parityunit in logorg_param_override\n");
            exit(0);
         }
         if ((logorgs[i].parityunit < 1) || (logorgs[i].parityunit > logorgs[i].blksperpart)) {
            fprintf(stderr, "Invalid value for parity stripe unit - %d\n", logorgs[i].parityunit);
            exit(0);
         }
      } else {
         fprintf(stderr, "Unsupported paramname at logorg_param_override: %s\n", paramname);
         exit(0);
      }
   }
}


void logorg_resetstats(logorgs, numlogorgs)
logorg *logorgs;
int numlogorgs;
{
   int i, j, k;

   for (i=0; i<numlogorgs; i++) {
      logorgs[i].stat.maxoutstanding = 0;
      logorgs[i].stat.nonzeroouttime = 0.0;
      logorgs[i].stat.runouttime = 0.0;
      logorgs[i].stat.outtime = simtime;
      logorgs[i].stat.reads = 0;
      logorgs[i].stat.idlestart = simtime;
      for (j=0; j<10; j++) {
         logorgs[i].stat.distavgdiff[j] = 0;
         logorgs[i].stat.distmaxdiff[j] = 0;
      }
      logorgs[i].stat.critreads = 0;
      logorgs[i].stat.critwrites = 0;
      logorgs[i].stat.seqreads = 0;
      logorgs[i].stat.seqwrites = 0;
      logorgs[i].stat.numlocal = 0;
      logorgs[i].stat.seqdiskswitches = 0;
      logorgs[i].stat.locdiskswitches = 0;
      if (logorgs[i].stat.intdist) {
         for (j=0; j<(INTERFEREMAX*INTDISTMAX); j++) {
            logorgs[i].stat.intdist[j] = 0;
         }
      }
      if (logorgs[i].stat.blocked) {
         for (j=0; j<BLOCKINGMAX; j++) {
            logorgs[i].stat.blocked[j] = 0;
            logorgs[i].stat.aligned[j] = 0;
         }
      }
      for (j=0; j<logorgs[i].actualnumdisks; j++) {
         logorgs[i].devs[j].seqreads = 0;
         logorgs[i].devs[j].seqwrites = 0;
         logorgs[i].devs[j].intreads = 0;
         logorgs[i].devs[j].intwrites = 0;
	 for (k=0; k<10; k++) {
            logorgs[i].devs[j].distnumout[k] = 0;
         }
         stat_reset(&logorgs[i].devs[j].streakstats);
         stat_reset(&logorgs[i].devs[j].localitystats);
      }
      stat_reset(&logorgs[i].stat.intarrstats);
      stat_reset(&logorgs[i].stat.readintarrstats);
      stat_reset(&logorgs[i].stat.writeintarrstats);
      stat_reset(&logorgs[i].stat.sizestats);
      stat_reset(&logorgs[i].stat.readsizestats);
      stat_reset(&logorgs[i].stat.writesizestats);
      stat_reset(&logorgs[i].stat.idlestats);
      stat_reset(&logorgs[i].stat.resptimestats);
   }
}


void logorg_initialize(logorgs, numlogorgs, queueset)
logorg *logorgs;
int numlogorgs;
struct ioq **queueset;
{
   int i, j, k;

   StaticAssert (sizeof(depends) <= DISKSIM_EVENT_SIZE);
   StaticAssert (sizeof(outstand) <= DISKSIM_EVENT_SIZE);

   for (i = 0; i < numlogorgs; i++) {
      if (logorgs[i].reduntype == SHADOWED) {
         if ((logorgs[i].numdisks % logorgs[i].copies) != 0) {
            fprintf(stderr, "Number of disks not multiple of number of copies\n");
            exit(0);
         }
	 logorgs[i].numdisks = logorgs[i].numdisks / logorgs[i].copies;
      } else if (logorgs[i].reduntype == PARITY_DISK) {
	 if ((logorgs[i].maptype == STRIPED) && (logorgs[i].stripeunit != 0)) {
	    logorgs[i].maptype = ASIS;
	    logorg_create_table(&logorgs[i]);
	    logorgs[i].reduntype = PARITY_TABLE;
	 }
	 logorgs[i].numdisks--;
      } else if ((logorgs[i].reduntype == PARITY_ROTATED) && (logorg_tabular_rottype(logorgs[i].maptype, logorgs[i].reduntype, logorgs[i].rottype, logorgs[i].stripeunit))) {
	 logorg_create_table(&logorgs[i]);
	 logorgs[i].maptype = ASIS;
	 logorgs[i].reduntype = PARITY_TABLE;
	 logorgs[i].blksperpart = (int) ((double) logorgs[i].blksperpart * (double) (logorgs[i].numdisks - 1) / (double) logorgs[i].numdisks);
      } else if (logorgs[i].reduntype == PARITY_ROTATED) {
	 logorgs[i].blksperpart = (int) ((double) logorgs[i].blksperpart * (double) (logorgs[i].numdisks - 1) / (double) logorgs[i].numdisks) - logorgs[i].parityunit;
      }
      logorgs[i].opid = 0;
      logorgs[i].idealno = 0;
      logorgs[i].reduntoggle = 0;
      logorgs[i].lastdiskaccessed = logorgs[i].actualnumdisks;
      while (logorgs[i].outstandqlen != 0) {
         addtoextraq((event *) logorg_getfromoutstandq(&logorgs[i], -1));
      }
      if ((logorgs[i].maptype == STRIPED) && (logorgs[i].stripeunit)) {
         logorgs[i].numfull = logorgs[i].blksperpart / logorgs[i].stripeunit;
      } else if (logorgs[i].reduntype == PARITY_ROTATED) {
         logorgs[i].numfull = logorgs[i].actualblksperpart / logorgs[i].parityunit;
      } else if (logorgs[i].reduntype == PARITY_TABLE) {
         logorgs[i].numfull = logorgs[i].actualblksperpart / logorgs[i].stripeunit;
	 logorgs[i].numfull *= logorgs[i].stripeunit;
      } else {
	 logorgs[i].numfull = 0;
      }
      if (logorgs[i].sizes != NULL) {
	 free(logorgs[i].sizes);
      }
      if (logorgs[i].redunsizes != NULL) {
	 free(logorgs[i].redunsizes);
      }
      if (logorgs[i].stat.blocked != NULL) {
	 free(logorgs[i].stat.blocked);
      }
      if (logorgs[i].stat.aligned != NULL) {
	 free(logorgs[i].stat.aligned);
      }
      if (logorgs[i].stat.lastreq != NULL) {
	 free(logorgs[i].stat.lastreq);
      }
      if (logorgs[i].stat.intdist != NULL) {
	 free(logorgs[i].stat.intdist);
      }
      logorgs[i].sizes = (int *) malloc(logorgs[i].actualnumdisks*sizeof(int));
      ASSERT(logorgs[i].sizes != NULL);
      logorgs[i].redunsizes = (int *) malloc(logorgs[i].actualnumdisks*sizeof(int));
      ASSERT(logorgs[i].redunsizes != NULL);
      if (logorgs[i].printinterferestats) {
         logorgs[i].stat.lastreq = (int *) malloc(2*INTERFEREMAX*sizeof(int));
         ASSERT(logorgs[i].stat.lastreq != NULL);
         for (j=0; j<(2*INTERFEREMAX); j++) {
            logorgs[i].stat.lastreq[j] = 0;
         }
         logorgs[i].stat.intdist = (int *) malloc(INTERFEREMAX*INTDISTMAX*sizeof(int));
         ASSERT(logorgs[i].stat.intdist != NULL);
      }
      if (logorgs[i].printblockingstats) {
         logorgs[i].stat.blocked = (int *) malloc(BLOCKINGMAX*sizeof(int));
         ASSERT(logorgs[i].stat.blocked != NULL);
         logorgs[i].stat.aligned = (int *) malloc(BLOCKINGMAX*sizeof(int));
         ASSERT(logorgs[i].stat.aligned != NULL);
      }
      logorgs[i].stat.outstanding = 0;
      logorgs[i].stat.readoutstanding = 0;

      stat_initialize(statdeffile, statdesc_intarr, &logorgs[i].stat.intarrstats);
      stat_initialize(statdeffile, statdesc_readintarr, &logorgs[i].stat.readintarrstats);
      stat_initialize(statdeffile, statdesc_writeintarr, &logorgs[i].stat.writeintarrstats);
      stat_initialize(statdeffile, statdesc_reqsize, &logorgs[i].stat.sizestats);
      stat_initialize(statdeffile, statdesc_readsize, &logorgs[i].stat.readsizestats);
      stat_initialize(statdeffile, statdesc_writesize, &logorgs[i].stat.writesizestats);
      stat_initialize(statdeffile, statdesc_idles, &logorgs[i].stat.idlestats);
      stat_initialize(statdeffile, statdesc_resptime, &logorgs[i].stat.resptimestats);

      logorgs[i].stat.lastarr = 0.0;
      logorgs[i].stat.lastread = 0.0;
      logorgs[i].stat.lastwrite = 0.0;
      for (j=0; j<NUMGENS; j++) {
         logorgs[i].stat.gens[j] = -1;
      }
      for (k=0; k<logorgs[i].actualnumdisks; k++) {
         logorgs[i].devs[k].queue = queueset[logorgs[i].devs[k].devno];
         logorgs[i].devs[k].lastblkno = -1;
         logorgs[i].devs[k].lastblkno2 = -1;
         logorgs[i].devs[k].numout = 0;
         logorgs[i].devs[k].curstreak = 0;
         stat_initialize(statdeffile, statdesc_streak, &logorgs[i].devs[k].streakstats);
         stat_initialize(statdeffile, statdesc_locality, &logorgs[i].devs[k].localitystats);
      }
      if (logorgs[i].stampinterval != 0.0) {
         ioreq_event *tmp = (ioreq_event *) getfromextraq();
         tmp->time = logorgs[i].stampstart;
         tmp->type = TIMESTAMP_LOGORG;
         tmp->tempptr1 = &logorgs[i];
         addtointq((event *) tmp);
      }
   }
   logorg_resetstats(logorgs, numlogorgs);
}


void logorg_cleanstats(logorgs, numlogorgs)
logorg *logorgs;
int numlogorgs;
{
   int i;
   double tpass;

   for (i=0; i<numlogorgs; i++) {
      logorg_streakstat(&logorgs[i], logorgs[i].actualnumdisks);
      if (logorgs[i].stat.outstanding > 0) {
         logorgs[i].stat.nonzeroouttime += simtime - logorgs[i].stat.outtime;
      } else {
         tpass = simtime - logorgs[i].stat.idlestart;
         stat_update(&logorgs[i].stat.idlestats, tpass);
      }
      logorgs[i].stat.idlestart = simtime;
      logorgs[i].stat.runouttime += (double) logorgs[i].stat.outstanding * (simtime - logorgs[i].stat.outtime);
      logorgs[i].stat.outtime = simtime;
   }
}


void logorg_printreqtimestats(currlogorg, prefix)
logorg *currlogorg;
char *prefix;
{
   int tmp1, tmp2;

   tmp1 = stat_get_count(&currlogorg->stat.intarrstats);
   fprintf(outputfile, "%sNumber of requests:       %d\n", prefix, tmp1);
   tmp2 = stat_get_count(&currlogorg->stat.readintarrstats);
   fprintf(outputfile, "%sNumber of read requests:  %d  \t%f\n", prefix, tmp2, ((double) tmp2 / (double) tmp1));

   tmp1 = stat_get_count(&currlogorg->stat.resptimestats) + currlogorg->stat.outstanding;
   fprintf(outputfile, "%sNumber of accesses:       %d\n", prefix, tmp1);
   tmp2 = currlogorg->stat.reads + currlogorg->stat.readoutstanding;
   fprintf(outputfile, "%sNumber of read accesses:  %d  \t%f\n", prefix, tmp2, ((double) tmp2 / (double) tmp1));

   fprintf(outputfile, "%sAverage outstanding:      %f\n", prefix, (currlogorg->stat.runouttime / (simtime - warmuptime)));
   fprintf(outputfile, "%sMaximum outstanding:      %d\n", prefix, currlogorg->stat.maxoutstanding);
   fprintf(outputfile, "%sAvg nonzero outstanding:  %f\n", prefix, (currlogorg->stat.runouttime / currlogorg->stat.nonzeroouttime));
   fprintf(outputfile, "%sCompletely idle time:     %f\n", prefix, (simtime - warmuptime - currlogorg->stat.nonzeroouttime));
   stat_print(&currlogorg->stat.resptimestats, prefix);
}


void logorg_printlocalitystats(currlogorg, prefix)
logorg *currlogorg;
char *prefix;
{
   int i;
   int seqreads = 0;
   int seqwrites = 0;
   int intreads = 0;
   int intwrites = 0;
   int numreqs;
   int numreads;
   int numwrites;
   statgen * statset[MAXDISKS];

   if (currlogorg->printlocalitystats) {
      for (i=0; i<currlogorg->actualnumdisks; i++) {
	 seqreads += currlogorg->devs[i].seqreads;
	 seqwrites += currlogorg->devs[i].seqwrites;
	 intreads += currlogorg->devs[i].intreads;
	 intwrites += currlogorg->devs[i].intwrites;
	 statset[i] = &currlogorg->devs[i].localitystats;
      }
      numreqs = stat_get_count(&currlogorg->stat.intarrstats);
      numreads = stat_get_count(&currlogorg->stat.readintarrstats);
      numwrites = stat_get_count(&currlogorg->stat.writeintarrstats);

      stat_print_set(statset, currlogorg->actualnumdisks, prefix);
      fprintf(outputfile, "%sSequential reads:           %4d\t%f\t%f\n", prefix, seqreads, ((double) seqreads / (double) max(numreqs,1)), ((double) seqreads / (double) max(numreads,1)));
      fprintf(outputfile, "%sSequential writes:          %4d\t%f\t%f\n", prefix, seqwrites, ((double) seqwrites / (double) max(numreqs,1)), ((double) seqwrites / (double) max(numwrites,1)));
      fprintf(outputfile, "%sInterleaved reads:          %4d\t%f\t%f\n", prefix, intreads, ((double) intreads / (double) max(numreqs,1)), ((double) intreads / (double) max(numreads,1)));
      fprintf(outputfile, "%sInterleaved writes:         %4d\t%f\t%f\n", prefix, intwrites, ((double) intwrites / (double) max(numreqs,1)), ((double) intwrites / (double) max(numwrites,1)));

      fprintf(outputfile, "%sLogical sequential reads:   %4d\t%f\t%f\n", prefix, currlogorg->stat.seqreads, ((double) currlogorg->stat.seqreads / (double) max(numreqs,1)), ((double) currlogorg->stat.seqreads / (double) max(numreads,1)));
      fprintf(outputfile, "%sLogical sequential writes:  %4d\t%f\t%f\n", prefix, currlogorg->stat.seqwrites, ((double) currlogorg->stat.seqwrites / (double) max(numreqs,1)), ((double) currlogorg->stat.seqwrites / (double) max(numwrites,1)));
      seqreads = currlogorg->stat.seqreads + currlogorg->stat.seqwrites;
      fprintf(outputfile, "%sSequential disk switches:   %4d\t%f\n", prefix, currlogorg->stat.seqdiskswitches, ((double) currlogorg->stat.seqdiskswitches / (double) max(seqreads,1)));
      fprintf(outputfile, "%sLogical local accesses:     %4d\t%f\n", prefix, currlogorg->stat.numlocal, ((double) currlogorg->stat.numlocal / (double) max(numreqs,1)));
      fprintf(outputfile, "%sLocal disk swicthes:        %4d\t%f\n", prefix, currlogorg->stat.locdiskswitches, ((double) currlogorg->stat.locdiskswitches / (double) max(currlogorg->stat.numlocal,1)));
   }
}


void logorg_printinterferestats(currlogorg, prefix)
logorg *currlogorg; 
char *prefix;
{
   int i;
   double frac;
   double reqcnt;
   int *intdist;
   int localoff[INTDISTMAX];

   if (currlogorg->printinterferestats == FALSE) {
      return;
   }

   localoff[1] = -4;
   localoff[2] = -8;
   localoff[3] = -16;
   localoff[4] = -64;
   localoff[5] = 8;
   localoff[6] = 16;
   localoff[7] = 64;
   intdist = currlogorg->stat.intdist;
   reqcnt = (double) stat_get_count(&currlogorg->stat.intarrstats);
   for (i=0; i<(INTERFEREMAX*INTDISTMAX); i+=INTDISTMAX) {
      frac = (double) intdist[i] / reqcnt;
      if (frac >= 0.002) {
         fprintf (outputfile, "%sSequential step %2d:  %6d \t%f\n", prefix, (i/INTDISTMAX), intdist[i], frac);
      }
   }
   for (i=0; i<(INTERFEREMAX*INTDISTMAX); i++) {
      if (i % INTDISTMAX) {
	 frac = (double) intdist[i] / reqcnt;
	 if (frac >= 0.002) {
	    fprintf (outputfile, "%sLocal (%3d) step %2d:  %6d \t%f\n", prefix, localoff[(i % INTDISTMAX)], (i/INTDISTMAX), intdist[i], frac);
	 }
      }
   }
}


void logorg_printblockingstats(currlogorg, prefix)
logorg *currlogorg;
char *prefix;
{
   int i;
   double frac;
   double reqcnt;

   if (currlogorg->printblockingstats == FALSE) {
      return;
   }

   reqcnt = (double) stat_get_count(&currlogorg->stat.intarrstats);
   fprintf(outputfile, "%sBlocking statistics\n", prefix);
   for (i=0; i<BLOCKINGMAX; i++) {
      frac = (double) currlogorg->stat.blocked[i] / reqcnt;
      if (frac >= (double) 0.01) {
	 fprintf(outputfile, "%sBlocking factor: %3d \t%6d \t%f\n", prefix, (i+1), currlogorg->stat.blocked[i], frac);
      }
   }
   fprintf(outputfile, "%sAlignment statistics\n", prefix);
   for (i=0; i<BLOCKINGMAX; i++) {
      frac = (double) currlogorg->stat.aligned[i] / reqcnt;
      if (frac >= (double) 0.1) {
	 fprintf(outputfile, "%sAlignment factor: %3d \t%6d \t%f\n", prefix, (i+1), currlogorg->stat.aligned[i], frac);
      }
   }
}


void logorg_printstampstats(currlogorg, prefix)
logorg *currlogorg;
char *prefix;
{
   int i, j;
   int distnumout[10];

   if (currlogorg->printstampstats == FALSE)
      return;

   for (i=0; i<10; i++) {
      distnumout[i] = 0;
   }
   for (i=0; i<currlogorg->actualnumdisks; i++) {
      for (j=0; j<10; j++) {
         distnumout[j] += currlogorg->devs[i].distnumout[j];
      }
   }
   fprintf(outputfile, "%sTimestamped # outstanding distribution (interval = %f)\n", prefix, currlogorg->stampinterval);
   fprintf(outputfile, "     0       1       2       3       4       5       6       7       8      9+\n");
   fprintf(outputfile, "%6d  %6d  %6d  %6d  %6d  %6d  %6d  %6d  %6d  %6d\n",
      distnumout[0], distnumout[1], distnumout[2], distnumout[3],
      distnumout[4], distnumout[5], distnumout[6], distnumout[7],
      distnumout[8], distnumout[9]);
   fprintf(outputfile, "%sTimestamped avg # outstanding difference distribution\n", prefix);
   fprintf(outputfile, "   <.5      <1    <1.5      <2    <2.5      <3      <4      <5      <6      6+\n");
   fprintf(outputfile, "%6d  %6d  %6d  %6d  %6d  %6d  %6d  %6d  %6d  %6d\n",
      currlogorg->stat.distavgdiff[0], currlogorg->stat.distavgdiff[1],
      currlogorg->stat.distavgdiff[2], currlogorg->stat.distavgdiff[3],
      currlogorg->stat.distavgdiff[4], currlogorg->stat.distavgdiff[5],
      currlogorg->stat.distavgdiff[6], currlogorg->stat.distavgdiff[7],
      currlogorg->stat.distavgdiff[8], currlogorg->stat.distavgdiff[9]);
   fprintf(outputfile, "%sTimestamped max # outstanding difference distribution\n", prefix);
   fprintf(outputfile, "   <.5      <1    <1.5      <2    <2.5      <3      <4      <5      <6      6+\n");
   fprintf(outputfile, "%6d  %6d  %6d  %6d  %6d  %6d  %6d  %6d  %6d  %6d\n",
      currlogorg->stat.distmaxdiff[0], currlogorg->stat.distmaxdiff[1],
      currlogorg->stat.distmaxdiff[2], currlogorg->stat.distmaxdiff[3],
      currlogorg->stat.distmaxdiff[4], currlogorg->stat.distmaxdiff[5],
      currlogorg->stat.distmaxdiff[6], currlogorg->stat.distmaxdiff[7],
      currlogorg->stat.distmaxdiff[8], currlogorg->stat.distmaxdiff[9]);
}


void logorg_printstats(logorgs, numlogorgs, sourcestr)
logorg *logorgs;
int numlogorgs;
char *sourcestr;
{
   int i, j;
   int actualnumdisks;
   statgen * statset[MAXDISKS];
   int set[MAXDISKS];
   char prefix[80];

   for (i = 0; i < numlogorgs; i++) {
      fprintf (outputfile, "%sLogical Organization #%d\n", sourcestr, i);
      sprintf(prefix, "%slogorg #%d ", sourcestr, i);
      actualnumdisks = logorgs[i].actualnumdisks;
      logorg_printreqtimestats(&logorgs[i], prefix);
      fprintf (outputfile, "%sTime-critical reads:  %d\n", prefix, logorgs[i].stat.critreads);
      fprintf (outputfile, "%sTime-critical writes: %d\n", prefix, logorgs[i].stat.critwrites);
      logorg_printlocalitystats(&logorgs[i], prefix);
      logorg_printinterferestats(&logorgs[i], prefix);
      logorg_printblockingstats(&logorgs[i], prefix);

      if (logorgs[i].printintarrstats) {
	 stat_print(&logorgs[i].stat.intarrstats, prefix);
	 stat_print(&logorgs[i].stat.readintarrstats, prefix);
	 stat_print(&logorgs[i].stat.writeintarrstats, prefix);
      }

      if (logorgs[i].printstreakstats) {
         for (j = 0; j < actualnumdisks; j++) {
	    statset[j] = &logorgs[i].devs[j].streakstats;
         }
         fprintf (outputfile, "%sNumber of streaks:\t\t%d\n", prefix, stat_get_count_set(statset, actualnumdisks));
	 stat_print_set(statset, actualnumdisks, prefix);
      }

      logorg_printstampstats(&logorgs[i], prefix);

      if (logorgs[i].printsizestats) {
	 stat_print(&logorgs[i].stat.sizestats, prefix);
	 stat_print(&logorgs[i].stat.readsizestats, prefix);
	 stat_print(&logorgs[i].stat.writesizestats, prefix);
      }

      if (logorgs[i].printidlestats) {
         fprintf (outputfile, "%sNumber of idle periods:	%d\n", prefix, stat_get_count(&logorgs[i].stat.idlestats));
         stat_print(&logorgs[i].stat.idlestats, prefix);
      }

      for (j = 0; j < actualnumdisks; j++) {
	 set[j] = logorgs[i].devs[j].devno;
      }
      disk_printsetstats(set, actualnumdisks, prefix);
      if ((i+1) < numlogorgs) {
         fprintf (outputfile, "\n");
      }
   }
}

