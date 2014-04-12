
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
#include "disksim_hptrace.h"

double tracebasetime = 0.0;

extern int traceheader;  /* From disksim.c: hack for combined HPL trace files */
extern int endian;
extern int traceformat;
extern int traceendian;

int syncreads = 0;
int syncwrites = 0;
int asyncreads = 0;
int asyncwrites = 0;
int hpreads = 0;
int hpwrites = 0;

int firstio = TRUE;
int basehighshort;
int basehighshort2;
int lasttime1;
double lasttime = 0.0;

int baseyear = 0;
int baseday = 0;
int basesecond = 0;

int basebigtime = -1;
int basesmalltime = -1;
double basesimtime = 0.0;

double validate_lastserv = 0.0;
int validate_lastblkno = 0;
int validate_lastbcount = 0;
int validate_lastread = 0;
double validate_nextinter = 0.0;
char validate_buffaction[20];


void iotrace_set_format(formatname)
char *formatname;
{
   traceendian = _LITTLE_ENDIAN;
   traceheader = TRUE;
   if (strcmp(formatname, "0") == 0) {
      traceformat = DEFAULT;
   } else if (strcmp(formatname, "ascii") == 0) {
	/* default ascii trace format */
      traceformat = ASCII;
   } else if (strcmp(formatname, "raw") == 0) {
	/* format of traces collected at Michigan */
      traceformat = RAW;
   } else if (strcmp(formatname, "validate") == 0) {
	/* format of disk validation traces */
      traceformat = VALIDATE;
   } else if (strcmp(formatname, "hpl") == 0) {
	/* format of traces provided by HPLabs for research purposes */
      traceformat = HPL;
      traceendian = _BIG_ENDIAN;
   } else if (strcmp(formatname, "hpl2") == 0) {
	/* format of traces provided by HPLabs for research purposes,     */
        /* after they have been modified/combined by the `hplcomb' program */
      traceformat = HPL;
      traceendian = _BIG_ENDIAN;
      traceheader = FALSE;
   } else if (strcmp(formatname, "dec") == 0) {
	/* format of some traces provided by dec for research purposes */
      traceformat = DEC;
   } else {
      fprintf(stderr, "Unknown trace format - %s\n", formatname);
      exit(0);
   }
}


int iotrace_read_space(tracefile, spaceptr, spacesize)
FILE *tracefile;
char *spaceptr;
int spacesize;
{
   if (fread(spaceptr, spacesize, 1, tracefile) != 1) {
      return(-1);
   }
   return(0);
}


int iotrace_read_char(tracefile, charptr)
FILE *tracefile;
char *charptr;
{
   StaticAssert (sizeof(char) == 1);
   if (fread(charptr, sizeof(char), 1, tracefile) != 1) {
      return(-1);
   }
   return(0);
}


int iotrace_read_short(tracefile, shortptr)
FILE *tracefile;
short *shortptr;
{
   StaticAssert (sizeof(short) == 2);
   if (fread(shortptr, sizeof(short), 1, tracefile) != 1) {
      return(-1);
   }
   if (endian != traceendian) {
      *shortptr = ((*shortptr) << 8) + (((*shortptr) >> 8) & 0xFF);
   }
   return(0);
}


int iotrace_read_int32(tracefile, intptr)
FILE *tracefile;
intchar *intptr;
{
   int i;
   intchar swapval;

   StaticAssert (sizeof(int) == 4);
   if (fread(&intptr->value, sizeof(int), 1, tracefile) != 1) {
      return(-1);
   }
   if (endian != traceendian) {
      for (i=0; i<sizeof(int); i++) {
         swapval.byte[i] = intptr->byte[(sizeof(int) - i - 1)];
      }
/*
      fprintf (outputfile, "intptr.value %x, swapval.value %x\n", intptr->value, swapval.value);
*/
      intptr->value = swapval.value;
   }
   return(0);
}


event * iotrace_validate_get_ioreq_event(iotracefile, new)
FILE *iotracefile;
ioreq_event *new;
{
   char line[201];
   char rw;
   double servtime;

   if (fgets(line, 200, iotracefile) == NULL) {
      addtoextraq((event *) new);
      return(NULL);
   }
   new->time = simtime + (validate_nextinter / (double) 1000);
   if (sscanf(line, "%c %s %d %d %lf %lf\n", &rw, validate_buffaction, &new->blkno, &new->bcount, &servtime, &validate_nextinter) != 6) {
      fprintf(stderr, "Wrong number of arguments for I/O trace event type\n");
      exit(0);
   }
   validate_lastserv = servtime / (double) 1000;
   if (rw == 'R') {
      new->flags = READ;
   } else if (rw == 'W') {
      new->flags = WRITE;
   } else {
      fprintf(stderr, "Invalid R/W value: %c\n", rw);
      exit(0);
   }
   new->devno = 0;
   new->buf = 0;
   new->opid = 0;
   new->cause = 0;
   new->busno = 0;
   new->tempint2 = 0;
   new->tempint1 = 0;
   validate_lastblkno = new->blkno;
   validate_lastbcount = new->bcount;
   validate_lastread = new->flags & READ;
   return((event *) new);
}


event * iotrace_dec_get_ioreq_event(iotracefile, new)
FILE *iotracefile;
ioreq_event *new;
{
   assert ("removed for distribution" == 0);
   return (NULL);
}


void iotrace_hpl_srt_convert_flags(curr)
ioreq_event *curr;
{
   int flags;

   flags = curr->flags;
   curr->flags = 0;
   if (flags & HPL_READ) {
      curr->flags |= READ;
      hpreads++;
   } else {
      hpwrites++;
   }
   if (!(flags & HPL_ASYNC)) {
      curr->flags |= TIME_CRITICAL;
      if (curr->flags & READ) {
         syncreads++;
      } else {
         syncwrites++;
      }
   }
   if (flags & HPL_ASYNC) {
      if (curr->flags & READ) {
         curr->flags |= TIME_LIMITED;
         asyncreads++;
      } else {
         asyncwrites++;
      }
   }
}


event * iotrace_hpl_get_ioreq_event(iotracefile, new)
FILE *iotracefile;
ioreq_event *new;
{
   int32_t size;
   int32_t id;
   u_int32_t sec;
   u_int32_t usec;
   int32_t val;
   char junk[20];
   unsigned int failure = 0;

   while (TRUE) {
      failure |= iotrace_read_int32(iotracefile, &size);
      failure |= iotrace_read_int32(iotracefile, &id);
      failure |= iotrace_read_int32(iotracefile, &sec);
      failure |= iotrace_read_int32(iotracefile, &usec);
      if (failure) {
         addtoextraq((event *) new);
         return(NULL);
      }
      if (((id >> 16) < 1) || ((id >> 16) > 4)) {
         fprintf(stderr, "Error in trace format - id %x\n", id);
         exit(0);
      }
      if (((id & 0xFFFF) != HPL_SHORTIO) && ((id & 0xFFFF) != HPL_SUSPECTIO)) {
         fprintf(stderr, "Unexpected record type - %x\n", id);
         exit(0);
      }
      new->time = (double) sec * (double) MILLI;
      new->time += (double) usec / (double) MILLI;

      if ((traceheader == FALSE) && (new->time == 0.0)) {
         tracebasetime = simtime;
      }

      failure |= iotrace_read_int32(iotracefile, &val);    /* traced request start time */
      new->tempint1 = val;
      failure |= iotrace_read_int32(iotracefile, &val);    /* traced request stop time */
      new->tempint2 = val;
      new->tempint2 -= new->tempint1;
      failure |= iotrace_read_int32(iotracefile, &val);
      new->bcount = val;
      if (new->bcount & 0x000001FF) {
         fprintf(stderr, "HPL request for non-512B multiple size: %d\n", new->bcount);
         exit(0);
      }
      new->bcount = new->bcount >> 9;
      failure |= iotrace_read_int32(iotracefile, &val);
      new->blkno = val;
      failure |= iotrace_read_int32(iotracefile, &val);
      new->devno = (val >> 8) & 0xFF;
      failure |= iotrace_read_int32(iotracefile, &val);         /* drivertype */
      failure |= iotrace_read_int32(iotracefile, &val);          /* cylno */
	/* for convenience and historical reasons, this cast is being allowed */
	/* (the value is certain to be less than 32 sig bits, and will not be */
	/* used as a pointer).                                                */
      new->buf = (void *) val;
      failure |= iotrace_read_int32(iotracefile, &val);
      new->flags = val;
      iotrace_hpl_srt_convert_flags(new);
      failure |= iotrace_read_int32(iotracefile, junk);            /* info */
      size -= 13 * sizeof(int32_t);
      if ((id >> 16) == 4) {
         failure |= iotrace_read_int32(iotracefile, &val);  /* queuelen */
         new->slotno = val;
         size -= sizeof(int32_t);
      }
      if ((id & 0xFFFF) == HPL_SUSPECTIO) {
         /*suspectiocnt++;*/
         failure |= iotrace_read_int32(iotracefile, junk);        /* susflags */
         size -= sizeof(int32_t);
      }
      if (failure) {
         addtoextraq((event *) new);
         return(NULL);
      }
      if (size) {
         fprintf(stderr, "Unmatched size for record - %d\n", size);
         exit(0);
      }
      new->cause = 0;
      new->opid = 0;
      new->busno = 0;
      if ((id & 0xFFFF) == HPL_SHORTIO) {
         return((event *) new);
      }
   }
}


int iotrace_month_convert(monthstr, year)
char *monthstr;
int year;
{
   if (strcmp(monthstr, "Jan") == 0) {
      return(0);
   } else if (strcmp(monthstr, "Feb") == 0) {
      return(31);
   } else if (strcmp(monthstr, "Mar") == 0) {
      return((year % 4) ? 59 : 60);
   } else if (strcmp(monthstr, "Apr") == 0) {
      return((year % 4) ? 90 : 91);
   } else if (strcmp(monthstr, "May") == 0) {
      return((year % 4) ? 120 : 121);
   } else if (strcmp(monthstr, "Jun") == 0) {
      return((year % 4) ? 151 : 152);
   } else if (strcmp(monthstr, "Jul") == 0) {
      return((year % 4) ? 181 : 182);
   } else if (strcmp(monthstr, "Aug") == 0) {
      return((year % 4) ? 212 : 213);
   } else if (strcmp(monthstr, "Sep") == 0) {
      return((year % 4) ? 243 : 244);
   } else if (strcmp(monthstr, "Oct") == 0) {
      return((year % 4) ? 273 : 274);
   } else if (strcmp(monthstr, "Nov") == 0) {
      return((year % 4) ? 304 : 305);
   } else if (strcmp(monthstr, "Dec") == 0) {
      return((year % 4) ? 334 : 335);
   }
   assert(0);
   return(-1);
}


double iotrace_raw_get_hirestime(bigtime, smalltime)
int bigtime;
int smalltime;
{
   unsigned int loresticks;
   int small, turnovers;
   int smallticks;

   if (basebigtime == -1) {
      basebigtime = bigtime;
      basesmalltime = smalltime;
      basesimtime = 0.0;
   } else {
      small = (basesmalltime - smalltime) & 0xFFFF;
      loresticks = (bigtime - basebigtime) * 11932 - small;
      turnovers = (int) (((double) loresticks / (double) 65536) + (double) 0.5);
      smallticks = turnovers * 65536 + small;
      basebigtime = bigtime;
      basesmalltime = smalltime;
      basesimtime += (double) smallticks * (double) 0.000838574;
   }
   return(basesimtime);
}


/* kept mainly as an example */

event * iotrace_raw_get_ioreq_event(iotracefile, new)
FILE *iotracefile;
ioreq_event *new;
{
   int bigtime;
   unsigned short small;
   int smalltime;
   int failure = 0;
   char order, crit;
   double schedtime, donetime;
   int32_t val;

   failure |= iotrace_read_int32(iotracefile, &val);
   bigtime = val;
   failure |= iotrace_read_short(iotracefile, &small);
   smalltime = ((int) small) & 0xFFFF;
   new->time = iotrace_raw_get_hirestime(bigtime, smalltime);
   failure |= iotrace_read_short(iotracefile, &small);
   failure |= iotrace_read_int32(iotracefile, &val);
   bigtime = val;
   smalltime = ((int) small) & 0xFFFF;
   schedtime = iotrace_raw_get_hirestime(bigtime, smalltime);
   failure |= iotrace_read_int32(iotracefile, &val);
   bigtime = val;
   failure |= iotrace_read_short(iotracefile, &small);
   smalltime = ((int) small) & 0xFFFF;
   donetime = iotrace_raw_get_hirestime(bigtime, smalltime);
   failure |= iotrace_read_char(iotracefile, &order);
   failure |= iotrace_read_char(iotracefile, &crit);
   if (crit) {
      new->flags |= TIME_CRITICAL;
   }
   failure |= iotrace_read_int32(iotracefile, &val);
   new->bcount = val >> 9;
   failure |= iotrace_read_int32(iotracefile, &val);
   new->blkno = val;
   failure |= iotrace_read_int32(iotracefile, &val);
   new->devno = val;
   failure |= iotrace_read_int32(iotracefile, &val);
   new->flags = val & READ;
   new->cause = 0;
   new->buf = 0;
   new->opid = 0;
   new->busno = 0;
   new->tempint1 = (int)((schedtime - new->time) * (double) 1000);
   new->tempint2 = (int)((donetime - schedtime) * (double) 1000);
   if (failure) {
      addtoextraq((event *) new);
      new = NULL;
   }
   return((event *) new);
}


event * iotrace_ascii_get_ioreq_event(iotracefile, new)
FILE *iotracefile;
ioreq_event *new;
{
   char line[201];

   if (fgets(line, 200, iotracefile) == NULL) {
      addtoextraq((event *) new);
      return(NULL);
   }
   if (sscanf(line, "%lf %d %d %d %x\n", &new->time, &new->devno, &new->blkno, &new->bcount, &new->flags) != 5) {
      fprintf(stderr, "Wrong number of arguments for I/O trace event type\n");
      fprintf(stderr, "line: %s", line);
      exit(0);
   }
   if (new->flags & ASYNCHRONOUS) {
      new->flags |= (new->flags & READ) ? TIME_LIMITED : 0;
   } else {
      new->flags |= TIME_CRITICAL;
   }
   new->buf = 0;
   new->opid = 0;
   new->busno = 0;
   new->cause = 0;
   return((event *) new);
}


event * iotrace_get_ioreq_event(iotracefile, traceformat, temp)
FILE *iotracefile;
int traceformat;
event *temp;
{
   switch (traceformat) {
      
      case ASCII:	temp = iotrace_ascii_get_ioreq_event(iotracefile, temp);
			break;
      
      case RAW:		temp = iotrace_raw_get_ioreq_event(iotracefile, temp);
			break;
      
      case HPL:         temp = iotrace_hpl_get_ioreq_event(iotracefile, temp);
			break;

      case DEC:         temp = iotrace_dec_get_ioreq_event(iotracefile, temp);
			break;

      case VALIDATE:    temp = iotrace_validate_get_ioreq_event(iotracefile, temp);
			break;

      default:  fprintf(stderr, "Unknown traceformat in iotrace_get_ioreq_event - %d\n", traceformat);
		exit(0);
   }
   return(temp);
}


void iotrace_hpl_srt_tracefile_start(tracedate)
char *tracedate;
{
   char crap[40];
   char monthstr[40];
   int day;
   int hour;
   int minute;
   int second;
   int year;

   if (sscanf(tracedate, "%s\t= \"%s %s %d %d:%d:%d %d\";\n", crap, crap, monthstr, &day, &hour, &minute, &second, &year) != 8) {
      fprintf(stderr, "Format problem with 'tracedate' line in HPL trace - %s\n", tracedate);
      exit(0);
   }
   if (baseyear == 0) {
      baseyear = year;
   }
   day = day + iotrace_month_convert(monthstr, year);
   if (year != baseyear) {
      day += (baseyear % 4) ? 365 : 366;
   }
   if (baseday == 0) {
      baseday = day;
   }
   second = second + (60 * minute) + (3600 * hour) + (86400 * (day - baseday));
   if (basesecond == 0) {
      basesecond = second;
   }
   second -= basesecond;
   tracebasetime += (double) 1000 * (double) second;
}


void iotrace_hpl_initialize_file(iotracefile, print_tracefile_header)
FILE *iotracefile;
int print_tracefile_header;
{
   char letter = '0';
   char line[201];
   char linetype[40];

   if (!traceheader) {
      return;
   }
   while (1) {
      if (fgets(line, 200, iotracefile) == NULL) {
         fprintf(stderr, "No 'tracedate' line in HPL trace\n");
         exit(0);
      }
      sscanf(line, "%s", linetype);
      if (strcmp(linetype, "tracedate") == 0) {
         break;
      }
   }
   iotrace_hpl_srt_tracefile_start(line);
   while (letter != 0x0C) {
      if (fscanf(iotracefile, "%c", &letter) != 1) {
         fprintf(stderr, "End of header information never found - end of file\n");
         exit(0);
      }
      if ((print_tracefile_header) && (letter != 0x0C)) {
         printf("%c", letter);
      }
   }
}


void iotrace_initialize_file(iotracefile, traceformat, print_tracefile_header)
FILE *iotracefile;
int traceformat;
int print_tracefile_header;
{
   if (traceformat == HPL) {
      iotrace_hpl_initialize_file(iotracefile, print_tracefile_header);
   }
}

