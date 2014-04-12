
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
#include "disksim_synthio.h"
#include "disksim_pfsim.h"

#define SYNTHIO_UNIFORM		0
#define SYNTHIO_NORMAL		1
#define SYNTHIO_EXPONENTIAL	2
#define SYNTHIO_POISSON		3
#define SYNTHIO_TWOVALUE	4

typedef struct dist {
   double base;
   double mean;
   double var;
   int    type;
} synthio_distr;

typedef struct gen {
   double         probseq;
   double         probloc;
   double         probread;
   double         probtmcrit;
   double         probtmlim;
   int            number;
   ioreq_event *  pendio;
   sleep_event *  limits;
   int            numdisks;
   int            numblocks;
   int            sectsperdisk;
   int            blksperdisk;
   int            blocksize;
   synthio_distr  tmlimit;
   synthio_distr  genintr;
   synthio_distr  seqintr;
   synthio_distr  locintr;
   synthio_distr  locdist;
   synthio_distr  sizedist;
} synthio_generator;


synthio_generator *synthio_gens;
int    synthio_gencnt;
int    synthio_iocnt = 0;
int    synthio_endiocnt;
double synthio_endtime;
int    synthio_syscalls;
double synthio_syscall_time;
double synthio_sysret_time;


double synthio_get_uniform(fromdistr)
synthio_distr *fromdistr;
{
   return(((fromdistr->var - fromdistr->base) * drand48()) + fromdistr->base);
}


double synthio_get_normal(fromdistr)
synthio_distr *fromdistr;
{
   double y1, y2;
   double y = 0;

   while (y <= 0.0) {
      y2 = - log((double) 1.0 - drand48());
      y1 = - log((double) 1.0 - drand48());
      y = y2 - ((y1 - (double) 1.0) * (y1 - (double) 1.0)) / 2;
   }
   if (drand48() < 0.5) {
      y1 = -y1;
   }
   return((fromdistr->var * y1) + fromdistr->mean);
}


double synthio_get_exponential(fromdistr)
synthio_distr *fromdistr;
{
   double dtmp;

   dtmp = log((double) 1.0 - drand48());
   return((fromdistr->base - (fromdistr->mean * dtmp)));
}


double synthio_get_poisson(fromdistr)
synthio_distr *fromdistr;
{
   double dtmp = 1.0;
   int count = 0;
   double stop;

   stop = exp(-fromdistr->mean);
   while (dtmp >= stop) {
      dtmp *= drand48();
      count++;
   }
   count--;
   return((double) count + fromdistr->base);
}


double synthio_get_twovalue(fromdistr)
synthio_distr *fromdistr;
{
   if (drand48() < fromdistr->var) {
      return(fromdistr->mean);
   } else {
      return(fromdistr->base);
   }
}


double synthio_getrand(fromdistr)
synthio_distr *fromdistr;
{
   switch (fromdistr->type) {
      case SYNTHIO_UNIFORM:
	                return(synthio_get_uniform(fromdistr));
      case SYNTHIO_NORMAL:
                        return(synthio_get_normal(fromdistr));
      case SYNTHIO_EXPONENTIAL:
			return(synthio_get_exponential(fromdistr));
      case SYNTHIO_POISSON:
			return(synthio_get_poisson(fromdistr));
      case SYNTHIO_TWOVALUE:
			return(synthio_get_twovalue(fromdistr));
      default:
	                fprintf(stderr, "Unrecognized distribution type - %d\n", fromdistr->type);
	                exit(0);
   }
}


void synthio_appendio(procp, tmp)
process *procp;
ioreq_event *tmp;
{
   synthio_generator *gen = (synthio_generator *) procp->space;
   sleep_event *limittmp;
   event *prev = NULL;
   sleep_event *newsleep = NULL;
   ioreq_event *new;
   int blocksize;
   int gennum;

   blocksize = gen->blocksize;
   gennum = gen->number;
   new = (ioreq_event *) getfromextraq();
   new->type = IOREQ_EVENT;
   new->time = tmp->time;
   new->devno = tmp->devno;
   new->blkno = tmp->blkno * blocksize;
   new->bcount = tmp->bcount * blocksize;
   new->flags = tmp->flags;
   new->cause = tmp->cause;
   new->opid = (synthio_endiocnt * gennum) + synthio_iocnt;
	/* this is being considered "ok" under the assumption that opid will */
	/* never exceed 2^32.....                                            */
   new->buf = (void *) new->opid;
   if (new->flags & (TIME_CRITICAL|TIME_LIMITED)) {
      newsleep = (sleep_event *) getfromextraq();
      newsleep->type = SLEEP_EVENT;
      newsleep->time = 0.0;
      newsleep->chan = new->buf;
      newsleep->iosleep = 1;
      if (new->flags & TIME_CRITICAL) {
         newsleep->next = procp->eventlist;
         new->next = (ioreq_event *) newsleep;
      } else {
         new->next = (ioreq_event *) procp->eventlist;
         newsleep->time = -1.0;
         while (newsleep->time < 0.0) {
            newsleep->time = synthio_getrand (&gen->tmlimit);
         }
         newsleep->time += new->time;
         limittmp = gen->limits;
         if ((limittmp == NULL) || (newsleep->time < limittmp->time)) {
            newsleep->next = (event *) gen->limits;
            gen->limits = (sleep_event *) newsleep;
            if (limittmp) {
               limittmp->time -= newsleep->time;
            }
         } else {
            newsleep->time -= limittmp->time;
            while ((limittmp->next) && (newsleep->time >= limittmp->next->time)) {
               limittmp = (sleep_event *) limittmp->next;
               newsleep->time -= limittmp->time;
            }
            newsleep->next = limittmp->next;
            limittmp->next = (event *) newsleep;
            if (newsleep->next) {
               newsleep->next->time -= newsleep->time;
            }
         }
         newsleep = NULL;
      }
   } else {
      new->next = (ioreq_event *) procp->eventlist;
   }

   procp->eventlist = (event *) new;

synthio_check_genlimits:
   limittmp = gen->limits;
   while ((limittmp) && (limittmp->time < new->time)) {
      gen->limits = (sleep_event *) limittmp->next;
      new->time -= limittmp->time;
      limittmp->next = (event *) new;
      if (prev) {
         prev->next = (event *) limittmp;
      } else {
         procp->eventlist = (event *) limittmp;
      }
      prev = (event *) limittmp;
      limittmp = gen->limits;
   }

   if ((newsleep) && (gen->limits)) {
      prev = (event *) new;
      new = (ioreq_event *) newsleep;
      newsleep = NULL;
      goto synthio_check_genlimits;
   }

/*
fprintf (outputfile, "New request %d, time %f, devno %d, blkno %d, bcount %d, flags %x\n", synthio_iocnt, new->time, new->devno, new->blkno, new->bcount, new->flags);
*/
}


void synthio_initialize_generator(procp)
process *procp;
{
   ioreq_event *tmp;
   synthio_generator *gen;
   event *evptr;
   double reqclass;

   evptr = getfromextraq();
   evptr->time = 0.0;
   evptr->type = SYNTHIO_EVENT;
   evptr->next = NULL;
   procp->eventlist = evptr;
   gen = (synthio_generator *) procp->space;
   if (gen == NULL) {
      fprintf(stderr, "Process with no synthetic generator in synthio_initialize\n");
      exit(0);
   }
   tmp = (ioreq_event *) getfromextraq();
   tmp->time = -1.0;
   while (tmp->time < 0.0) {
      tmp->time = synthio_getrand(&gen->genintr);
   }
   tmp->flags = 0;
   tmp->cause = gen->number;
   tmp->devno = (int) (drand48() * (double) gen->numdisks);
   tmp->blkno = tmp->bcount = gen->blksperdisk;
   while (((tmp->blkno + tmp->bcount) >= gen->blksperdisk) || (tmp->bcount == 0)) {
      tmp->blkno = (int) (drand48() * (double) gen->blksperdisk);
      tmp->bcount = ((int) synthio_getrand(&gen->sizedist) + gen->blocksize - 1) / gen->blocksize;
   }
   if (drand48() < gen->probread) {
      tmp->flags |= READ;
   }
   reqclass = drand48() - gen->probtmcrit;
   if (reqclass < 0.0) {
      tmp->flags |= TIME_CRITICAL;
   } else if (reqclass < gen->probtmlim) {
      tmp->flags |= TIME_LIMITED;
   }
   gen->pendio = tmp;
   synthio_appendio(procp, tmp);
/*
fprintf (outputfile, "Initialized synthio process #%d, first event at time %f\n", procp->pid, procp->eventlist->time);
*/
}


void synthio_generatenextio(gen)
synthio_generator *gen;
{
   double type;
   double reqclass;
   int blkno;
   ioreq_event *tmp;

   tmp = gen->pendio;
   type = drand48();
   if ((type < gen->probseq) && ((tmp->blkno + 2*tmp->bcount) < gen->blksperdisk)) {
      tmp->time = -1.0;
      while (tmp->time < 0.0) {
         tmp->time = synthio_getrand(&gen->seqintr);
      }
      tmp->flags = SEQ | (tmp->flags & READ);
      tmp->cause = gen->number;
      tmp->blkno += tmp->bcount;
   } else if ((type < (gen->probseq + gen->probloc)) && (type >= gen->probseq)) {
      tmp->time = -1.0;
      while (tmp->time < 0.0) {
         tmp->time = synthio_getrand(&gen->locintr);
      }
      tmp->flags = LOCAL;
      tmp->cause = gen->number;
      blkno = gen->blksperdisk;
      while (((blkno + tmp->bcount) >= gen->blksperdisk) || (blkno < 0) || (tmp->bcount <= 0)) {
         blkno = tmp->blkno + (int) synthio_getrand(&gen->locdist) / gen->blocksize;
         tmp->bcount = ((int) synthio_getrand(&gen->sizedist) + gen->blocksize - 1) / gen->blocksize;
      }
      tmp->blkno = blkno;
      if (drand48() < gen->probread) {
	 tmp->flags |= READ;
      }
   } else {
      tmp->time = -1.0;
      while (tmp->time < 0.0) {
         tmp->time = synthio_getrand(&gen->genintr);
      }
      tmp->flags = 0;
      tmp->cause = gen->number;
      tmp->devno = (int) (drand48() * (double) gen->numdisks);
      tmp->blkno = tmp->bcount = gen->blksperdisk;
      while (((tmp->blkno + tmp->bcount) >= gen->blksperdisk) || (tmp->bcount <= 0)) {
         tmp->blkno = (int) (drand48() * (double) gen->blksperdisk);
         tmp->bcount = ((int) synthio_getrand(&gen->sizedist) + gen->blocksize - 1) / gen->blocksize;
      }
      if (drand48() < gen->probread) {
	 tmp->flags = READ;
      }
   }
   reqclass = drand48() - gen->probtmcrit;
   if (reqclass < 0.0) {
      tmp->flags |= TIME_CRITICAL;
   } else if (reqclass < gen->probtmlim) {
      tmp->flags |= TIME_LIMITED;
   }
}


void synthio_generate_io_activity(procp)
process *procp;
{
   synthio_generator *gen;

   gen = (synthio_generator *) procp->space;
/*
fprintf (outputfile, "simtime %f, endtime %f, iocnt %d, endiocnt %d\n", simtime, synthio_endtime, synthio_iocnt, synthio_endiocnt);
*/
   synthio_iocnt++;
   if ((simtime < synthio_endtime) && (synthio_iocnt < synthio_endiocnt)) {
      synthio_generatenextio(gen);
      synthio_appendio(procp, gen->pendio);
   } else {
      simstop();
   }
}


void synthio_param_override(paramname, paramval, first, last)
char *paramname;
char *paramval;
int first;
int last;
{
   int i;

   if (first == -1) {
      first = 0;
      last = synthio_gencnt - 1;
   } else if ((first < 0) || (last >= synthio_gencnt) || (last < first)) {
      fprintf(stderr, "Invalid range at synthio_param_override: %d - %d\n", first, last);
      exit(0);
   }
   for (i=first; i<=last; i++) {
      if (strcmp(paramname, "genintr_mean") == 0) {
         if (sscanf(paramval, "%lf\n", &synthio_gens[i].genintr.mean) != 1) {
            fprintf(stderr, "Error reading genintr_mean from paramval in synthio_param_override\n");
            exit(0);
         }
      } else if (strcmp(paramname, "probread") == 0) {
         if (sscanf(paramval, "%lf\n", &synthio_gens[i].probread) != 1) {
            fprintf(stderr, "Error reading probread from paramval in synthio_param_override\n");
            exit(0);
         }
      } else {
	 fprintf(stderr, "Attempting to override unsupported value at synthio_param_override: %s\n", paramname);
	 exit(0);
      }
   }
}


void synthio_resetstats()
{
}


void synthio_cleanstats()
{
}


void synthio_printstats()
{
}


void synthio_read_uniform(parfile, newdistr, mult)
FILE *parfile;
synthio_distr *newdistr;
double mult;
{
   newdistr->type = SYNTHIO_UNIFORM;
   if (fscanf(parfile, "Minimum value: %lf\n", &newdistr->base) != 1) {
      fprintf(stderr, "Error reading 'minimum value'\n");
      exit(0);
   }
   fprintf (outputfile, "Minimum value: %f\n", newdistr->base);
   newdistr->base *= mult;
   if (fscanf(parfile, "Maximum value: %lf\n", &newdistr->var) != 1) {
      fprintf(stderr, "Error reading 'maximum value'\n");
      exit(0);
   }
   fprintf (outputfile, "Maximum value: %f\n", newdistr->var);
   newdistr->var *= mult;
   if (newdistr->var <= newdistr->base) {
      fprintf(stderr, "'maximum value' is not greater than 'minimum value' - %f <= %f\n", newdistr->var, newdistr->base);
      exit(0);
   }
}


void synthio_read_normal(parfile, newdistr, mult)
FILE *parfile;
synthio_distr *newdistr;
double mult;
{
   newdistr->type = SYNTHIO_NORMAL;
   if (fscanf(parfile, "Mean: %lf\n", &newdistr->mean) != 1) {
      fprintf(stderr, "Error reading normal mean\n");
      exit(0);
   }
   fprintf (outputfile, "Mean: %f\n", newdistr->mean);
   newdistr->mean *= mult;
   if (fscanf(parfile, "Variance: %lf\n", &newdistr->var) != 1) {
      fprintf(stderr, "Error reading normal variance\n");
      exit(0);
   }
   fprintf (outputfile, "Variance: %f\n", newdistr->var);
   if (newdistr->var < 0) {
      fprintf(stderr, "Invalid value for 'variance' - %f\n", newdistr->var);
      exit(0);
   }
   newdistr->var = mult * sqrt(newdistr->var);
}


void synthio_read_exponential(parfile, newdistr, mult)
FILE *parfile;
synthio_distr *newdistr;
double mult;
{
   newdistr->type = SYNTHIO_EXPONENTIAL;
   if (fscanf(parfile, "Base value: %lf\n", &newdistr->base) != 1) {
      fprintf(stderr, "Error reading exponential base\n");
      exit(0);
   }
   fprintf (outputfile, "Base value: %f\n", newdistr->base);
   newdistr->base *= mult;
   if (fscanf(parfile, "Mean: %lf\n", &newdistr->mean) != 1) {
      fprintf(stderr, "Error reading exponential mean\n");
      exit(0);
   }
   fprintf (outputfile, "Mean: %f\n", newdistr->mean);
   newdistr->mean *= mult;
}


void synthio_read_poisson(parfile, newdistr, mult)
FILE *parfile;
synthio_distr *newdistr;
double mult;
{
   newdistr->type = SYNTHIO_POISSON;
   if (fscanf(parfile, "Base value: %lf\n", &newdistr->base) != 1) {
      fprintf(stderr, "Error reading 'base value'\n");
      exit(0);
   }
   fprintf (outputfile, "Base value: %f\n", newdistr->base);
   newdistr->base *= mult;
   if (fscanf(parfile, "Mean: %lf\n", &newdistr->mean) != 1) {
      fprintf(stderr, "Error reading poisson mean\n");
      exit(0);
   }
   fprintf (outputfile, "Mean: %f\n", newdistr->mean);
   newdistr->mean *= mult;
}


void synthio_read_twovalue(parfile, newdistr, mult)
FILE *parfile;
synthio_distr *newdistr;
double mult;
{
   newdistr->type = SYNTHIO_TWOVALUE;
   if (fscanf(parfile, "Default value: %lf\n", &newdistr->base) != 1) {
      fprintf(stderr, "Error reading 'default value'\n");
      exit(0);
   }
   fprintf(outputfile, "Default value: %f\n", newdistr->base);
   newdistr->base *= mult;
   if (fscanf(parfile, "Second value: %lf\n", &newdistr->mean) != 1) {
      fprintf(stderr, "Error reading 'second value'\n");
      exit(0);
   }
   fprintf(outputfile, "Second value: %f\n", newdistr->mean);
   newdistr->mean *= mult;
   if (fscanf(parfile, "Probability of second value: %lf\n", &newdistr->var) != 1) {
      fprintf(stderr, "Error reading 'probability of second value'\n");
      exit(0);
   }
   if ((newdistr->var < 0) || (newdistr->var > 1)) {
      fprintf(stderr, "Invalid probability of second value - %f\n", newdistr->var);
      exit(0);
   }
   fprintf(outputfile, "Probability of second value: %f\n", newdistr->var);
}


void synthio_read_distr(parfile, newdistr, mult)
FILE *parfile;
synthio_distr *newdistr;
double mult;
{
   char type[80];

   if (fscanf(parfile, "Type of distribution: %s\n", type) != 1) {
      fprintf(stderr, "Error reading 'type of distribution'\n");
      exit(0);
   }
   fprintf (outputfile, "Type of distribution: %s\n", type);
   switch (type[0]) {
      case 'u':
		synthio_read_uniform(parfile, newdistr, mult);
		break;
      case 'n':
		synthio_read_normal(parfile, newdistr, mult);
		break;
      case 'e':
		synthio_read_exponential(parfile, newdistr, mult);
		break;
      case 'p':
		synthio_read_poisson(parfile, newdistr, mult);
		break;
      case 't':
		synthio_read_twovalue(parfile, newdistr, mult);
		break;
      default:
                fprintf(stderr, "Invalid value for 'type of distribution' - %s\n", type);
                exit(0);
   }

}


void synthio_distrcopy (distr1, distr2)
synthio_distr *distr1;
synthio_distr *distr2;
{
   distr2->base = distr1->base;
   distr2->mean = distr1->mean;
   distr2->var = distr1->var;
   distr2->type = distr1->type;
}


void synthio_gencopy (gen1, gen2)
synthio_generator *gen1;
synthio_generator *gen2;
{
   gen2->probseq = gen1->probseq;
   gen2->probloc = gen1->probloc;
   gen2->probread = gen1->probread;
   gen2->probtmcrit = gen1->probtmcrit;
   gen2->probtmlim = gen1->probtmlim;
   gen2->pendio = NULL;
   gen2->limits = NULL;
   gen2->numdisks = gen1->numdisks;
   gen2->numblocks = gen1->numblocks;
   gen2->sectsperdisk = gen1->sectsperdisk;
   gen2->blksperdisk = gen1->blksperdisk;
   gen2->blocksize = gen1->blocksize;
   synthio_distrcopy(&gen1->tmlimit, &gen2->tmlimit);
   synthio_distrcopy(&gen1->genintr, &gen2->genintr);
   synthio_distrcopy(&gen1->seqintr, &gen2->seqintr);
   synthio_distrcopy(&gen1->locintr, &gen2->locintr);
   synthio_distrcopy(&gen1->locdist, &gen2->locdist);
   synthio_distrcopy(&gen1->sizedist, &gen2->sizedist);
}


void synthio_readparams(parfile)
FILE *parfile;
{
   int i = 0;
   int j = 1;
   int descs;
   int genno;
   double mult;
   double probseq;
   double probloc;
   double probread;
   double probtmcrit;
   double probtmlim;
   int sectsperdisk;
   int diskcnt;
   int numgens;
   int blocksize;
   synthio_generator *gen;
   process *procp;

   if (fscanf(parfile, "Number of generators: %d\n", &numgens) != 1) {
      fprintf(stderr, "Error reading 'number of generators'\n");
      exit(0);
   }
   if (numgens < 0) {
      fprintf(stderr, "Invalid value for 'number of generators' - %d\n", numgens);
      exit(0);
   }
   fprintf (outputfile, "Number of generators:  %d\n", numgens);
   synthio_gencnt = numgens;

   if (numgens == 0) {
      return;
   }

   synthio_gens = (synthio_generator *)malloc(numgens * sizeof(synthio_generator));
   
   if (fscanf(parfile, "Number of I/O requests to generate: %d\n", &synthio_endiocnt) != 1) {
      fprintf(stderr, "Error reading 'number of I/O requests'\n");
      exit(0);
   }
   if (synthio_endiocnt <= 0) {
      fprintf(stderr, "Invalid value for 'number of I/O requests' - %d\n", synthio_endiocnt);
      exit(0);
   }
   fprintf (outputfile, "Number of I/O requests to generate: %d\n", synthio_endiocnt);

   if (fscanf(parfile, "Maximum time of trace generated (in seconds): %lf\n", &synthio_endtime) != 1) {
      fprintf(stderr, "Error reading 'time of trace'\n");
      exit(0);
   }
   if (synthio_endtime <= 0) {
      fprintf(stderr, "Invalid value for 'time of trace' - %f\n", synthio_endtime);
      exit(0);
   }
   fprintf (outputfile, "Maximum time of trace generated (in seconds): %f\n", synthio_endtime);
   synthio_endtime *= (double) 1000.0;
   
   if (fscanf(parfile, "System call/return with each request: %d\n", &synthio_syscalls) != 1) {
      fprintf(stderr, "Error reading 'system call per request'\n");
      exit(0);
   }
   if ((synthio_syscalls != FALSE) && (synthio_syscalls != TRUE)) {
      fprintf(stderr, "Invalid value for 'system call per request' - %d\n", synthio_syscalls);
      exit(0);
   }
   fprintf (outputfile, "System call/return with each request: %d\n", synthio_syscalls);

   if (fscanf(parfile, "Think time from call to request: %lf\n", &synthio_syscall_time) != 1) {
      fprintf(stderr, "Error reading 'time from call to request'\n");
      exit(0);
   }
   if (synthio_syscall_time < 0.0) {
      fprintf(stderr, "Invalid value for 'time from call to request' - %f\n", synthio_syscall_time);
      exit(0);
   }
   fprintf (outputfile, "Think time from call to request: %f\n", synthio_syscall_time);

   if (fscanf(parfile, "Think time from request to return: %lf\n", &synthio_sysret_time) != 1) {
      fprintf(stderr, "Error reading 'time from request to return'\n");
      exit(0);
   }
   if (synthio_sysret_time < 0.0) {
      fprintf(stderr, "Invalid value for 'time from request to return' - %f\n", synthio_sysret_time);
      exit(0);
   }
   fprintf (outputfile, "Think time from request to return: %f\n", synthio_sysret_time);

   while (i < numgens) {

      procp = pf_new_process();
      gen = &synthio_gens[i];

      gen->pendio = NULL;
      gen->limits = NULL;

      if (fscanf(parfile, "\nGenerator description #%d\n", &genno) != 1) {
	 fprintf(stderr, "Error reading 'Generator Description #%d'\n", (genno + 1));
	 exit(0);
      }
      if (genno != j) {
         fprintf(stderr, "Generator descriptions not in order - %d\n", genno);
         exit(0);
      }
      j++;

      if (fscanf(parfile, "Generators with description: %d\n", &descs) != 1) {
         fprintf(stderr, "Error reading 'generators with description'\n");
         exit(0);
      }
      if ((descs <= 0) || (descs > (numgens - i))) {
         fprintf(stderr, "Invalid value for 'generators with description' - %d\n", descs);
         exit(0);
      }
      fprintf (outputfile, "Generators with description: %d\n", descs);

      if (fscanf(parfile, "Storage capacity per device (in blocks): %d\n", &sectsperdisk) != 1) {
         fprintf(stderr, "Error reading 'storage capacity per device'\n");
         exit(0);
      }
      if (sectsperdisk <= 0) {
         fprintf(stderr, "Invalid value for 'storage capacity per device' - %d\n", sectsperdisk);
         exit(0);
      }
      fprintf (outputfile, "Storage capacity per device (in blocks): %d\n", sectsperdisk);
      gen->sectsperdisk = sectsperdisk;
   
      if (fscanf(parfile, "Number of storage devices: %d\n", &diskcnt) != 1) {
         fprintf(stderr, "Error reading 'number of storage devices'\n");
         exit(0);
      }
      if (diskcnt <= 0) {
         fprintf(stderr, "Invalid value for 'number of storage devices' - %d\n", diskcnt);
         exit(0);
      }
      fprintf (outputfile, "Number of storage devices: %d\n", diskcnt);
      gen->numdisks = diskcnt;
   
      if (fscanf(parfile, "Blocking factor: %d\n", &blocksize) != 1) {
         fprintf(stderr, "Error reading 'number of storage devices'\n");
         exit(0);
      }
      if ((blocksize <= 0) || (blocksize > sectsperdisk)) {
         fprintf(stderr, "Invalid value for 'blocking factor' - %d\n", blocksize);
         exit(0);
      }
      fprintf (outputfile, "Blocking factor: %d\n", blocksize);
      gen->blocksize = blocksize;

      gen->numblocks = sectsperdisk * diskcnt;
      gen->blksperdisk = sectsperdisk / blocksize;

      if (fscanf(parfile, "Probability of sequential access: %lf\n", &probseq) != 1) {
         fprintf(stderr, "Error reading 'probability sequential'\n");
         exit(0);
      }
      if ((probseq < 0) || (probseq > 1)) {
         fprintf(stderr, "Invalid value for 'probability sequential' - %f\n", probseq);
         exit(0);
      }
      fprintf (outputfile, "Probability of sequential access: %f\n", probseq);
      gen->probseq = probseq;

      if (fscanf(parfile, "Probability of local access: %lf\n", &probloc) != 1) {
         fprintf(stderr, "Error reading 'probability local'\n");
         exit(0);
      }
      if ((probloc < 0) || ((probseq + probloc) > 1)) {
         fprintf(stderr, "Invalid value for 'probability local' - %f\n", probloc);
         exit(0);
      }
      fprintf (outputfile, "Probability of local access: %f\n", probloc);
      gen->probloc = probloc;

      if (fscanf(parfile, "Probability of read access: %lf\n", &probread) != 1) {
         fprintf(stderr, "Error reading 'probability read'\n");
         exit(0);
      }
      if ((probread < 0) || (probread > 1)) {
         fprintf(stderr, "Invalid value for 'probability read' - %f\n", probread);
         exit(0);
      }
      fprintf (outputfile, "Probability of read access: %f\n", probread);
      gen->probread = probread;

      if (fscanf(parfile, "Probability of time-critical request: %lf\n", &probtmcrit) != 1) {
         fprintf(stderr, "Error reading 'time-critical request probability'\n");
         exit(0);
      }
      if ((probtmcrit < 0) || (probtmcrit > 1)) {
         fprintf(stderr, "Invalid value for 'time-critical request probability' - %f\n", probtmcrit);
         exit(0);
      }
      fprintf (outputfile, "Probability of time-critical request: %f\n", probtmcrit);
      gen->probtmcrit = probtmcrit;

      if (fscanf(parfile, "Probability of time-limited request: %lf\n", &probtmlim) != 1) {
         fprintf(stderr, "Error reading 'time-limited request probability'\n");
         exit(0);
      }
      if ((probtmlim < 0) || (probtmlim > 1)) {
         fprintf(stderr, "Invalid value for 'time-limited request probability' - %f\n", probtmlim);
         exit(0);
      }
      fprintf (outputfile, "Probability of time-limited request: %f\n", probtmlim);
      gen->probtmlim = probtmlim;

      mult = (double) 1;
      fscanf(parfile, "Time-limited think times (in milliseconds)\n");
      fprintf (outputfile, "Time-limited think times (in milliseconds)\n");
      synthio_read_distr(parfile, &gen->tmlimit, mult);

      fscanf(parfile, "General inter-arrival times (in milliseconds)\n");
      fprintf(outputfile, "General inter-arrival times (in milliseconds)\n");
      synthio_read_distr(parfile, &gen->genintr, mult);

      fscanf(parfile, "Sequential inter-arrival times (in milliseconds)\n");
      fprintf(outputfile, "Sequential inter-arrival times (in milliseconds)\n");
      synthio_read_distr(parfile, &gen->seqintr, mult);

      fscanf(parfile, "Local inter-arrival times (in milliseconds)\n");
      fprintf(outputfile, "Local inter-arrival times (in milliseconds)\n");
      synthio_read_distr(parfile, &gen->locintr, mult);

      fscanf(parfile, "Local distances (in blocks)\n");
      fprintf(outputfile, "Local distances (in blocks)\n");
      synthio_read_distr(parfile, &gen->locdist, mult);

      fscanf(parfile, "Sizes (in blocks)\n");
      fprintf(outputfile, "Sizes (in blocks)\n");
      synthio_read_distr(parfile, &gen->sizedist, mult);

      gen->number = i;
      i++;
      descs--;
      procp->space = (char *) gen;
      for (; descs > 0; descs--) {
	 procp = pf_new_process();
	 synthio_gencopy(gen, &synthio_gens[i]);
	 procp->space = (char *) &synthio_gens[i];
	 synthio_gens[i].number = i;
	 i++;
      }
   }
}
