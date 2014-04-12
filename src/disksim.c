
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
#include "disksim_pfface.h"

int external_control = 0;
void (*external_io_done_notify)(ioreq_event *curr) = NULL;

extern void intr_acknowledge();
extern void iotrace_initialize_file();
extern void iotrace_set_format();

extern int closedios;
extern double closedthinktime;

int endian = _LITTLE_ENDIAN;
int traceformat = ASCII;
int traceendian = _LITTLE_ENDIAN;
int traceheader = TRUE;
int print_tracefile_header = FALSE;

FILE *parfile = NULL;
FILE *statdeffile = NULL;
FILE *iotracefile = NULL;
FILE *outputfile = NULL;
FILE *outios = NULL;

int iotrace = 0;
int synthgen = 0;

event *intq = NULL;
event *extraq = NULL;
int intqlen = 0;
int extraqlen = 0;

double simtime = 0.0;
int stop_sim = FALSE;

int seedval;

int warmup_iocnt = 0;
double warmuptime = 0.0;
timer_event *warmup_event = NULL;


void allocateextra()
{
   int i;
   event *temp = NULL;

   StaticAssert (sizeof(event) == DISKSIM_EVENT_SIZE);
   if ((temp = (event *)malloc(ALLOCSIZE)) == NULL) {
      fprintf (stderr, "Error allocating space for events\n");
      exit(0);
   }
   for (i=0; i<((ALLOCSIZE/DISKSIM_EVENT_SIZE)-1); i++) {
      temp[i].next = &temp[i+1];
   }
   temp[((ALLOCSIZE/DISKSIM_EVENT_SIZE)-1)].next = NULL;
   extraq = temp;
   extraqlen = ALLOCSIZE / DISKSIM_EVENT_SIZE;
}


void addtoextraq(temp)
event *temp;
{
   if (temp == NULL) {
      return;
   }
   temp->next = extraq;
   temp->prev = NULL;
   extraq = temp;
   extraqlen++;
}


event * getfromextraq()
{
   event *temp = NULL;

   temp = extraq;
   if (extraqlen == 0) {
      allocateextra();
      temp = extraq;
      extraq = extraq->next;
   } else if (extraqlen == 1) {
      extraq = NULL;
   } else {
      extraq = extraq->next;
   }
   extraqlen--;
   temp->next = NULL;
   temp->prev = NULL;
   return(temp);
}


void addlisttoextraq(headptr)
event **headptr;
{
   event *tmp;

   while ((tmp = *headptr)) {
      *headptr = tmp->next;
      addtoextraq(tmp);
   }
   *headptr = NULL;
}


event *event_copy(orig)
event *orig;
{
   event *new = getfromextraq();
   bcopy ((char *)orig, (char *)new, DISKSIM_EVENT_SIZE);
   return((event *) new);
}


void printintq()
{
   event *tmp;
   int i = 0;

   tmp = intq;
   while (tmp != NULL) {
      i++;
      fprintf (outputfile, "Item #%d: time %f, type %d\n", i, tmp->time, tmp->type);
      tmp = tmp->next;
   }
}


void addtointq(temp)
event *temp;
{
   event *run = NULL;

   if ((temp->time + 0.0001) < simtime) {
      fprintf(stderr, "Attempting to addtointq an event whose time has passed\n");
      fprintf(stderr, "simtime %f, curr->time %f, type = %d\n", simtime, temp->time, temp->type);
      exit(0);
   }

   if (intq == NULL) {
      intq = temp;
      temp->next = NULL;
      temp->prev = NULL;
   } else if (temp->time < intq->time) {
      temp->next = intq;
      intq->prev = temp;
      intq = temp;
      temp->prev = NULL;
   } else {
      run = intq;
      while (run->next != NULL) {
         if (temp->time < run->next->time) {
            break;
         }
         run = run->next;
      }
      temp->next = run->next;
      run->next = temp;
      temp->prev = run;
      if (temp->next != NULL) {
         temp->next->prev = temp;
      }
   }
}


event * getfromintq()
{
   event *temp = NULL;

   if (intq == NULL) {
      return(NULL);
   }
   temp = intq;
   intq = intq->next;
   if (intq != NULL) {
      intq->prev = NULL;
   }
   temp->next = NULL;
   temp->prev = NULL;
   return(temp);
}


int removefromintq(curr)
event *curr;
{
   event *tmp;

   tmp = intq;
   while (tmp != NULL) {
      if (tmp == curr) {
         break;
      }
      tmp = tmp->next;
   }
   if (tmp == NULL) {
      return(FALSE);
   }
   if (curr->next != NULL) {
      curr->next->prev = curr->prev;
   }
   if (curr->prev == NULL) {
      intq = curr->next;
   } else {
      curr->prev->next = curr->next;
   }
   curr->next = NULL;
   curr->prev = NULL;
   return(TRUE);
}


void scanparam_int(parline, parname, parptr, parchecks, parminval, parmaxval)
char *parline;
char *parname;
int *parptr;
int parchecks;
int parminval;
int parmaxval;
{
   if (sscanf(parline, "%d", parptr) != 1) {
      fprintf(stderr, "Error reading '%s'\n", parname);
      exit(0);
   }
   if (((parchecks & 1) && (*parptr < parminval)) || ((parchecks & 2) && (*parptr > parmaxval))) {
      fprintf(stderr, "Invalid value for '%s': %d\n", parname, *parptr);
      exit(0);
   }
}


void getparam_int(parfile, parname, parptr, parchecks, parminval, parmaxval)
FILE *parfile;
char *parname;
int *parptr;
int parchecks;
int parminval;
int parmaxval;
{
   char line[201];

   sprintf(line, "%s: %s\n", parname, "%d");
   if (fscanf(parfile, line, parptr) != 1) {
      fprintf(stderr, "Error reading '%s'\n", parname);
      exit(0);
   }
   if (((parchecks & 1) && (*parptr < parminval)) || ((parchecks & 2) && (*parptr > parmaxval))) {
      fprintf(stderr, "Invalid value for '%s': %d\n", parname, *parptr);
      exit(0);
   }
   fprintf (outputfile, "%s: %d\n", parname, *parptr);
}


void getparam_double(parfile, parname, parptr, parchecks, parminval, parmaxval)
FILE *parfile;
char *parname;
double *parptr;
int parchecks;
double parminval;
double parmaxval;
{
   char line[201];

   sprintf(line, "%s: %s\n", parname, "%lf");
   if (fscanf(parfile, line, parptr) != 1) {
      fprintf(stderr, "Error reading '%s'\n", parname);
      exit(0);
   }
   if (((parchecks & 1) && (*parptr < parminval)) || ((parchecks & 2) && (*parptr > parmaxval)) || ((parchecks & 4) && (*parptr <= parminval))) {
      fprintf(stderr, "Invalid value for '%s': %f\n", parname, *parptr);
      exit(0);
   }
   fprintf (outputfile, "%s: %f\n", parname, *parptr);
}


void getparam_bool(parfile, parname, parptr)
FILE *parfile;
char *parname;
int *parptr;
{
   char line[201];

   sprintf(line, "%s %s\n", parname, "%d");
   if (fscanf(parfile, line, parptr) != 1) {
      fprintf(stderr, "Error reading '%s'\n", parname);
      exit(0);
   }
   if ((*parptr != TRUE) && (*parptr != FALSE)) {
      fprintf(stderr, "Invalid value for '%s': %d\n", parname, *parptr);
      exit(0);
   }
   fprintf (outputfile, "%s %d\n", parname, *parptr);
}


void resetstats()
{
   if (external_control | synthgen | iotrace) {
      io_resetstats();
   }
   if (synthgen) {
      pf_resetstats();
   }
}


void stat_warmup_done(timer)
event *timer;
{
   warmuptime = simtime;
   resetstats();
   addtoextraq(timer);
}


void readparams(parfile)
FILE *parfile;
{
   char seed[200];
   double warmup;
   int newend = -1;

   if (fscanf(parfile, "\nByte Order (Endian): %s\n", seed) != 1) {
      fprintf(stderr, "Error reading 'Byte Order:'\n");
      exit(0);
   }
   if (strcmp(seed, "Little") == 0) {
      endian = _LITTLE_ENDIAN;
   } else if (strcmp(seed, "Big") == 0) {
      endian = _BIG_ENDIAN;
   } else {
      fprintf(stderr, "Invalid byte ordering scheme - %s\n", seed);
      exit(0);
   }

   {
      int tmp = 0x11223344;
      char *tmp2 = (char *) &tmp;
      newend = (tmp2[0] == 0x44) ? _LITTLE_ENDIAN : ((tmp2[0] == 0x11) ? _BIG_ENDIAN : -1);
   }

   if (newend != -1) {
      fprintf (outputfile, "Byte Order (Endian): %s (using detected value: %s)\n", seed, ((newend == _LITTLE_ENDIAN) ? "Little" : "Big"));
      endian = newend;
   } else {
      fprintf (outputfile, "Byte Order (Endian): %s\n", seed);
   }

   if (fscanf(parfile, "Init Seed: %s\n", seed) != 1) {
      fprintf(stderr, "Error reading 'Seed:'\n");
      exit(0);
   }
   if (strcmp(seed, "PID") == 0) {
      srand48((int)getpid());
      fprintf (outputfile, "Init Seed: PID (%d)\n", (int) getpid());
   } else if (sscanf(seed, "%d", &seedval) != 1) {
      fprintf(stderr, "Invalid init seed value: %s\n", seed);
      exit(0);
   } else {
      srand48(seedval);
      fprintf (outputfile, "Init Seed: %d\n", seedval);
   }

   if (fscanf(parfile, "Real Seed: %s\n", seed) != 1) {
      fprintf(stderr, "Error reading 'Seed:'\n");
      exit(0);
   }
   if (strcmp(seed, "PID") == 0) {
      seedval = getpid();
      fprintf (outputfile, "Real Seed: PID\n");
   } else if (sscanf(seed, "%d", &seedval) != 1) {
      fprintf(stderr, "Invalid init seed value: %s\n", seed);
      exit(0);
   } else {
      fprintf (outputfile, "Real Seed: %d\n", seedval);
   }

   if (fscanf(parfile, "Statistic warm-up period: %lf %s\n", &warmup, seed) != 2) {
      fprintf(stderr, "Error reading 'Statistic warm-up period:'\n");
      exit(0);
   }
   fprintf (outputfile, "Statistic warm-up period: %f %s\n", warmup, seed);
   if (warmup > 0.0) {
      if (strcmp(seed, "seconds") == 0) {
         warmup_event = (timer_event *) getfromextraq();
         warmup_event->type = TIMER_EXPIRED;
         warmup_event->time = warmup * (double) 1000.0;
         warmup_event->func = stat_warmup_done;
      } else if (strcmp(seed, "I/Os") == 0) {
         warmup_iocnt = ceil(warmup) + 1;
      } else {
         fprintf(stderr, "Invalid type of statistic warm-up period: %s\n", seed);
         exit(0);
      }
   }

   if (fscanf(parfile, "Stat (dist) definition file: %s\n", seed) != 1) {
      fprintf(stderr, "Error reading 'Stat (dist) definition file:'\n");
      exit(0);
   }
   if ((statdeffile = fopen(seed, "r")) == NULL) {
      fprintf(stderr, "Statdeffile %s cannot be opened for read access\n", seed);
      exit(0);
   }
   fprintf (outputfile, "Stat (dist) definition file: %s\n", seed);

   if (fscanf(parfile, "Output file for trace of I/O requests simulated: %s\n", seed) != 1) {
      fprintf(stderr, "Error reading 'Output file for I/O requests simulated:'\n");
      exit(0);
   }
   if ((strcmp(seed, "0") != 0) && (strcmp(seed, "null") != 0)) {
      if ((outios = fopen(seed, "w")) == NULL) {
         fprintf(stderr, "Outios %s cannot be opened for write access\n", seed);
         exit(0);
      }
   }
   fprintf(outputfile, "Output file for I/O requests simulated: %s\n", seed);

   if (external_control | synthgen | iotrace) {
      io_readparams(parfile);
   }
   if (synthgen) {
      pf_readparams(parfile);
   }
}


void doparamoverrides(overrides, overcnt)
char **overrides;
int overcnt;
{
   int i;
   int first, last;
   int vals;

   for (i=0; i<overcnt; i+=4) {
      vals = sscanf(overrides[(i+1)], "%d-%d", &first, &last);
      if ((vals != 2) && ((vals != 1) || (first != -1))) {
         fprintf(stderr, "Invalid format for range in doparamoverrides\n");
	 exit(0);
      }
      if (strcmp(overrides[i], "iodriver") == 0) {
	 io_param_override(IODRIVER, overrides[(i+2)], overrides[(i+3)], first, last);
      } else if (strcmp(overrides[i], "iosim") == 0) {
	 io_param_override(IOSIM, overrides[(i+2)], overrides[(i+3)], first, last);
      } else if (strcmp(overrides[i], "sysorg") == 0) {
	 io_param_override(SYSORG, overrides[(i+2)], overrides[(i+3)], first, last);
      } else if (strcmp(overrides[i], "synthio") == 0) {
	 pf_param_override(SYNTHIO, overrides[(i+2)], overrides[(i+3)], first, last);
      } else if (strcmp(overrides[i], "disk") == 0) {
	 io_param_override(DISK, overrides[(i+2)], overrides[(i+3)], first, last);
      } else if (strcmp(overrides[i], "controller") == 0) {
	 io_param_override(CONTROLLER, overrides[(i+2)], overrides[(i+3)], first, last);
      } else if (strcmp(overrides[i], "bus") == 0) {
	 io_param_override(BUS, overrides[(i+2)], overrides[(i+3)], first, last);
      } else {
	 fprintf(stderr, "Structure with parameter to override not supported: %s\n", overrides[i]);
	 exit(0);
      }
      fprintf (outputfile, "Override: %s %s %s %s\n", overrides[i], overrides[(i+1)], overrides[(i+2)], overrides[(i+3)]);
   }
}


void printstats()
{
   fprintf (outputfile, "\nSIMULATION STATISTICS\n");
   fprintf (outputfile, "---------------------\n\n");
   fprintf (outputfile, "Total time of run:       %f\n\n", simtime);
   fprintf (outputfile, "Warm-up time:            %f\n\n", warmuptime);

   if (synthgen) {
      pf_printstats();
   }
   if (external_control | synthgen | iotrace) {
      io_printstats();
   }
}


void initialize()
{
   int val = (synthgen) ? 0 : 1;

   iotrace_initialize_file(iotracefile, traceformat, print_tracefile_header);
   while (intq) {
      addtoextraq(getfromintq());
   }
   if (external_control | synthgen | iotrace) {
      io_initialize(val);
   }
   if (synthgen) {
      pf_initialize(seedval);
   } else {
      srand48(seedval);
   }
   simtime = 0.0;
}


void cleanstats()
{
   if (external_control | synthgen | iotrace) {
      io_cleanstats();
   }
   if (synthgen) {
      pf_cleanstats();
   }
}


void set_external_io_done_notify (void (*func)(ioreq_event *))
{
   external_io_done_notify = func;
}


event *io_done_notify(curr)
ioreq_event *curr;
{
   if (synthgen) {
      return(pf_io_done_notify(curr));
   }
   if (external_control) {
      external_io_done_notify (curr);
   }
   return(NULL);
}


event * getnextevent()
{
   event *curr = getfromintq();
   event *temp;

   if (curr) {
      simtime = curr->time;
   }
   if ((curr) && (curr->type == NULL_EVENT)) {
      if ((iotrace) && io_using_external_event(curr)) {
         if ((temp = io_get_next_external_event(iotracefile)) == NULL) {
            stop_sim = TRUE;
         }
      } else {
         fprintf(stderr, "NULL_EVENT not linked to any external source\n");
         exit(0);
      }
      if (temp) {
         temp->type = NULL_EVENT;
         addtointq(temp);
      }
   }
   if (curr == NULL) {
      fprintf (outputfile, "Returning NULL from getnextevent\n");
      fflush (outputfile);
   }
   return(curr);
}


void simstop()
{
   stop_sim = TRUE;
}


void prime_simulation()
{
   event *curr;

   if (warmup_event) {
      addtointq(warmup_event);
      warmup_event = NULL;
   }
   if (iotrace) {
      if ((curr = io_get_next_external_event(iotracefile)) == NULL) {
         cleanstats();
         return;
      }
      if ((traceformat != VALIDATE) && (closedios == 0)) {
         curr->type = NULL_EVENT;
      } else if (closedios) {
         int iocnt;
         io_using_external_event(curr);
         curr->time = simtime + closedthinktime;
         for (iocnt=1; iocnt < closedios; iocnt++) {
            curr->next = io_get_next_external_event(iotracefile);
            if (curr->next) {
               io_using_external_event(curr->next);
               curr->time = simtime + closedthinktime;
               addtointq(curr->next);
            }
         }
      }
      addtointq(curr);
   }
}


void disksim_simulate_event ()
{
   event *curr;

   if ((curr = getnextevent()) == NULL) {
      stop_sim = TRUE;
   } else {
      simtime = curr->time;
/*
fprintf (outputfile, "%f: next event to handle: type %d, temp %x\n", simtime, curr->type, curr->temp);
fflush (outputfile);
*/

      if (curr->type == INTR_EVENT) {
	 intr_acknowledge(curr);
      } else if ((curr->type >= IO_MIN_EVENT) && (curr->type <= IO_MAX_EVENT)) {
	 io_internal_event(curr);
      } else if ((curr->type >= PF_MIN_EVENT) && (curr->type <= PF_MAX_EVENT)) {
	 pf_internal_event(curr);
      } else if (curr->type == TIMER_EXPIRED) {
         ((timer_event *)curr)->func(curr);
      } else {
         fprintf(stderr, "Unrecognized event in simulate: %d\n", curr->type);
         exit(0);
      }
/*
fprintf (outputfile, "Event handled, going for next\n");
fflush (outputfile);
*/
   }
}


#ifdef EXTERNAL_MAIN
void disksim_main(argc, argv)
#else
void main(argc, argv)
#endif
int argc;
char **argv;
{
   StaticAssert (sizeof(intchar) == 4);
   if (argc < 6) {
      fprintf(stderr,"Usage: %s paramfile outfile format iotrace synthgen?\n", argv[0]);
      exit(0);
   }
/*
fprintf (stderr, "%s %s %s %s %s %s\n", argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]);
*/
   if ((argc - 6) % 4) {
      fprintf(stderr, "Parameter file overrides must be four-tuples\n");
      exit(0);
   } 
   if ((parfile = fopen(argv[1],"r")) == NULL) {
      fprintf(stderr,"%s cannot be opened for read access\n", argv[1]);
      exit(0);
   }
   if (strcmp(argv[2], "stdout") == 0) {
      outputfile = stdout;
   } else {
      if ((outputfile = fopen(argv[2],"w")) == NULL) {
         fprintf(stderr, "Outfile %s cannot be opened for write access\n", argv[2]);
         exit(0);
      }
   }
   fprintf (outputfile, "\nOutput file name: %s\n", argv[2]);
   fflush (outputfile);

   if (strcmp (argv[3], "external") == 0) {
      external_control = 1;
   } else {
      iotrace_set_format(argv[3]);
   }
   fprintf (outputfile, "Input trace format: %s\n", argv[3]);
   fflush (outputfile);

   if (strcmp(argv[4], "0") != 0) {
      assert (external_control == 0);
      iotrace = 1;
      if (strcmp(argv[4], "stdin") == 0) {
	 iotracefile = stdin;
      } else {
	 if ((iotracefile = fopen(argv[4],"r")) == NULL) {
	    fprintf(stderr, "Tracefile %s cannot be opened for read access\n", argv[4]);
	    exit(0);
	 }
      }
   }
   fprintf (outputfile, "I/O trace used: %s\n", argv[4]);
   fflush (outputfile);

   if (strcmp(argv[5], "0") != 0) {
      synthgen = 1;
   }
   fprintf (outputfile, "Synthgen to be used?: %s\n\n", argv[5]);
   fflush (outputfile);

   readparams(parfile);
fprintf(outputfile, "Readparams complete\n");
fflush(outputfile);
   if (argc > 6) {
      doparamoverrides(&argv[6], (argc - 6));
fprintf(outputfile, "Parameter overrides complete\n");
fflush(outputfile);
   }
   initialize();
fprintf(outputfile, "Initialization complete\n");
fflush(outputfile);
   prime_simulation();
   if (external_control) {
      return;
   }

   while (stop_sim == FALSE) {
      disksim_simulate_event();
   }
fprintf(outputfile, "Simulation complete\n");
fflush(outputfile);
   cleanstats();
   printstats();

   if (parfile) {
      fclose (parfile);
   }
   if (outputfile) {
      fclose (outputfile);
   }
   if (iotracefile) {
      fclose(iotracefile);
   }
}

