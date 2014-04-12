
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


int trackstart = 0;
double addtolatency = 0.0;
extern int currcylno;
extern int currsurface;
extern double currtime;
extern double currangle;
int seekdistance;
double disk_seek_stoptime = 0.0;

int disk_last_distance = 0;
double disk_last_seektime = 0.0;
double disk_last_latency = 0.0;
double disk_last_xfertime = 0.0;
double disk_last_acctime = 0.0;
int disk_last_cylno = 0;
int disk_last_surface = 0;

extern int printhack;
extern int remapsector;


int diskspecialseektime(seektime)
double seektime;
{
   if (seektime == THREEPOINT_LINE) {
      return(TRUE);
   } else if (seektime == THREEPOINT_CURVE) {
      return(TRUE);
   } else if (seektime == HPL_SEEK_EQUATION) {
      return(TRUE);
   } else if (seektime == FIRST10_PLUS_HPL_SEEK_EQUATION) {
      return(TRUE);
   } else if (seektime == EXTRACTION_SEEK) {
      return(TRUE);
   }
   return(FALSE);
}


int diskspecialaccesstime(acctime)
double acctime;
{
   if (acctime == AVGROTATE) {
      return(TRUE);
   } else if (acctime == SKEWED_FOR_TRACK_SWITCH) {
      return(TRUE);
/*
   } else if (acctime == NON_SKEWED) {
      return(TRUE);
*/
   }
   return(FALSE);
}


void disk_acctimestats(currdisk, distance, seektime, latency, xfertime, acctime)
disk *currdisk;
int distance;
double seektime;
double latency;
double xfertime;
double acctime;
{
   int dist;

   disk_last_distance = distance;
   disk_last_seektime = seektime;
   disk_last_latency = latency;
   disk_last_xfertime = xfertime;
   disk_last_acctime = acctime;
   disk_last_cylno = currdisk->currcylno;
   disk_last_surface = currdisk->currsurface;
   if (disk_printseekstats == TRUE) {
      dist = abs(distance);
      if (dist == 0) {
         currdisk->stat.zeroseeks++;
      }
      stat_update(&currdisk->stat.seekdiststats, (double) dist);
      stat_update(&currdisk->stat.seektimestats, seektime);
   }
   if (disk_printlatencystats == TRUE) {
      if (latency == (double) 0.0) {
         currdisk->stat.zerolatency++;
      }
      stat_update(&currdisk->stat.rotlatstats, latency);
   }
   if (disk_printxferstats == TRUE) {
      stat_update(&currdisk->stat.xfertimestats, xfertime);
   }
   if (disk_printacctimestats == TRUE) {
      stat_update(&currdisk->stat.acctimestats, acctime);
   }
}


double disk_seektime_threepoint_curve(currdisk, distance, headswitch)
disk *currdisk;
int distance;
int headswitch;
{
   double seektime;
   int cyls;

   cyls = abs(distance);
   if (cyls == 0) {
      seektime = 0.0;
   } else {
      seektime = currdisk->seekone;
      seektime += currdisk->seekavg * (double) (cyls - 1);
      seektime += currdisk->seekfull * sqrt((double) (cyls - 1));
   }
   if ((headswitch) && (seektime < currdisk->headswitch)) {
      seektime = currdisk->headswitch;
   }
   return(seektime);
}


double disk_seektime_threepoint_line(currdisk, distance, headswitch)
disk *currdisk;
int distance;
int headswitch;
{
   double seektime;
   double mult;
   double numcyls;
   int cyls;

   cyls = abs(distance);
   numcyls = (double) currdisk->numcyls;
   if (cyls == 1) {
      seektime = currdisk->seekone;
   } else if (cyls == 0) {
      seektime = 0.0;
   } else if (cyls <= (currdisk->numcyls / 3)) {
      mult = ((double)(cyls-1) / (numcyls / (double)3));
      seektime = currdisk->seekone;
      seektime += mult * (currdisk->seekavg - seektime);
   } else {
      mult = ((double)(3*cyls) / ((double)2*numcyls)) - (double) 0.5;
      seektime = currdisk->seekavg;
      seektime += mult * (currdisk->seekfull - seektime);
   }
   if ((headswitch) && (seektime < currdisk->headswitch)) {
      seektime = currdisk->headswitch;
   }
   return(seektime);
}


/* seek equation described in Ruemmler and Wilkes's IEEE Computer article */
/* (March 1994).  Uses six parameters: a division point (between the root */
/* based equation and the linear equation), a constant and mulitplier for */
/* the square root of the distance, a constant and multiplier for the     */
/* distance (linear part of curve) and a value for single cylinder seeks. */

double disk_seektime_hpl_equation(currdisk, distance, headswitch)
disk *currdisk; 
int distance;
int headswitch;
{
   double seektime;
   double *hpseek;
   int dist;

   hpseek = &currdisk->hpseek[0];
   dist = abs(distance);
   if (distance == 0) {
      seektime = 0.0;
   } else if ((dist == 1) && (hpseek[5] != -1)) {
      seektime = currdisk->seekone;
   } else if (dist < hpseek[0]) {
      seektime = sqrt((double) dist) * hpseek[2] + hpseek[1];
   } else {
      seektime = (hpseek[4] * (double) dist) + hpseek[3];
   }
   if ((headswitch) && (seektime < currdisk->headswitch)) {
      seektime = currdisk->headswitch;
   }
   return(seektime);
}


/* An extended version of the equation described above, wherein the first  */
/* ten seek distances are explicitly provided, since seek time curves tend */
/* to be choppy in this region.  (See UM TR CSE-TR-194-94) */

double disk_seektime_first10_plus_hpl_equation(currdisk, distance, headswitch)
disk *currdisk; 
int distance;
int headswitch;
{
   double seektime;
   double *hpseek;
   int dist = abs(distance);

   hpseek = &currdisk->hpseek[0];
   if (distance == 0) {
      seektime = 0.0;
   } else if (dist <= 10) {
      seektime = currdisk->first10seeks[(dist - 1)];
   } else if (dist < hpseek[0]) {
      seektime = sqrt((double) dist) * hpseek[2] + hpseek[1];
   } else {
      seektime = (hpseek[4] * (double) dist) + hpseek[3];
   }
   if ((headswitch) && (seektime < currdisk->headswitch)) {
      seektime = currdisk->headswitch;
   }
   return(seektime);
}


void disk_read_extracted_seek_curve(filename, cntptr, distsptr, timesptr)
char *filename;
int *cntptr;
int **distsptr;
double **timesptr;
{
   int count;
   int *dists;
   double *times;
   int i;
   FILE *seekfile;

   if ((seekfile = fopen(filename, "r")) == NULL) {
      fprintf(stderr, "Can't open extracted seek curve file: %s\n", filename);
      exit(0);
   }
   if (fscanf(seekfile, "Seek distances measured: %d\n", &count) != 1) {
      fprintf(stderr, "Can't get Seek distances measured from extracted seek curve file: %s\n", filename);
      exit(0);
   }
   if (count < 0) {
      fprintf(stderr, "Invalid value for seek distances measured: %d\n", count);
      exit(0);
   }
   dists = (int *) malloc(count * sizeof(int));
   times = (double *) malloc(count * sizeof(double));
   if ((dists == NULL) || (times == NULL)) {
      fprintf(stderr, "Can't malloc space for extracted seek curve\n");
      exit(0);
   }
   for (i=0; i<count; i++) {
      if (fscanf(seekfile, "%d, %lf\n", &dists[i], &times[i]) != 2) {
	 fprintf(stderr, "Invalid line in extracted seek curve\n");
	 exit(0);
      }
   }
   fclose(seekfile);
   *cntptr = count;
   *distsptr = dists;
   *timesptr = times;
}


double disk_seektime_extraction_seek(currdisk, distance, headswitch)
disk *currdisk; 
int distance;
int headswitch;
{
   double seektime = 0.0;
   int dist = abs(distance);
   int i;

   if (dist) {
      for (i=0; i<currdisk->extractseekcnt; i++) {
	 if (dist <= currdisk->extractseekdists[i]) {
	    if (dist == currdisk->extractseekdists[i]) {
	       seektime = currdisk->extractseektimes[i];
	    } else {
	       double ddiff = (double) (dist - currdisk->extractseekdists[(i-1)]) / (double) (currdisk->extractseekdists[i] - currdisk->extractseekdists[(i-1)]);
	       seektime = currdisk->extractseektimes[(i-1)];
	       seektime += ddiff * (currdisk->extractseektimes[i] - currdisk->extractseektimes[(i-1)]);
	    }
	    break;
	 }
      }
      if (seektime == 0.0) {
	 fprintf(stderr, "Seek distance exceeds extracted seek curve: %d\n", dist);
	 exit(0);
      }
   }
   if ((headswitch) && (seektime < currdisk->headswitch)) {
      seektime = currdisk->headswitch;
   }
   return(seektime);
}


double diskseektime(currdisk, distance, headswitch, read)
disk *currdisk;
int distance;
int headswitch;
int read;
{
   double seektime;

   if (currdisk->seektime >= 0.0) {
      seektime = currdisk->seektime;
   } else if (currdisk->seektime == THREEPOINT_LINE) {
      seektime = disk_seektime_threepoint_line(currdisk, distance, headswitch);
   } else if (currdisk->seektime == THREEPOINT_CURVE) {
      seektime = disk_seektime_threepoint_curve(currdisk, distance, headswitch);
   } else if (currdisk->seektime == HPL_SEEK_EQUATION) {
      seektime = disk_seektime_hpl_equation(currdisk, distance, headswitch);
   } else if (currdisk->seektime == FIRST10_PLUS_HPL_SEEK_EQUATION) {
      seektime = disk_seektime_first10_plus_hpl_equation(currdisk, distance, headswitch);
   } else if (currdisk->seektime == EXTRACTION_SEEK) {
      seektime = disk_seektime_extraction_seek(currdisk, distance, headswitch);
   }
   if ((!read) && (seektime != 0.0)) {
      seektime += currdisk->seekwritedelta;
   }

   return(seektime);
}


/* currangle is a global that this function sets equal to the rotational     */
/* offset (from logical zero) for the given physical blkno.                  */

void disk_reset_currangle(currdisk, currband, physblkno)
disk *currdisk;
band *currband;
int physblkno;
{
   currangle = (double) physblkno / (double) currband->blkspertrack;
   currangle += disk_map_pbn_skew(currdisk, currband, currcylno, currsurface);
   currangle = currangle - ((int) currangle);
}


/* currangle is a global, set to equal the rotational offset (from logical */
/* zero) of the read/write head (or media, depending on one's viewpoint).  */

double disk_get_blkno_from_currangle(currdisk, currband)
disk *currdisk;
band *currband;
{
   double rotloc = currangle;
   int introtloc;

   rotloc -= disk_map_pbn_skew(currdisk, currband, currcylno, currsurface);
   if (rotloc > 0.0) {
      introtloc = (int) rotloc;
   } else {
      introtloc = - (int) (-rotloc);
      if ((double) introtloc > rotloc) {
	 introtloc--;
      }
   }
   rotloc -= (double) introtloc;
   rotloc *= (double) currband->blkspertrack;
   return(rotloc);
}


double disklatency(currdisk, currband, rotstarttime, blkno, bcount, immedaccess)
disk *currdisk;
band *currband;
double rotstarttime;
int blkno;
int bcount;
int immedaccess;
{
   double latency;
   double rotloc;
   double rotdistance;
   int endblock;
   double blkspertrack;

   if (currdisk->acctime == AVGROTATE) {
      return((double) 0.5 * currdisk->rotatetime);
   }
   blkspertrack = (double) currband->blkspertrack;
   currangle += (rotstarttime - currtime) / currdisk->rotatetime;
   currangle = currangle - (double) ((int) currangle);
   rotloc = disk_get_blkno_from_currangle(currdisk, currband);
   if (fabs((double) blkno - rotloc) < 0.0001) {
      rotloc = (double) blkno;
   }
   if (immedaccess == FALSE) {
      rotdistance = (double) blkno - rotloc;
      if (rotdistance < 0.0) {
         rotdistance += blkspertrack;
      }
      trackstart = blkno;
   } else {
      endblock = blkno + bcount - 1;
      if (endblock >= (int) blkspertrack) {
	 endblock = (int) blkspertrack - 1;
      }
      if (rotloc <= (double) blkno) {
	 rotdistance = (double) blkno - rotloc;
	 trackstart = blkno;
      } else if ((double) endblock >= rotloc) {
	 rotdistance = (double) 1 - (rotloc - (double) ((int) rotloc));
	 trackstart = (int) rotloc;
	 if (rotloc > (double) trackstart) {
	    trackstart++;
	 } else {
	    rotdistance = 0.0;
	 }
      } else {
	 rotdistance = blkspertrack - rotloc;
	 rotdistance += (double) blkno;
	 trackstart = blkno;
      }
   }
   latency = (rotdistance / blkspertrack) * currdisk->rotatetime;
   return(latency);
}


double diskxfertime(currdisk, currband, blkno, reqsize)
disk *currdisk;
band *currband;
int blkno;
int reqsize;
{
   double xfertime;
   int blks_on_track;
   int physblkno;

   blks_on_track = currband->blkspertrack;
   if ((reqsize + blkno) > blks_on_track) {
      fprintf(stderr, "Request %d goes beyond end of track, in diskxfertime: %d + %d >= %d\n", blkno, reqsize, blkno, blks_on_track);
      exit(0);
   }
   xfertime = ((double) reqsize / (double) blks_on_track) * currdisk->rotatetime;
   if (trackstart != blkno) {
      physblkno = trackstart;
      addtolatency = (double) (blks_on_track - reqsize);
      addtolatency *= currdisk->rotatetime / (double) blks_on_track;
   } else {
      physblkno = (blkno + reqsize) % blks_on_track;
      addtolatency = (double) 0;
   }
   disk_reset_currangle(currdisk, currband, physblkno);

   return(xfertime);
}

      
double diskacctime(currdisk, currband, type, rw, reqtime, cylno, surfaceno, blkno, bcount, immedaccess)
disk *currdisk;
band *currband;
int type;
int rw;
double reqtime;
int cylno;
int surfaceno;
int blkno;
int bcount;
int immedaccess;
{
   int distance;
   int headswitch;
   double seektime;
   double latency;
   double xfertime;
   double acctime;

   if (currdisk->acctime >= 0.0) {
      return(currdisk->acctime);
   }
   currcylno = currdisk->currcylno;
   currsurface = currdisk->currsurface;
   currangle = currdisk->currangle;
   currtime = currdisk->currtime;
   distance = cylno - currcylno;
   headswitch = surfaceno - currsurface;

   seektime = 0.0;
   seekdistance = abs(distance);
   if ((type != DISKPOS) && (type != DISKACCESS)) {
      seektime = diskseektime(currdisk, distance, headswitch, rw);
      reqtime += seektime;
      currcylno = cylno;
      currsurface = surfaceno;
      if (type == DISKSEEKTIME) {
         return(seektime);
      } else if (type == DISKSEEK) {
         currdisk->currcylno = cylno;
         currdisk->currsurface = surfaceno;
         return(seektime);
      }
   }
   latency = disklatency(currdisk, currband, reqtime, blkno, bcount, immedaccess);
   reqtime += latency;
   if (type == DISKPOSTIME) {
      return(seektime + latency);
   } else if (type == DISKPOS) {
      currdisk->currcylno = cylno;
      currdisk->currsurface = surfaceno;
      currdisk->currtime = reqtime;
      disk_reset_currangle(currdisk, currband, trackstart);
      currdisk->currangle = currangle;
      return(seektime + latency);
   }
   xfertime = diskxfertime(currdisk, currband, blkno, bcount);
   latency += addtolatency;
   acctime = seektime + latency + xfertime;
   if (type == DISKACCESS) {
      currdisk->currcylno = currcylno;
      currdisk->currsurface = currsurface;
      currdisk->currangle = currangle;
      currdisk->currtime = reqtime + addtolatency + xfertime;
      if (distance | headswitch) {
	 fprintf(stderr, "Shouldn't have non-zero seek for DISKACCESS\n");
	 exit(0);
      }
      return(acctime);
   }
   if (type == DISKSERVTIME) {
      return(seektime + latency);
   } else if (type == DISKACCTIME) {
      return(acctime);
   }
   fprintf(stderr, "Unknown type passed to diskacctime\n");
   exit(0);
}

