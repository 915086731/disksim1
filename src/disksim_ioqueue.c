
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
#include "disksim_ioqueue.h"
#include "disksim_iosim.h"
#include "disksim_stat.h"

double lastphystime = 0.0;
double vscan_value = 0.2;
int global_surfoverforw = FALSE;

int priority_mix = 1;

int force_absolute_fcfs = FALSE;

int (*enablement)() = NULL;

#define READY_TO_GO(iobufptr) ((iobufptr->state == WAITING) && ((enablement == NULL) || ((*enablement)(iobufptr->iolist))))

/* Request scheduling algorithms */

#define MINSCHED	1
#define FCFS            1
#define ELEVATOR_LBN    2
#define CYCLE_LBN       3
#define SSTF_LBN        4
#define ELEVATOR_CYL    5
#define CYCLE_CYL       6
#define SSTF_CYL        7
#define SPTF_OPT        8
#define SPCTF_OPT       9
#define SATF_OPT        10
#define WPTF_OPT        11
#define WPCTF_OPT       12
#define WATF_OPT        13
#define ASPTF_OPT       14
#define ASPCTF_OPT      15
#define ASATF_OPT       16
#define VSCAN_LBN       17
#define VSCAN_CYL       18
#define PRI_VSCAN_LBN   19
#define PRI_ASPTF_OPT   20
#define PRI_ASPCTF_OPT  21
#define MAXSCHED        21

/* Sequential Stream Schemes */

#define IOQUEUE_CONCAT_READS		1
#define IOQUEUE_CONCAT_WRITES		2
#define IOQUEUE_CONCAT_BOTH		3
#define IOQUEUE_SEQSTREAM_READS		4
#define IOQUEUE_SEQSTREAM_WRITES	8
#define IOQUEUE_SEQSTREAM_EITHER	16

/* Sub queue identifications */

#define IOQUEUE_BASE		1
#define IOQUEUE_TIMEOUT		2
#define IOQUEUE_PRIORITY	3

/* Buffer states while in ioqueue */

#define WAITING		0
#define PENDING		1

/* Priority schemes */

#define ALLEQUAL	0
#define TWOQUEUE	1

/* Timeout schemes */

#define NOTIMEOUT	0
#define BASETIMEOUT	1
#define HALFTIMEOUT	2

/* Directions of movement for scanning policies */

#define ASC     1
#define DESC    2

char *statdesc_accstats		=	"Physical access time";
char *statdesc_qtimestats	=	"Queue time";
char *statdesc_outtimestats	=	"Response time";
char *statdesc_intarrstats	=	"Inter-arrival time";
char *statdesc_readintarrstats	=	"Read inter-arrival";
char *statdesc_writeintarrstats	=	"Write inter-arrival";
char *statdesc_idlestats	=	"Idle period length";
char *statdesc_infopenalty	=	"Sub-optimal mapping penalty";
char *statdesc_reqsizestats	=	"Request size";
char *statdesc_readsizestats	=	"Read request size";
char *statdesc_writesizestats	=	"Write request size";
char *statdesc_instqueuelen	=	"Instantaneous queue length";

typedef struct iob {
   double    starttime;
   int       state;
   struct iob *next;
   struct iob *prev;
   int       totalsize;
   int       blkno;
   int       flags;
   union {
      struct {
 	 int       waittime;
         ioreq_event *concat;
      } pend;
      double time;
   } iob_un;
   ioreq_event *iolist;
   int       reqcnt;
   int       cylinder;
   int       surface;
   int       opid;
} iobuf;

struct ioq;

typedef struct subq {
   struct ioq *	bigqueue;
   int		sched_alg;
   int		surfoverforw;
   iobuf *	list;
   iobuf *	current;
   int		prior;
   int		dir;
   int		vscan_cyls;
   u_int	lastblkno;
   int		lastsurface;
   int		lastcylno;
   int		optcylno;
   int		optsurface;
   int		sstfupdown;
   int		sstfupdowncnt;
   int		numreads;
   int		numwrites;
   int		switches;
   int		maxqlen;
   double	lastalt;
   int		listlen;
   int		iobufcnt;
   int		maxlistlen;
   double	runlistlen;
   int		readlen;
   int		maxreadlen;
   double	runreadlen;
   int		maxwritelen;
   int		numcomplete;
   int		reqoutstanding;
   int		numoutstanding;
   int		maxoutstanding;
   double	runoutstanding;
   statgen	outtimestats;
   statgen	critreadstats;
   statgen	critwritestats;
   statgen	nocritreadstats;
   statgen	nocritwritestats;
   statgen	qtimestats;
   statgen	accstats;
   statgen	instqueuelen;
   statgen	infopenalty;
} subqueue;

typedef struct ioq {
   subqueue	base;
   subqueue	timeout;
   subqueue	priority;
   int		devno;
   void		(*idlework)();
   void	*	idleworkparam;
   int		idledelay;
   timer_event *idledetect;
   int		(*concatok)();
   void *	concatokparam;
   int		concatmax;
   int		comboverlaps;
   int		seqscheme;
   int 		seqstreamdiff;
   int		to_scheme;
   int		to_time;
   int		pri_scheme;
   int		cylmaptype;
   double	writedelay;
   double	readdelay;
   int		sectpercyl;
   int		lastsubqueue;
   int		timeouts;
   int		timeoutreads;
   int		halfouts;
   int		halfoutreads;
   double       idlestart;
   double	lastarr;
   double	lastread;
   double	lastwrite;
   statgen	idlestats;
   statgen	intarrstats;
   statgen	readintarrstats;
   statgen	writeintarrstats;
   statgen	reqsizestats;
   statgen	readsizestats;
   statgen	writesizestats;
   int		maxlistlen;
   int		maxqlen;
   int		maxoutstanding;
   int		maxreadlen;
   int		maxwritelen;
   int		overlapscombed;
   int		readoverlapscombed;
   int		seqblkno;
   int		seqflags;
   int		seqreads;
   int		seqwrites;
   int		printqueuestats;
   int		printcritstats;
   int		printidlestats;
   int		printintarrstats;
   int		printsizestats;
} ioqueue;


void ioqueue_print_contents(queue)
ioqueue *queue;
{
   int i;
   iobuf *tmp;

   tmp = queue->base.list->next;
   fprintf (outputfile, "Contents of base queue: listlen %d\n", queue->base.listlen);
   for (i = 0; i < queue->base.iobufcnt; i++) {
      fprintf(outputfile, "State: %d, blkno: %d\n", tmp->state, tmp->blkno);
      tmp = tmp->next;
   }

   tmp = queue->timeout.list->next;
   fprintf(outputfile, "Contents of timeout queue: listlen %d\n", queue->timeout.listlen);
   for (i = 0; i < queue->timeout.iobufcnt; i++) {
      fprintf(outputfile, "State: %d, blkno: %d\n", tmp->state, tmp->blkno);
      tmp = tmp->next;
   }

   tmp = queue->priority.list->next;
   fprintf(outputfile, "Contents of priority queue: listlen %d\n", queue->priority.listlen);
   for (i = 0; i < queue->priority.iobufcnt; i++) {
      fprintf(outputfile, "State: %d, blkno: %d\n", tmp->state, tmp->blkno);
      tmp = tmp->next;
   }
}


void ioqueue_print_subqueue_state(queue)
subqueue *queue;
{
   int i;
   iobuf *tmp;

   tmp = queue->list->next;
   fprintf(outputfile, "\nContents of subqueue: listlen %d\n", queue->listlen);
   fprintf(outputfile, "Subqueue state: lastblkno %d, lastcylno %d, lastsurface %d, dir %d\n", queue->lastblkno, queue->lastcylno, queue->lastsurface, queue->dir);
   for (i = 0; i < queue->iobufcnt; i++) {
      fprintf(outputfile, "State: %d, blkno: %d, cylno %d, surface %d\n", tmp->state, tmp->blkno, tmp->cylinder, tmp->surface);
      tmp = tmp->next;
   }
}


int ioqueue_get_number_in_queue(queue)
ioqueue *queue;
{
   return(queue->base.listlen + queue->timeout.listlen + queue->priority.listlen);
}


int ioqueue_get_number_pending(queue)
ioqueue *queue;
{
   int numpend = 0;

   numpend += queue->base.listlen - queue->base.numoutstanding;
   numpend += queue->timeout.listlen - queue->timeout.numoutstanding;
   numpend += queue->priority.listlen - queue->priority.numoutstanding;
   return(numpend);
}


int ioqueue_get_reqoutstanding(queue)
ioqueue *queue;
{
   int numout = 0;

   numout += queue->base.reqoutstanding;
   numout += queue->timeout.reqoutstanding;
   numout += queue->priority.reqoutstanding;
   return(numout);
}


int ioqueue_get_number_of_requests(queue)
ioqueue *queue;
{
   int numreqs = 0;

   numreqs += queue->base.numcomplete + queue->base.listlen;
   numreqs += queue->timeout.numcomplete + queue->timeout.listlen;
   numreqs += queue->priority.numcomplete + queue->priority.listlen;
   return(numreqs);
}


int ioqueue_get_number_of_requests_initiated(queue)
ioqueue *queue;
{
   int numreqs = 0;

   numreqs += queue->base.numcomplete + queue->base.numoutstanding;
   numreqs += queue->timeout.numcomplete + queue->timeout.numoutstanding;
   numreqs += queue->priority.numcomplete + queue->priority.numoutstanding;
   return(numreqs);
}


int ioqueue_get_dist(queue, blkno)
ioqueue *queue;
int blkno;
{
   int lastblkno;

   if (queue->lastsubqueue == IOQUEUE_BASE) {
      lastblkno = queue->base.lastblkno;
   } else if (queue->lastsubqueue == IOQUEUE_TIMEOUT) {
      lastblkno = queue->timeout.lastblkno;
   } else if (queue->lastsubqueue == IOQUEUE_PRIORITY) {
      lastblkno = queue->priority.lastblkno;
   } else {
      fprintf(stderr, "Unknown queue identification at ioqueue_get_dist - %d\n", queue->lastsubqueue);
      exit(0);
   }
   return(abs(blkno - lastblkno));
}


void ioqueue_get_cylinder_mapping(queue, curr, blkno, cylptr, surfptr, cylmaptype)
ioqueue *queue;
iobuf *curr;
int blkno;
int *cylptr;
int *surfptr;
int cylmaptype;
{
   cylmaptype = (cylmaptype == -1) ? queue->cylmaptype : cylmaptype;
   switch (cylmaptype) {
      case MAP_NONE:
         *cylptr = 0;
         *surfptr = 0;
         break;

      case MAP_IGNORESPARING:
      case MAP_ZONEONLY:
      case MAP_ADDSLIPS:
      case MAP_FULL:
	 disk_get_mapping(cylmaptype, curr->iolist->devno, blkno, cylptr, surfptr, NULL);
         break;

      case MAP_FROMTRACE:
	/* this yucky cast is being allowed for convenience.  We know it */
	/* can't really hurt us...                                       */
         *cylptr = (int) curr->iolist->buf;
	 *surfptr = 0;
         break;

      case MAP_AVGCYLMAP:
         *cylptr = blkno / queue->sectpercyl;
         *surfptr = 0;
         break;

      default:
         fprintf(stderr, "Unknown cylinder mapping type: %d\n", queue->cylmaptype);
         exit(0);
   }
}


void ioqueue_set_concatok_function(queue, concatok, concatokparam)
ioqueue *queue;
int (*concatok)();
void *concatokparam;
{
   queue->concatok = concatok;
   queue->concatokparam = concatokparam;
}


void ioqueue_set_idlework_function(queue, idlework, idleworkparam, idledelay)
ioqueue *queue;
void (*idlework)();
void *idleworkparam;
double idledelay;
{
   queue->idlework = idlework;
   queue->idleworkparam = idleworkparam;
   queue->idledelay = idledelay;
   queue->idledetect = NULL;
}


void ioqueue_idledetected(timereq)
timer_event *timereq;
{
   ioqueue *queue = (ioqueue *) timereq->ptr;

   queue->idledetect = NULL;
   queue->idlework(queue->idleworkparam, queue->devno);
   addtoextraq(timereq);
}


void ioqueue_reset_idledetecter(queue, timechange)
ioqueue *queue;
int timechange;
{
   timer_event *idledetect = queue->idledetect;
   int listlen = queue->base.listlen + queue->timeout.listlen + queue->priority.listlen;

   if ((listlen) || ((idledetect) && (!timechange))) {
      return;
   }
   if (idledetect) {
      if (!(removefromintq(idledetect))) {
	 fprintf(stderr, "existing idledetect event not on intq in ioqueue_reset_idledetecter\n");
	 exit(0);
      }
   } else {
      idledetect = (timer_event *) getfromextraq();
      idledetect->type = TIMER_EXPIRED;
      idledetect->func = ioqueue_idledetected;
      idledetect->ptr = queue;
      queue->idledetect = idledetect;
   }
   idledetect->time = simtime + queue->idledelay;
   addtointq(idledetect);
}


void ioqueue_update_arrival_stats(queue, curr)
ioqueue *queue;
ioreq_event *curr;
{
   double tdiff;

   if (queue->printintarrstats) {
      tdiff = simtime - queue->lastarr;
      queue->lastarr = simtime;
      stat_update(&queue->intarrstats, tdiff);
      if (curr->flags & READ) {
         tdiff = simtime - queue->lastread;
         queue->lastread = simtime;
	 stat_update(&queue->readintarrstats, tdiff);
      } else {
         tdiff = simtime - queue->lastwrite;
         queue->lastwrite = simtime;
	 stat_update(&queue->writeintarrstats, tdiff);
      }
   }
   if (queue->printsizestats) {
      stat_update(&queue->reqsizestats, (double) curr->bcount);
      if (curr->flags & READ) {
         stat_update(&queue->readsizestats, (double) curr->bcount);
      } else {
         stat_update(&queue->writesizestats, (double) curr->bcount);
      }
   }
   if (curr->blkno == queue->seqblkno) {
      if ((curr->flags & READ) && (queue->seqflags & READ)) {
	 queue->seqreads++;
      } else if (!(curr->flags & READ) && !(queue->seqflags & READ)) {
	 queue->seqwrites++;
      }
   }
   queue->seqblkno = curr->blkno + curr->bcount;
   queue->seqflags = curr->flags;
}


void ioqueue_update_queue_statistics(queue)
subqueue *queue;
{
   double tdiff = simtime - queue->lastalt;

   queue->runlistlen += tdiff * queue->listlen;
   queue->runreadlen += tdiff * queue->readlen;
   if (queue->readlen > queue->maxreadlen) {
      queue->maxreadlen = queue->readlen;
   }
   if ((queue->listlen - queue->readlen) > queue->maxwritelen) {
      queue->maxwritelen = queue->listlen - queue->readlen;
   }
   queue->runoutstanding += tdiff * queue->numoutstanding;
   if (queue->numoutstanding > queue->maxoutstanding) {
      queue->maxoutstanding = queue->numoutstanding;
   }
   if (queue->listlen > queue->maxlistlen) {
      queue->maxlistlen = queue->listlen;
   }
   if ((queue->listlen - queue->numoutstanding) > queue->maxqlen) {
      queue->maxqlen = queue->listlen - queue->numoutstanding;
   }
   queue->lastalt = simtime;
}


void ioqueue_remove_from_subqueue(queue, tmp)
subqueue *queue;
iobuf *tmp;
{
   if ((queue->list == tmp) && (tmp == tmp->next)) {
      queue->list = NULL;
   } else {
      tmp->next->prev = tmp->prev;
      tmp->prev->next = tmp->next;
      if (queue->list == tmp) {
         queue->list = tmp->prev;
      }
   }
   tmp->next = NULL;
   tmp->prev = NULL;
}


/* queue->list not NULL when entered */

void ioqueue_insert_fcfs_to_queue(queue, temp)
subqueue *queue;
iobuf *temp;
{
   temp->next = queue->list->next;
   temp->prev = queue->list;
   temp->next->prev = temp;
   temp->prev->next = temp;
   queue->list = temp;
}


/* queue->list not NULL when entered */

void ioqueue_insert_ordered_to_queue(queue, temp)
subqueue *queue;
iobuf *temp;
{
   iobuf *head;
   iobuf *run;

   head = queue->list->next;

   if ((temp->cylinder < head->cylinder)       ||
       ((temp->cylinder == head->cylinder) && 
        (temp->surface < head->surface))       ||
       ((temp->cylinder == head->cylinder) && 
        (temp->surface == head->surface)   && 
        (temp->blkno < head->blkno))) {
      queue->list->next = temp;
      temp->next = head;
      temp->prev = queue->list;
      head->prev = temp;

   } else if ((temp->cylinder > queue->list->cylinder)       ||
              ((temp->cylinder == queue->list->cylinder) && 
               (temp->surface > queue->list->surface))       ||
              ((temp->cylinder == queue->list->cylinder) && 
               (temp->surface == queue->list->surface)   && 
               (temp->blkno >= queue->list->blkno))) {
      temp->next = head;
      temp->prev = queue->list;
      head->prev = temp;
      queue->list->next = temp;
      queue->list = temp;
   } else {
      run = head;
      while ((temp->cylinder > run->next->cylinder)     ||
             ((temp->cylinder == run->next->cylinder)  && 
              (temp->surface > run->next->surface))     ||
             ((temp->cylinder == run->next->cylinder)  && 
              (temp->surface == run->next->surface)    && 
              (temp->blkno >= run->next->blkno))) {
         run = run->next;
      }
      temp->next = run->next;
      temp->prev = run;
      temp->next->prev = temp;
      run->next = temp;
   }
}


int ioqueue_check_concat(queue, req1, req2, concatmax)
subqueue *queue;
iobuf *req1;
iobuf *req2;
int concatmax;
{
   ioreq_event *tmp;
   int seqscheme = queue->bigqueue->seqscheme;
   int (*concatok)() = queue->bigqueue->concatok;
   void *concatokparam = queue->bigqueue->concatokparam;

   if (req1->next != req2) {
      fprintf(stderr, "Non-adjacent requests sent to ioqueue_check_concat\n");
      exit(0);
   }
   if (req1->flags & READ) {
      if (!(seqscheme & IOQUEUE_CONCAT_READS) || !(req2->flags & READ)) {
	 return(0);
      }
   } else {
      if (!(seqscheme & IOQUEUE_CONCAT_WRITES) || (req2->flags & READ)) {
	 return(0);
      }
   }
   if ((req1 == queue->list) || (req1 == req2) || !READY_TO_GO(req1) || !READY_TO_GO(req2) || ((req1->blkno + req1->totalsize) != req2->blkno) || ((req1->totalsize + req2->totalsize) > concatmax)) {
      return(0);
   }
   if (concatok) {
      if (concatok(concatokparam, req1->blkno, req1->totalsize, req2->blkno, req2->totalsize) == 0) {
	 return(0);
      }
   }
   tmp = req1->iolist;
   if (tmp == NULL) {
      req1->iolist = req2->iolist;
   } else {
      while (tmp->next) {
         tmp = tmp->next;
      }
      tmp->next = req2->iolist;
   }
   req1->next = req2->next;
   req1->next->prev = req1;
   req1->reqcnt += req2->reqcnt;
   req1->totalsize += req2->totalsize;
   if (queue->list == req2) {
      queue->list = req1;
   }
   queue->iobufcnt--;
   addtoextraq((event *) req2);
   return(1);
}


void ioqueue_insert_new_request(queue, temp)
subqueue *queue;
iobuf *temp;
{
   int concatmax;

   ioqueue_update_queue_statistics(queue);
   queue->iobufcnt++;
   if (queue->list == NULL) {
      queue->list = temp;
      temp->next = temp;
      temp->prev = temp;
   } else {
      if ((queue->sched_alg == FCFS) || (queue->sched_alg == PRI_VSCAN_LBN)) {
         ioqueue_insert_fcfs_to_queue(queue, temp);
      } else {
         ioqueue_insert_ordered_to_queue(queue, temp);
      }
   }
   concatmax = queue->bigqueue->concatmax;
   if (concatmax) {
      temp = temp->prev;
      if (!(ioqueue_check_concat(queue, temp, temp->next, concatmax))) {
	 temp = temp->next;
      }
      ioqueue_check_concat(queue, temp, temp->next, concatmax);
   }
   queue->listlen++;
   if (temp->flags & READ) {
      queue->readlen++;
      queue->numreads++;
   } else {
      queue->numwrites++;
   }
}


/* Returns 0 if request is sequential from a previous request (and detection */
/* enabled).  Otherwise returns 1.				             */
/* Assuming no command queueing.					     */

int ioqueue_seqstream_head(queue, listhead, current)
ioqueue *queue;
iobuf *listhead;
iobuf *current;
{
   int idiff;
   int ret = 0;

   if (!(queue->seqscheme & (IOQUEUE_SEQSTREAM_READS|IOQUEUE_SEQSTREAM_WRITES|IOQUEUE_SEQSTREAM_EITHER))) {
      return(1);
   }
   idiff = queue->seqstreamdiff;
   if ((current == listhead) || !READY_TO_GO(current->prev)) {
      ret = 1;
   }
   if (ret == 0) {
      if (current->flags & READ) {
         if (current->prev->flags & READ) {
	    if (!(queue->seqscheme & (IOQUEUE_SEQSTREAM_READS|IOQUEUE_SEQSTREAM_EITHER))) {
	       ret = 1;
	    }
         } else {
	    if (!(queue->seqscheme & IOQUEUE_SEQSTREAM_EITHER)) {
	       ret = 1;
	    }
         }
	 if ((current->prev->blkno != current->blkno) && (((current->prev->blkno + current->prev->totalsize + idiff) < current->blkno))) {
	    ret = 1;
	 }
      } else {
         if (current->prev->flags & READ) {
	    if (!(queue->seqscheme & IOQUEUE_SEQSTREAM_EITHER)) {
	       ret = 1;
	    }
         } else {
	    if (!(queue->seqscheme & (IOQUEUE_SEQSTREAM_WRITES|IOQUEUE_SEQSTREAM_EITHER))) {
	       ret = 1;
	    }
         }
	 if ((current->prev->blkno != current->blkno) && ((current->prev->blkno + current->prev->totalsize) != current->blkno)) {
	    ret = 1;
	 }
      }
   }
   return(ret);
}


/* Queue contains >= 2 items when called */

iobuf * ioqueue_get_request_from_fcfs_queue(queue)
subqueue *queue;
{
   iobuf *tmp;
   int i;

   tmp = queue->list->next;
   for (i = 0; i < queue->iobufcnt; i++) {
      if (force_absolute_fcfs) {
         if (tmp->state == WAITING) {
            if (READY_TO_GO(tmp)) {
               return(tmp);
            } else {
               return(NULL);
            }
         }
      } else {
         if (READY_TO_GO(tmp)) {
            return(tmp);
         }
      }
      tmp = tmp->next;
   }
   return(NULL);
}


/* Queue contains >= 2 items when called */

int ioqueue_get_priority_level(req)
iobuf *req;
{
   int ret = 0;

   if (priority_mix == 1) {
      if ((req->flags & TIME_CRITICAL) || (req->flags & READ)) {
	 ret = 1;
      }
   } else if (priority_mix == 2) {
      if (req->flags & TIME_CRITICAL) {
	 ret = 2;
      } else if (req->flags & READ) {
	 ret = 1;
      }
   } else if (priority_mix == 3) {
      if (req->flags & READ) {
	 ret = 2;
      } else if (req->flags & TIME_CRITICAL) {
	 ret = 1;
      }
   } else if (priority_mix == 4) {
      if (req->flags & TIME_CRITICAL) {
	 ret = (req->flags & READ) ? 2 : 3;
      } else {
	 ret = (req->flags & READ) ? 1 : 0;
      }
   } else if (priority_mix == 5) {
      if (req->flags & READ) {
	 ret = 1;
      }
   } else if (priority_mix == 6) {
      if (req->flags & TIME_CRITICAL) {
	 ret = 1;
      }
   }
   return(ret);
}


iobuf * ioqueue_get_request_from_pri_lbn_vscan_queue(queue, numlbns, vscan_value)
subqueue *queue;
int numlbns;
int vscan_value;
{
   int schedalg;
   int priority_factor;
   int age_factor;
   int curr_effpri, best_effpri;
   iobuf *tmp;
   iobuf *ret = NULL;
   int i;

   schedalg = queue->bigqueue->timeout.sched_alg;
   priority_factor = queue->bigqueue->priority.sched_alg;
   age_factor = queue->bigqueue->to_time;
   if (schedalg == ELEVATOR_LBN) {
      vscan_value = numlbns;
   } else if (schedalg == SSTF_LBN) {
      vscan_value = 0;
   }
   tmp = queue->list->next;
   for (i = 0; i < queue->iobufcnt; i++) {
      if (READY_TO_GO(tmp)) {
	 curr_effpri = tmp->blkno - queue->lastblkno;
	 if (schedalg == CYCLE_LBN) {
	    if (curr_effpri < 0) {
	       curr_effpri += numlbns;
	    }
	 } else if (curr_effpri) {
	    if (curr_effpri < 0) {
	       curr_effpri = -curr_effpri;
	       if (queue->dir == ASC) {
		  curr_effpri += vscan_value;
	       }
	    } else {
	       if (queue->dir == DESC) {
		  curr_effpri += vscan_value;
	       }
	    }
	 }
	 curr_effpri = numlbns - curr_effpri;
	 curr_effpri += (priority_factor * numlbns * ioqueue_get_priority_level(tmp)) / 100;
	 curr_effpri += (age_factor * (int) (simtime - tmp->starttime)) / 1280;
	 if ((ret == NULL) || (curr_effpri > best_effpri)) {
	    ret = tmp;
	    best_effpri = curr_effpri;
	 }
      }
      tmp = tmp->next;
   }
   return(ret);
}


/* Queue contains >= 2 items when called */

iobuf * ioqueue_get_request_from_lbn_scan_queue(queue)
subqueue *queue;
{
   iobuf *temp;
   iobuf *stop;

   temp = queue->list->next;
   stop = temp;
   if (queue->lastblkno > temp->blkno) {
      while ((temp->next != stop) && (queue->lastblkno > temp->next->blkno)) {
         temp = temp->next;
      }
      temp = temp->next;
   }
   stop = temp;
   while ((temp->next != stop) && !READY_TO_GO(temp)) {
      temp = temp->next;
   }
   if ((temp->next == stop) && !READY_TO_GO(temp)) {
      temp = NULL;
   }
   return(temp);
}


/* Queue contains >= 2 items when called */

iobuf * ioqueue_get_request_from_lbn_vscan_queue(queue, value)
subqueue *queue;
int value;
{
   iobuf *temp;
   iobuf *head;
   iobuf *top = NULL;
   iobuf *bottom = NULL;
   int diff1, diff2;
   int tmpdir;
   iobuf *bestbottom;

   tmpdir = queue->dir;
   temp = queue->list->next;
   head = queue->list->next;

   while ((temp->next != head) && (queue->lastblkno > temp->next->blkno)) {
      temp = temp->next;
   }
   if (temp->blkno < queue->lastblkno) {
      bottom = temp;
      while ((bottom != head) && !READY_TO_GO(bottom)) {
         bottom = bottom->prev;
      }
      if ((bottom == head) && !READY_TO_GO(bottom)) {
	 bottom = NULL;
      } else {
         bestbottom = bottom;
         while ((bottom != head) && ((bottom->prev->blkno == bestbottom->blkno) || (!(ioqueue_seqstream_head(queue->bigqueue, head, bottom))))) {
	    bottom = bottom->prev;
	    bestbottom = (READY_TO_GO(bottom)) ? bottom : bestbottom;
         }
         bottom = bestbottom;
      }
   }
   if (temp->next != head) {
      if (temp->blkno >= queue->lastblkno) {
	 top = temp;
      } else {
         top = temp->next;
      }
      while ((top->next != head) && !READY_TO_GO(top)) {
         top = top->next;
      }
   }
   if ((top == NULL) || !READY_TO_GO(top) || (!(ioqueue_seqstream_head(queue->bigqueue, head, top)))) {
      temp = bottom;
   } else if ((bottom == NULL) || !READY_TO_GO(bottom)) {
      temp = top;
   } else {
      diff1 = diff(top->blkno, queue->lastblkno);
      diff2 = diff(queue->lastblkno, bottom->blkno);
      if (queue->dir == ASC) {
	 diff2 = (diff2) ? (diff2 + value) : diff2;
      } else {
	 diff1 = (diff1) ? (diff1 + value) : diff1;
      }
      if (diff1 < diff2) {
         temp = top;
      } else if ((diff1 > diff2) || (queue->sstfupdown)) {
         temp = bottom;
      } else {
	 temp = top;
      }
      if (diff1 == diff2) {
	 queue->sstfupdown = (queue->sstfupdown) ? 0 : 1;
	 queue->sstfupdowncnt++;
      }
   }
   if (temp->blkno != queue->lastblkno) {
      queue->dir = (temp->blkno > queue->lastblkno) ? ASC : DESC;
   }
   top = (temp == top) ? bottom : top;
   if ((top) && READY_TO_GO(top)) {
      int cylno1, cylno2;
      int surface1, surface2;

      ioqueue_get_cylinder_mapping(queue->bigqueue, temp, temp->blkno, &cylno1, &surface1, MAP_FULL);
      ioqueue_get_cylinder_mapping(queue->bigqueue, top, top->blkno, &cylno2, &surface2, MAP_FULL);
      diff1 = abs(cylno1 - queue->optcylno);
      diff2 = abs(cylno2 - queue->optcylno);
      if (((tmpdir == ASC) && (temp != bottom)) || ((tmpdir == DESC) && (temp == bottom))) {
	 if (diff2) {
	    diff2 += (int) (vscan_value * (double) disk_get_numcyls(temp->iolist->devno));
	 }
      } else if (diff1) {
	 if (diff1) {
	    diff1 += (int) (vscan_value * (double) disk_get_numcyls(temp->iolist->devno));
	 }
      }
      if (diff1 > diff2) {
         stat_update(&queue->infopenalty, (double) (diff1 - diff2));
/*
fprintf (outputfile, "diff1 %d, diff2 %d, diffdiff %d\n", diff1, diff2, (diff1 - diff2));
*/
      } else if ((diff1 == diff2) && (diff1 == 0)) {
         if ((queue->optsurface == surface2) && (surface1 != surface2)) {
            stat_update(&queue->infopenalty, (double) 0);
         }
      }
   }
   return(temp);
}


/* Queue contains >= 2 items when called */

/* Returns pointer to "best" request on queue->lastcylno, or first request
   on next available forward cylinder, or head (in priority order). */

iobuf *ioqueue_identify_request_on_cylinder(queue)
subqueue *queue;
{
   iobuf *temp;
   iobuf *head;
   iobuf *cylstop;
   iobuf *surfstop;

   int lastcylno;
   int lastsurface;
   int lastblkno;

   lastcylno = queue->lastcylno;
   lastsurface = queue->lastsurface;
   lastblkno = queue->lastblkno;

   head = queue->list->next;
   temp = head;

   if ((head->cylinder < lastcylno) && (lastcylno <= queue->list->cylinder)) {
      while ((temp->next != head) && (lastcylno > temp->next->cylinder)) {
         temp = temp->next;
      }
      temp = temp->next;
   }

   if (temp->cylinder != lastcylno) {
      return(temp);
   }

   cylstop = temp;      /* first req on cylinder */

   while ((temp->next != head) && (lastcylno == temp->next->cylinder) && (lastsurface > temp->surface)) {
      temp = temp->next;
   }

   if (lastsurface > temp->surface) {
      temp = cylstop;   /* drop out and try from first req on cylinder */

   } else if (!queue->surfoverforw) {
      while ((temp->next != head) && (lastcylno == temp->next->cylinder) && (!READY_TO_GO(temp) || (temp->blkno < lastblkno))) {
         temp = temp->next;
      }
      if (READY_TO_GO(temp) && (temp->blkno >= lastblkno)) {
        return(temp);
      } else {
         temp = cylstop;   /* drop out and try from first req on cylinder */
      }

   } else {    /* surfoverforw */

      surfstop = temp;     /* first req on surface */

      while ((temp->next != head) && (lastcylno == temp->next->cylinder) && (lastsurface == temp->next->surface) && (!READY_TO_GO(temp) || (temp->blkno < lastblkno))) {
         temp = temp->next;
      }
      if (READY_TO_GO(temp) && (temp->blkno >= lastblkno) && (temp->surface == lastsurface)) {
         return(temp);
      }

      temp = surfstop;

      while ((temp->next != head) && (lastcylno == temp->next->cylinder) && !READY_TO_GO(temp)) {
         temp = temp->next;
      }
      if (READY_TO_GO(temp)) {
         return(temp);
      }

      temp = cylstop;   /* drop out and try from first req on cylinder */
   }

   /* drop out recovery point */

   if (!READY_TO_GO(temp)) {
      while (!READY_TO_GO(temp) && (temp->next != head) && (temp->next->cylinder == lastcylno)) {
         temp = temp->next;
      }
      temp = temp->next;
   }
   return(temp);
}


/* Queue contains >= 2 items when called */

iobuf * ioqueue_get_request_from_cyl_scan_queue(queue)
subqueue *queue;
{
   iobuf *temp;
   iobuf *stop;

   temp = queue->list->next;
   if (queue->lastcylno >= temp->cylinder) {
      temp = ioqueue_identify_request_on_cylinder(queue);
   }
   stop = temp;
   while (!READY_TO_GO(temp) && (temp->next != stop)) {
      temp = temp->next;
   }
   if ((temp->next == stop) && !READY_TO_GO(temp)) {
      temp = NULL;
   }
   return(temp);
}


/* Queue contains >= 2 items when called */

iobuf * ioqueue_get_request_from_cyl_vscan_queue(queue, value)
subqueue *queue;
int value;
{
   iobuf *temp;
   iobuf *head;
   iobuf *top = NULL;
   iobuf *bottom = NULL;
   iobuf *bottom2 = NULL;
   iobuf *bestone;
   int diff1, diff2, diff3;
   int tmpdir;
   int cylno1, cylno2, cylno3;
   int surface1, surface2, surface3;

   tmpdir = queue->dir;
   temp = queue->list->next;
   head = queue->list->next;

   bestone = ioqueue_identify_request_on_cylinder(queue);
   if (!READY_TO_GO(bestone) || (bestone->cylinder != queue->lastcylno)) {
      bestone = NULL;
   }
   while ((temp->next != head) && (queue->lastblkno > temp->next->blkno)) {
      temp = temp->next;
   }
   if (temp->blkno < queue->lastblkno) {
      bottom = temp;
      while ((bottom != head) && !READY_TO_GO(bottom)) {
         bottom = bottom->prev;
      }
      bottom2 = temp;
      if (READY_TO_GO(bottom) && (!bestone)) {
         while ((bottom != head) && (bottom->prev->cylinder == bottom->cylinder)) {
	    bottom = bottom->prev;
         }
         while ((bottom != temp) && !READY_TO_GO(bottom)) {
	    bottom = bottom->next;
         }
      }
   }
   if (temp->next != head) {
      top = (temp->blkno >= queue->lastblkno) ? temp : temp->next;
      while ((top->next != head) && !READY_TO_GO(top)) {
         top = top->next;
      }
   }
   if (!bestone) {
      if ((top == NULL) || !READY_TO_GO(top)) {
         temp = bottom;
      } else if ((bottom == NULL) || !READY_TO_GO(bottom)) {
         temp = top;
      } else {
         diff1 = diff(top->cylinder, queue->lastcylno);
         diff2 = diff(queue->lastcylno, bottom->cylinder);
         if (queue->dir == ASC) {
	    diff2 += value;
         } else {
	    diff1 += value;
         }
         if (diff1 < diff2) {
	    temp = top;
         } else if ((diff1 > diff2) || (queue->sstfupdown)) {
            temp = bottom;
         } else {
	    temp = top;
         }
         if (diff1 == diff2) {
	    queue->sstfupdown = (queue->sstfupdown) ? 0 : 1;
	    queue->sstfupdowncnt++;
         }
      }
   } else {
      temp = bestone;
   }
   if (temp->cylinder != queue->lastcylno) {
      queue->dir = (temp->cylinder > queue->lastcylno) ? ASC : DESC;
   }
   ioqueue_get_cylinder_mapping(queue->bigqueue, temp, temp->blkno, &cylno1, &surface1, MAP_FULL);
   diff1 = abs(cylno1 - queue->optcylno);
   if ((top == NULL) || !READY_TO_GO(top)) {
      top = temp;
   }
   ioqueue_get_cylinder_mapping(queue->bigqueue, top, top->blkno, &cylno2, &surface2, MAP_FULL);
   diff2 = abs(cylno2 - queue->optcylno);
   if ((bottom2 == NULL) || !READY_TO_GO(bottom2)) {
      bottom2 = temp;
   }
   ioqueue_get_cylinder_mapping(queue->bigqueue, bottom2, bottom2->blkno, &cylno3, &surface3, MAP_FULL);
   diff3 = abs(cylno3 - queue->optcylno);
   if (tmpdir == ASC) {
      diff3 = (diff3) ? (diff3 + value) : diff3;
      diff1 = (temp->cylinder < queue->lastcylno) ? (diff1 + value) : diff1;
   } else {
      diff2 = (diff2) ? (diff2 + value) : diff2;
      diff1 = (temp->cylinder > queue->lastcylno) ? (diff1 + value) : diff1;
   }
   if ((diff3 < diff2) || ((!diff3) && (surface3 == queue->optsurface))) {
      surface2 = surface3;
      diff2 = diff3;
   }
   if (diff1 > diff2) {
      stat_update(&queue->infopenalty, (double) (diff1 - diff2));
/*
fprintf (outputfile, "diff1 %d, diff2 %d, diffdiff %d, value %d\n", diff1, diff2, (diff1 - diff2), value);
*/
   } else if ((diff1 == diff2) && (diff1 == 0)) {
      if ((queue->optsurface == surface2) && (surface1 != surface2)) {
         stat_update(&queue->infopenalty, (double) 0);
      }
   }
   return(temp);
}


/* Queue contains >= 2 items when called */

iobuf *ioqueue_get_request_from_opt_sptf_queue(queue, checkcache, ageweight, posonly)
subqueue *queue;
int checkcache;
int ageweight;
int posonly;
{
   int i;
   iobuf *temp;
   iobuf *best = NULL;
   ioreq_event *test;
   double mintime = 100000.0;
   double readdelay;
   double writedelay;
   double delay;
   double age;
   double weight;
   ioreq_event *tmp;

   ASSERT((ageweight >= 0) && (ageweight <= 3));
   readdelay = queue->bigqueue->readdelay;
   writedelay = queue->bigqueue->writedelay;
   weight = (double) queue->bigqueue->to_time;
   test = (ioreq_event *) getfromextraq();
   temp = queue->list->next;
   for (i=0; i<queue->listlen; i++) {
      if (READY_TO_GO(temp) && (ioqueue_seqstream_head(queue->bigqueue, queue->list->next, temp))) {
	 test->blkno = temp->blkno;
	 test->bcount = temp->totalsize;
         test->devno = temp->iolist->devno;
         test->flags = temp->flags;
	 delay = (temp->flags & READ) ? readdelay : writedelay;
	 test->time = simtime + delay;
	 if (ageweight) {
	    tmp = temp->iolist;
	    age = tmp->time;
	    while (tmp) {
	       if (tmp->time < age) {
		  age = tmp->time;
	       }
	       tmp = tmp->next;
	    }
	    age = simtime - age;
	 }
	 if ((ageweight == 2) || 
	     ((ageweight == 3) && 
	      (test->flags & (TIME_CRITICAL | TIME_LIMITED)))) {
	    delay -= age * weight * (double) 0.001;
         }
	 if (delay < mintime) {
	    if (posonly) {
	       delay += disk_get_servtime(test->devno, test, checkcache, (mintime - delay));
            } else {
	       delay += disk_get_acctime(test->devno, test, (mintime - delay));
	    }
	    if (ageweight == 1) {
	       delay *= (weight - age) / weight;
	    }
	    if (delay < mintime) {
	       best = temp;
	       mintime = delay;
	    }
	 }
      }
      temp = temp->next;
   }
   addtoextraq((event *) test);
/*
fprintf (outputfile, "Selected request: %f, cylno %d, blkno %d, read %d, devno %d\n", mintime, best->cylinder, best->blkno, (best->flags & READ), best->iolist->devno);
*/
   return(best);
}


ioreq_event * ioqueue_show_next_request_from_subqueue(queue)
subqueue *queue;
{
   iobuf *temp;
   ioreq_event *ret;
   int tmpdir;

   tmpdir = queue->dir;
   if ((queue->iobufcnt - queue->reqoutstanding) == 0) {
      return(NULL);
   } else if (queue->iobufcnt == 1) {
      if (READY_TO_GO(queue->list)) {
         temp = queue->list;
      } else {
         temp = NULL;
      }
   } else {
      if (queue->sched_alg == FCFS) {
         temp = ioqueue_get_request_from_fcfs_queue(queue);
      } else if (queue->sched_alg == PRI_VSCAN_LBN) {
         temp = ioqueue_get_request_from_pri_lbn_vscan_queue(queue, disk_get_number_of_blocks(queue->list->iolist->devno), queue->vscan_cyls);
      } else if (queue->sched_alg == ELEVATOR_LBN) {
         temp = ioqueue_get_request_from_lbn_vscan_queue(queue, disk_get_number_of_blocks(queue->list->iolist->devno));
      } else if (queue->sched_alg == CYCLE_LBN) {
         temp = ioqueue_get_request_from_lbn_scan_queue(queue);
      } else if (queue->sched_alg == SSTF_LBN) {
         temp = ioqueue_get_request_from_lbn_vscan_queue(queue, 0);
      } else if (queue->sched_alg == ELEVATOR_CYL) {
         temp = ioqueue_get_request_from_cyl_vscan_queue(queue, disk_get_numcyls(queue->list->iolist->devno));
      } else if (queue->sched_alg == CYCLE_CYL) {
         temp = ioqueue_get_request_from_cyl_scan_queue(queue);
      } else if (queue->sched_alg == SSTF_CYL) {
         temp = ioqueue_get_request_from_cyl_vscan_queue(queue, 0);
      } else if (queue->sched_alg == SPTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, FALSE, 0, TRUE);
      } else if (queue->sched_alg == SPCTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, TRUE, 0, TRUE);
      } else if (queue->sched_alg == SATF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, 0, 0, FALSE);
      } else if (queue->sched_alg == WPTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, FALSE, 1, TRUE);
      } else if (queue->sched_alg == WPCTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, TRUE, 1, TRUE);
      } else if (queue->sched_alg == WATF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, 0, 1, FALSE);
      } else if (queue->sched_alg == ASPTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, FALSE, 2, TRUE);
      } else if (queue->sched_alg == ASPCTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, TRUE, 2, TRUE);
      } else if (queue->sched_alg == PRI_ASPTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, FALSE, 3, TRUE);
      } else if (queue->sched_alg == PRI_ASPCTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, TRUE, 3, TRUE);
      } else if (queue->sched_alg == ASATF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, 0, 2, FALSE);
      } else if (queue->sched_alg == VSCAN_LBN) {
         temp = ioqueue_get_request_from_lbn_vscan_queue(queue, queue->vscan_cyls);
      } else if (queue->sched_alg == VSCAN_CYL) {
         temp = ioqueue_get_request_from_cyl_vscan_queue(queue, queue->vscan_cyls);
      }
   }
   queue->dir = tmpdir;
   if (temp == NULL) {
      return(NULL);
   } else if (!READY_TO_GO(temp)) {
      fprintf(stderr, "Selected request is !READY_TO_GO in show_next_request_from_subqueue\n");
      exit(0);
   }
   ASSERT(temp->iolist != NULL);
   if (temp->reqcnt == 1) {
      ret = temp->iolist;
   } else if (temp->iob_un.pend.concat) {
      ret = temp->iob_un.pend.concat;
   } else {
      ret = (ioreq_event *) getfromextraq();
      temp->iob_un.pend.concat = ret;
      ret->time = simtime;
      ret->type = temp->iolist->type;
      ret->next = NULL;
      ret->prev = NULL;
      ret->bcount = temp->totalsize;
      ret->blkno = temp->blkno;
      ret->flags = temp->flags;
      ret->busno = temp->iolist->busno;
      ret->slotno = temp->iolist->slotno;
      ret->devno = temp->iolist->devno;
      ret->opid = temp->opid;
      ret->buf = temp->iolist->buf;
   }
   return(ret);
}


ioreq_event * ioqueue_get_next_request_from_subqueue(queue)
subqueue *queue;
{
   iobuf *temp = NULL;
   ioreq_event *ret;

   ioqueue_update_queue_statistics(queue);
/*
   ioqueue_print_subqueue_state(queue);
*/
   if ((queue->iobufcnt - queue->reqoutstanding) == 0) {
      return(NULL);
   } else if (queue->iobufcnt == 1) {
      if (READY_TO_GO(queue->list)) {
         temp = queue->list;
         switch (queue->sched_alg) {
	    case ELEVATOR_LBN:
	    case PRI_VSCAN_LBN:
	    case VSCAN_LBN:
	       if (queue->lastblkno != temp->blkno) {
	          queue->dir = (queue->lastblkno < temp->blkno) ? ASC : DESC;
	       }
	       break;
   
	    case ELEVATOR_CYL:
	    case VSCAN_CYL:
	       if (queue->lastcylno != temp->cylinder) {
	          queue->dir = (queue->lastcylno < temp->cylinder) ? ASC : DESC;
	       }
	       break;
         }
      } else {                      /* !READY_TO_GO */
         temp = NULL;
      }
   } else {

      if (queue->sched_alg == FCFS) {
         temp = ioqueue_get_request_from_fcfs_queue(queue);
      } else if (queue->sched_alg == PRI_VSCAN_LBN) {
         temp = ioqueue_get_request_from_pri_lbn_vscan_queue(queue, disk_get_number_of_blocks(queue->list->iolist->devno), queue->vscan_cyls);
      } else if (queue->sched_alg == ELEVATOR_LBN) {
         temp = ioqueue_get_request_from_lbn_vscan_queue(queue, disk_get_number_of_blocks(queue->list->iolist->devno));
      } else if (queue->sched_alg == CYCLE_LBN) {
         temp = ioqueue_get_request_from_lbn_scan_queue(queue);
      } else if (queue->sched_alg == SSTF_LBN) {
         temp = ioqueue_get_request_from_lbn_vscan_queue(queue, 0);
      } else if (queue->sched_alg == ELEVATOR_CYL) {
         temp = ioqueue_get_request_from_cyl_vscan_queue(queue, disk_get_numcyls(queue->list->iolist->devno));
      } else if (queue->sched_alg == CYCLE_CYL) {
         temp = ioqueue_get_request_from_cyl_scan_queue(queue);
      } else if (queue->sched_alg == SSTF_CYL) {
         temp = ioqueue_get_request_from_cyl_vscan_queue(queue, 0);
      } else if (queue->sched_alg == SPTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, FALSE, 0, TRUE);
      } else if (queue->sched_alg == SPCTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, TRUE, 0, TRUE);
      } else if (queue->sched_alg == SATF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, 0, 0, FALSE);
      } else if (queue->sched_alg == WPTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, FALSE, 1, TRUE);
      } else if (queue->sched_alg == WPCTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, TRUE, 1, TRUE);
      } else if (queue->sched_alg == WATF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, 0, 1, FALSE);
      } else if (queue->sched_alg == ASPTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, FALSE, 2, TRUE);
      } else if (queue->sched_alg == ASPCTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, TRUE, 2, TRUE);
      } else if (queue->sched_alg == PRI_ASPTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, FALSE, 3, TRUE);
      } else if (queue->sched_alg == PRI_ASPCTF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, TRUE, 3, TRUE);
      } else if (queue->sched_alg == ASATF_OPT) {
         temp = ioqueue_get_request_from_opt_sptf_queue(queue, 0, 2, FALSE);
      } else if (queue->sched_alg == VSCAN_LBN) {
         temp = ioqueue_get_request_from_lbn_vscan_queue(queue, queue->vscan_cyls);
      } else if (queue->sched_alg == VSCAN_CYL) {
         temp = ioqueue_get_request_from_cyl_vscan_queue(queue, queue->vscan_cyls);
      }
   }

   if (temp == NULL) {
      return(NULL);
   } else if (!READY_TO_GO(temp)) {
      fprintf(stderr, "Selected request is !READY_TO_GO in get_next_request_from_subqueue\n");
      exit(0);
   }
/*
   fprintf (outputfile, "Selected request: blkno %d, cylno %d, surface %d\n", temp->blkno, temp->cylinder, temp->surface);
*/
   switch (queue->sched_alg) {
      case ELEVATOR_LBN:
      case CYCLE_LBN:
      case PRI_VSCAN_LBN:
      case ELEVATOR_CYL:
      case CYCLE_CYL:
                         queue->lastblkno = temp->blkno;
			 break;

      case VSCAN_LBN:
      case VSCAN_CYL:
			 if (queue->vscan_cyls > 0) {
                            queue->lastblkno = temp->blkno;
			    break;
			 }
      case SSTF_LBN:
                         if (queue->sched_alg != VSCAN_CYL) {
			    queue->lastblkno = temp->blkno + temp->totalsize;
			    if (queue->lastblkno != disk_get_number_of_blocks(temp->iolist->devno)) {
			       break;
			    }
			 }
      
      default:
                         queue->lastblkno = temp->blkno + temp->totalsize - 1;
			 break;
   }
   ioqueue_get_cylinder_mapping(queue->bigqueue, temp, queue->lastblkno, &queue->lastcylno, &queue->lastsurface, -1);
   ioqueue_get_cylinder_mapping(queue->bigqueue, temp, queue->lastblkno, &queue->optcylno, &queue->optsurface, MAP_FULL);
   if (temp->starttime == -1.0) {
      temp->starttime = simtime;
   }
   ret = temp->iolist;
   stat_update(&queue->instqueuelen, (double)(queue->iobufcnt - queue->reqoutstanding));
   queue->reqoutstanding++;
   queue->numoutstanding += temp->reqcnt;
   while (ret) {
      stat_update(&queue->qtimestats, (temp->starttime - ret->time));
      ret = ret->next;
   }
   if (temp->iolist == NULL) {
      fprintf(stderr, "iobuf structure with no iolist chosen to be scheduled\n");
      exit(0);
   } else if (temp->reqcnt == 1) {
      ret = temp->iolist;
      if (ret->next) {
	 fprintf(stderr, "More than one request when temp->reqcnt == 1\n");
	 exit(0);
      }
      temp->iolist = NULL;
      temp->iob_un.time = ret->time;
   } else if (temp->iob_un.pend.concat) {
      ret = temp->iob_un.pend.concat;
      temp->iob_un.pend.concat = NULL;
   } else {
      ret = (ioreq_event *) getfromextraq();
      ret->time = temp->starttime;
      ret->type = temp->iolist->type;
      ret->next = NULL;
      ret->prev = NULL;
      ret->bcount = temp->totalsize;
      ret->blkno = temp->blkno;
      ret->flags = temp->flags;
      ret->busno = temp->iolist->busno;
      ret->slotno = temp->iolist->slotno;
      ret->devno = temp->iolist->devno;
      ret->opid = temp->opid;
      ret->buf = temp->iolist->buf;
   }
   queue->current = temp;
   temp->state = PENDING;
   return(ret);
}


int ioqueue_request_match(wanted, curr)
ioreq_event *wanted;
iobuf *curr;
{
   ioreq_event *tmp;

/* bcount undefined for completing requests */
   if ((curr->blkno <= wanted->blkno) && ((curr->blkno + curr->totalsize) >= wanted->blkno)) {
      if ((curr->blkno == wanted->blkno) && (curr->opid == wanted->opid) && ((curr->flags & READ) == (wanted->flags & READ))) {
	 return(1);
      }
      tmp = curr->iolist;
      while (tmp) {
	 if ((tmp->blkno == wanted->blkno) && (tmp->opid == wanted->opid)) {
	    return(1);
	 }
	 tmp = tmp->next;
      }
   }
   return(0);
}


/* Note: use of this function with concat will result in iobuf containing
   the specified request */

ioreq_event * ioqueue_get_specific_request_from_subqueue(queue, wanted)
subqueue *queue;
ioreq_event *wanted;
{
   iobuf *temp;
   ioreq_event *ret;

   ioqueue_update_queue_statistics(queue);
   if ((queue->iobufcnt - queue->reqoutstanding) == 0) {
      return(NULL);
   } else {
      temp = queue->list;
      while ((temp->next != queue->list) && (ioqueue_request_match(wanted, temp) == 0)) {
	 temp = temp->next;
      }
   }
   if (ioqueue_request_match(wanted, temp) == 0) {
      return(NULL);
   }
   ASSERT(READY_TO_GO(temp));

   switch (queue->sched_alg) {
      case ELEVATOR_LBN:
      case CYCLE_LBN:
      case PRI_VSCAN_LBN:
      case ELEVATOR_CYL:
      case CYCLE_CYL:
                         queue->lastblkno = temp->blkno;
                         break;

      case VSCAN_LBN:
      case VSCAN_CYL:
                         if (queue->vscan_cyls > 0) {
                            queue->lastblkno = temp->blkno;
                            break;
                         }
      case SSTF_LBN:
                         if (queue->sched_alg != VSCAN_CYL) {
                            queue->lastblkno = temp->blkno + temp->totalsize;
                            if (queue->lastblkno != disk_get_number_of_blocks(temp->iolist->devno)) {
                               break;
                            }
                         }

      default:
                         queue->lastblkno = temp->blkno + temp->totalsize - 1;
                         break;
   }
   ioqueue_get_cylinder_mapping(queue->bigqueue, temp, queue->lastblkno, &queue->lastcylno, &queue->lastsurface, -1);
   ioqueue_get_cylinder_mapping(queue->bigqueue, temp, queue->lastblkno, &queue->optcylno, &queue->optsurface, MAP_FULL);
   if (temp->starttime == -1.0) {
      temp->starttime = simtime;
   }
   ret = temp->iolist;
   stat_update(&queue->instqueuelen, (double)(queue->iobufcnt - queue->reqoutstanding));
   queue->reqoutstanding++;
   queue->numoutstanding += temp->reqcnt;
   while (ret) {
      stat_update(&queue->qtimestats, (temp->starttime - ret->time));
      ret = ret->next;
   }
   ASSERT(temp->iolist != NULL);
   if (temp->reqcnt == 1) {
      ret = temp->iolist;
      temp->iolist = NULL;
      temp->iob_un.time = ret->time;
   } else if (temp->iob_un.pend.concat) {
      ret = temp->iob_un.pend.concat;
      temp->iob_un.pend.concat = NULL;
   } else {
      ret = (ioreq_event *) getfromextraq();
      ret->time = temp->starttime;
      ret->type = temp->iolist->type;
      ret->next = NULL;
      ret->prev = NULL;
      ret->bcount = temp->totalsize;
      ret->blkno = temp->blkno;
      ret->flags = temp->flags;
      ret->busno = temp->iolist->busno;
      ret->slotno = temp->iolist->slotno;
      ret->devno = temp->iolist->devno;
      ret->opid = temp->opid;
      ret->buf = temp->iolist->buf;
   }
   queue->current = temp;
   temp->state = PENDING;
   return(ret);
}


ioreq_event * ioqueue_set_starttime_in_subqueue(queue, target)
subqueue *queue;
ioreq_event *target;
{
   iobuf *temp = queue->list;
   ioreq_event *ret;

   if ((queue->iobufcnt - queue->reqoutstanding) == 0) {
      return(NULL);
   } else {
      while ((temp->next != queue->list) && (!ioqueue_request_match(target, temp))) {
         temp = temp->next;
      }
   }
   if (!ioqueue_request_match(target, temp)) {
      return(NULL);
   }
   ASSERT(temp->state == WAITING);
   if (temp->starttime == -1.0) {
      temp->starttime = simtime;
   }
   ret = temp->iolist;
   return(ret);
}


ioreq_event * ioqueue_remove_completed_request(queue, done)
subqueue *queue;
ioreq_event *done;
{
   iobuf *tmp;
   iobuf *tail;
   ioreq_event *trv;
/*
fprintf (outputfile, "Entering remove_completed_request - %d\n", queue->listlen);
*/
   ioqueue_update_queue_statistics(queue);
   tmp = queue->current;
   /* NOTE- the following match will fail with concat unless fixed properly */
   if ((tmp->state != PENDING) || (ioqueue_request_match(done, tmp) == 0)) {
      tmp = queue->list->next;
      tail = queue->list;
      while ((tmp != tail) && (ioqueue_request_match(done, tmp) == 0)) {
	 tmp = tmp->next;
      }
      if ((tmp->state != PENDING) || (ioqueue_request_match(done, tmp) == 0)) {
	 fprintf(stderr, "Completed event not found pending in ioqueue - blkno %d, tmp->blkno %d, tmp->totalsize %d, %d, %d, %d, %d\n", done->blkno, tmp->blkno, tmp->totalsize, (tmp == NULL), done->opid, tmp->opid, done->bcount);
	 exit(0);
      }
   }
   ioqueue_remove_from_subqueue(queue, tmp);
   if (tmp->reqcnt == 1) {
      trv = done;
      trv->bcount = tmp->totalsize;
      trv->time = tmp->starttime;
      done->next = NULL;
      queue->iobufcnt--;
      queue->reqoutstanding--;
      queue->listlen--;
      if (tmp->flags & READ) {
	 queue->readlen--;
      }
      queue->numoutstanding--;
      queue->numcomplete++;
      stat_update(&queue->accstats, (simtime - tmp->starttime));
      lastphystime = simtime - tmp->starttime;
      stat_update(&queue->outtimestats, (simtime - tmp->iob_un.time));
      if (tmp->flags & READ) {
         if (tmp->flags & TIME_CRITICAL) {
            stat_update(&queue->critreadstats, (simtime - tmp->iob_un.time));
         } else {
            stat_update(&queue->nocritreadstats, (simtime - tmp->iob_un.time));
         }
      } else {
         if (tmp->flags & TIME_CRITICAL) {
            stat_update(&queue->critwritestats, (simtime - tmp->iob_un.time));
         } else {
            stat_update(&queue->nocritwritestats, (simtime - tmp->iob_un.time));
         }
      }
   } else {
      trv = tmp->iolist;
      queue->iobufcnt--;
      queue->reqoutstanding--;
      queue->listlen -= tmp->reqcnt;
      if (trv->flags & READ) {
         queue->readlen -= tmp->reqcnt;
      }
      queue->numoutstanding -= tmp->reqcnt;
      queue->numcomplete += tmp->reqcnt;
      while (trv) {
         stat_update(&queue->accstats, (simtime - tmp->starttime));
         lastphystime = simtime - tmp->starttime;
         stat_update(&queue->outtimestats, (simtime - trv->time));
         if (trv->flags & READ) {
            if (trv->flags & TIME_CRITICAL) {
               stat_update(&queue->critreadstats, (simtime - trv->time));
            } else {
               stat_update(&queue->nocritreadstats, (simtime - trv->time));
            }
         } else {
            if (trv->flags & TIME_CRITICAL) {
               stat_update(&queue->critwritestats, (simtime - trv->time));
            } else {
               stat_update(&queue->nocritwritestats, (simtime - trv->time));
            }
         }
         trv = trv->next;
      }
      addtoextraq((event *) done);
      trv = tmp->iolist;
   }
   addtoextraq((event *) tmp);
/*
fprintf (outputfile, "Exiting remove_completed_request\n");
*/
   return(trv);
}


ioreq_event * ioqueue_get_specific_request(queue, wanted)
ioqueue *queue;
ioreq_event *wanted;
{
   ioreq_event *tmp = NULL;

/*
fprintf (outputfile, "Entering ioqueue_get_specific_request: %d\n", wanted->blkno);
*/
   if ((queue->priority.listlen - queue->priority.numoutstanding) > 0) {
      tmp = ioqueue_get_specific_request_from_subqueue(&queue->priority, wanted);
      queue->lastsubqueue = IOQUEUE_PRIORITY;
   }
   if ((tmp == NULL) && ((queue->timeout.listlen - queue->timeout.numoutstanding) > 0)) {
      tmp = ioqueue_get_specific_request_from_subqueue(&queue->timeout, wanted);
      queue->lastsubqueue = IOQUEUE_TIMEOUT;
   }
   if ((tmp == NULL) && ((queue->base.listlen - queue->base.numoutstanding) > 0)) {
      tmp = ioqueue_get_specific_request_from_subqueue(&queue->base, wanted);
      queue->lastsubqueue = IOQUEUE_BASE;
   }
   if (tmp) {
      tmp->time = 0.0;
   }
/*
fprintf (outputfile, "Exiting ioqueue_get_specific_request: %x\n", tmp);
*/
   return(tmp);
}


/* This routine allows the queue owner to set the starttime without changing
   the state of the queue or the request (i.e., setting the state to PENDING)
   Specifically, this is used by advanced disk models which allow other
   activity to occur for a request in advance of the actual selection of the
   request to receive dedicated HDA (mechanical) activity.  Thus, the request
   needs to remain WAITING in order to enable algorithmic mechanical latency
   reduction.

   Not callable for concat queues, as a concat request may change inside
   the queue after the starttime has been set, which means the queue owner
   and the queue itself may have different ideas as to what constitutes the
   entire request.
*/

ioreq_event * ioqueue_set_starttime(queue, target)
ioqueue *queue;
ioreq_event *target;
{
   ioreq_event *tmp = NULL;

/*
fprintf (outputfile, "Entering ioqueue_set_starttime: %d\n", target->blkno);
*/
      /* Queue must not be concatable! */
   ASSERT((queue->seqscheme & IOQUEUE_CONCAT_BOTH) == 0);

   if ((queue->priority.listlen - queue->priority.numoutstanding) > 0) {
      tmp = ioqueue_set_starttime_in_subqueue(&queue->priority, target);
   }
   if (!tmp && ((queue->timeout.listlen - queue->timeout.numoutstanding) > 0)) {
      tmp = ioqueue_set_starttime_in_subqueue(&queue->timeout, target);
   }
   if (!tmp && ((queue->base.listlen - queue->base.numoutstanding) > 0)) {
      tmp = ioqueue_set_starttime_in_subqueue(&queue->base, target);
   }
/*
fprintf (outputfile, "Exiting ioqueue_set_starttime: %x\n", tmp);
*/
   return(tmp);
}


ioreq_event * ioqueue_show_next_request(queue, enablement_in)
ioqueue *queue;
int (*enablement_in)();
{
   ioreq_event *tmp = NULL;

/*
fprintf (outputfile, "Entering ioqueue_show_next_request\n");
*/
   enablement = enablement_in;

   if ((queue->priority.listlen - queue->priority.numoutstanding) > 0) {
      tmp = ioqueue_show_next_request_from_subqueue(&queue->priority);
   }
   if (!tmp && ((queue->timeout.listlen - queue->timeout.numoutstanding) > 0)) {
      tmp = ioqueue_show_next_request_from_subqueue(&queue->timeout);
   }
   if (!tmp && ((queue->base.listlen - queue->base.numoutstanding) > 0)) {
      tmp = ioqueue_show_next_request_from_subqueue(&queue->base);
   }
/*
fprintf (outputfile, "Exiting ioqueue_show_next_request: %d\n", ((tmp) ? tmp->blkno : -1));
*/
   enablement = NULL;
   return(tmp);
}


ioreq_event * ioqueue_get_next_request(queue, enablement_in)
ioqueue *queue;
int (*enablement_in)();
{
   ioreq_event *tmp = NULL;
/*
fprintf (outputfile, "Entering ioqueue_get_next_request\n");
*/
   enablement = enablement_in;

   if ((queue->priority.listlen - queue->priority.numoutstanding) > 0) {
      if ((tmp = ioqueue_get_next_request_from_subqueue(&queue->priority))) {
         queue->lastsubqueue = IOQUEUE_PRIORITY;
      }
   }
   if (!tmp && ((queue->timeout.listlen - queue->timeout.numoutstanding) > 0)) {
      if ((tmp = ioqueue_get_next_request_from_subqueue(&queue->timeout))) {
         queue->lastsubqueue = IOQUEUE_TIMEOUT;
      }
   }
   if (!tmp && ((queue->base.listlen - queue->base.numoutstanding) > 0)) {
      if ((tmp = ioqueue_get_next_request_from_subqueue(&queue->base))) {
         queue->lastsubqueue = IOQUEUE_BASE;
      }
   }
   if (tmp) {
      int numout = queue->base.numoutstanding + queue->timeout.numoutstanding + queue->priority.numoutstanding;
      if (numout > queue->maxoutstanding) {
         queue->maxoutstanding = numout;
      }
      tmp->time = 0.0;
   }
/*
fprintf (outputfile, "Exiting ioqueue_get_next_request: %d\n", ((tmp) ? tmp->blkno : -1));
*/
   enablement = NULL;
   return(tmp);
}


double ioqueue_add_new_request(queue, new)
ioqueue *queue;
ioreq_event *new;
{
   iobuf *tmp;
   int listlen;
   int qlen;
   int readlen;

/*
fprintf (outputfile, "Entering ioqueue_add_new_request: %d\n", new->blkno);
*/
   new->time = simtime;
   ioqueue_update_arrival_stats(queue, new);
   tmp = (iobuf *) getfromextraq();
   tmp->starttime = -1.0;
   tmp->state = WAITING;
   tmp->next = NULL;
   tmp->prev = NULL;
   tmp->totalsize = new->bcount;
   tmp->blkno = new->blkno;
   tmp->flags = new->flags;
   tmp->iolist = new;
   new->next = NULL;
   tmp->iob_un.pend.waittime = 0;
   tmp->iob_un.pend.concat = NULL;
   tmp->reqcnt = 1;
   tmp->opid = new->opid;
   switch(queue->pri_scheme) {
      case ALLEQUAL:
         if ((queue->base.sched_alg == ELEVATOR_LBN) || 
             (queue->base.sched_alg == CYCLE_LBN)    || 
             (queue->base.sched_alg == SSTF_LBN)     || 
             (queue->base.sched_alg == VSCAN_LBN)) {
           ioqueue_get_cylinder_mapping(queue, tmp, tmp->blkno, &tmp->cylinder, &tmp->surface, MAP_NONE);
         } else {
           ioqueue_get_cylinder_mapping(queue, tmp, tmp->blkno, &tmp->cylinder, &tmp->surface, -1);
         }
	 ioqueue_insert_new_request(&queue->base, tmp);
	 break;
      case TWOQUEUE:
         if (new->flags & (TIME_CRITICAL|TIME_LIMITED)) {
            if ((queue->priority.sched_alg == ELEVATOR_LBN) || 
                (queue->priority.sched_alg == CYCLE_LBN)    || 
                (queue->priority.sched_alg == SSTF_LBN)     || 
                (queue->priority.sched_alg == VSCAN_LBN)) {
              ioqueue_get_cylinder_mapping(queue, tmp, tmp->blkno, &tmp->cylinder, &tmp->surface, MAP_NONE);
            } else {
              ioqueue_get_cylinder_mapping(queue, tmp, tmp->blkno, &tmp->cylinder, &tmp->surface, -1);
            }
            ioqueue_insert_new_request(&queue->priority, tmp);
         } else {
            if ((queue->base.sched_alg == ELEVATOR_LBN) || 
                (queue->base.sched_alg == CYCLE_LBN)    || 
                (queue->base.sched_alg == SSTF_LBN)     || 
                (queue->base.sched_alg == VSCAN_LBN)) {
              ioqueue_get_cylinder_mapping(queue, tmp, tmp->blkno, &tmp->cylinder, &tmp->surface, MAP_NONE);
            } else {
              ioqueue_get_cylinder_mapping(queue, tmp, tmp->blkno, &tmp->cylinder, &tmp->surface, -1);
            }
	    ioqueue_insert_new_request(&queue->base, tmp);
         }
	 break;
      default:
         fprintf(stderr, "Unknown priority scheme being employed at ioqueue_add_new_request\n");
	 exit(0);
   }
   listlen = queue->base.listlen + queue->timeout.listlen + queue->priority.listlen;
   qlen = listlen - (queue->base.numoutstanding + queue->timeout.numoutstanding + queue->priority.numoutstanding);
   readlen = queue->base.readlen + queue->timeout.readlen + queue->priority.readlen;
   queue->maxlistlen = max(listlen, queue->maxlistlen);
   queue->maxqlen = max(qlen, queue->maxqlen);
   queue->maxreadlen = max(readlen, queue->maxreadlen);
   queue->maxwritelen = max((listlen - readlen), queue->maxwritelen);
   if (listlen == 1) {
      stat_update(&queue->idlestats, (simtime - queue->idlestart));
   }
   queue->idlestart = simtime;
   if (queue->idledetect) {
      if (!(removefromintq(queue->idledetect))) {
	 fprintf(stderr, "existing idledetect event not on intq in ioqueue_add_new_request\n");
	 exit(0);
      }
      addtoextraq(queue->idledetect);
      queue->idledetect = NULL;
   }
/*
fprintf (outputfile, "Exiting ioqueue_add_new_request\n");
*/
   return(0.0);
}


int ioqueue_raise_priority_subqueue(queue, priority, opid, pri_scheme)
subqueue *queue;
subqueue *priority;
int opid;
int pri_scheme;
{
   int stop = FALSE;
   iobuf *tmp;
   ioreq_event *trv;
   iobuf *bump;
   int found = 0;
   int iocnt;
/*
fprintf (outputfile, "Entered ioqueue_raise_priority_subqueue - opid %d (queue %p, priority %p, pri_scheme %d)\n", opid, queue, priority, pri_scheme);
*/
   tmp = queue->list;
   if (tmp == NULL) {
      return (FALSE);
   }
   while (stop != TRUE) {
      bump = NULL;
      if ((tmp->iolist == NULL) || (tmp->totalsize == tmp->iolist->bcount)) {
	 if (tmp->opid == opid) {
	    bump = tmp;
	 }
      } else {
	 trv = tmp->iolist;
	 while (trv != NULL) {
	    if (trv->opid == opid) {
	       bump = tmp;
	    }
	    trv = trv->next;
	 }
      }
      tmp = tmp->next;
      if (tmp == queue->list) {
	 stop = TRUE;
      }
      if (bump) {
/*
fprintf (outputfile, "Raising priority of opid %d, buf %x, bump->state %d, read %d, crit %d, blkno %x\n", opid, bump->opid, bump->state, (bump->flags & READ), (bump->flags & TIME_CRITICAL), bump->blkno);
*/
         if ((bump->state == WAITING) && (pri_scheme != ALLEQUAL)) {
            ioqueue_update_queue_statistics(queue);
            ioqueue_remove_from_subqueue(queue, bump);
            ioqueue_insert_new_request(priority, bump);
	    found = 1;
         }
         found = (bump->state == WAITING) ? 1 : 2;
         bump->flags |= TIME_CRITICAL;
         trv = bump->iolist;
         while (trv != NULL) {
            if (trv->opid == opid) {
               trv->flags |= TIME_CRITICAL;
            }
            trv = trv->next;
         }
	 if ((bump->state == WAITING) && (pri_scheme != ALLEQUAL)) {
	    iocnt = bump->reqcnt;
	    priority->switches += iocnt;
	    queue->iobufcnt--;
            queue->listlen -= iocnt;
            if (bump->flags & READ) {
	       queue->readlen -= iocnt;
	       queue->numreads -= iocnt;
            } else {
	       queue->numwrites -= iocnt;
	    }
         }
	 break;
      }
   }
/*
fprintf (outputfile, "Returning found %d\n", found);
*/
   return(found);
}


int ioqueue_raise_priority(queue, opid)
ioqueue *queue;
int opid;
{
   int found = 0;

   found = ioqueue_raise_priority_subqueue(&queue->base, &queue->priority, opid, queue->pri_scheme);
   if (found) {
      return(found);
   }
   found = ioqueue_raise_priority_subqueue(&queue->timeout, &queue->priority, opid, queue->pri_scheme);
   return(found);
}


int ioqueue_implement_timeouts(queue, timeout, maxtime, flag)
subqueue *queue;
subqueue *timeout;
int maxtime;
u_int flag;
{
   iobuf *tmp;
   iobuf *done;
   ioreq_event *trv;
   int timeouts = 0;
   int iocnt;

   if (queue->list == NULL) {
      return (0);
   }
   tmp = queue->list->next;
   while (tmp != queue->list) {
      tmp->iob_un.pend.waittime++;
/*
fprintf (outputfile, "New waittime for opid %d is %d, maxtime %d\n", tmp->opid, tmp->iob_un.pend.waittime, maxtime);
*/
      if ((tmp->iob_un.pend.waittime > maxtime) && (tmp->state == WAITING)) {
/*
fprintf (outputfile, "Inside one of the request moving sections 1 - blkno %d, state %d\n", tmp->blkno, tmp->state);
*/
	 done = tmp;
	 tmp = tmp->next;
         ioqueue_update_queue_statistics(queue);
	 ioqueue_remove_from_subqueue(queue, done);
	 ioqueue_insert_new_request(timeout, done);
         done->flags |= flag;
	 trv = done->iolist;
	 while (trv != NULL) {
	    trv->flags |= flag;
	    trv = trv->next;
	 }
	 iocnt = done->reqcnt;
	 queue->iobufcnt--;
	 queue->listlen -= iocnt;
	 if (done->flags & READ) {
	    queue->numreads -= iocnt;
	    queue->readlen -= iocnt;
         } else {
	    queue->numwrites -= iocnt;
	 }
	 timeouts += iocnt;
	 timeout->switches += iocnt;
	 if (queue->listlen == 0) {
	    tmp = NULL;
	 }
      } else {
         tmp = tmp->next;
      }
   }
   if (tmp != NULL) {
      tmp->iob_un.pend.waittime++;
/*
fprintf (outputfile, "New waittime for opid %d is %d, maxtime %d\n", tmp->opid, tmp->iob_un.pend.waittime, maxtime);
*/
      if ((tmp->iob_un.pend.waittime > maxtime) && (tmp->state == WAITING)) {
/*
fprintf (outputfile, "Inside one of the request moving sections 2 - blkno %d, state %d\n", tmp->blkno, tmp->state);
*/
	 done = tmp;
         ioqueue_update_queue_statistics(queue);
	 ioqueue_remove_from_subqueue(queue, done);
	 ioqueue_insert_new_request(timeout, done);
	 done->flags |= flag;
	 trv = done->iolist;
	 while (trv != NULL) {
	    trv->flags |= flag;
	    trv = trv->next;
	 }
	 iocnt = done->reqcnt;
	 queue->iobufcnt--;
	 queue->listlen -= iocnt;
	 if (done->flags & READ) {
	    queue->numreads -= iocnt;
	    queue->readlen -= iocnt;
         } else {
	    queue->numwrites -= iocnt;
	 }
	 timeouts += iocnt;
         timeout->switches += iocnt;
      }
   }
   return(timeouts);
}


double ioqueue_tick(queue)
ioqueue *queue;
{
   int timeouts;
   int readcnt;

   if (queue->to_scheme == BASETIMEOUT) {
      if (queue->pri_scheme == ALLEQUAL) {
/*
fprintf(outputfile, "base queue to timeout queue\n");
*/
         readcnt = queue->timeout.numreads;
         timeouts = ioqueue_implement_timeouts(&queue->base, &queue->timeout, queue->to_time, TIMED_OUT);
         queue->timeouts += timeouts;
         queue->timeoutreads += queue->timeout.numreads - readcnt;
      } else {
/*
fprintf(outputfile, "base queue to priority queue\n");
*/
         readcnt = queue->priority.numreads;
         timeouts = ioqueue_implement_timeouts(&queue->base, &queue->priority, queue->to_time, TIMED_OUT);
         queue->timeouts += timeouts;
         queue->timeoutreads += queue->priority.numreads - readcnt;
      }
   } else if (queue->to_scheme == HALFTIMEOUT) {
/*
fprintf(outputfile, "timeout queue to priority queue\n");
*/
      readcnt = queue->priority.numreads;
      timeouts = ioqueue_implement_timeouts(&queue->timeout, &queue->priority, queue->to_time, TIMED_OUT);
      queue->timeouts += timeouts;
      queue->timeoutreads += queue->priority.numreads - readcnt;
/*
fprintf(outputfile, "base queue to timeout queue\n");
*/
      readcnt = queue->timeout.numreads;
      timeouts = ioqueue_implement_timeouts(&queue->base, &queue->timeout, (queue->to_time / 2), HALF_OUT);
      queue->halfouts += timeouts;
      queue->halfoutreads += queue->timeout.numreads - readcnt;
   }
   return(0.0);
}


void ioqueue_clobber_overlaps_subqueue(queue, ret, arrtimemax)
subqueue *queue;
ioreq_event *ret;
double arrtimemax;
{
   iobuf *tmp;
   ioreq_event *done;
   int read = ret->flags & READ;
   while ((tmp = queue->list)) {
      if ((tmp->state != WAITING) || (tmp->blkno != ret->blkno) || (tmp->totalsize != ret->bcount) || ((read) && !(tmp->flags & READ)) || (tmp->iolist->time > arrtimemax)) {
         tmp = tmp->next;
         while ((tmp != queue->list) && ((tmp->state != WAITING) || (tmp->blkno != ret->blkno) || (tmp->totalsize != ret->bcount) || ((read) && !(tmp->flags & READ)) || (tmp->iolist->time > arrtimemax))) {
            tmp = tmp->next;
         }
         if (tmp == queue->list) {
            return;
         }
      }
      ASSERT(tmp->reqcnt == 1);
      done = ioqueue_get_specific_request_from_subqueue(queue, tmp->iolist);
      ASSERT(done != NULL);
      done = ioqueue_remove_completed_request(queue, done);
      ASSERT(done->next == NULL);
      done->next = ret->next;
      ret->next = done;
      queue->bigqueue->overlapscombed++;
      if (done->flags & READ) {
         queue->bigqueue->overlapscombed++;
      }
   }
}


ioreq_event * ioqueue_physical_access_done(queue, curr)
ioqueue *queue;
ioreq_event *curr;
{
   ioreq_event *ret;
/*
fprintf(outputfile, "Entering ioqueue_physical_access_done: %d, devno %d\n", curr->blkno, curr->devno);
ioqueue_print_contents(queue);
*/
   queue->idlestart = simtime;
   if ((queue->pri_scheme != ALLEQUAL) && ((curr->flags & (TIME_CRITICAL|TIME_LIMITED)) || ((queue->to_scheme != NOTIMEOUT) && (curr->flags & TIMED_OUT)))) {
/*
fprintf(outputfile, "Checking the priority queue\n");
*/
      ret = ioqueue_remove_completed_request(&queue->priority, curr);
   } else if ((queue->to_scheme != NOTIMEOUT) && (curr->flags & (TIMED_OUT|HALF_OUT))) {
/*
fprintf(outputfile, "Checking the timeout queue\n");
*/
      ret = ioqueue_remove_completed_request(&queue->timeout, curr);
   } else {
/*
fprintf(outputfile, "Checking the base queue\n");
*/
      ret = ioqueue_remove_completed_request(&queue->base, curr);
   }
   ASSERT(ret != NULL);
   if (queue->comboverlaps) {
      double arrtimemax = (queue->comboverlaps == 1) ? simtime : ret->time;
      ioqueue_clobber_overlaps_subqueue(&queue->priority, ret, arrtimemax);
      ioqueue_clobber_overlaps_subqueue(&queue->timeout, ret, arrtimemax);
      ioqueue_clobber_overlaps_subqueue(&queue->base, ret, arrtimemax);
   }
   if (queue->idlework) {
      ioqueue_reset_idledetecter(queue, 1);
   }
/*
{
extern double disk_last_seektime, disk_last_latency;
fprintf (outputfile, "Exiting ioqueue_physical_access_done: %f, seek %f, lat %f\n", lastphystime, disk_last_seektime, disk_last_latency);
}
*/
   return(ret);
}


void ioqueue_subqueue_copy(queue, new)
subqueue *queue;
subqueue *new;
{
   new->sched_alg = queue->sched_alg;
   new->surfoverforw = queue->surfoverforw;
   new->list = NULL;
   new->current = NULL;
}


ioqueue * ioqueue_copy(queue)
ioqueue *queue;
{
   ioqueue *new = (ioqueue *) malloc(sizeof(ioqueue));
   ASSERT(new != NULL);

   ioqueue_subqueue_copy(&queue->base, &new->base);
   ioqueue_subqueue_copy(&queue->timeout, &new->timeout);
   ioqueue_subqueue_copy(&queue->priority, &new->priority);
   new->concatmax = queue->concatmax;
   new->comboverlaps = queue->comboverlaps;
   new->seqscheme = queue->seqscheme;
   new->seqstreamdiff = queue->seqstreamdiff;
   new->to_scheme = queue->to_scheme;
   new->to_time = queue->to_time;
   new->pri_scheme = queue->pri_scheme;
   new->cylmaptype = queue->cylmaptype;
   new->writedelay = queue->writedelay;
   new->readdelay = queue->readdelay;
   new->sectpercyl = queue->sectpercyl;
   new->printqueuestats = queue->printqueuestats;
   new->printcritstats = queue->printcritstats;
   new->printidlestats = queue->printidlestats;
   new->printintarrstats = queue->printintarrstats;
   new->printsizestats = queue->printsizestats;
   return(new);
}


void ioqueue_param_override(queue, paramname, paramval)
ioqueue *queue;
char *paramname;
char *paramval;
{
   if (strcmp(paramname, "ioqueue_schedalg") == 0) {
      scanparam_int(paramval, paramname, &queue->base.sched_alg, 3, MINSCHED, MAXSCHED);
   } else if (strcmp(paramname, "ioqueue_cylmaptype") == 0) {
      scanparam_int(paramval, paramname, &queue->cylmaptype, 3, MAP_NONE, MAXMAPTYPE);
   } else if (strcmp(paramname, "ioqueue_seqscheme") == 0) {
      scanparam_int(paramval, paramname, &queue->seqscheme, 1, 0, 0);
   } else if (strcmp(paramname, "ioqueue_to_time") == 0) {
      scanparam_int(paramval, paramname, &queue->to_time, 1, 0, 0);
   } else if (strcmp(paramname, "ioqueue_priority_schedalg") == 0) {
      scanparam_int(paramval, paramname, &queue->priority.sched_alg, 3, MINSCHED, MAXSCHED);
   } else if (strcmp(paramname, "ioqueue_timeout_schedalg") == 0) {
      scanparam_int(paramval, paramname, &queue->timeout.sched_alg, 3, MINSCHED, MAXSCHED);
   } else if (strcmp(paramname, "ioqueue_priority_mix") == 0) {
      scanparam_int(paramval, paramname, &priority_mix, 1, 0, 0);
   } else {
      fprintf(stderr, "Unsupported param override at ioqueue_param_override: %s\n", paramname);
      exit(0);
   }
}


void ioqueue_subqueue_resetstats(queue)
subqueue *queue;
{
   queue->numreads = 0;
   queue->numwrites = 0;
   queue->switches = 0;
   queue->runreadlen = 0.0;
   queue->maxreadlen = 0;
   queue->maxwritelen = 0;
   queue->maxlistlen = 0;
   queue->runlistlen = 0.0;
   queue->maxqlen = 0;
   queue->numcomplete = 0;
   queue->maxoutstanding = 0;
   queue->runoutstanding = 0.0;
   queue->sstfupdowncnt = 0;
   queue->lastalt = simtime;
   stat_reset(&queue->accstats);
   stat_reset(&queue->qtimestats);
   stat_reset(&queue->outtimestats);
   stat_reset(&queue->critreadstats);
   stat_reset(&queue->nocritreadstats);
   stat_reset(&queue->critwritestats);
   stat_reset(&queue->nocritwritestats);
   stat_reset(&queue->instqueuelen);
   stat_reset(&queue->infopenalty);
}


void ioqueue_subqueue_initialize(queue, devno)
subqueue *queue;
int devno;
{
   addlisttoextraq(&queue->list);
   queue->dir = ASC;
   queue->lastblkno = 0;
   queue->lastsurface = 0;
   queue->lastcylno = 0;
   queue->prior = 0;
   queue->sstfupdown = 0;
   queue->current = NULL;
   queue->readlen = 0;
   queue->iobufcnt = 0;
   queue->reqoutstanding = 0;
   queue->listlen = 0;
   queue->numoutstanding = 0;
   stat_initialize(statdeffile, statdesc_accstats, &queue->accstats);
   stat_initialize(statdeffile, statdesc_qtimestats, &queue->qtimestats);
   stat_initialize(statdeffile, statdesc_outtimestats, &queue->outtimestats);
   stat_initialize(statdeffile, statdesc_outtimestats, &queue->critreadstats);
   stat_initialize(statdeffile, statdesc_outtimestats, &queue->nocritreadstats);
   stat_initialize(statdeffile, statdesc_outtimestats, &queue->critwritestats);
   stat_initialize(statdeffile, statdesc_outtimestats, &queue->nocritwritestats);
   stat_initialize(statdeffile, statdesc_instqueuelen, &queue->instqueuelen);
   stat_initialize(statdeffile, statdesc_infopenalty, &queue->infopenalty);
   if ((queue->sched_alg == VSCAN_LBN) || (queue->sched_alg == PRI_VSCAN_LBN)) {
      queue->vscan_cyls = vscan_value * (double) disk_get_number_of_blocks(devno);
   } else {
      queue->vscan_cyls = vscan_value * (double) disk_get_numcyls(devno);
   }
}


void ioqueue_resetstats(queue)
ioqueue *queue;
{
   queue->timeouts = 0;
   queue->timeoutreads = 0;
   queue->halfouts = 0;
   queue->halfoutreads = 0;
   queue->idlestart = simtime;
   queue->maxlistlen = 0;
   queue->maxqlen = 0;
   queue->maxoutstanding = 0;
   queue->maxreadlen = 0;
   queue->maxwritelen = 0;
   queue->overlapscombed = 0;
   queue->readoverlapscombed = 0;
   queue->seqreads = 0;
   queue->seqwrites = 0;
   stat_reset(&queue->intarrstats);
   stat_reset(&queue->readintarrstats);
   stat_reset(&queue->writeintarrstats);
   stat_reset(&queue->idlestats);
   stat_reset(&queue->reqsizestats);
   stat_reset(&queue->readsizestats);
   stat_reset(&queue->writesizestats);
   ioqueue_subqueue_resetstats(&queue->base);
   ioqueue_subqueue_resetstats(&queue->timeout);
   ioqueue_subqueue_resetstats(&queue->priority);
}


void ioqueue_initialize(queue, devno)
ioqueue *queue;
int devno;
{
   StaticAssert (sizeof(iobuf) <= DISKSIM_EVENT_SIZE);
   ioqueue_subqueue_initialize(&queue->base, devno);
   queue->base.bigqueue = queue;
   ioqueue_subqueue_initialize(&queue->timeout, devno);
   queue->timeout.bigqueue = queue;
   ioqueue_subqueue_initialize(&queue->priority, devno);
   queue->priority.bigqueue = queue;
   queue->devno = devno;
   queue->concatok = NULL;
   queue->idlework = NULL;
   queue->idledelay = 0.0;
   queue->idledetect = NULL;
   queue->sectpercyl = disk_get_avg_sectpercyl(devno);
   queue->lastsubqueue = IOQUEUE_BASE;
   queue->lastarr = 0.0;
   queue->lastread = 0.0;
   queue->lastwrite = 0.0;
   queue->seqblkno = -1;
   stat_initialize(statdeffile, statdesc_intarrstats, &queue->intarrstats);
   stat_initialize(statdeffile, statdesc_readintarrstats, &queue->readintarrstats);
   stat_initialize(statdeffile, statdesc_writeintarrstats, &queue->writeintarrstats);
   stat_initialize(statdeffile, statdesc_idlestats, &queue->idlestats);
   stat_initialize(statdeffile, statdesc_reqsizestats, &queue->reqsizestats);
   stat_initialize(statdeffile, statdesc_readsizestats, &queue->readsizestats);
   stat_initialize(statdeffile, statdesc_writesizestats, &queue->writesizestats);
}


void ioqueue_cleanstats(queue)
ioqueue *queue;
{
   double tpass;

   ioqueue_update_queue_statistics(&queue->base);
   ioqueue_update_queue_statistics(&queue->timeout);
   ioqueue_update_queue_statistics(&queue->priority);
   if ((queue->base.listlen + queue->timeout.listlen + queue->priority.listlen) == 0) {
      tpass = simtime - queue->idlestart;
      stat_update(&queue->idlestats, tpass);
   }
   queue->idlestart = simtime;
}


ioqueue * ioqueue_readparams(parfile, printqueuestats, printcritstats, printidlestats, printintarrstats, printsizestats)
FILE *parfile;
int printqueuestats;
int printcritstats;
int printidlestats;
int printintarrstats;
int printsizestats;
{
   ioqueue * queue = (ioqueue *) malloc(sizeof(ioqueue));
   ASSERT(queue != NULL);

   queue->printqueuestats = printqueuestats;
   queue->printcritstats = printcritstats;
   queue->printidlestats = printidlestats;
   queue->printintarrstats = printintarrstats;
   queue->printsizestats = printsizestats;
   queue->base.list = NULL;
   queue->timeout.list = NULL;
   queue->priority.list = NULL;
   queue->base.surfoverforw = global_surfoverforw;
   queue->timeout.surfoverforw = global_surfoverforw;
   queue->priority.surfoverforw = global_surfoverforw;

   getparam_int(parfile, "Scheduling policy", &queue->base.sched_alg, 3, MINSCHED, MAXSCHED);
   getparam_int(parfile, "Cylinder mapping strategy", &queue->cylmaptype, 3, MAP_NONE, MAXMAPTYPE);
   getparam_double(parfile, "Write initiation delay", &queue->writedelay, 1, (double) 0.0, (double) 0.0);
   getparam_double(parfile, "Read initiation delay", &queue->readdelay, 1, (double) 0.0, (double) 0.0);
   getparam_int(parfile, "Sequential stream scheme", &queue->seqscheme, 1, 0,0);
   getparam_int(parfile, "Maximum concat size", &queue->concatmax, 1, 0, 0);
   getparam_int(parfile, "Overlapping request scheme", &queue->comboverlaps, 3, 0, 2);
   getparam_int(parfile, "Sequential stream diff maximum", &queue->seqstreamdiff, 1, 0, 0);
   getparam_int(parfile, "Scheduling timeout scheme", &queue->to_scheme, 1, 0, 0);
   getparam_int(parfile, "Timeout time/weight", &queue->to_time, 1, 0, 0);

   getparam_int(parfile, "Timeout scheduling", &queue->timeout.sched_alg, 0, 0, 0);
   if ((queue->to_scheme != NOTIMEOUT) && ((queue->timeout.sched_alg < MINSCHED) || (queue->timeout.sched_alg > MAXSCHED))) {
      fprintf(stderr, "Invalid value for 'Timeout scheduling': %d\n", queue->timeout.sched_alg);
      exit(0);
   }

   getparam_int(parfile, "Scheduling priority scheme", &queue->pri_scheme, 1, 0, 0);
   getparam_int(parfile, "Priority scheduling", &queue->priority.sched_alg, 0, 0, 0);
   if ((queue->pri_scheme != ALLEQUAL) && ((queue->priority.sched_alg < MINSCHED) || (queue->priority.sched_alg > MAXSCHED))) {
      fprintf(stderr, "Invalid value for 'Priority scheduling': %d\n", queue->priority.sched_alg);
      exit(0);
   }

   return(queue);
}


void ioqueue_printqueuestats(set, setsize, prefix)
ioqueue **set;
int setsize;
char *prefix;
{
   int maxlistlen = 0.0;
   int maxqlen = 0.0;
   int maxreadlen = 0.0;
   int maxwritelen = 0.0;
   double runlistlen = 0.0;
   double runoutstanding = 0.0;
   double runreadlen = 0.0;
   double runwritelen = 0.0;
   int listlen = 0;
   int numoutstanding = 0;
   int numcomplete = 0;
   int i;
   statgen * statset[300];

   for (i=0; i<setsize; i++) {
      if (set[i]->printqueuestats == FALSE)
         return;

      if (set[i]->maxlistlen > maxlistlen)
	 maxlistlen = set[i]->maxlistlen;
      if (set[i]->maxqlen > maxqlen)
	 maxqlen = set[i]->maxqlen;
      if (set[i]->maxreadlen > maxreadlen)
	 maxreadlen = set[i]->maxreadlen;
      if (set[i]->maxwritelen > maxwritelen)
	 maxwritelen = set[i]->maxwritelen;
      numcomplete += set[i]->base.numcomplete + set[i]->timeout.numcomplete + set[i]->priority.numcomplete;
      runlistlen += set[i]->base.runlistlen + set[i]->timeout.runlistlen + set[i]->priority.runlistlen;
      runoutstanding += set[i]->base.runoutstanding + set[i]->timeout.runoutstanding + set[i]->priority.runoutstanding;
      listlen += set[i]->base.listlen + set[i]->timeout.listlen + set[i]->priority.listlen;
      numoutstanding += set[i]->base.numoutstanding + set[i]->timeout.numoutstanding + set[i]->priority.numoutstanding;
      runreadlen += set[i]->base.runreadlen + set[i]->timeout.runreadlen + set[i]->priority.runreadlen;
   }
   runwritelen = runlistlen - runreadlen;

   fprintf(outputfile, "%sAverage # requests:      %f\n", prefix, (runlistlen/((simtime - warmuptime) * (double) setsize)));
   fprintf(outputfile, "%sMaximum # requests:      %d\n", prefix, maxlistlen);
   fprintf(outputfile, "%sEnd # requests:          %d\n", prefix, listlen);
   fprintf(outputfile, "%sAverage queue length:    %f\n", prefix, ((runlistlen - runoutstanding)/((simtime - warmuptime) * (double) setsize)));
   fprintf(outputfile, "%sMaximum queue length:    %d\n", prefix, maxqlen);
   fprintf(outputfile, "%sEnd queued requests:     %d\n", prefix, (listlen - numoutstanding));

   for (i=0; i<setsize; i++) {
      statset[(3*i)] = &set[i]->base.qtimestats;
      statset[((3*i)+1)] = &set[i]->priority.qtimestats;
      statset[((3*i)+2)] = &set[i]->timeout.qtimestats;
   }
   stat_print_set(statset, (3*setsize), prefix);

   fprintf (outputfile, "%sAvg # read requests:     %f\n", prefix, (runreadlen / ((simtime - warmuptime) * (double) setsize)));
   fprintf (outputfile, "%sMax # read requests:     %d\n", prefix, maxreadlen);
   fprintf (outputfile, "%sAvg # write requests:    %f\n", prefix, (runwritelen / ((simtime - warmuptime) * (double) setsize)));
   fprintf (outputfile, "%sMax # write requests:    %d\n", prefix, maxwritelen);

   for (i=0; i<setsize; i++) {
      statset[(3*i)] = &set[i]->base.accstats;
      statset[((3*i)+1)] = &set[i]->priority.accstats;
      statset[((3*i)+2)] = &set[i]->timeout.accstats;
   }
   stat_print_set(statset, (3*setsize), prefix);
}


void ioqueue_printintarrstats(set, setsize, prefix)
ioqueue **set;
int setsize;
char *prefix;
{
   int i;
   statgen * statset[MAXDISKS];

   for (i=0; i<setsize; i++) {
      statset[i] = &set[i]->intarrstats;
      if (set[i]->printidlestats == FALSE) {
	 return;
      }
   }
   stat_print_set(statset, setsize, prefix);

   for (i=0; i<setsize; i++) {
      statset[i] = &set[i]->readintarrstats;
   }
   stat_print_set(statset, setsize, prefix);

   for (i=0; i<setsize; i++) {
      statset[i] = &set[i]->writeintarrstats;
   }
   stat_print_set(statset, setsize, prefix);
}


void ioqueue_printsizestats(set, setsize, prefix)
ioqueue **set;
int setsize;
char *prefix;
{
   int i;
   statgen * statset[MAXDISKS];
   statgen * statset1[MAXDISKS];
   statgen * statset2[MAXDISKS];

   for (i=0; i<setsize; i++) {
      statset[i] = &set[i]->reqsizestats;
      statset1[i] = &set[i]->readsizestats;
      statset2[i] = &set[i]->writesizestats;
      if (set[i]->printsizestats == FALSE) {
         return;
      }
   }
   stat_print_set(statset, setsize, prefix);
   stat_print_set(statset1, setsize, prefix);
   stat_print_set(statset2, setsize, prefix);
}


void ioqueue_printidlestats(set, setsize, prefix)
ioqueue **set;
int setsize;
char *prefix;
{
   int i;
   statgen * statset[MAXDISKS];

   for (i=0; i<setsize; i++) {
      statset[i] = &set[i]->idlestats;
      if (set[i]->printidlestats == FALSE) {
         return;
      }
   }
   fprintf (outputfile, "%sNumber of idle periods:  %d\n", prefix, stat_get_count_set(statset, setsize));
   stat_print_set(statset, setsize, prefix);
}


void ioqueue_printcritstats (set, setsize, prefix, numreqs)
subqueue **set;
int setsize;
char *prefix;
int numreqs;
{
   char prefix2[256];
   statgen * statset[3*MAXDISKS];
   int cnt;
   int i;

	/* check for printability is external */

   for (i=0; i<setsize; i++) {
      statset[i] = &set[i]->critreadstats;
   }
   cnt = stat_get_count_set(statset, setsize);
   fprintf (outputfile, "%sCritical Reads:      \t%6d  \t%f\n", prefix, cnt, ((double) cnt / (double) max(numreqs,1)));
   sprintf(prefix2, "%sCritical Read ", prefix);
   stat_print_set(statset, setsize, prefix2);
   
   for (i=0; i<setsize; i++) {
      statset[i] = &set[i]->nocritreadstats;
   }
   cnt = stat_get_count_set(statset, setsize);
   fprintf (outputfile, "%sNon-Critical Reads:  \t%6d  \t%f\n", prefix, cnt, ((double) cnt / (double) max(numreqs,1)));
   sprintf(prefix2, "%sNon-Critical Read ", prefix);
   stat_print_set(statset, setsize, prefix2);
   
   for (i=0; i<setsize; i++) {
      statset[i] = &set[i]->critwritestats;
   }
   cnt = stat_get_count_set(statset, setsize);
   fprintf (outputfile, "%sCritical Writes:     \t%6d  \t%f\n", prefix, cnt, ((double) cnt / (double) max(numreqs,1)));
   sprintf(prefix2, "%sCritical Write ", prefix);
   stat_print_set(statset, setsize, prefix2);
   
   for (i=0; i<setsize; i++) {
      statset[i] = &set[i]->nocritwritestats;
   }
   cnt = stat_get_count_set(statset, setsize);
   fprintf (outputfile, "%sNon-Critical Writes: \t%6d  \t%f\n", prefix, cnt, ((double) cnt / (double) max(numreqs,1)));
   sprintf(prefix2, "%sNon-Critical Write ", prefix);
   stat_print_set(statset, setsize, prefix2);
}


void ioqueue_subqueue_printstats(set, setsize, prefix, printcritstats)
subqueue **set;
int setsize;
char *prefix;
int printcritstats;
{
   int numcomplete = 0;
   int maxlistlen = 0;
   int listlen = 0;
   int maxqlen = 0;
   int numoutstanding = 0;
   int numreqs = 0;
   double runlistlen = 0.0;
   double runoutstanding = 0.0;
   statgen * statset[MAXDISKS];
   int i;

   for (i=0; i<setsize; i++) {
      numreqs += set[i]->numreads + set[i]->numwrites;
      numcomplete += set[i]->numcomplete;
      runlistlen += set[i]->runlistlen;
      runoutstanding += set[i]->runoutstanding;
      listlen += set[i]->listlen;
      numoutstanding += set[i]->numoutstanding;
      if (set[i]->maxqlen > maxqlen)
	 maxqlen = set[i]->maxqlen;
      if (set[i]->maxlistlen > maxlistlen)
	 maxlistlen = set[i]->maxlistlen;
   }

   if (maxlistlen == 0)
      return;

   for (i=0; i<setsize; i++) {
      statset[i] = &set[i]->outtimestats;
   }
   stat_print_set(statset, setsize, prefix);

   if (printcritstats) {
      ioqueue_printcritstats (set, setsize, prefix, numreqs);
   }

   fprintf(outputfile, "%sAverage # requests:      %f\n", prefix, (runlistlen/((simtime - warmuptime) * (double) setsize)));
   fprintf(outputfile, "%sMaximum # requests:      %d\n", prefix, maxlistlen);
   fprintf(outputfile, "%sEnd # requests:          %d\n", prefix, listlen);
   fprintf(outputfile, "%sAverage queue length:    %f\n", prefix, ((runlistlen - runoutstanding)/((simtime - warmuptime) * (double) setsize)));
   fprintf(outputfile, "%sMaximum queue length:    %d\n", prefix, maxqlen);
   fprintf(outputfile, "%sEnd queued requests:     %d\n", prefix, (listlen - numoutstanding));

   for (i=0; i<setsize; i++) {
      statset[i] = &set[i]->qtimestats;
   }
   stat_print_set(statset, setsize, prefix);

   for (i=0; i<setsize; i++) {
      statset[i] = &set[i]->accstats;
   }
   stat_print_set(statset, setsize, prefix);
}


void ioqueue_printstats(set, setsize, sourcestr)
ioqueue **set;
int setsize;
char *sourcestr;
{
   int to_scheme, pri_scheme;
   int numcomplete = 0;
   int i;
   int numreads = 0;
   int numwrites = 0;
   int numreqs = 0;
   int seqreads = 0;
   int seqwrites = 0;
   int printcritstats = 1;
   double idletime = 0.0;
   int subcheck = 0;
   int switches = 0;
   int timeouts = 0;
   int halfouts = 0;
   int overlapscombed = 0;
   int readoverlapscombed = 0;
   subqueue *subset[3*MAXDISKS];
   statgen * statset[300];
   char prefix[80];

   ASSERT(setsize <= MAXDISKS);

   for (i=0; i<setsize; i++) {

      if (set[i]->printqueuestats == FALSE)
         return;

      if ((set[i]->to_scheme != NOTIMEOUT) || (set[i]->pri_scheme != ALLEQUAL)) {
	 to_scheme = set[i]->to_scheme;
	 pri_scheme = set[i]->pri_scheme;
         subcheck = 1;
      }

      statset[(3*i)] = &set[i]->base.outtimestats;
      statset[((3*i)+1)] = &set[i]->timeout.outtimestats;
      statset[((3*i)+2)] = &set[i]->priority.outtimestats;

      printcritstats &= set[i]->printcritstats;
      numreads += set[i]->base.numreads + set[i]->timeout.numreads + set[i]->priority.numreads;
/*
      numreads -= (set[i]->timeoutreads + set[i]->halfoutreads);
*/
      numwrites += set[i]->base.numwrites + set[i]->timeout.numwrites + set[i]->priority.numwrites;
/*
      numwrites -= (set[i]->timeouts - set[i]->timeoutreads);
      numwrites -= (set[i]->halfouts - set[i]->halfoutreads);
*/
      seqreads += set[i]->seqreads;
      seqwrites += set[i]->seqwrites;
      idletime += stat_get_runval(&set[i]->idlestats);
      overlapscombed += set[i]->overlapscombed;
      readoverlapscombed += set[i]->readoverlapscombed;
   }
   numreqs = (double) (numreads + numwrites);
   idletime /= (double) setsize;

   numcomplete = stat_get_count_set(statset, (3*setsize));

   fprintf(outputfile, "%sTotal Requests handled:\t%d\n", sourcestr, numcomplete);
   fprintf(outputfile, "%sRequests per second:   \t%f\n", sourcestr, ((double)1000 * (double)numcomplete / (simtime - warmuptime)));
   fprintf(outputfile, "%sCompletely idle time:  \t%f   \t%f\n", sourcestr, idletime, (idletime / (simtime - warmuptime)));

   stat_print_set(statset, (3*setsize), sourcestr);

   fprintf(outputfile, "%sOverlaps combined:     \t%d\t%f\n", sourcestr, overlapscombed, ((double) overlapscombed / (double) max(numcomplete,1)));
   fprintf(outputfile, "%sRead overlaps combined:\t%d\t%f\t%f\n", sourcestr, readoverlapscombed, ((double) readoverlapscombed / (double) max(overlapscombed,1)), ((double) readoverlapscombed / (double) max(numreads,1)));

   if (printcritstats) {
      for (i=0; i<setsize; i++) {
         subset[(3*i)] = &set[i]->base;
         subset[((3*i)+1)] = &set[i]->timeout;
         subset[((3*i)+2)] = &set[i]->priority;
      }
      ioqueue_printcritstats (subset, (3*setsize), sourcestr, numreqs);
   }

   fprintf(outputfile, "%sNumber of reads:    %6d  \t%f\n", sourcestr, numreads, ((double) numreads / max(numreqs,1)));
   fprintf(outputfile, "%sNumber of writes:   %6d  \t%f\n", sourcestr, numwrites, ((double) numwrites / max(numreqs,1)));
   fprintf(outputfile, "%sSequential reads:   %6d  \t%f  \t%f\n", sourcestr, seqreads, ((double) seqreads / (double) max(numreads,1)), ((double) seqreads / max(numreqs,1)));
   fprintf(outputfile, "%sSequential writes:  %6d  \t%f  \t%f\n", sourcestr, seqwrites, ((double) seqwrites / (double) max(numwrites,1)), ((double) seqwrites / max(numreqs,1)));
   ioqueue_printqueuestats(set, setsize, sourcestr);
   ioqueue_printintarrstats(set, setsize, sourcestr);
   ioqueue_printidlestats(set, setsize, sourcestr);
   ioqueue_printsizestats(set, setsize, sourcestr);
   for (i=0; i<setsize; i++) {
      statset[(3*i)] = &set[i]->base.instqueuelen;
      statset[((3*i)+1)] = &set[i]->timeout.instqueuelen;
      statset[((3*i)+2)] = &set[i]->priority.instqueuelen;
   }
   stat_print_set(statset, (3*setsize), sourcestr);
   for (i=0; i<setsize; i++) {
      statset[(3*i)] = &set[i]->base.infopenalty;
      statset[((3*i)+1)] = &set[i]->timeout.infopenalty;
      statset[((3*i)+2)] = &set[i]->priority.infopenalty;
   }
   stat_print_set(statset, (3*setsize), sourcestr);
   if (subcheck) {
      fprintf (outputfile, "%sBase queue statistics\n", sourcestr);
      for (i=0; i<setsize; i++) {
	 subset[i] = &set[i]->base;
      }
      sprintf(prefix, "%sbase ", sourcestr);
      ioqueue_subqueue_printstats(subset, setsize, prefix, printcritstats);
      if (to_scheme != NOTIMEOUT) {
         fprintf (outputfile, "%sTimeout queue statistics\n", sourcestr);
	 switches = 0;
         for (i=0; i<setsize; i++) {
	    subset[i] = &set[i]->timeout;
	    switches += set[i]->timeout.switches;
	    timeouts += set[i]->timeouts;
	    halfouts += set[i]->halfouts;
         }
	 sprintf(prefix, "%stimeout ", sourcestr);
	 fprintf (outputfile, "%sRequests switched to timeout queue:   %d\n", sourcestr, switches);
	 fprintf(outputfile, "%sTimed out requests:                   %d\n", sourcestr, timeouts);
	 fprintf(outputfile, "%sHalfway timed out requests:           %d\n", sourcestr, halfouts);
         ioqueue_subqueue_printstats(subset, setsize, prefix, printcritstats);
      }
      if (pri_scheme != ALLEQUAL) {
         fprintf(outputfile, "%sPriority queue statistics\n", sourcestr);
	 switches = 0;
         for (i=0; i<setsize; i++) {
	    subset[i] = &set[i]->priority;
	    switches += set[i]->priority.switches;
         }
	 sprintf(prefix, "%spriority ", sourcestr);
	 fprintf (outputfile, "%sRequests switched to priority queue:   %d\n", sourcestr, switches);
         ioqueue_subqueue_printstats(subset, setsize, prefix, printcritstats);
      }
   }
}

