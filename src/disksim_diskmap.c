
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


int remapsector = FALSE;


int disk_get_numcyls(diskno)
int diskno;
{
   if ((diskno < 0) || (diskno >= numdisks)) {
      fprintf(stderr, "Invalid diskno passed to disk_get_numcyls\n");
      exit(0);
   }
   return(disks[diskno].numcyls);
}


void disk_get_mapping(maptype, diskno, blkno, cylptr, surfaceptr, blkptr)
int maptype;
int diskno;
int blkno;
int *cylptr;
int *surfaceptr;
int *blkptr;
{
   if ((diskno < 0) || (diskno >= numdisks)) {
      fprintf(stderr, "Invalid diskno passed to disk_get_mapping\n");
      exit(0);
   }
   (void) disk_translate_lbn_to_pbn(&disks[diskno], blkno, maptype, cylptr, surfaceptr, blkptr);
}


int disk_get_number_of_blocks(diskno)
int diskno;
{
   if ((diskno < 0) || (diskno >= numdisks)) {
      fprintf(stderr, "Invalid diskno passed to disk_get_number_of_blocks\n");
      exit(0);
   }
   return(disks[diskno].numblocks);
}


int disk_get_avg_sectpercyl(devno)
int devno;
{
   if ((devno < 0) || (devno >= numdisks)) {
      fprintf(stderr, "Invalid devno passed to disk_get_avg_sectpercyl: %d\n", devno);
      exit(0);
   }
   return(disks[devno].sectpercyl);
}


void disk_check_numblocks(currdisk)
disk *currdisk;
{
   int numblocks = 0;
   int i;

   for (i=0; i < currdisk->numbands; i++) {
      numblocks += currdisk->bands[i].blksinband;
   }
   if (numblocks != currdisk->numblocks) {
      fprintf (outputfile, "Numblocks provided by user does not match specifications - user %d, actual %d\n", currdisk->numblocks, numblocks);
   }
   currdisk->numblocks = numblocks;
}


double disk_map_pbn_skew(currdisk, currband, cylno, surfaceno)
disk *currdisk;
band *currband;
int cylno;
int surfaceno;
{
   double skew = (double) currband->firstblkno;
   int trackswitches;
   int slipoffs = 0;

   cylno -= currband->startcyl;
   trackswitches = surfaceno + (currdisk->numsurfaces - 1) * cylno;
   skew += currband->trackskew * (double) trackswitches;
   skew += currband->cylskew * (double) cylno;
   skew /= currdisk->rotatetime;
   if ((currdisk->sparescheme == SECTPERCYL_SPARING) || (currdisk->sparescheme == SECTPERTRACK_SPARING)) {
      int tracks = cylno * currdisk->numsurfaces;
      int cutoff;
      int i;

      if (currdisk->sparescheme == SECTPERTRACK_SPARING) {
	 tracks += surfaceno;
      }
      cutoff = tracks * currband->blkspertrack;
      for (i=0; i<currband->numslips; i++) {
	 if (currband->slip[i] < cutoff) {
	    slipoffs++;
	 }
      }
   }
   skew += (double) slipoffs / (double) currband->blkspertrack;
   return(skew);
}


int disk_pbn_to_lbn_sectpertrackspare(currdisk, currband, cylno, surfaceno, blkno, lbn)
disk *currdisk;
band *currband;
int cylno;
int surfaceno;
int blkno;
int lbn;
{
   int i;
   int blkspercyl;
   int firstblkoncyl;

   blkspercyl = (currband->blkspertrack - currband->sparecnt) * currdisk->numsurfaces;
   firstblkoncyl = (cylno - currband->startcyl) * currdisk->numsurfaces * currband->blkspertrack;
   blkno += firstblkoncyl + (surfaceno * currband->blkspertrack);
   for (i=(currband->numdefects-1); i>=0; i--) {
      if (blkno == currband->defect[i]) {        /* Remapped bad block */
	 return(-2);
      }
      if (blkno == currband->remap[i]) {
	 remapsector = TRUE;
	 blkno = currband->defect[i];
	 break;
      }
   }
   for (i=(currband->numslips-1); i>=0; i--) {
      if (blkno == currband->slip[i]) {          /* Slipped bad block */
	 return(-1);
      }
      if ((blkno > currband->slip[i]) && ((currband->slip[i] / (currband->blkspertrack * currdisk->numsurfaces)) == (cylno - currband->startcyl)) && ((currband->slip[i] % (currband->blkspertrack * currdisk->numsurfaces)) == surfaceno)) {
	 blkno--;
      }
   }
   blkno -= firstblkoncyl + (surfaceno * currband->blkspertrack);
   if (blkno >= (currband->blkspertrack - currband->sparecnt)) {   /* Unused spare block */
      return(-1);
   }
   lbn += (cylno - currband->startcyl) * blkspercyl;
   lbn += surfaceno * (currband->blkspertrack - currband->sparecnt);
   lbn += blkno - currband->deadspace;
   lbn = (lbn < 0) ? -1 : lbn;
   return(lbn);
}


int disk_pbn_to_lbn_sectpercylspare(currdisk, currband, cylno, surfaceno, blkno, lbn)
disk *currdisk;
band *currband;
int cylno;
int surfaceno;
int blkno;
int lbn;
{
   int i;
   int blkspercyl;
   int firstblkoncyl;

   blkspercyl = (currband->blkspertrack * currdisk->numsurfaces) - currband->sparecnt;
   firstblkoncyl = (cylno - currband->startcyl) * currdisk->numsurfaces * currband->blkspertrack;
   blkno += firstblkoncyl + (surfaceno * currband->blkspertrack);
   for (i=(currband->numdefects-1); i>=0; i--) {
      if (blkno == currband->defect[i]) {        /* Remapped bad block */
	 return(-2);
      }
      if (blkno == currband->remap[i]) {
	 remapsector = TRUE;
	 blkno = currband->defect[i];
	 break;
      }
   }
   for (i=(currband->numslips-1); i>=0; i--) {
      if (blkno == currband->slip[i]) {          /* Slipped bad block */
	 return(-1);
      }
      if ((blkno > currband->slip[i]) && ((currband->slip[i] / (currband->blkspertrack * currdisk->numsurfaces)) == (cylno - currband->startcyl))) {
	 blkno--;
      }
   }
   if (blkno >= (firstblkoncyl + blkspercyl)) {   /* Unused spare block */
      return(-1);
   }
   lbn += (cylno - currband->startcyl) * blkspercyl;
   lbn += blkno - firstblkoncyl - currband->deadspace;
   lbn = (lbn < 0) ? -1 : lbn;
   return(lbn);
}


int disk_pbn_to_lbn_trackspare(currdisk, currband, cylno, surfaceno, blkno, lbn)
disk *currdisk;
band *currband;
int cylno;
int surfaceno;
int blkno;
int lbn;
{
   int i;
   int trackno;
   int lasttrack;

   trackno = (cylno - currband->startcyl) * currdisk->numsurfaces + surfaceno;
   for (i=(currband->numdefects-1); i>=0; i--) {
      if (trackno == currband->defect[i]) {   /* Remapped bad track */
	 return(-2);
      }
      if (trackno == currband->remap[i]) {
	 trackno = currband->defect[i];
	 break;
      }
   }
   for (i=(currband->numslips-1); i>=0; i--) {
      if (trackno == currband->slip[i]) {     /* Slipped bad track */
	 return(-1);
      }
      if (trackno > currband->slip[i]) {
	 trackno--;
      }
   }
   lasttrack = (currband->blksinband + currband->deadspace) / currband->blkspertrack;
   if (trackno > lasttrack) {                 /* Unused spare track */
      return(-1);
   }
   lbn += blkno + (trackno * currband->blkspertrack) - currband->deadspace;
   lbn = (lbn < 0) ? -1 : lbn;
   return(lbn);
}


/* -2 means defect, -1 means dead space (reserved, spare or slip) */

int disk_translate_pbn_to_lbn(currdisk, currband, cylno, surfaceno, blkno)
disk *currdisk;
band *currband;
int cylno;
int surfaceno;
int blkno;
{
   int lbn = 0;
   int bandno = 0;

   if ((!currband) | (cylno < 0) | (cylno < currband->startcyl) | (cylno >= currdisk->numcyls) | (surfaceno < 0) | (surfaceno >= currdisk->numsurfaces) | (blkno < 0) | (blkno >= currband->blkspertrack)) {
      fprintf(stderr, "Illegal PBN values at disk_translate_pbn_to_lbn: %d %d %d\n", cylno, surfaceno, blkno);
      exit(0);
   }
   while (currband != &currdisk->bands[bandno]) {
      lbn += currdisk->bands[bandno].blksinband;
      bandno++;
      if (bandno >= currdisk->numbands) {
	 fprintf(stderr, "Currband not found in band list for currdisk\n");
	 exit(0);
      }
   }
   if (currdisk->sparescheme == NO_SPARING) {
      lbn += (((cylno - currband->startcyl) * currdisk->numsurfaces) + surfaceno) * currband->blkspertrack;
      lbn += blkno - currband->deadspace;
      lbn = (lbn < 0) ? -1 : lbn;
   } else if (currdisk->sparescheme == TRACK_SPARING) {
      lbn = disk_pbn_to_lbn_trackspare(currdisk, currband, cylno, surfaceno, blkno, lbn);
   } else if (currdisk->sparescheme == SECTPERCYL_SPARING) {
      lbn = disk_pbn_to_lbn_sectpercylspare(currdisk, currband, cylno, surfaceno, blkno, lbn);
   } else if (currdisk->sparescheme == SECTPERTRACK_SPARING) {
      lbn = disk_pbn_to_lbn_sectpertrackspare(currdisk, currband, cylno, surfaceno, blkno, lbn);
   } else {
      fprintf(stderr, "Unknown sparing scheme at disk_translate_pbn_to_lbn: %d\n", currdisk->sparescheme);
      exit(0);
   }

   return(lbn);
}


/* Assume that we never slip sectors over track boundaries! */

void disk_get_lbn_boundaries_sectpertrackspare(currdisk, currband, cylno, surfaceno, startptr, endptr, lbn)
disk *currdisk;
band *currband;
int cylno;
int surfaceno;
int *startptr;
int *endptr;
int lbn;             /* equals first block in band */
{
   int i;
   int blkno;
   int temp_lbn = lbn;
   int datablkspertrack = currband->blkspertrack - currband->sparecnt;

   lbn += ((((cylno - currband->startcyl) * currdisk->numsurfaces) + surfaceno) * datablkspertrack) - currband->deadspace;
   if (startptr) {
      *startptr = ((lbn + datablkspertrack) <= temp_lbn) ? -1 : max(temp_lbn, lbn);
   }
   if (endptr) {
      lbn += datablkspertrack;
      *endptr = (lbn <= temp_lbn) ? -1 : lbn;
   }

   blkno = (((cylno - currband->startcyl) * currdisk->numsurfaces) + surfaceno) * currband->blkspertrack;
   for (i=0; i<currband->numdefects; i++) {
      if ((blkno <= currband->defect[i]) && ((blkno + currband->blkspertrack) > currband->defect[i])) {
	 remapsector = TRUE;
      }
   }
}


void disk_get_lbn_boundaries_sectpercylspare(currdisk, currband, cylno, surfaceno, startptr, endptr, lbn)
disk *currdisk;
band *currband;
int cylno;
int surfaceno;
int *startptr;
int *endptr;
int lbn;             /* equals first block in band */
{
   int i;
   int blkno;
   int lbnadd;
   int temp_lbn = lbn;

   if (startptr) {
      lbnadd = 0;
      for (blkno=0; blkno<currband->blkspertrack; blkno++) {
	 remapsector = FALSE;
	 *startptr = disk_pbn_to_lbn_sectpercylspare(currdisk, currband, cylno, surfaceno, blkno, lbn);
	 if ((*startptr) == -2) {
            lbnadd++;
	 }
	 if (remapsector) {
	    *startptr = -1;
	 }
	 if ((*startptr) >= 0) {
	    *startptr = max(((*startptr) - lbnadd),temp_lbn);
	    break;
	 }
      }
      if (blkno == currband->blkspertrack) {
	 *startptr = -1;
      }
   }
   if (endptr) {
      lbnadd = 1;
      for (blkno=(currband->blkspertrack-1); blkno>0; blkno--) {
	 remapsector = FALSE;
	 *endptr = disk_pbn_to_lbn_sectpercylspare(currdisk, currband, cylno, surfaceno, blkno, lbn);
	 if ((*endptr) == -2) {
	    lbnadd++;
	 }
	 if (remapsector) {
	    *endptr = -1;
	 }
	 if ((*endptr) >= 0) {
	    *endptr = (*endptr) + lbnadd;
	    break;
	 }
      }
      if (blkno == 0) {
	 *endptr = -1;
      }
   }
   blkno = (((cylno - currband->startcyl) * currdisk->numsurfaces) + surfaceno) * currband->blkspertrack;
   for (i=0; i<currband->numdefects; i++) {
      if ((blkno <= currband->defect[i]) && ((blkno + currband->blkspertrack) > currband->defect[i])) {
	 remapsector = TRUE;
      }
   }
}


void disk_get_lbn_boundaries_trackspare(currdisk, currband, cylno, surfaceno, startptr, endptr, lbn)
disk *currdisk;
band *currband;
int cylno;
int surfaceno;
int *startptr;
int *endptr;
int lbn;
{
   int i;
   int trackno;
   int lasttrack;
   int blkno;

   trackno = ((cylno - currband->startcyl) * currdisk->numsurfaces) + surfaceno;
   for (i=(currband->numdefects-1); i>=0; i--) {
      if (trackno == currband->defect[i]) {   /* Remapped bad track */
	 lbn = -2;
      }
      if (trackno == currband->remap[i]) {
	 trackno = currband->defect[i];
	 break;
      }
   }
   for (i=(currband->numslips-1); i>=0; i--) {
      if (trackno == currband->slip[i]) {     /* Slipped bad track */
	 lbn = -1;
      }
      if (trackno > currband->slip[i]) {
	 trackno--;
      }
   }
   lasttrack = (currband->blksinband + currband->deadspace) / currband->blkspertrack;
   if (trackno > lasttrack) {                 /* Unused spare track */
      lbn = -1;
   }
   blkno = lbn + (trackno * currband->blkspertrack) - currband->deadspace;
   if (startptr) {
      if (lbn < 0) {
	 *startptr = lbn;
      } else {
         *startptr = ((blkno + currband->blkspertrack) <= lbn) ? -1 : max(blkno, lbn);
      }
   }
   if (endptr) {
      blkno += currband->blkspertrack;
      if (lbn < 0) {
	 *endptr = lbn;
      } else {
         *endptr = (blkno <= lbn) ? -1 : blkno;
      }
   }
}


void disk_get_lbn_boundaries_for_track(currdisk, currband, cylno, surfaceno, startptr, endptr)
disk *currdisk;
band *currband;
int cylno;
int surfaceno;
int *startptr;
int *endptr;
{
   int lbn = 0;
   int bandno = 0;

   if ((!startptr) && (!endptr)) {
      return;
   }
   if ((!currband) | (cylno < currband->startcyl) | (cylno >= currdisk->numcyls) | (surfaceno < 0) | (surfaceno >= currdisk->numsurfaces)) {
      fprintf(stderr, "Illegal PBN values at disk_get_lbn_boundaries_for_track: %d %d\n", cylno, surfaceno);
      exit(0);
   }
   while (currband != &currdisk->bands[bandno]) {
      lbn += currdisk->bands[bandno].blksinband;
      bandno++;
      if (bandno >= currdisk->numbands) {
	 fprintf(stderr, "Currband not found in band list for currdisk\n");
	 exit(0);
      }
   }
   if (currdisk->sparescheme == NO_SPARING) {
      int temp_lbn = lbn;

      lbn += ((((cylno - currband->startcyl) * currdisk->numsurfaces) + surfaceno) * currband->blkspertrack) - currband->deadspace;
      if (startptr) {
         *startptr = ((lbn + currband->blkspertrack) <= temp_lbn) ? -1 : max(lbn, temp_lbn);
      }
      if (endptr) {
         lbn += currband->blkspertrack;
         *endptr = (lbn <= temp_lbn) ? -1 : lbn;
      }
   } else if (currdisk->sparescheme == TRACK_SPARING) {
      disk_get_lbn_boundaries_trackspare(currdisk, currband, cylno, surfaceno, startptr, endptr, lbn);
   } else if (currdisk->sparescheme == SECTPERCYL_SPARING) {
      disk_get_lbn_boundaries_sectpercylspare(currdisk, currband, cylno, surfaceno, startptr, endptr, lbn);
   } else if (currdisk->sparescheme == SECTPERTRACK_SPARING) {
      disk_get_lbn_boundaries_sectpertrackspare(currdisk, currband, cylno, surfaceno, startptr, endptr, lbn);
   } else {
      fprintf(stderr, "Unknown sparing scheme at disk_translate_pbn_to_lbn: %d\n", currdisk->sparescheme);
      exit(0);
   }
}


/* NOTE:  The total number of allowable slips and remaps per track is 
	  equal to the number of spares per track.  The following code 
	  will produce incorrect results if this rule is violated.
          To fix this, cylptr and surfptr need to be re-calculated 
	  after the detection of a slip or spare.  Also, slips on 
	  immediately previous tracks need to be accounted for.
	  Low Priority.
*/

void disk_lbn_to_pbn_sectpertrackspare(currdisk, currband, blkno, maptype, cylptr, surfaceptr, blkptr)
disk *currdisk;
band *currband;
int blkno;
int maptype;
int *cylptr;
int *surfaceptr;
int *blkptr;
{
   int i;
   int firstblkontrack;

   int blkspertrack = currband->blkspertrack;
   int datablkspertrack = blkspertrack - currband->sparecnt;
   int blkspercyl = datablkspertrack * currdisk->numsurfaces;
   int cyl = blkno/blkspercyl;
   int surf = (blkno / datablkspertrack) % currdisk->numsurfaces;

   if (cylptr) {
      *cylptr = cyl + currband->startcyl;
   }
   if (surfaceptr) {
      *surfaceptr = surf;
   }
   blkno = blkno % datablkspertrack;
   if ((maptype == MAP_ADDSLIPS) || (maptype == MAP_FULL)) {
      firstblkontrack = datablkspertrack * (surf + (cyl * currdisk->numsurfaces));
      for (i=0; i<currband->numslips; i++) {
	 if ((currband->slip[i] >= firstblkontrack) && ((currband->slip[i] - firstblkontrack) <= blkno)) {
	    blkno++;
	 }
      }
   }
   if (maptype == MAP_FULL) {
      for (i=0; i<currband->numdefects; i++) {
	 if (currband->defect[i] == (firstblkontrack + blkno)) {
	    remapsector = TRUE;
	    blkno = currband->remap[i];
	 }
      }
   }
   if (blkptr) {
      *blkptr = blkno % blkspertrack;
   }
}


/* NOTE:  The total number of allowable slips and remaps per cylinder 
	  is equal to the number of spares per cylinder.  The following 
	  code will produce incorrect results if this rule is violated.
          To fix this, cylptr needs to be re-calculated after the 
	  detection of a slip or spare.  Also, slips on immediately 
	  previous cylinders need to be accounted for.  Low priority.
*/

void disk_lbn_to_pbn_sectpercylspare(currdisk, currband, blkno, maptype, cylptr, surfaceptr, blkptr)
disk *currdisk;
band *currband;
int blkno;
int maptype;
int *cylptr;
int *surfaceptr;
int *blkptr;
{
   int i;
   int blkspertrack;
   int blkspercyl;
   int firstblkoncyl;
   int cyl;
   int slips = 0;

   blkspertrack = currband->blkspertrack;
   blkspercyl = (blkspertrack * currdisk->numsurfaces) - currband->sparecnt;
   cyl = blkno/blkspercyl;
   if (cylptr) {
      *cylptr = cyl + currband->startcyl;
   }
   blkno = blkno % blkspercyl;
   if ((maptype == MAP_ADDSLIPS) || (maptype == MAP_FULL)) {
      firstblkoncyl = cyl * blkspertrack * currdisk->numsurfaces;
      for (i=0; i<currband->numslips; i++) {
	 if ((currband->slip[i] >= firstblkoncyl) && ((currband->slip[i] - firstblkoncyl) <= blkno)) {
	    slips++;
	 }
      }
   }
   blkno += slips;
   if (maptype == MAP_FULL) {
      for (i=0; i<currband->numdefects; i++) {
	 if (currband->defect[i] == (firstblkoncyl + blkno)) {
	    remapsector = TRUE;
	    blkno = currband->remap[i];
	 }
      }
   }
   if (surfaceptr) {
      *surfaceptr = blkno / blkspertrack;
   }
   if (blkptr) {
      *blkptr = blkno % blkspertrack;
   }
}


/* NOTE:  The total number of allowable slips and remaps per band
	  is equal to the number of spare tracks per band.  The 
	  following code will produce incorrect results if this rule 
	  is violated.  To fix this, currband needs to be re-calculated 
	  after the detection of a slip or spare.  Also, slips on 
	  immediately previous bands need to be accounted for.  Lastly,
	  the mismatch in sectors per track between zones must be 
	  handled.  Extremely low priority.
*/

void disk_lbn_to_pbn_trackspare(currdisk, currband, blkno, maptype, cylptr, surfaceptr, blkptr)
disk *currdisk;
band *currband;
int blkno;
int maptype;
int *cylptr;
int *surfaceptr;
int *blkptr;
{
   int i;
   int blkspertrack;
   int trackno;

   blkspertrack = currband->blkspertrack;
   trackno = blkno/blkspertrack;
   if ((maptype == MAP_ADDSLIPS) || (maptype == MAP_FULL)) {
      for (i=0; i<currband->numslips; i++) {
	 if (currband->slip[i] <= trackno) {
	    trackno++;
	 }
      }
   }
   if (maptype == MAP_FULL) {
      for (i=0; i<currband->numdefects; i++) {
	 if (currband->defect[i] == trackno) {
	    trackno = currband->remap[i];
	    break;
	 }
      }
   }
   if (cylptr) {
      *cylptr = (trackno/currdisk->numsurfaces) + currband->startcyl;
   }
   if (surfaceptr) {
      *surfaceptr = trackno % currdisk->numsurfaces;
   }
   if (blkptr) {
      *blkptr = blkno % blkspertrack;
   }
}


band * disk_translate_lbn_to_pbn(currdisk, blkno, maptype, cylptr, surfaceptr, blkptr)
disk *currdisk;
int blkno;
int maptype;
int *cylptr;
int *surfaceptr;
int *blkptr;
{
   int bandno = 0;
   int blkspertrack;
   band *currband = &currdisk->bands[0];
   extern int bandstart;

   if ((maptype > MAP_FULL) || (maptype < MAP_IGNORESPARING)) {
      fprintf(stderr, "Unimplemented mapping type at disk_translate_lbn_to_pbn: %d\n", maptype);
      exit(0);
   }
   bandstart = 0;
   while ((blkno >= currband->blksinband) || (blkno < 0)) {
      bandstart += currband->blksinband;
      blkno -= currband->blksinband;
      bandno++;
      currband = &currdisk->bands[bandno];
      if ((bandno >= currdisk->numbands) || (blkno < 0)) {
         fprintf(stderr, "blkno outside addressable space of disk: %d, %d\n", blkno, bandno);
         exit(0);
      }
   }
   blkno += currband->deadspace;
   if ((maptype == MAP_IGNORESPARING) || (currdisk->sparescheme == NO_SPARING)) {
      blkspertrack = currband->blkspertrack;
      if (cylptr) {
         *cylptr = blkno/(blkspertrack * currdisk->numsurfaces) + currband->startcyl;
      }
      if (surfaceptr) {
         *surfaceptr = (blkno/blkspertrack) % currdisk->numsurfaces;
      }
      if (blkptr) {
         *blkptr = blkno % blkspertrack;
      }
   } else {
      if (currdisk->sparescheme == TRACK_SPARING) {
	 disk_lbn_to_pbn_trackspare(currdisk, currband, blkno, maptype, cylptr, surfaceptr, blkptr);
      } else if (currdisk->sparescheme == SECTPERCYL_SPARING) {
	disk_lbn_to_pbn_sectpercylspare(currdisk, currband, blkno, maptype, cylptr, surfaceptr, blkptr);
      } else if (currdisk->sparescheme == SECTPERTRACK_SPARING) {
	disk_lbn_to_pbn_sectpertrackspare(currdisk, currband, blkno, maptype, cylptr, surfaceptr, blkptr);
      } else {
	 fprintf(stderr, "Unknown sparing scheme at disk_translate_lbn_to_pbn: %d\n", currdisk->sparescheme);
	 exit(0);
      }
   }
   return(currband);
}

