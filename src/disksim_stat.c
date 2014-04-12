
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


int stat_get_count(statptr)
statgen *statptr;
{
   return(statptr->count);
}


int stat_get_count_set(statset, statcnt)
statgen **statset;
int statcnt;
{
   int i;
   int count = 0;

   for (i=0; i<statcnt; i++) {
      statgen *statptr = statset[i];
      count += statptr->count;
   }
   return(count);
}


double stat_get_runval(statptr)
statgen *statptr;
{
   return(statptr->runval);
}


void stat_update(statptr, value)
statgen *statptr;
double value;
{
   int  i = 0;
   int  start = statptr->distbrks[0];
   int  step = statptr->distbrks[1];
   int  grow = statptr->distbrks[2];
   int  equals = statptr->equals;
   int  buckets = statptr->distbrks[(DISTSIZE-1)];
   int  intval = (int) (value * (double) statptr->scale);

   statptr->count++;
   if (statptr->maxval < value) {
      statptr->maxval = value;
   }
   statptr->runval += value;
   statptr->runsquares += (value*value);
   if (buckets > DISTSIZE) {
      if (intval < start) {
      } else if (!grow) {
         if ((equals) && (intval <= (start - step + (step * equals))) && (((intval - start) % step) == 0)) {
            i = intval / step;
         } else {
            i = (intval + step - start) / step;
            if (i >= buckets) {
               i = buckets - 1;
            }
         }
      } else {
         int top = buckets - 1;

         while (i < equals) {
            if (intval == statptr->largediststarts[i]) {
               break;
            }
            i++;
         }
         if ((i >= equals) && (i < buckets)) {
            top--;
            if (intval < statptr->largediststarts[top]) {
               int bottom = i;
               while (top != bottom) {
                  i = bottom + ((top - bottom) / 2);
                  if (intval < statptr->largediststarts[(i+1)]) {
                     top = i;
                  } else {
                     bottom = i+1;
                  }
               }
            }
            i = top + 1;
         }
      }
      statptr->largedistvals[i]++;
   } else {
      i = 0;
      while (i<(DISTSIZE-1)) {
         if ((i < equals) && (value == (double) statptr->distbrks[i])) {
            break;
         } else if ((i >= equals) && (intval < statptr->distbrks[i])) {
            break;
         }
         i++;
      }
      statptr->smalldistvals[i]++;
   }
}


void stat_print_large_dist(statset, statcnt, count, identstr)
statgen **statset;
int statcnt;
int count;
char *identstr;
{
   int  i, j;
   statgen *statptr = statset[0];
   int  step = statptr->distbrks[1];
   int  grow = statptr->distbrks[2];
   int  buckets = statptr->distbrks[(DISTSIZE-1)];
   int  bucketno = statptr->distbrks[0];
   double scale = (double) statptr->scale;
   int scaled = (scale == 1.0) ? FALSE : TRUE;
   double dist = 0.0;
   int  bucketcnt;
   double density;

   if (count == 0) {
      return;
   }
   fprintf (outputfile, "%s%s distribution\n", identstr, statptr->statdesc);
   for (i=0; i<buckets; i++) {
      if (statcnt > 1) {
         bucketcnt = 0;
         for (j=0; j<statcnt; j++) {
            statptr = statset[j];
            bucketcnt += statptr->largedistvals[i];
         }
      } else {
         bucketcnt = statptr->largedistvals[i];
      }
      density = (double) bucketcnt / (double) count;
      dist += density;
      if (scaled) {
         fprintf (outputfile, "%6.3f  \t%7d  \t%f  \t%f\n", ((double) bucketno / scale), bucketcnt, density, dist);
      } else {
         fprintf (outputfile, "%4d  \t%7d  \t%f  \t%f\n", bucketno, bucketcnt, density, dist);
      }
      bucketno += step + (int)((double) (abs(bucketno) * grow) / (double) 100);
   }
}


void stat_print(statptr, identstr)
statgen *statptr;
char *identstr;
{
   int i;
   int count = 0;
   int buckets = statptr->distbrks[(DISTSIZE-1)];
   double scale = (double) statptr->scale;
   int intval = statptr->distbrks[(DISTSIZE-2)];
   char *statdesc = statptr->statdesc;
   char distchar = '=';
   double runsquares = 0.0;
   double avg = 0.0;

   if (statptr->count > 0) {
      avg = statptr->runval / (double) statptr->count;
      runsquares = statptr->runsquares / (double) statptr->count - (avg*avg);
      runsquares = (runsquares > 0.0) ? sqrt(runsquares) : 0.0;
   }
   fprintf(outputfile, "%s%s average: \t%f\n", identstr, statdesc, avg);
   fprintf(outputfile, "%s%s std.dev.:\t%f\n", identstr, statdesc, runsquares);
   if (statptr->maxval == (double) ((int)statptr->maxval)) {
      fprintf(outputfile, "%s%s maximum:\t%d\n", identstr, statdesc, ((int)statptr->maxval));
   } else {
      fprintf(outputfile, "%s%s maximum:\t%f\n", identstr, statdesc, statptr->maxval);
   }
   if (buckets > DISTSIZE) {
      stat_print_large_dist(&statptr, 1, statptr->count, identstr);
      return;
   }
   fprintf(outputfile, "%s%s distribution\n", identstr, statdesc);
   for (i=(DISTSIZE-buckets); i<(DISTSIZE-1); i++) {
      if (i >= statptr->equals) {
         distchar = '<';
      }
      if (statptr->scale == 1) {
         fprintf(outputfile, "   %c%3d ", distchar, statptr->distbrks[i]);
      } else {
         fprintf(outputfile, "  %c%4.1f ", distchar, ((double) statptr->distbrks[i] / scale));
      }
   }
   if (statptr->equals == (DISTSIZE-1)) {
      intval++;
   }
   fprintf(outputfile, "   %3d+\n", intval);
   for (i=(DISTSIZE-buckets); i<DISTSIZE; i++) {
      count += statptr->smalldistvals[i];
      fprintf(outputfile, " %6d ", statptr->smalldistvals[i]);
   }
   fprintf(outputfile, "\n");
   ASSERT(count == statptr->count);
}


void stat_print_set(statset, statcnt, identstr)
statgen **statset;
int statcnt;
char *identstr;
{
   int i, j;
   int count = 0;
   statgen *statptr = statset[0];
   char *statdesc = statptr->statdesc;
   int buckets = statptr->distbrks[(DISTSIZE-1)];
   int intval = statptr->distbrks[(DISTSIZE-2)];
   double runval = 0.0;
   double runsquares = 0.0;
   double avg = 0.0;
   double maxval = statptr->maxval;
   double scale = (double) statptr->scale;
   int runcount = 0;
   int smalldistvals[DISTSIZE];
   char distchar = '=';

   for (i=0; i<statcnt; i++) {
      statptr = statset[i];
      runcount += statptr->count;
      runval += statptr->runval;
      runsquares += statptr->runsquares;
      if (maxval < statptr->maxval) {
         maxval = statptr->maxval;
      }
   }
   if (runcount > 0) {
      avg = runval / (double) runcount;
      runsquares = (runsquares / (double) runcount) - (avg*avg);
      runsquares = (runsquares > 0.0) ? sqrt(runsquares) : 0.0;
   } else {
      runsquares = 0.0;
   }
   fprintf(outputfile, "%s%s average: \t%f\n", identstr, statdesc, avg);
   fprintf(outputfile, "%s%s std.dev.:\t%f\n", identstr, statdesc, runsquares);
   if (maxval == (double) ((int)maxval)) {
      fprintf(outputfile, "%s%s maximum:\t%d\n", identstr, statdesc, (int)maxval);
   } else {
      fprintf(outputfile, "%s%s maximum:\t%f\n", identstr, statdesc, maxval);
   }
   if (buckets > DISTSIZE) {
      stat_print_large_dist(statset, statcnt, runcount, identstr);
      return;
   }
   fprintf(outputfile, "%s%s distribution\n", identstr, statdesc);
   statptr = statset[0];
   for (i=(DISTSIZE-buckets); i<(DISTSIZE-1); i++) {
      if (i >= statptr->equals) {
         distchar = '<';
      }
      if (statptr->scale == 1) {
         fprintf(outputfile, "   %c%3d ", distchar, statptr->distbrks[i]);
      } else {
         fprintf(outputfile, "  %c%4.1f ", distchar, ((double) statptr->distbrks[i] / (double) scale));
      }
   }
   if (statptr->equals == (DISTSIZE-1)) {
      intval++;
   }
   fprintf(outputfile, "   %3d+\n", intval);
   for (i=0; i<DISTSIZE; i++) {
      smalldistvals[i] = 0;
   }
   for (i=0; i<statcnt; i++) {
      statptr = statset[i];
      for (j=0; j<DISTSIZE; j++) {
         smalldistvals[j] += statptr->smalldistvals[j];
      }
   }
   for (i=(DISTSIZE-buckets); i<DISTSIZE; i++) {
      count += smalldistvals[i];
      fprintf(outputfile, " %6d ", smalldistvals[i]);
   }
   fprintf(outputfile, "\n");
   ASSERT2(count == runcount, "count", count, "runcount", runcount);
}


void stat_reset(statptr)
statgen *statptr;
{
   int buckets = statptr->distbrks[(DISTSIZE-1)];
   int i;

   statptr->count = 0;
   statptr->runval = 0.0;
   statptr->runsquares = 0.0;
   statptr->maxval = 0.0;
   if (buckets > DISTSIZE) {
      for (i=0; i<buckets; i++) {
         statptr->largedistvals[i] = 0;
      }
   } else {
      for (i=0; i<DISTSIZE; i++) {
         statptr->smalldistvals[i] = 0;
      }
   }
}


void stat_get_large_dist(statdeffile, statptr, buckets)
FILE *statdeffile;
statgen *statptr;
int buckets;
{
   int i;
   char line[201];

   statptr->largedistvals = (int *) malloc(buckets*sizeof(int));
   ASSERT(statptr->largedistvals != NULL);

   if (fgets(line, 200, statdeffile) == NULL) {
      fprintf(stderr, "Can't get line describing large dist\n");
      exit(0);
   }
   if (sscanf(line, "Start %d  step %d  grow %d", &statptr->distbrks[0], &statptr->distbrks[1], &statptr->distbrks[2]) != 3) {
      fprintf(stderr, "Bad format for line describing large dist - %s\n", line);
      exit(0);
   }
   for (i=0; i<buckets; i++) {
      statptr->largedistvals[i] = 0;
   }
   if (statptr->distbrks[2]) {
      int grow = statptr->distbrks[2];
      int step = statptr->distbrks[1];
      int bucketval = statptr->distbrks[0];
      int i;

      statptr->largediststarts = (int *) malloc(buckets*sizeof(int));
      ASSERT(statptr->largediststarts != NULL);

      for (i=0; i < buckets; i++) {
         statptr->largediststarts[i] = bucketval;
         bucketval += step + (int)((double) (abs(bucketval) * grow) / (double) 100);
      }
   }
}


void stat_initialize(statdeffile, statdesc, statptr)
FILE *statdeffile;
char *statdesc;
statgen *statptr;
{
   int i;
   int buckets;
   char line[201];
   char line2[201];

   statptr->count = 0;
   statptr->runval = 0.0;
   statptr->runsquares = 0.0;
   statptr->maxval = 0.0;
   statptr->statdesc = statdesc;
   for (i=0; i<DISTSIZE; i++) {
      statptr->distbrks[i] = 0;
      statptr->smalldistvals[i] = 0;
   }
   if (fseek(statdeffile, 0L, 0)) {
      fprintf(stderr, "Can't rewind the statdeffile\n");
      exit(0);
   }
   sprintf(line2, "%s\n", statdesc);
   while (1) {
      if (fgets(line, 200, statdeffile) == NULL) {
         fprintf(stderr, "No match for desired stat: %s\n", statdesc);
         exit(0);
      }
      if (strcmp(line, line2) == 0) {
         break;
      }
   }
   if (fgets(line, 200, statdeffile) == NULL) {
      fprintf(stderr, "Stat definition only partially found: %s\n", statdesc);
      exit(0);
   }
   if (sscanf(line, "Distribution size: %d\n", &buckets) != 1) {
      fprintf(stderr, "Invalid format for 'distribution size': %s\n", statdesc);
      exit(0);
   }
   if (fgets(line, 200, statdeffile) == NULL) {
      fprintf(stderr, "Stat definition only partially found: %s\n", statdesc);
      exit(0);
   }
   if (sscanf(line, "Scale/Equals: %d/%d\n", &statptr->scale, &statptr->equals) != 2) {
      fprintf(stderr, "Invalid format for 'scale/equals': %s\n", statdesc);
      exit(0);
   }
   statptr->distbrks[(DISTSIZE-1)] = buckets;
   statptr->largedistvals = NULL;
   statptr->largediststarts = NULL;
   if (buckets > DISTSIZE) {
      stat_get_large_dist(statdeffile, statptr, buckets);
   } else {
      for (i=0; i<(DISTSIZE-1); i++) {
         if (fscanf(statdeffile, "%d", &statptr->distbrks[i]) != 1) {
            fprintf(stderr, "Distribution buckets only partially specified: %s %d\n", statdesc, i);
            exit(0);
         }
      }
   }
}

