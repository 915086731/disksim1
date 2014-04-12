
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
#include "disksim_ioqueue.h"

#define CACHE_MAXSEGMENTS	10		/* For S-LRU */

#define CACHE_HASHSIZE		(ALLOCSIZE/sizeof(int))
#define CACHE_HASHMASK		(0x00000000 | (CACHE_HASHSIZE - 1))

/* cache replacement policies */

#define CACHE_REPLACE_MIN	1
#define CACHE_REPLACE_FIFO	1
#define CACHE_REPLACE_SLRU	2
#define CACHE_REPLACE_RANDOM	3
#define CACHE_REPLACE_LIFO	4
#define CACHE_REPLACE_MAX	4

/* state components of atom */

#define CACHE_VALID		0x80000000
#define CACHE_DIRTY		0x40000000
#define CACHE_LOCKDOWN		0x20000000
#define CACHE_LOCKED		0x10000000
#define CACHE_ATOMFLUSH		0x08000000
#define CACHE_REALLOCATE_WRITE	0x04000000
#define CACHE_SEGNUM		0x000000FF		/* for S-LRU */

/* cache event flags */

#define CACHE_FLAG_WASBLOCKED		1
#define CACHE_FLAG_LINELOCKED_ALLOCATE	2

/* cache event types */

#define	CACHE_EVENT_IOREQ		0
#define CACHE_EVENT_ALLOCATE		1
#define CACHE_EVENT_READ		2
#define CACHE_EVENT_WRITE		3
#define CACHE_EVENT_SYNC		4
#define CACHE_EVENT_SYNCPART		5
#define CACHE_EVENT_READEXTRA		6
#define CACHE_EVENT_WRITEFILLEXTRA	7
#define CACHE_EVENT_IDLESYNC		8

/* cache write schemes */

#define CACHE_WRITE_MIN		1
#define CACHE_WRITE_SYNCONLY	1
#define CACHE_WRITE_THRU	2
#define CACHE_WRITE_BACK	3
#define CACHE_WRITE_MAX		3

/* cache allocate policy flags */

#define CACHE_ALLOCATE_MIN		0
#define CACHE_ALLOCATE_NONDIRTY		1
#define CACHE_ALLOCATE_MAX		1

/* cache prefetch types */

#define CACHE_PREFETCH_MIN		0
#define CACHE_PREFETCH_NONE		0
#define CACHE_PREFETCH_FRONTOFLINE	1
#define CACHE_PREFETCH_RESTOFLINE	2
#define CACHE_PREFETCH_ALLOFLINE	3
#define CACHE_PREFETCH_MAX		3

/* cache background flush types */

#define CACHE_FLUSH_MIN		0
#define CACHE_FLUSH_DEMANDONLY	0
#define CACHE_FLUSH_PERIODIC	1
#define CACHE_FLUSH_MAX		1


#define CACHE_LOCKSPERSTRUCT	15

typedef struct cachelockh {
   struct ioreq_ev *entry[CACHE_LOCKSPERSTRUCT];
   struct cachelockh *next;
} cache_lockholders;

typedef struct cachelockw {
   struct cacheevent *entry[CACHE_LOCKSPERSTRUCT];
   struct cachelockw *next;
} cache_lockwaiters;

typedef struct cacheatom {
   struct cacheatom *hash_next;
   struct cacheatom *hash_prev;
   struct cacheatom *line_next;
   struct cacheatom *line_prev;
   int devno;
   int lbn;
   int state;
   struct cacheatom *lru_next;
   struct cacheatom *lru_prev;
   cache_lockholders *readlocks;
   ioreq_event *writelock;
   cache_lockwaiters *lockwaiters;
   int busno;
   int slotno;
} cache_atom;

typedef struct cacheevent {
   double time;
   int type;
   struct cacheevent *next;
   struct cacheevent *prev;
   void (*donefunc)();		/* Function to call when complete */
   void *doneparam;		/* parameter for donefunc */
   int flags;
   ioreq_event *req;
   int accblkno;		/* start blkno of waited for ioacc */
   cache_atom *cleaned;
   cache_atom *lineprev;
   int locktype;
   int lockstop;
   int allocstop;
   struct cacheevent *waitees;
   int validpoint;
} cache_event;

typedef struct {
   int reads;
   int readatoms;
   int readhitsfull;
   int readhitsfront;
   int readhitsback;
   int readhitsmiddle;
   int readmisses;
   int fillreads;
   int fillreadatoms;
   int writes;
   int writeatoms;
   int writehitsclean;
   int writehitsdirty;
   int writemisses;
   int writeinducedfills;
   int writeinducedfillatoms;
   int destagewrites;
   int destagewriteatoms;
   int getblockreadstarts;
   int getblockreaddones;
   int getblockwritestarts;
   int getblockwritedones;
   int freeblockcleans;
   int freeblockdirtys;
} cache_stats;

typedef struct {                    /* per-set structure for set-associative */
   cache_atom *freelist;
   int space;
   cache_atom *lru[CACHE_MAXSEGMENTS];
   int numactive[CACHE_MAXSEGMENTS];
   int maxactive[CACHE_MAXSEGMENTS];
} cache_mapentry;

typedef struct cache_def {
   cache_atom *hash[CACHE_HASHSIZE];
   void (*issuefunc)();				/* to issue a disk access    */
   void *issueparam;				/* first param for issuefunc */
   struct ioq * (*queuefind)();			/* to get ioqueue ptr for dev*/
   void *queuefindparam;			/* first param for queuefind */
   void (*wakeupfunc)();			/* to re-activate slept proc */
   void *wakeupparam;				/* first param for wakeupfunc */
   int size;					/* in 512B blks  */
   int atomsize;
   int numsegs;					/* for S-LRU */
   int linesize;
   int atomsperbit;
   int lockgran;
   int sharedreadlocks;
   int maxreqsize;
   int replacepolicy;
   int mapmask;
   int writescheme;
   int read_prefetch_type;
   int writefill_prefetch_type;
   int prefetch_waitfor_locks;
   int startallflushes;
   int allocatepolicy;
   int read_line_by_line;
   int write_line_by_line;
   int maxscatgath;
   int no_write_allocate;
   int flush_policy;
   double flush_period;
   double flush_idledelay;
   int flush_maxlinecluster;
   cache_mapentry *map;
   int linebylinetmp;
   cache_event *IOwaiters;
   cache_event *partwrites;
   cache_event *linewaiters;
   cache_stats stat;
} cache_def;

int cachedebugprinthack = 0;

/* prototypes */
int cache_read_continue();
int cache_write_continue();
int cache_free_block_dirty();


int cache_get_maxreqsize(cache)
cache_def *cache;
{
   if (cache) {
       return(cache->maxreqsize);
   }
   return(0);
}


void cache_empty_donefunc(doneparam, req)
void *doneparam;
ioreq_event *req;
{
   addtoextraq((event *) req);
}


int cache_concatok(cache, blkno1, bcount1, blkno2, bcount2)
cache_def *cache;
int blkno1;
int bcount1;
int blkno2;
int bcount2;
{
   if ((cache->size) && (cache->maxscatgath != 0)) {
      int linesize = max(cache->linesize, 1);
      int lineno1 = blkno1 / linesize;
      int lineno2 = (blkno2 + bcount2 - 1) / linesize;
      int scatgathcnt = lineno2 - lineno1;
      if (scatgathcnt > cache->maxscatgath) {
	 return(0);
      }
   }
   return(1);
}


void cache_waitfor_IO(cache, waitcnt, cachereq, ioacc)
cache_def *cache;
int waitcnt;
cache_event *cachereq;
ioreq_event *ioacc;
{
   cachereq->next = cache->IOwaiters;
   cachereq->prev = NULL;
   if (cachereq->next) {
      cachereq->next->prev = cachereq;
   }
   cache->IOwaiters = cachereq;
/*
fprintf (outputfile, "IOwaiters %x, next %x, nextprev %x\n", cachereq, cachereq->next, ((cachereq->next) ? cachereq->next->prev : 0));
*/
   cachereq->accblkno = ioacc->blkno;
}


void cache_insert_new_into_hash(cache, new)
cache_def *cache;
cache_atom *new;
{
   new->hash_next = cache->hash[(new->lbn & CACHE_HASHMASK)];
   cache->hash[(new->lbn & CACHE_HASHMASK)] = new;
   new->hash_prev = NULL;
   if (new->hash_next) {
      new->hash_next->hash_prev = new;
   }
}


void cache_remove_entry_from_hash(cache, old)
cache_def *cache;
cache_atom *old;
{
	  /* Line must be in hash if to be removed! */
   ASSERT((old->hash_prev != NULL) || (old->hash_next != NULL) || (cache->hash[(old->lbn & CACHE_HASHMASK)] == old));

   if (old->hash_prev) {
      old->hash_prev->hash_next = old->hash_next;
   } else {
      cache->hash[(old->lbn & CACHE_HASHMASK)] = old->hash_next;
   }
   if (old->hash_next) {
      old->hash_next->hash_prev = old->hash_prev;
   }
   old->hash_next = NULL;
   old->hash_prev = NULL;
}


int cache_count_dirty_atoms(cache)
cache_def *cache;
{
   int i;
   int dirty = 0;

   for (i=0; i<CACHE_HASHMASK; i++) {
      cache_atom *tmp = cache->hash[i];
      while (tmp) {
         dirty += (tmp->state & CACHE_DIRTY) ? 1 : 0;
         tmp = tmp->hash_next;
      }
   }
   return(dirty);
}


cache_atom * cache_find_atom(cache, devno, lbn)
cache_def *cache;
int devno;
int lbn;
{
   cache_atom *tmp = cache->hash[(lbn & CACHE_HASHMASK)];
/*
fprintf (outputfile, "Entered cache_find_atom: devno %d, lbn %d, CACHE_HASHMASK %x, setno %d\n", devno, lbn, CACHE_HASHMASK, (lbn & CACHE_HASHMASK));
*/
   while ((tmp) && ((tmp->lbn != lbn) || (tmp->devno != devno))) {
      tmp = tmp->hash_next;
   }
   return(tmp);
}


void cache_remove_lbn_from_hash(cache, devno, lbn)
cache_def *cache;
int devno;
int lbn;
{
   cache_atom *tmp;

   if ((tmp = cache_find_atom(cache, devno, lbn))) {
      cache_remove_entry_from_hash(cache, tmp);
   }
}


void cache_check_for_residence(cache, devno, lbn, size, miss)
cache_def *cache;
int devno;
int lbn; 
int size;
int *miss;
{
   cache_atom *line = NULL;
   int i;

   for (i=0; i<size; i++) {
      if (line == NULL) {
         line = cache_find_atom(cache, devno, (lbn + i));
      }
      if ((line == NULL) || ((line->state & CACHE_VALID) == 0)) {
         miss[(i & INV_BITS_PER_INT_MASK)] |= 1 << (i & BITS_PER_INT_MASK);
      }
      if (line) {
         line = line->line_next;
      }
   }
}


/* Use for setting VALID, LOCKDOWN, DIRTY and other atom state bits */

void cache_set_state(cache, devno, lbn, size, mask)
cache_def *cache;
int devno;
int lbn;
int size;
int mask;
{
   cache_atom *line = NULL;
   int i;

   for (i=0; i<size; i++) {
      if (line == NULL) {
         line = cache_find_atom(cache, devno, (lbn + i));
      }
	       /* Can't change state of unallocated cache atoms */
      ASSERT(line != NULL);

      line->state |= mask;
      line = line->line_next;
   }
}


/* Use for clearing VALID, LOCKDOWN, DIRTY and other atom state bits */

void cache_reset_state(cache, devno, lbn, size, mask)
cache_def *cache;
int devno;
int lbn;
int size;
int mask;
{
   cache_atom *line = NULL;
   int i;

   for (i=0; i<size; i++) {
      if (line == NULL) {
         line = cache_find_atom(cache, devno, (lbn + i));
      }
	       /* Can't change state of unallocated cache atoms */
      ASSERT(line != NULL);

      line->state &= ~mask;
      line = line->line_next;
   }
}


void cache_add_to_lrulist(map, line, segnum)
cache_mapentry *map;
cache_atom *line;
int segnum;
{
   cache_atom **head;

   if (segnum == CACHE_SEGNUM) {
      head = &map->freelist;
   } else {
      head = &map->lru[segnum];
      map->numactive[segnum]++;
   }
   line->state |= segnum;
   if (*head) {
      line->lru_next = *head;
      line->lru_prev = (*head)->lru_prev;
      (*head)->lru_prev = line;
      line->lru_prev->lru_next = line;
   } else {
      line->lru_next = line;
      line->lru_prev = line;
      *head = line;
   }
}


void cache_remove_from_lrulist(map, line, segnum)
cache_mapentry *map;
cache_atom *line;
int segnum;
{
   cache_atom **head;

   if (segnum == CACHE_SEGNUM) {
      head = &map->freelist;
   } else {
      head = &map->lru[segnum];
      map->numactive[segnum]--;
   }
   if (line->lru_next != line) {
      line->lru_prev->lru_next = line->lru_next;
      line->lru_next->lru_prev = line->lru_prev;
      if (*head == line) {
         *head = line->lru_next;
      }
   } else {
      *head = NULL;
   }
   line->state &= ~CACHE_SEGNUM;
   line->lru_next = NULL;
   line->lru_prev = NULL;
}


/* Reset state of LRU list given access to line */

void cache_access(cache, line)
cache_def *cache;
cache_atom *line;
{
   int set;
   int segnum = 0;

   if (cache->replacepolicy != CACHE_REPLACE_SLRU) {
      return;
   }
   while (line->line_prev) {
      line = line->line_prev;
   }
   set = (cache->mapmask) ? (line->lbn % cache->mapmask) : 0;
   if (line->lru_next) {
      segnum = line->state & CACHE_SEGNUM;
      cache_remove_from_lrulist(&cache->map[set], line, segnum);
      if (segnum != (cache->numsegs-1)) {
         segnum = (segnum + 1) & CACHE_SEGNUM;
      }
   }
   cache_add_to_lrulist(&cache->map[set], line, segnum);
   while ((segnum) && (cache->map[set].numactive[segnum] == cache->map[set].maxactive[segnum])) {
      line = cache->map[set].lru[segnum];
      cache_remove_from_lrulist(&cache->map[set], line, segnum);
      segnum--;
      cache_add_to_lrulist(&cache->map[set], line, segnum);
   }
}


void cache_replace_waitforline(cache, allocdesc)
cache_def *cache;
cache_event *allocdesc;
{
if (cachedebugprinthack)
fprintf (outputfile, "entered cache_replace_waitforline: linelocked %d\n", (allocdesc->flags & CACHE_FLAG_LINELOCKED_ALLOCATE));

   if (cache->linewaiters) {
      allocdesc->next = cache->linewaiters->next;
      cache->linewaiters->next = allocdesc;
      if (!(allocdesc->flags & CACHE_FLAG_LINELOCKED_ALLOCATE)) {
         cache->linewaiters = allocdesc;
      }
   } else {
      allocdesc->next = allocdesc;
      cache->linewaiters = allocdesc;
   }
   allocdesc->flags |= CACHE_FLAG_LINELOCKED_ALLOCATE;
}


cache_atom *cache_get_replace_startpoint(cache, set)
cache_def *cache;
int set;
{
   cache_atom *line = cache->map[set].lru[0];

   if (line) {
      if (cache->replacepolicy == CACHE_REPLACE_RANDOM) {
         int choice = cache->map[set].numactive[0] * drand48();
         int i;
         for (i=0; i<choice; i++) {
            line = line->lru_prev;
         }
      } else if (cache->replacepolicy == CACHE_REPLACE_LIFO) {
         line = line->lru_prev;
      } else if ((cache->replacepolicy != CACHE_REPLACE_FIFO) && (cache->replacepolicy != CACHE_REPLACE_SLRU)) {
         fprintf(stderr, "Unknown replacement policy at cache_get_replace_startpoint: %d\n", cache->replacepolicy);
         exit(0);
      }
   }
   return(line);
}


/* Add identifier to lockstruct only if not already present */

void cache_add_to_lockstruct(head, identifier)
struct cachelockw **head;
void *identifier;
{
   struct cachelockw *tmp = *head;
   int start = FALSE;
   int i;

   if (tmp == NULL) {
      tmp = (struct cachelockw *) getfromextraq();
      bzero ((char *)tmp, sizeof(struct cachelockw));
      tmp->entry[0] = identifier;
      *head = tmp;
   } else {
      while (tmp->next) {
         for (i=0; i<CACHE_LOCKSPERSTRUCT; i++) {
            if (tmp->entry[i] == identifier) {
               return;
            }
         }
         tmp = tmp->next;
      }
      for (i=0; i<CACHE_LOCKSPERSTRUCT; i++) {
         if (tmp->entry[i] == identifier) {
            return;
         } else if (tmp->entry[i]) {
            start = TRUE;
         } else if (start) {
            tmp->entry[i] = identifier;
            return;
         }
      }
      tmp->next = (struct cachelockw *) getfromextraq();
      tmp = tmp->next;
      bzero ((char *)tmp, sizeof(struct cachelockw));
      tmp->entry[0] = identifier;
   }
}


/* Does anyone have a write lock on the atom?? */

int cache_atom_iswritelocked(cache, target)
cache_def *cache;
cache_atom *target;
{
   while (target->lbn % cache->lockgran) {
      target = target->line_prev;
   }
   return(target->writelock != NULL);
}


/* Does anyone have any lock on the atom?? */

int cache_atom_islocked(cache, target)
cache_def *cache;
cache_atom *target;
{
   while (target->lbn % cache->lockgran) {
      target = target->line_prev;
   }
   return(target->readlocks || target->writelock);
}


/* should change this code so that newly enabled "I-streams" do not */
/* necessarily preempt the "I-stream" that freed the lock...        */

void cache_give_lock_to_waiter(cache, target, rwdesc)
cache_def *cache;
cache_atom *target;
cache_event *rwdesc;
{
   switch (rwdesc->type) {

      case CACHE_EVENT_READ:
                               cache_read_continue(cache, rwdesc);
                               break;

      case CACHE_EVENT_WRITE:
                               cache_write_continue(cache, rwdesc);
                               break;

      default:
       fprintf(stderr, "Unknown type at cache_give_lock_to_waiter: %d\n", rwdesc->type);
       exit(0);
   }
}


void cache_lock_free(cache, target)
cache_def *cache;
cache_atom *target;
{
   cache_lockwaiters *tmp;
   int writelocked = FALSE;

	      /* Can't give away a line that is writelock'd */
   ASSERT(!target->writelock);

   if ((tmp = target->lockwaiters)) {
      int i = 0;
      while ((tmp) && (!writelocked) && (!target->writelock)) {
         cache_event *waiter = NULL;
         if (tmp->entry[i]) {
            writelocked = (cache->sharedreadlocks == 0) || tmp->entry[i]->locktype;
            if ((!writelocked) || (target->readlocks == NULL)) {
               waiter = tmp->entry[i];
               tmp->entry[i] = NULL;
            }
         }
         i++;
         if (i == CACHE_LOCKSPERSTRUCT) {
            target->lockwaiters = tmp->next;
            addtoextraq((event *) tmp);
            tmp = target->lockwaiters;
            i = 0;
         }
         if (waiter) {
	    cache->wakeupfunc(cache->wakeupparam, waiter);
         }
      }
   } else if (cache->linewaiters) {
      int linesize = max(cache->linesize, 1);
      cache_event *allocdesc;
      while (target->lbn % linesize) {
         target = target->line_prev;
      }
      while (target) {
         if ((target->writelock) || (target->readlocks)) {
            return;
         }
         target = target->line_next;
      }
      allocdesc = cache->linewaiters->next;
      if (allocdesc->next == allocdesc) {
         cache->linewaiters = NULL;
      } else {
         cache->linewaiters->next = allocdesc->next;
      }
      allocdesc->next = NULL;
if (cachedebugprinthack)
fprintf (outputfile, "allocation continuing: line finally freed\n");
      cache->wakeupfunc(cache->wakeupparam, allocdesc);
   }
}


/* gransize is assumed to be a factor of linesize */

int cache_get_write_lock(cache, target, rwdesc)
cache_def *cache;
cache_atom *target;
cache_event *rwdesc;
{

if (cachedebugprinthack)  
fprintf (outputfile, "Entered cache_get_write_lock: target %p, lbn %d, lockgran %d\n", target, target->lbn, cache->lockgran);

   while (target->lbn % cache->lockgran) {
      target = target->line_prev;
   }

if (cachedebugprinthack)  
fprintf (outputfile, "doing cache_get_write_lock: target %p, lbn %d, lockgran %d\n", target, target->lbn, cache->lockgran);

   if (target->writelock == rwdesc->req) {
      return(cache->lockgran);
   } else if ((target->writelock) || (target->readlocks)) {
      rwdesc->locktype = 1;
      cache_add_to_lockstruct(&target->lockwaiters, rwdesc);
      return(0);
   } else {
      target->writelock = rwdesc->req;
      return(cache->lockgran);
   }
}


int cache_free_write_lock(cache, target, owner)
cache_def *cache;
cache_atom *target;
ioreq_event *owner;
{

if (cachedebugprinthack)  
fprintf (outputfile, "Entered cache_free_write_lock: target %p, lbn %d, lockgran %d\n", target, target->lbn, cache->lockgran);

   while (target->lbn % cache->lockgran) {
      target = target->line_prev;
   }

if (cachedebugprinthack)  
fprintf (outputfile, "doing cache_free_write_lock: target %p, lbn %d, lockgran %d\n", target, target->lbn, cache->lockgran);

   if (owner == target->writelock) {
      target->writelock = NULL;
      cache_lock_free(cache, target);
      return(cache->lockgran);
   } else {
      return(0);
   }
}


int cache_get_read_lock(cache, target, rwdesc)
cache_def *cache;
cache_atom *target;
cache_event *rwdesc;
{

if (cachedebugprinthack)  
fprintf (outputfile, "Entered cache_get_read_lock: target %p, lbn %d, lockgran %d\n", target, target->lbn, cache->lockgran);

   if (!cache->sharedreadlocks) {
      return(cache_get_write_lock(cache, target, rwdesc));
   }
   while (target->lbn % cache->lockgran) {
      target = target->line_prev;
   }

if (cachedebugprinthack)  
fprintf (outputfile, "doing cache_get_read_lock: target %p, lbn %d, lockgran %d\n", target, target->lbn, cache->lockgran);

   if ((target->writelock) && (target->writelock != rwdesc->req)) {
      rwdesc->locktype = 0;
      cache_add_to_lockstruct(&target->lockwaiters, rwdesc);
      return(0);
   } else {
      cache_add_to_lockstruct(&target->readlocks, rwdesc->req);
      if (target->writelock) {
         target->writelock = NULL;
         cache_lock_free(cache, target);
      }
      return(cache->lockgran);
   }
}


int cache_free_read_lock(cache, target, owner)
cache_def *cache;
cache_atom *target;
ioreq_event *owner;
{
   cache_lockholders *tmp;
   int found = FALSE;
   int i;

if (cachedebugprinthack)  
fprintf (outputfile, "Entered cache_free_read_lock: target %p, lbn %d, lockgran %d\n", target, target->lbn, cache->lockgran);

   if (!cache->sharedreadlocks) {
      return(cache_free_write_lock(cache, target, owner));
   }

if (cachedebugprinthack)  
fprintf (outputfile, "doing cache_free_read_lock: target %p, lbn %d, lockgran %d\n", target, target->lbn, cache->lockgran);

   while (target->lbn % cache->lockgran) {
      target = target->line_prev;
   }
   tmp = target->readlocks;
   while ((tmp) && (!found)) {
      int active = FALSE;
      for (i=0; i<CACHE_LOCKSPERSTRUCT; i++) {
         if (tmp->entry[i] == owner) {
            tmp->entry[i] = 0;
            found = TRUE;
         } else if (tmp->entry[i]) {
            active = TRUE;
         }
      }
      if ((tmp == target->readlocks) && (active == FALSE)) {
         target->readlocks = tmp->next;
         addtoextraq((event *) tmp);
         tmp = target->readlocks;
      } else {
         tmp = tmp->next;
      }
   }
   if (found) {
      if (!target->readlocks) {
         cache_lock_free(cache, target);
      }
      return(cache->lockgran);
   } else {
      return(0);
   }
}


void cache_get_read_lock_range(cache, start, end, startatom, waiter)
cache_def *cache;
int start;
int end;
cache_atom *startatom;
cache_event *waiter;
{
   cache_atom *line = (startatom->lbn == start) ? startatom : NULL;
   int lockgran = 1;
   int i;

   for (i=start; i<=end; i++) {
      if (line == NULL) {
         line = cache_find_atom(cache, startatom->devno, i);
         ASSERT(line != NULL);
      }
      if ((line->lbn % lockgran) == 0) {
         lockgran = cache_get_read_lock(cache, line, waiter);
             /* Must not fail to acquire lock */
         ASSERT(lockgran != 0);
      }
      line = line->line_next;
   }
}


int cache_issue_flushreq(cache, start, end, startatom, waiter)
cache_def *cache;
int start;
int end;
cache_atom *startatom;
cache_event *waiter;
{
   ioreq_event *flushreq;
   ioreq_event *flushwait;
   int waiting = (cache->IOwaiters == waiter) ? 1 : 0;

if (cachedebugprinthack)  
fprintf (outputfile, "Entered issue_flushreq: start %d, end %d\n", start, end);

   flushreq = (ioreq_event *) getfromextraq();
   flushreq->devno = startatom->devno;
   flushreq->blkno = start;
   flushreq->bcount = end - start + 1;
   flushreq->busno = startatom->busno;
   flushreq->slotno = startatom->slotno;
   flushreq->type = IO_ACCESS_ARRIVE;
   flushreq->flags = 0;

   flushwait = (ioreq_event *) getfromextraq();
   flushwait->type = IO_REQUEST_ARRIVE;
   flushwait->devno = flushreq->devno;
   flushwait->blkno = flushreq->blkno;
   flushwait->bcount = flushreq->bcount;
   flushwait->next = waiter->req;
   flushwait->prev = NULL;
   if (waiter->req) {
      waiter->req->prev = flushwait;
   }
   waiter->req = flushwait;
   waiter->accblkno = -1;
   if (!waiting) {
      cache_waitfor_IO(cache, 1, waiter, flushwait);
   }
   waiter->accblkno = -1;
   cache->stat.destagewrites++;
   cache->stat.destagewriteatoms += end - start + 1;

   cache_get_read_lock_range(cache, start, end, startatom, waiter);

if (cachedebugprinthack)  
fprintf (outputfile, "Issueing dirty block write-back: blkno %d, bcount %d, devno %d\n", flushreq->blkno, flushreq->bcount, flushreq->devno);

   cache->issuefunc(cache->issueparam, flushreq);
   return(1);
}


int cache_flush_cluster(cache, devno, blkno, linecnt, dir)
cache_def *cache;
int devno;
int blkno;
int linecnt;
int dir;
{
   cache_atom *line = NULL;
   int lastclean = 0;
   int writelocked;

   ASSERT1((dir == 1) || (dir == -1), "dir", dir);
   while (linecnt <= cache->flush_maxlinecluster) {
      if (line == NULL) {
	 line = cache_find_atom(cache, devno, (blkno+dir));
	 if ((line == NULL) || (lastclean)) {
	    break;
	 }
	 linecnt++;
	 continue;
      }
      writelocked = cache_atom_iswritelocked(cache, line);
      if ((line->state & CACHE_DIRTY) && (!writelocked)) {
         line->state &= ~CACHE_DIRTY;
	 lastclean = 0;
	 blkno = line->lbn;
      } else if ((writelocked) || (!(line->state & CACHE_VALID))) {
	 break;
      } else {
	 lastclean = 1;
      }
      line = (dir == 1) ? line->line_next : line->line_prev;
   }
   return(blkno);
}


int cache_initiate_dirty_block_flush(cache, dirtyline, allocdesc)
cache_def *cache;
cache_atom *dirtyline;
cache_event *allocdesc;
{
   cache_atom *dirtyatom;
   int dirtyend;
   int dirtystart = -1;
   cache_atom *tmp = dirtyline;
   int flushcnt = 0;

if (cachedebugprinthack)  
fprintf (outputfile, "Entered cache_initiate_dirty_block_flush: %d\n", dirtyline->lbn);

   while (tmp) {
      int writelocked = cache_atom_iswritelocked(cache, tmp);
      if ((tmp->state & CACHE_DIRTY) && (!writelocked)) {
         tmp->state &= ~CACHE_DIRTY;
         if (dirtystart == -1) {
            dirtyatom = tmp;
            dirtystart = tmp->lbn;
         }
         dirtyend = tmp->lbn;
      } else if ((dirtystart != -1) && ((!(tmp->state & CACHE_VALID)) || (writelocked))) {
         if ((cache->flush_maxlinecluster > 1) && (dirtystart == dirtyline->lbn)) {
            dirtystart = cache_flush_cluster(cache, dirtyatom->devno, dirtystart, 1, -1);
         }
         if (cache_issue_flushreq(cache, dirtystart, dirtyend, dirtyatom, allocdesc) == 0) {
            return(flushcnt);
         }
         dirtystart = -1;
         flushcnt++;
      }
      tmp = tmp->line_next;
   }
   if (dirtystart != -1) {
      int linesize = max(cache->linesize, 1);
      int linecnt;
      if ((cache->flush_maxlinecluster > 1) && (dirtystart == dirtyline->lbn)) {
         dirtystart = cache_flush_cluster(cache, dirtyatom->devno, dirtystart, 1, -1);
      }
      linecnt = 1 + ((dirtyline->lbn - dirtystart) / linesize);
      if ((linecnt < cache->flush_maxlinecluster) && (dirtyend == (dirtyline->lbn + linesize -1))) {
	 dirtyend = cache_flush_cluster(cache, dirtyatom->devno, dirtyend, linecnt, 1);
      }
      flushcnt += cache_issue_flushreq(cache, dirtystart, dirtyend, dirtyatom, allocdesc);
   }

if (cachedebugprinthack)  
fprintf (outputfile, "flushcnt %d\n", flushcnt);

   return(flushcnt);
}


cache_event *cache_get_flushdesc()
{
   cache_event *flushdesc = (cache_event *) getfromextraq();
   flushdesc->type = CACHE_EVENT_SYNC;
   flushdesc->donefunc = cache_empty_donefunc;
   flushdesc->req = NULL;
   return(flushdesc);
}


void cache_cleanup_flushdesc(flushdesc)
cache_event *flushdesc;
{
   if (flushdesc->req) {
      if (flushdesc->req->next == NULL) {
         flushdesc->accblkno = flushdesc->req->blkno;
      }
   } else {
      addtoextraq((event *) flushdesc);
   }
}


/* Not currently dealing with case of two-handed flushing.  Easiest way to */
/* do this will be to allocate the cache as one big chunk of memory.  Then,*/
/* use the addresses of cache_atoms rather than the pointers to traverse.  */

void cache_periodic_flush(timereq)
timer_event *timereq;
{
   cache_def *cache = (cache_def *) timereq->ptr;
   int segcnt = (cache->replacepolicy == CACHE_REPLACE_SLRU) ? cache->numsegs : 1;
   int i, j;
   cache_atom *line;
   cache_atom *stop;
   cache_atom *tmp;
   cache_event *flushdesc = cache_get_flushdesc();
   int flushcnt = 0;
   int startit;

   for (i=0; i<=cache->mapmask; i++) {
      for (j=0; j<segcnt; j++) {
         line = cache->map[i].lru[j];
         stop = line;
         startit = 1;
         while ((startit) || (line != stop)) {
            startit = 0;
            tmp = line;
            while (tmp) {
               if (tmp->state & CACHE_DIRTY) {
                  flushcnt += cache_initiate_dirty_block_flush(cache, tmp, flushdesc);
               }
               tmp = tmp->line_next;
            }
            line = line->lru_next;
         };
      }
   }
   cache_cleanup_flushdesc(flushdesc);
   timereq->time += cache->flush_period;
   addtointq(timereq);

if (cachedebugprinthack)  
fprintf (outputfile, "%f: cache_periodic_flush, %d flushes started\n", simtime, flushcnt);
}


void cache_idletime_detected(cache, idledevno)
cache_def *cache;
int idledevno;
{
   cache_atom *line = cache_get_replace_startpoint(cache, 0);
   cache_atom *stop = line;
   cache_atom *tmp;
   int segcnt = (cache->replacepolicy == CACHE_REPLACE_SLRU) ? cache->numsegs : 1;
   int i;
   cache_event *flushdesc;
   int startit;

   if (ioqueue_get_number_in_queue(cache->queuefind(cache->queuefindparam, idledevno))) {
      return;
   }

   flushdesc = cache_get_flushdesc();
   flushdesc->type = CACHE_EVENT_IDLESYNC;

   for (i=0; i<segcnt; i++) {
      if (i) {
         line = cache->map[0].lru[i];
         stop = line;
      }
      startit = 1;
      while ((startit) || (line != stop)) {
         startit = 0;
         if (line->devno == idledevno) {
            tmp = line;
            while (tmp) {
               if (tmp->state & CACHE_DIRTY) {
                  (void)cache_initiate_dirty_block_flush(cache, tmp, flushdesc);
                  if (flushdesc->req) {
                     goto cache_idletime_detected_idleused;
                  }
               }
               tmp = tmp->line_next;
            }
         }
         line = line->lru_next;
      }
   }

cache_idletime_detected_idleused:

   cache_cleanup_flushdesc(flushdesc);
}


void cache_unmap_line(cache, line, set)
cache_def *cache;
cache_atom *line;
int set;
{
   cache_atom *tmp;

   if (line->lru_next) {
      cache_remove_from_lrulist(&cache->map[set], line, (line->state & CACHE_SEGNUM));
   }
   if (cache->linesize == 0) {
      while ((tmp = line)) {
         line = line->line_next;
         tmp->line_next = NULL;
         tmp->line_prev = NULL;
         cache_remove_entry_from_hash(cache, tmp);
         cache_add_to_lrulist(&cache->map[set], tmp, CACHE_SEGNUM);
      }
   } else {
      cache_add_to_lrulist(&cache->map[set], line, CACHE_SEGNUM);
      while (line) {
         cache_remove_entry_from_hash(cache, line);
         line = line->line_next;
      }
   }
}


int cache_replace(cache, set, allocdesc)
cache_def *cache;
int set;
cache_event *allocdesc;
{
   int numwrites;
   cache_atom *line;
   cache_atom *tmp;
   cache_atom *stop;
   int dirty = FALSE;
   int locked = FALSE;
   cache_event *flushdesc = (cache->allocatepolicy & CACHE_ALLOCATE_NONDIRTY) ? NULL : allocdesc;

   if (cache->map[set].freelist) {
      return(0);
   }
   if ((line = cache_get_replace_startpoint(cache, set)) == NULL) {
          /* All lines between ownership */
      cache_replace_waitforline(cache, allocdesc);
      return(-1);
   }
   stop = line;

cache_replace_loop_continue:

   if (locked | dirty) {
      line = (cache->replacepolicy == CACHE_REPLACE_LIFO) ? line->lru_prev : line->lru_next;
   }
   if (line == stop) {
      if (locked) {
         if ((flushdesc) && (cache->allocatepolicy & CACHE_ALLOCATE_NONDIRTY)) {
            cache_cleanup_flushdesc(flushdesc);
         }
         cache_replace_waitforline(cache, allocdesc);
         return(-1);
      }
   }

   locked = FALSE;
   tmp = line;
   while (tmp) {
      if ((locked = (tmp->readlocks || tmp->writelock))) {
	 goto cache_replace_loop_continue;
      }
      tmp = tmp->line_next;
   }

   dirty = FALSE;
   tmp = line;
   while (tmp) {
      if ((dirty = tmp->state & CACHE_DIRTY)) {
         if (flushdesc == NULL) {
            flushdesc = cache_get_flushdesc();
         }
         numwrites = cache_initiate_dirty_block_flush(cache, tmp, flushdesc);
         if (cache->allocatepolicy & CACHE_ALLOCATE_NONDIRTY) {
            goto cache_replace_loop_continue;
         } else {
            return(numwrites);
         }
      }
      tmp = tmp->line_next;
   }

   cache_unmap_line(cache, line, set);

   return(0);
}


/* Return number of writeouts (dirty block flushes) to be waited for.      */
/* Also fill pointer to block allocated.  Null indicates that blocks must  */
/* be written out but no specific one has yet been allocated.              */

int cache_get_free_atom(cache, lbn, ret, allocdesc)
cache_def *cache;
int lbn;
cache_atom **ret;
cache_event *allocdesc;
{
   int writeouts = 0;
   int set = (cache->mapmask) ? (lbn % cache->mapmask) : 0;

if (cachedebugprinthack)  
fprintf (outputfile, "Entered cache_get_free_atom: lbn %d, set %d, freelist %p\n", lbn, set, cache->map[set].freelist);

   if (cache->map[set].freelist == NULL) {
      writeouts = cache_replace(cache, set, allocdesc);
   }
   if ((*ret = cache->map[set].freelist)) {
      cache_remove_from_lrulist(&cache->map[set], *ret, CACHE_SEGNUM);
   }
   return(writeouts);
}


/* Still need to add check for outstanding allocations by other people, to
avoid allocation replication */

cache_event *cache_allocate_space_continue(cache, allocdesc)
cache_def *cache;
cache_event *allocdesc;
{
   int numwrites = 0;
   cache_atom *new;
/*
   cache_atom *toclean = NULL;
   cache_atom *tocleanlast;
   int flushstart = -1;
*/

   int devno = allocdesc->req->devno;
   int lbn = allocdesc->lockstop;
   int stop = allocdesc->allocstop;
   cache_atom *cleaned = allocdesc->cleaned;
   cache_atom *lineprev = allocdesc->lineprev;
   int linesize = (cache->linesize) ? cache->linesize : 1;

if (cachedebugprinthack)  
fprintf (outputfile, "Entered allocate_space_continue: lbn %d, stop %d\n", lbn, stop);

   if (allocdesc->waitees) {
      cache_event *rwdesc = allocdesc->waitees;
      if (rwdesc->type == CACHE_EVENT_READ) {
         cache_read_continue(cache, rwdesc);
      } else {
         cache_write_continue(cache, rwdesc);
      }
      addtoextraq((event *) allocdesc);
      return(NULL);
   }
   while (lbn < stop) {
      if ((new = cleaned) == NULL) {
         numwrites += cache_get_free_atom(cache, lbn, &new, allocdesc);
      }
      if (numwrites == 0) {
	 ASSERT(new != NULL);
         do {
            new->devno = devno;
            new->lbn = lbn;
		 /* Re-allocated cache atom must not still be locked */
	    ASSERT((!new->writelock) && (!new->readlocks));
/*
            new->writelock = allocdesc->prev->req;
*/
            new->state = CACHE_LOCKDOWN;
            cache_insert_new_into_hash(cache, new);
            lbn++;
            new = (lbn % linesize) ? new->line_next : new;
         } while (lbn % linesize);
         if (cache->linesize == 0) {
            new->line_next = NULL;
            new->line_prev = lineprev;
            if (lineprev) {
               lineprev->line_next = new;
            }
            lineprev =  ((cache->linesize == -1) || (lbn % linesize)) ? new : NULL;
         }
/*
      } else if (cache->startallflushes) {
         if (flushstart == -1) {
            flushstart = i;
         }
         if (new) {
            if (toclean) {
               tocleanlast->line_next = new;
               tocleanlast = new;
            } else {
               toclean = new;
               tocleanlast = new;
            }
            while (tocleanlast->line_next) {
               tocleanlast = tocleanlast->line_next;
            }
         }
*/
      } else {
         allocdesc->lockstop = lbn;
         allocdesc->cleaned = new;
         allocdesc->lineprev = lineprev;
         /* This needs fixing! */
/*
         cache_waitfor_IO(cache, numwrites, allocdesc, NULL);
*/
         return(allocdesc);
      }
   }
/*
   if (numwrites) {
      allocdesc->lockstop = flushstart;
      allocdesc->cleaned = toclean;
      allocdesc->lineprev = lineprev;
      cache_wait(cache, numwrites, allocdesc);
      return(allocdesc);
   }
*/
   addtoextraq((event *) allocdesc);
   return(NULL);
}


cache_event * cache_allocate_space(cache, lbn, size, rwdesc)
cache_def *cache;
int lbn;
int size;
cache_event *rwdesc;
{
   cache_event *allocdesc = (cache_event *) getfromextraq();
   int linesize = max(1, cache->linesize);

if (cachedebugprinthack)  
fprintf (outputfile, "Entered cache_allocate_space: lbn %d, size %d, linesize %d\n", lbn, size, cache->linesize);

   allocdesc->type = CACHE_EVENT_ALLOCATE;
   allocdesc->req = rwdesc->req;
   allocdesc->flags = rwdesc->flags & CACHE_FLAG_LINELOCKED_ALLOCATE;
   allocdesc->lockstop = lbn - (lbn % linesize);
   allocdesc->allocstop =  lbn + size + (linesize - 1 - ((lbn + size - 1) % linesize));
   allocdesc->cleaned = NULL;
   allocdesc->lineprev = NULL;
   allocdesc->prev = rwdesc;
   allocdesc->waitees = NULL;
   if ((allocdesc = cache_allocate_space_continue(cache, allocdesc))) {
      allocdesc->waitees = rwdesc;
      rwdesc->flags |= allocdesc->flags & CACHE_FLAG_LINELOCKED_ALLOCATE;
   }
   return(allocdesc);
}


int cache_get_rw_lock(cache, locktype, rwdesc, line, i, stop)
cache_def *cache;
int locktype;
cache_event *rwdesc;
cache_atom *line;
int i;
int stop;
{
   int lockgran;
   cache_atom *tmp = line;
   int j = 0;

   int lbn = rwdesc->req->blkno;
   int devno = rwdesc->req->devno;

if (cachedebugprinthack)  
fprintf (outputfile, "Entered cache_get_rw_lock: lbn %d, i %d, stop %d, locktype %d\n", line->lbn, i, stop, locktype);

   while (j < stop) {
      if (locktype == 1) {
         lockgran = cache_get_read_lock(cache, tmp, rwdesc);
      } else {
         if (locktype == 3) {
            cache_free_read_lock(cache, tmp, rwdesc->req);
         }
         lockgran = cache_get_write_lock(cache, tmp, rwdesc);
      }

if (cachedebugprinthack)  
fprintf (outputfile, "got lock: lockgran %d, lbn %d\n", lockgran, tmp->lbn);

      if (lockgran == 0) {
         return(1);
      } else {
         if ((line->lbn != (lbn + i)) || (line->devno != devno)) {

            /* NOTE: this precaution only covers us when FIRST atom of line */
            /* changes identity.  Otherwise, must have other support.       */

            if (locktype == 1) {
               cache_free_read_lock(cache, tmp, rwdesc->req);
            } else {
               cache_free_write_lock(cache, tmp, rwdesc->req);
            }
            return(2);
         }
         j++;
         tmp = tmp->line_next;
         while ((tmp) && (tmp->lbn % lockgran)) {
            j++;
            tmp = tmp->line_next;
         }
      }
   }
   return(0);
}


int cache_issue_fillreq(cache, start, end, rwdesc, prefetchtype)
cache_def *cache;
int start;
int end;
cache_event *rwdesc;
int prefetchtype;
{
   ioreq_event *fillreq;
   int linesize = max(cache->linesize, 1);

if (cachedebugprinthack)  
fprintf (outputfile, "Entered cache_issue_fillreq: start %d, end %d, prefetchtype %d\n", start, end, prefetchtype);

   if (prefetchtype & CACHE_PREFETCH_FRONTOFLINE) {
      cache_atom *line = cache_find_atom(cache, rwdesc->req->devno, start);
      int validstart = -1;
      int lockgran = cache->lockgran;
      while (start % linesize) {
         line = line->line_prev;
         if (line->state & CACHE_VALID) {
/*
fprintf (outputfile, "already valid backwards: lbn %d\n", line->lbn);
*/
            break;
/*
            if (line->state & CACHE_DIRTY) {
               break;
            }
            if (validstart == -1) {
               validstart = line->lbn;
            }
*/
         } else {
            validstart = -1;
         }
         if ((line->lbn % lockgran) == (lockgran-1)) {
            if ((!cache->prefetch_waitfor_locks) && (cache_atom_islocked(cache, line))) {
               break;
            }
            if ((lockgran = cache_get_write_lock(cache, line, rwdesc)) == 0) {
               return(0);
            }
         }
         start--;
         line->state |= CACHE_VALID;
      }
/* Need to free some locks if do this...
      if (validstart != -1) {
         start = validstart;
      }
*/
   }
   if (prefetchtype & CACHE_PREFETCH_RESTOFLINE) {
      cache_atom *line = cache_find_atom(cache, rwdesc->req->devno, end);
      int validend = -1;
      int lockgran = cache->lockgran;
      while ((end+1) % linesize) {
         line = line->line_next;
         if (line->state & CACHE_VALID) {
/*
fprintf (outputfile, "already valid forwards: lbn %d\n", line->lbn);
*/
            break;
/*
            if (line->state & CACHE_DIRTY) {
               break;
            }
            if (validend == -1) {
               validend = line->lbn;
            }
*/
         } else {
            validend = -1;
         }
         if ((line->lbn % lockgran) == 0) {
            if ((!cache->prefetch_waitfor_locks) && (cache_atom_islocked(cache, line))) {
               break;
            }
            if ((lockgran = cache_get_write_lock(cache, line, rwdesc)) == 0) {
               return(0);
            }
         }
         end++;
         line->state |= CACHE_VALID;
      }
/* Need to free some locks if do this...
      if (validend != -1) {
         end = validend;
      }
*/
   }

   fillreq  = (ioreq_event *) event_copy(rwdesc->req);
   fillreq->blkno = start;
   fillreq->bcount = end - start + 1;
   fillreq->type = IO_ACCESS_ARRIVE;
   fillreq->flags |= READ;
   rwdesc->req->tempint1 = start;
   rwdesc->req->tempint2 = end;
   rwdesc->type = (rwdesc->type == CACHE_EVENT_READ) ? CACHE_EVENT_READEXTRA : CACHE_EVENT_WRITEFILLEXTRA;
   cache_waitfor_IO(cache, 1, rwdesc, fillreq);

if (cachedebugprinthack)  
fprintf (outputfile, "%f: Issueing line fill request: blkno %d, bcount %d\n", simtime, fillreq->blkno, fillreq->bcount);

   cache->issuefunc(cache->issueparam, fillreq);
   return(end - start + 1);
}


void cache_unlock_attached_prefetch(cache, rwdesc)
cache_def *cache;
cache_event *rwdesc;
{
   int fillstart = rwdesc->req->tempint1;
   int fillend = rwdesc->req->tempint2 + 1;  /* one beyond, actually */
   int reqstart = rwdesc->req->blkno;
   int reqend = reqstart + rwdesc->req->bcount;  /* one beyond, actually */

if (cachedebugprinthack)  
fprintf (outputfile, "Entered cache_unlock_attached_prefetch: fillstart %d, fillend %d, reqstart %d, reqend %d\n", fillstart, fillend, reqstart, reqend);

   if (fillstart < reqstart) {
      int lockgran = cache->lockgran;
      while (reqstart % cache->lockgran) {
         reqstart--;
      }

      reqstart--;
      if (fillstart <= reqstart) {
         cache_atom *line = cache_find_atom(cache, rwdesc->req->devno, reqstart);
         do {
            if ((line->lbn % lockgran) == (lockgran-1)) {
               lockgran = cache_free_write_lock(cache, line, rwdesc->req);
                    /* Can't free lock if not held */
               ASSERT(lockgran != 0);
            }
            line = line->line_prev;
            reqstart--;
         } while (fillstart <= reqstart);
      }
   }
   if (fillend > reqend) {
      int lockgran = cache->lockgran;
         while (reqend % cache->lockgran) {
            reqend++;
         }
/*
      if ((fillend / cache->lockgran) == (reqend / cache->lockgran)) {
      } else {
         reqend++;
      }
*/
      if (fillend > reqend) {
         cache_atom *line = cache_find_atom(cache, rwdesc->req->devno, reqend);
         do {
            if ((line->lbn % lockgran) == 0) {
               lockgran = cache_free_write_lock(cache, line, rwdesc->req);
                    /* Can't free lock if not held */
               ASSERT(lockgran != 0);
            }
            line = line->line_next;
            reqend++;
         } while (fillend > reqend);
      }
   }
}


int cache_read_continue(cache, readdesc)
cache_def *cache;
cache_event *readdesc;
{
   cache_atom *line = NULL;
   cache_atom *tmp;
   int i, j;
   cache_event *waitee;
   int stop;
   int curlock;
   int lockgran;
   int ret;

   int linesize = max(1, cache->linesize);
   int devno = readdesc->req->devno;
   int lbn = readdesc->req->blkno;
   int size = readdesc->req->bcount;
   int validpoint = readdesc->validpoint;

   if (cache->size == 0) {
      cache_waitfor_IO(cache, 1, readdesc, readdesc->req);
      cache->stat.readmisses++;
      cache->stat.fillreads++;
      cache->stat.fillreadatoms += readdesc->req->bcount;
      readdesc->req->type = IO_ACCESS_ARRIVE;
      cache->issuefunc(cache->issueparam, ioreq_copy(readdesc->req));
      return(1);
   }
   i = readdesc->lockstop;

if (cachedebugprinthack)  
fprintf (outputfile, "Entered cache_read_continue: lbn %d, size %d, i %d\n", lbn, size, i);

read_cont_loop:
   while (i < size) {
      line = cache_find_atom(cache, devno, (lbn + i));
      waitee = NULL;
      if (line == NULL) {
         if ((waitee = cache_allocate_space(cache, (lbn + i), 1, readdesc))) {
            readdesc->lockstop = i;
            return(1);
         } else {
            continue;
         }
      }
      stop = min(rounduptomult((size - i), cache->atomsperbit), (linesize - ((lbn + i) % linesize)));

if (cachedebugprinthack)  
fprintf (outputfile, "stop %d, lbn %d, atomsperbit %d, i %d, size %d, linesize %d\n", stop, lbn, cache->atomsperbit, i, size, linesize);
if (cachedebugprinthack)  
fprintf (outputfile, "validpoint %d, i %d\n", validpoint, i);

      j = 0;
      tmp = line;
      curlock = 2;
      lockgran = 0;
      while (j < stop) {
         int locktype = (tmp->state & CACHE_VALID) ? 1 : 2;

if (cachedebugprinthack)  
fprintf (outputfile, "j %d, valid %d, validpoint %d, curlock %d, lockgran %d\n", j, (tmp->state & CACHE_VALID), validpoint, curlock, lockgran);

         if (locktype > curlock) {
            curlock = locktype;
            lockgran = 0;
            locktype = 3;
         } else {
            curlock = locktype;
         }
         if ((lockgran) && ((lbn+i+j) % lockgran)) {
         } else if ((ret = cache_get_rw_lock(cache, locktype, readdesc, tmp, (i+j), 1))) {

if (cachedebugprinthack)  
fprintf (outputfile, "Non-zero return from cache_get_rw_lock: %d\n", ret);

            if (ret == 1) {
               readdesc->lockstop = i + j;
               return(1);
            } else {   /* ret == 2, indicating that identity changed */
               goto read_cont_loop;
            }
         }
         lockgran = cache->lockgran;
         if ((tmp->state & CACHE_VALID) == 0) {
            tmp->state |= CACHE_VALID;
            if (validpoint == -1) {
               validpoint = tmp->lbn;
               readdesc->validpoint = validpoint;
            }
/* Possibly begin filling (one at a time) ?? */
         } else {
            if (validpoint != -1) {
               /* start fill of partial line */
               readdesc->allocstop |= 2;
               cache->stat.fillreads++;
               readdesc->lockstop = - (readdesc->req->blkno % cache->atomsperbit);

if (cachedebugprinthack)  
fprintf (outputfile, "Going to issue_fillreq on partial line\n");

               cache->stat.fillreadatoms += cache_issue_fillreq(cache, validpoint, (tmp->lbn - 1), readdesc, cache->read_prefetch_type);
               readdesc->validpoint = -1;
               return(1);
            }
         }
         tmp = tmp->line_next;
         j++;
      }
      if ((validpoint != -1) && ((cache->read_line_by_line) || (!cache_concatok(cache, validpoint, 1, (validpoint+1), (line->lbn + stop - validpoint))))) {
         /* Start fill of the line */
         readdesc->allocstop |= 1;
         cache->stat.fillreads++;

if (cachedebugprinthack)  
fprintf (outputfile, "Going to issue_fillreq on full line\n");

         cache->stat.fillreadatoms += cache_issue_fillreq(cache, validpoint, (line->lbn + stop - 1), readdesc, cache->read_prefetch_type);
         readdesc->validpoint = -1;
         return(1);
      }
      i += linesize - ((lbn + i) % linesize);

if (cachedebugprinthack)  
fprintf (outputfile, "validpoint %d, i %d\n", validpoint, i);

   }
   if (validpoint != -1) {
      /* Do the fill if necessary */
      readdesc->allocstop |= 1;
      cache->stat.fillreads++;
      /* reset to what was in beginning */
      readdesc->lockstop = - (readdesc->req->blkno % cache->atomsperbit);
      readdesc->validpoint = -1;

if (cachedebugprinthack)  
fprintf (outputfile, "Going to issue_fillreq on full request\n");

      cache->stat.fillreadatoms += cache_issue_fillreq(cache, validpoint, (line->lbn + stop - 1), readdesc, cache->read_prefetch_type);
      return(1);
   }
   cache->stat.getblockreaddones++;
   cache->stat.reads++;
   cache->stat.readatoms += readdesc->req->bcount;
   if (readdesc->allocstop) {
      cache->stat.readmisses++;
   } else {
      cache->stat.readhitsfull++;
   }
   if (readdesc->flags & CACHE_FLAG_WASBLOCKED) {
      /* callback to say done */
      readdesc->donefunc(readdesc->doneparam, readdesc->req);
      addtoextraq((event *) readdesc);
   }
   return(0);
}


int cache_write_continue(cache, writedesc)
cache_def *cache;
cache_event *writedesc;
{
   int stop;
   cache_event *waitee;
   cache_atom *line;
   cache_atom *tmp;
   int lockgran;
   int i, j;
   int startfillstart;
   int startfillstop;
   int endfillstart;
   int endfillstop;
   int ret;

   int devno = writedesc->req->devno;
   int lbn = writedesc->req->blkno;
   int size = writedesc->req->bcount;
   int linesize = (cache->linesize > 1) ? cache->linesize : 1;

   if (cache->size == 0) {
      return(0);
   }
   i = writedesc->lockstop;

if (cachedebugprinthack)  
fprintf (outputfile, "Entered cache_write_continue: lbn %d, size %d, i %d\n", lbn, size, i);

write_cont_loop:

   while (i < size) {
      line = cache_find_atom(cache, devno, (lbn + i));
      waitee = NULL;
      if (line == NULL) {
         if (cache->no_write_allocate) {
            /* track non-resident part and continue */
            fprintf(stderr, "Not yet handling write-no-allocate\n");
            exit(0);
         } else {
            if ((waitee = cache_allocate_space(cache, (lbn + i), 1, writedesc))) {
               writedesc->lockstop = i;
               return(1);
            } else {
               continue;
            }
         }
      }
      stop = min(rounduptomult((size - i), cache->atomsperbit), (linesize - ((lbn + i) % linesize)));
      j = 0;
      tmp = line;
      lockgran = 0;
      startfillstart = -1;
      endfillstart = -1;
      while (j < stop) {
         if ((lockgran) && ((lbn+i+j) % lockgran)) {
         } else if ((ret = cache_get_rw_lock(cache, 2, writedesc, tmp, (i+j), 1))) {
            if (ret == 1) {
               writedesc->lockstop = i;
               return(1);
            } else {     /* ret == 2, indicates that line changed identity */
               goto write_cont_loop;
            }
         }
         lockgran = cache->lockgran;
         if ((tmp->lbn < lbn) && ((tmp->state & CACHE_VALID) == 0)) {
            writedesc->allocstop |= 2;
            tmp->state |= CACHE_VALID;
            if (startfillstart == -1) {
               startfillstart = tmp->lbn;
            }
            startfillstop = tmp->lbn;
         } else if ((tmp->state & CACHE_VALID) == 0) {
            int tmpval = tmp->lbn - (lbn + size - 1);
            writedesc->allocstop |= 2;
            if ((tmpval > 0) && (tmpval < (cache->atomsperbit - ((lbn + size - 1) % cache->atomsperbit)))) {
               tmp->state |= CACHE_VALID;
               if (endfillstart == -1) {
                  endfillstart = tmp->lbn;
               }
               endfillstop = tmp->lbn;
            }
         } else if (tmp->state & CACHE_DIRTY) {
            writedesc->allocstop |= 4;
         }
         tmp = tmp->line_next;
         j++;
      }

      /* if writing only part of space covered by valid/dirty bit, read       */
      /* (fill) first -- flag undo of allocation to bypass (no bypass for now */

      if ((startfillstart != -1) || (endfillstart != -1)) {
         int fillblkno = (startfillstart != -1) ? startfillstart : endfillstart;
         int fillbcount = 1 - fillblkno;
         fillbcount += ((startfillstart != -1) && (endfillstart == -1)) ? startfillstop : endfillstop;
         cache->stat.writeinducedfills++;

if (cachedebugprinthack)  
fprintf (outputfile, "Write induced fill: blkno %d, bcount %d\n", fillblkno, fillbcount);

         cache->stat.writeinducedfillatoms += cache_issue_fillreq(cache, fillblkno, (fillblkno + fillbcount - 1), writedesc, cache->writefill_prefetch_type);
         return(1);
      }

      i += linesize - ((lbn + i) % linesize);
   }
   cache->stat.writes++;
   cache->stat.writeatoms += writedesc->req->bcount;
   cache->stat.getblockwritedones++;
   if (writedesc->allocstop & 4) {
      cache->stat.writehitsdirty++;
   } else if (writedesc->allocstop) {
      cache->stat.writemisses++;
   } else {
      cache->stat.writehitsclean++;
   }
   if (writedesc->flags & CACHE_FLAG_WASBLOCKED) {
      /* callback */
      writedesc->donefunc(writedesc->doneparam, writedesc->req);
      addtoextraq((event *) writedesc);
   }
   return(0);
}


/* Gets the appropriate block, locked and ready to be accessed read or write */

int cache_get_block(cache, req, donefunc, doneparam)
cache_def *cache;
ioreq_event *req;
void (*donefunc)();
void *doneparam;
{
   cache_event *rwdesc = (cache_event *) getfromextraq();
   int ret;
/*
cachedebugprinthack = 1;
if (cachedebugprinthack) {
fprintf (outputfile, "totalreqs = %d\n", totalreqs);
fprintf (outputfile, "%.5f: Entered cache_get_block: rw %d, devno %d, blkno %d, size %d\n", simtime, (req->flags & READ), req->devno, req->blkno, req->bcount);
}
*/
   rwdesc->type = (req->flags & READ) ? CACHE_EVENT_READ : CACHE_EVENT_WRITE;
   rwdesc->donefunc = donefunc;
   rwdesc->doneparam = doneparam;
   rwdesc->req = req;
   req->next = NULL;
   req->prev = NULL;
   rwdesc->validpoint = -1;
   rwdesc->lockstop = - (req->blkno % cache->atomsperbit);
   rwdesc->allocstop = 0;     /* overload -- use for determining hit type */
   rwdesc->flags = 0;
   if (req->flags & READ) {
      cache->stat.getblockreadstarts++;
      ret = cache_read_continue(cache, rwdesc);
   } else {
      cache->stat.getblockwritestarts++;
      ret = cache_write_continue(cache, rwdesc);
   }

if (cachedebugprinthack)
fprintf (outputfile, "rwdesc %p, ret %x, validpoint %d\n", rwdesc, ret, rwdesc->validpoint);

   if (ret == 0) {
      donefunc(doneparam, req);
      addtoextraq((event *) rwdesc);
   } else {
      rwdesc->flags |= CACHE_FLAG_WASBLOCKED;
   }
   return(ret);
}


/* frees the block after access complete, block is clean so remove locks */
/* and update lru                                                        */

void cache_free_block_clean(cache, req)
cache_def *cache;
ioreq_event *req;
{
   cache_atom *line = NULL;
   int lockgran = 0;
   int i;

if (cachedebugprinthack)  
fprintf (outputfile, "%.5f: Entered cache_free_block_clean: blkno %d, bcount %d, devno %d\n", simtime, req->blkno, req->bcount, req->devno);

   cache->stat.freeblockcleans++;
   if (cache->size == 0) {
      return;
   }
   for (i=0; i<req->bcount; i++) {
      if (line == NULL) {
         line = cache_find_atom(cache, req->devno, (req->blkno + i));
             /* Can't free unallocated space */
         ASSERT(line != NULL);

         if (req->type) {
            cache_access(cache, line);
         }
      }
      if (((line->lbn % cache->lockgran) == (cache->lockgran-1)) || (i == (req->bcount-1))) {
         lockgran += cache_free_read_lock(cache, line, req);
      }
      line = line->line_next;
   }
       /* Must have unlocked entire requests worth of data */
   ASSERT2((lockgran >= req->bcount), "lockgran", lockgran, "reqbcount", req->bcount);
}


void cache_write_line_by_line(cache, flushreq, writedesc, reqdone)
cache_def *cache;
ioreq_event *flushreq;
cache_event *writedesc;
int reqdone;
{
   cache_event *tmp = cache->partwrites;

   while ((tmp) && (tmp->req != writedesc->req)) {
      tmp = tmp->next;
   }

   if (tmp == NULL) {
          /* partial write sync must have been initiated if it is done */
      ASSERT(!reqdone);

      tmp = (cache_event *) getfromextraq();
      tmp->req = writedesc->req;
      tmp->locktype = writedesc->req->blkno;
      tmp->lockstop = writedesc->req->bcount;
      tmp->next = cache->partwrites;
      tmp->prev = NULL;
      if (tmp->next) {
         tmp->next->prev = tmp;
      }
      cache->partwrites = tmp;
   }
   if (reqdone) {
      tmp->req->bcount = tmp->accblkno - flushreq->blkno;
      tmp->req->blkno = flushreq->blkno;
      tmp->req->type = 0;
      cache_free_block_clean(cache, tmp->req);
      if (tmp->accblkno >= (tmp->locktype + tmp->lockstop)) {
         if (tmp->prev) {
            tmp->prev->next = tmp->next;
         } else {
            cache->partwrites = tmp->next;
         }
         if (tmp->next) {
            tmp->next->prev = tmp->prev;
         }
         tmp->req->blkno = tmp->locktype;
         tmp->req->bcount = tmp->lockstop;
         writedesc->donefunc(writedesc->doneparam, tmp->req);
         addtoextraq((event *) tmp);
      } else {
         tmp->req->bcount = tmp->locktype + tmp->lockstop - tmp->accblkno;
         tmp->req->blkno = tmp->accblkno;
	 cache->linebylinetmp = 1;
         cache_free_block_dirty(cache, tmp->req, writedesc->donefunc, writedesc->doneparam);
      }
      addtoextraq((event *) writedesc);
   } else {
      writedesc->type = CACHE_EVENT_SYNCPART;
      tmp->accblkno = flushreq->blkno + flushreq->bcount;
   }
}


/* a delayed write - set dirty bits, remove locks and update lru.        */
/* If cache doesn't allow delayed writes, forward this to async          */

int cache_free_block_dirty(cache, req, donefunc, doneparam)
cache_def *cache;
ioreq_event *req;
void (*donefunc)();
void *doneparam;
{
   cache_atom *line = NULL;
   ioreq_event *flushreq;
   cache_event *writedesc;
   int lockgran = 0;
   int flushblkno = req->blkno;
   int flushbcount = req->bcount;
   int linebyline = cache->linebylinetmp;
   int i;

   int writethru = (cache->size == 0) || (cache->writescheme != CACHE_WRITE_BACK);

if (cachedebugprinthack)  
fprintf (outputfile, "%.5f, Entered cache_free_block_dirty: blkno %d, size %d, writethru %d\n", simtime, req->blkno, req->bcount, writethru);

   cache->linebylinetmp = 0;
   cache->stat.freeblockdirtys++;
   if (writethru) {
      writedesc = (cache_event *) getfromextraq();
      writedesc->type = CACHE_EVENT_SYNC;
      writedesc->donefunc = donefunc;
      writedesc->doneparam = doneparam;
      writedesc->req = req;
      req->type = IO_REQUEST_ARRIVE;
      req->next = NULL;
      req->prev = NULL;
      flushreq = (ioreq_event *) event_copy(req);
      flushreq->type = IO_ACCESS_ARRIVE;
      flushreq->buf = cache;
   }
   if (cache->size == 0) {
      cache->stat.destagewrites++;
      cache->stat.destagewriteatoms += flushreq->bcount;
      cache_waitfor_IO(cache, 1, writedesc, flushreq);
      cache->issuefunc(cache->issueparam, flushreq);
      return(1);
   }

if (cachedebugprinthack)  
fprintf (outputfile, "flushblkno %d, reqblkno %d, atomsperbit %d\n", flushblkno, req->blkno, cache->atomsperbit);

   flushblkno -= (req->blkno % cache->atomsperbit);
   flushbcount += (req->blkno % cache->atomsperbit);
   i = flushblkno + flushbcount;
   flushbcount += rounduptomult(i, cache->atomsperbit) - i;

if (cachedebugprinthack)
fprintf (outputfile, "in free_block_dirty: flushblkno %d, flushsize %d\n", flushblkno, flushbcount);

   for (i=0; i<flushbcount; i++) {
      if (line == NULL) {
         if ((lockgran) && (writethru) && ((cache->write_line_by_line) || (!cache_concatok(cache, flushblkno, 1, (flushblkno+1), i)))) {
            flushbcount = i;
            linebyline = 1;
            break;
         }
         line = cache_find_atom(cache, req->devno, (flushblkno + i));
              /* dirtied space must be allocated */
         ASSERT(line != NULL);

         cache_access(cache, line);
      }
      if (!writethru) {
         line->busno = req->busno;
         line->slotno = req->slotno;
      }
      line->state |= (writethru) ? CACHE_VALID : (CACHE_VALID|CACHE_DIRTY);
      if (((line->lbn % cache->lockgran) != (cache->lockgran-1)) && (i != (flushbcount-1))) {
      } else if (writethru) {
         lockgran += cache_get_read_lock(cache, line, writedesc);
      } else {
         lockgran += cache_free_write_lock(cache, line, req);
      }
      line = line->line_next;
   }
	/* locks must be held over entire space */
   ASSERT2((lockgran >= flushbcount), "lockgran", lockgran, "flushbcount", flushbcount);

   if (writethru) {
      cache->stat.destagewrites++;
      cache->stat.destagewriteatoms += flushbcount;
      flushreq->blkno = flushblkno;
      flushreq->bcount = flushbcount;
      if (linebyline) {
         cache_write_line_by_line(cache, flushreq, writedesc, 0);
      }
      cache_waitfor_IO(cache, 1, writedesc, flushreq);

if (cachedebugprinthack)
fprintf (outputfile, "Issueing dirty block flush: writedesc %p, req %p, blkno %d, bcount %d, devno %d\n", writedesc, writedesc->req, flushreq->blkno, flushreq->bcount, flushreq->devno);

      cache->issuefunc(cache->issueparam, flushreq);
      if (cache->writescheme == CACHE_WRITE_SYNCONLY) {
         return(1);
      } else {
       /* Assuming that it is safe to touch it after call to cache_waitfor_IO */
         req->type = -1;
         writedesc->donefunc = cache_empty_donefunc;
         req = ioreq_copy(req);
      }
   } else if (cache->flush_idledelay >= 0.0) {
      ioqueue_reset_idledetecter(cache->queuefind(cache->queuefindparam, req->devno), 0);
   }
   donefunc(doneparam, req);
   return(0);
}


int cache_sync(cache)
cache_def *cache;
{
   return(0);
}


struct cacheevent *cache_disk_access_complete(cache, curr)
cache_def *cache;
ioreq_event *curr;
{
   ioreq_event *req;
   cache_event *tmp = cache->IOwaiters;

if (cachedebugprinthack)
fprintf (outputfile, "Entered cache_disk_access_complete: blkno %d, bcount %d, devno %d\n", curr->blkno, curr->bcount, curr->devno);

   while (tmp) {
      req = tmp->req;
      while (req) {
         if ((curr->devno == req->devno) && ((curr->blkno == tmp->accblkno) || ((tmp->accblkno == -1) && ((req->next) || (tmp->type == CACHE_EVENT_SYNC) || (tmp->type == CACHE_EVENT_IDLESYNC)) && (curr->blkno == req->blkno)))) {

if (cachedebugprinthack)
fprintf (outputfile, "Matched: tmp %p, req %p, blkno %d, accblkno %d, reqblkno %d\n", tmp, req, curr->blkno, tmp->accblkno, req->blkno);

            goto completed_access;
         }
         req = req->next;
      }
      tmp = tmp->next;
   }

completed_access:

   if (tmp == NULL) {
      fprintf(stderr, "Not yet supporting non-waited for disk accesses in cache\n");
      exit(0);
   }
   if (tmp->prev) {
      tmp->prev->next = tmp->next;
   } else {
      cache->IOwaiters = tmp->next;
   }
   if (tmp->next) {
      tmp->next->prev = tmp->prev;
   }
/*
fprintf (outputfile, "IOwaiters: %x, tmp->prev %x, tmp->next %x, 3 %d\n", cache->IOwaiters, tmp->prev, tmp->next, 3);
*/
   if ((cache->size == 0) || (tmp->type == CACHE_EVENT_SYNC) || (tmp->type == CACHE_EVENT_IDLESYNC)) {
      int type = req->type;
      if (req->next) {
         req->next->prev = req->prev;
      }
      if (req->prev) {
         req->prev->next = req->next;
      } else {
         tmp->req = req->next;
      }
      req->type = 0;
      cache_free_block_clean(cache, req);
      req->type = type;
      if (type != -1) {
         tmp->donefunc(tmp->doneparam, req);
      }
      if (tmp->req) {
         cache_waitfor_IO(cache, 1, tmp, tmp->req);
         tmp->accblkno = (tmp->req->next) ? -1 : tmp->req->blkno;
         tmp = (type == -1) ? (cache_event *) event_copy(tmp) : NULL;
      } else {
         if (tmp->type == CACHE_EVENT_IDLESYNC) {
            cache_idletime_detected(cache, curr->devno);
         }
         if (type != -1) {
            addtoextraq((event *) tmp);
            tmp = NULL;
         }
      }
      if (type == -1) {
         tmp->req = req;
      }
   } else if (tmp->type == CACHE_EVENT_READ) {
   } else if (tmp->type == CACHE_EVENT_WRITE) {
   } else if (tmp->type == CACHE_EVENT_SYNCPART) {
      curr->next = tmp->req;
      tmp->req= curr;
      curr = NULL;
   } else if (tmp->type == CACHE_EVENT_ALLOCATE) {

      /* Must be a replacement-induced write-back */

      req->next->prev = req->prev;
      if (req->prev) {
         req->prev->next = req->next;
      } else {
         tmp->req = req->next;
      }
      req->type = 0;
      cache_free_block_clean(cache, req);
      addtoextraq((event *) req);
      if (tmp->req != tmp->waitees->req) {
         cache_waitfor_IO(cache, 1, tmp, tmp->req);
         tmp->accblkno = -1;
	 tmp = NULL;
      }
   } else if (tmp->type == CACHE_EVENT_READEXTRA) {
      tmp->type = CACHE_EVENT_READ;
      cache_unlock_attached_prefetch(cache, tmp);
   } else if (tmp->type == CACHE_EVENT_WRITEFILLEXTRA) {
      tmp->type = CACHE_EVENT_WRITE;
      cache_unlock_attached_prefetch(cache, tmp);
   } else {
      fprintf(stderr, "Unknown type at cache_disk_access_complete: %d\n", tmp->type);
      exit(0);
   }
   addtoextraq((event *) curr);
   return(tmp);
}


void cache_wakeup_complete(cache, desc)
cache_def *cache;
cache_event *desc;
{
   if (desc->type == CACHE_EVENT_READ) {
      cache_read_continue(cache, desc);
   } else if (desc->type == CACHE_EVENT_WRITE) {
      cache_write_continue(cache, desc);
   } else if (desc->type == CACHE_EVENT_ALLOCATE) {
      cache_allocate_space_continue(cache, desc);
   } else if (desc->type == CACHE_EVENT_SYNC) {
      desc->donefunc(desc->doneparam, desc->req);
      addtoextraq((event *) desc);
   } else if (desc->type == CACHE_EVENT_SYNCPART) {
      ioreq_event *flushreq = desc->req;
      desc->req = flushreq->next;
      cache_write_line_by_line(cache, flushreq, desc, 1);
      addtoextraq((event *) flushreq);
   } else {
      fprintf(stderr, "Unknown event type in cache_wakeup_complete: %d\n", desc->type);
      assert(0);
      exit(0);
   }
}


void cache_resetstats(cache)
cache_def *cache;
{
   cache->stat.reads = 0;
   cache->stat.readatoms = 0;
   cache->stat.readhitsfull = 0;
   cache->stat.readhitsfront = 0;
   cache->stat.readhitsback = 0;
   cache->stat.readhitsmiddle = 0;
   cache->stat.readmisses = 0;
   cache->stat.fillreads = 0;
   cache->stat.fillreadatoms = 0;
   cache->stat.writes = 0;
   cache->stat.writeatoms = 0;
   cache->stat.writehitsclean = 0;
   cache->stat.writehitsdirty = 0;
   cache->stat.writemisses = 0;
   cache->stat.writeinducedfills = 0;
   cache->stat.writeinducedfillatoms = 0;
   cache->stat.destagewrites = 0;
   cache->stat.destagewriteatoms = 0;
   cache->stat.getblockreadstarts = 0;
   cache->stat.getblockreaddones = 0;
   cache->stat.getblockwritestarts = 0;
   cache->stat.getblockwritedones = 0;
   cache->stat.freeblockcleans = 0;
   cache->stat.freeblockdirtys = 0;
}


void cache_initialize(cache, issuefunc, issueparam, queuefind, queuefindparam, wakeupfunc, wakeupparam, numdevs)
cache_def *cache;
void (*issuefunc)();
void *issueparam;
struct ioq * (*queuefind)();
void *queuefindparam;
void (*wakeupfunc)();
void *wakeupparam;
int numdevs;
{
   int i, j;
   cache_atom *tmp;

   StaticAssert (sizeof(cache_atom) <= DISKSIM_EVENT_SIZE);
   StaticAssert (sizeof(cache_event) <= DISKSIM_EVENT_SIZE);
   StaticAssert (sizeof(cache_lockholders) <= DISKSIM_EVENT_SIZE);
   StaticAssert (sizeof(cache_lockholders) == sizeof(cache_lockwaiters));

   cache->issuefunc = issuefunc;
   cache->issueparam = issueparam;
   cache->queuefind = queuefind;
   cache->queuefindparam = queuefindparam;
   cache->wakeupfunc = (void (*)()) wakeupfunc;
   cache->wakeupparam = wakeupparam;
   cache->IOwaiters = NULL;
   cache->partwrites = NULL;
   cache->linewaiters = NULL;
   cache->linebylinetmp = 0;
   for (i=0; i<CACHE_HASHSIZE; i++) {
      cache->hash[i] = 0;
   }
   for (j=0; j<(cache->mapmask+1); j++) {
      cache_mapentry *mapentry = &cache->map[j];
      for (i=0; i<CACHE_MAXSEGMENTS; i++) {
         while ((tmp = mapentry->lru[i])) {
            cache_remove_from_lrulist(mapentry, tmp, i);
            cache_add_to_lrulist(mapentry, tmp, CACHE_SEGNUM);
         }
         mapentry->numactive[i] = 0;
      }
      if (mapentry->freelist) {
         /* reset all valid bits */
      } else {
         int atomcnt = cache->size / (cache->mapmask+1);
         int linesize = (cache->linesize) ? cache->linesize : 1;
         tmp = NULL;
         for (i=0; i<atomcnt; i++) {
            cache_atom *newatom = (cache_atom *) getfromextraq();
            bzero ((char *) newatom, DISKSIM_EVENT_SIZE);
            if (i % linesize) {
               newatom->line_next = tmp;
               tmp->line_prev = newatom;
            } else {
               if (tmp) {
                  cache_add_to_lrulist(mapentry, tmp, CACHE_SEGNUM);
               }
            }
            tmp = newatom;
         }
         if (tmp) {
            cache_add_to_lrulist(mapentry, tmp, CACHE_SEGNUM);
         }
      }
   }
   if (cache->flush_policy == CACHE_FLUSH_PERIODIC) {
      timer_event *timereq = (timer_event *) getfromextraq();
      timereq->type = TIMER_EXPIRED;
      timereq->func = cache_periodic_flush;
      timereq->time = cache->flush_period;
      timereq->ptr = cache;
      addtointq(timereq);
   }
   for (i=0; i<numdevs; i++) {
      struct ioq *queue = queuefind(queuefindparam, i);
      if (cache->flush_idledelay >= 0.0) {
	 ioqueue_set_idlework_function(queue, cache_idletime_detected, cache, cache->flush_idledelay);
      }
      if (cache == NULL) {
	 ioqueue_set_concatok_function(queue, cache_concatok, cache);
      }
   }
   cache_resetstats(cache);
}


void cache_cleanstats(cache)
cache_def *cache;
{
}


void cache_printstats(cache, prefix)
cache_def *cache;
char *prefix;
{
   int reqs = cache->stat.reads + cache->stat.writes;
   int atoms = cache->stat.readatoms + cache->stat.writeatoms;

   fprintf (outputfile, "%scache requests:             %6d\n", prefix, reqs);
   if (reqs == 0) {
      return;
   }
   fprintf (outputfile, "%scache read requests:        %6d  \t%6.4f\n", prefix, cache->stat.reads, ((double) cache->stat.reads / (double) reqs));
   if (cache->stat.reads) {
      fprintf(outputfile, "%scache atoms read:           %6d  \t%6.4f\n", prefix, cache->stat.readatoms, ((double) cache->stat.readatoms / (double) atoms));
      fprintf(outputfile, "%scache read misses:          %6d  \t%6.4f  \t%6.4f\n", prefix, cache->stat.readmisses, ((double) cache->stat.readmisses / (double) reqs), ((double) cache->stat.readmisses / (double) cache->stat.reads));
      fprintf(outputfile, "%scache read full hits:       %6d  \t%6.4f  \t%6.4f\n", prefix, cache->stat.readhitsfull, ((double) cache->stat.readhitsfull / (double) reqs), ((double) cache->stat.readhitsfull / (double) cache->stat.reads));
      fprintf(outputfile, "%scache fills (read):         %6d  \t%6.4f  \t%6.4f\n", prefix, cache->stat.fillreads, ((double) cache->stat.fillreads / (double) reqs), ((double) cache->stat.fillreads / (double) cache->stat.reads));
      fprintf(outputfile, "%scache atom fills (read):    %6d  \t%6.4f  \t%6.4f\n", prefix, cache->stat.fillreadatoms, ((double) cache->stat.fillreadatoms / (double) atoms), ((double) cache->stat.fillreadatoms / (double) cache->stat.readatoms));
   }
   fprintf(outputfile, "%scache write requests:       %6d  \t%6.4f\n", prefix, cache->stat.writes, ((double) cache->stat.writes / (double) reqs));
   if (cache->stat.writes) {
      fprintf(outputfile, "%scache atoms written:        %6d  \t%6.4f\n", prefix, cache->stat.writeatoms, ((double) cache->stat.writeatoms / (double) atoms));
      fprintf(outputfile, "%scache write misses:         %6d  \t%6.4f  \t%6.4f\n", prefix, cache->stat.writemisses, ((double) cache->stat.writemisses / (double) reqs), ((double) cache->stat.writemisses / (double) cache->stat.writes));
      fprintf(outputfile, "%scache write hits (clean):   %6d  \t%6.4f  \t%6.4f\n", prefix, cache->stat.writehitsclean, ((double) cache->stat.writehitsclean / (double) reqs), ((double) cache->stat.writehitsclean / (double) cache->stat.writes));
      fprintf(outputfile, "%scache write hits (dirty):   %6d  \t%6.4f  \t%6.4f\n", prefix, cache->stat.writehitsdirty, ((double) cache->stat.writehitsdirty / (double) reqs), ((double) cache->stat.writehitsdirty / (double) cache->stat.writes));
      fprintf(outputfile, "%scache fills (write):        %6d  \t%6.4f  \t%6.4f\n", prefix, cache->stat.writeinducedfills, ((double)cache->stat.writeinducedfills / (double) reqs), ((double) cache->stat.writeinducedfills / (double) cache->stat.writes));
      fprintf(outputfile, "%scache atom fills (write):   %6d  \t%6.4f  \t%6.4f\n", prefix, cache->stat.writeinducedfillatoms, ((double)cache->stat.writeinducedfillatoms / (double) atoms), ((double) cache->stat.writeinducedfillatoms / (double) cache->stat.writeatoms));
      fprintf(outputfile, "%scache destages (write):     %6d  \t%6.4f  \t%6.4f\n", prefix, cache->stat.destagewrites, ((double) cache->stat.destagewrites / (double) reqs), ((double) cache->stat.destagewrites / (double) cache->stat.writes));
      fprintf(outputfile, "%scache atom destages (write): %6d  \t%6.4f  \t%6.4f\n", prefix, cache->stat.destagewriteatoms, ((double) cache->stat.destagewriteatoms / (double) atoms), ((double) cache->stat.destagewriteatoms / (double) cache->stat.writeatoms));
      fprintf(outputfile, "%scache end dirty atoms:      %6d  \t%6.4f\n", prefix, cache_count_dirty_atoms(cache), ((double) cache_count_dirty_atoms(cache) / (double) cache->stat.writeatoms));
   }
#if 0	/* extra info that is helpful when debugging */
   fprintf(outputfile, "%scache get_block starts (read): %6d\n", prefix, cache->stat.getblockreadstarts);
   fprintf(outputfile, "%scache get_block dones (read):  %6d\n", prefix, cache->stat.getblockreaddones);
   fprintf(outputfile, "%scache get_block starts (write): %6d\n", prefix, cache->stat.getblockwritestarts);
   fprintf(outputfile, "%scache get_block dones (write):  %6d\n", prefix, cache->stat.getblockwritedones);
   fprintf(outputfile, "%scache free_block_cleans:       %6d\n", prefix, cache->stat.freeblockcleans);
   fprintf(outputfile, "%scache free_block_dirtys:       %6d\n", prefix, cache->stat.freeblockdirtys);
#endif
}


cache_def * cache_copy(cache)
cache_def *cache;
{
   cache_def *new = (cache_def *) malloc(sizeof(cache_def));
   cache_mapentry *mapentry = (cache_mapentry *) malloc(sizeof(cache_mapentry));
   int i, j;

   ASSERT((new != NULL) && (mapentry != NULL));

   new->map = mapentry;
   for (i=0; i<(cache->mapmask+1); i++) {
      mapentry[i].freelist = NULL;
      for (j=0; j<CACHE_MAXSEGMENTS; j++) {
         mapentry[i].maxactive[j] = cache->map[i].maxactive[j];
         mapentry[i].lru[j] = NULL;
      }
   }

   new->issuefunc = cache->issuefunc;
   new->issueparam = cache->issueparam;
   new->size = cache->size;
   new->atomsize = cache->atomsize;
   new->numsegs = cache->numsegs;
   new->linesize = cache->linesize;
   new->atomsperbit = cache->atomsperbit;
   new->lockgran = cache->lockgran;
   new->sharedreadlocks = cache->sharedreadlocks;
   new->maxreqsize = cache->maxreqsize;
   new->replacepolicy = cache->replacepolicy;
   new->mapmask = cache->mapmask;
   new->writescheme = cache->writescheme;
   new->flush_policy = cache->flush_policy;
   new->flush_period = cache->flush_period;
   new->flush_idledelay = cache->flush_idledelay;
   new->flush_maxlinecluster = cache->flush_maxlinecluster;
   new->read_prefetch_type = cache->read_prefetch_type;
   new->writefill_prefetch_type = cache->writefill_prefetch_type;
   new->prefetch_waitfor_locks = cache->prefetch_waitfor_locks;
   new->startallflushes = cache->startallflushes;
   new->allocatepolicy = cache->allocatepolicy;
   new->read_line_by_line = cache->read_line_by_line;
   new->write_line_by_line = cache->write_line_by_line;
   new->maxscatgath = cache->maxscatgath;
   new->no_write_allocate = cache->no_write_allocate;

   return(new);
}


void cache_param_override(cache, paramname, paramval)
cache_def *cache;
char *paramname;
char *paramval;
{
   if (cache == NULL) {
      return;
   }
   if (strcmp(paramname, "cache_size") == 0) {
      scanparam_int(paramval, paramname, &cache->size, 1, 0, 0);
   } else if (strcmp(paramname, "cache_segcount") == 0) {
      scanparam_int(paramval, paramname, &cache->numsegs, 1, 0, 0);
   } else if (strcmp(paramname, "cache_linesize") == 0) {
      scanparam_int(paramval, paramname, &cache->linesize, 1, 0, 0);
   } else if (strcmp(paramname, "cache_bitgran") == 0) {
      scanparam_int(paramval, paramname, &cache->atomsperbit, 3, 1, max(1,cache->linesize));
   } else if (strcmp(paramname, "cache_lockgran") == 0) {
      scanparam_int(paramval, paramname, &cache->lockgran, 3, 1, max(1,cache->linesize));
   } else if (strcmp(paramname, "cache_readshare") == 0) {
      scanparam_int(paramval, paramname, &cache->sharedreadlocks, 3, 0, 1);
   } else if (strcmp(paramname, "cache_maxreqsize") == 0) {
      scanparam_int(paramval, paramname, &cache->maxreqsize, 1, 0, 0);
   } else if (strcmp(paramname, "cache_replace") == 0) {
      scanparam_int(paramval, paramname, &cache->replacepolicy, 3, CACHE_REPLACE_MIN, CACHE_REPLACE_MAX);
   } else if (strcmp(paramname, "cache_writescheme") == 0) {
      scanparam_int(paramval, paramname, &cache->writescheme, 3, CACHE_WRITE_MIN, CACHE_WRITE_MAX);
   } else if (strcmp(paramname, "cache_readprefetch") == 0) {
      scanparam_int(paramval, paramname, &cache->read_prefetch_type, 3, CACHE_PREFETCH_MIN, CACHE_PREFETCH_MAX);
   } else if (strcmp(paramname, "cache_writeprefetch") == 0) {
      scanparam_int(paramval, paramname, &cache->writefill_prefetch_type, 3, CACHE_PREFETCH_MIN, CACHE_PREFETCH_MAX);
   } else if (strcmp(paramname, "cache_linebyline") == 0) {
      scanparam_int(paramval, paramname, &cache->read_line_by_line, 3, 0, 1);
      cache->write_line_by_line = cache->read_line_by_line;
   } else {
      fprintf(stderr, "Unsupported paramname at cache_param_override: %s\n", paramname);
      exit(0);
   }
}


struct cache_def * cache_readparams(parfile)
FILE *parfile;
{
   cache_def *cache = (cache_def *) malloc(sizeof(cache_def));
   int rem;
   int linesize;
   int i, j;

fprintf (outputfile, "CACHE_HASHSIZE %d, CACHE_HASHMASK %x\n", (u_int)CACHE_HASHSIZE, (u_int)CACHE_HASHMASK);
   ASSERT(cache != NULL);

   getparam_int(parfile, "Cache size (in 512B blks)", &cache->size, 1, -1, 0);

   cache->mapmask = 0;
   cache->map = (cache_mapentry *) malloc((cache->mapmask+1)*sizeof(cache_mapentry));
   ASSERT(cache->map != NULL);

   getparam_int(parfile, "Cache segment count (SLRU)", &cache->numsegs, 3, 1, CACHE_MAXSEGMENTS);
   rem = cache->size;
   for (i=(cache->numsegs-1); i>0; i--) {
      char line[201];
      double maxsegfrac;
      sprintf(line, "Max frac for segment #%d", i);
      getparam_double(parfile, line, &maxsegfrac, 3, 0.0, 1.0);
      cache->map[0].maxactive[i] = (int)((double) cache->size * maxsegfrac);
      rem -= cache->map[0].maxactive[i];
   }
      /* Sum of maximum segment sizes must not exceed cache size */
   ASSERT((cache->size == 0) || (rem > 0));

   cache->map[0].maxactive[0] = rem;
   for (i=0; i<(cache->mapmask+1); i++) {
      cache->map[i].freelist = NULL;
      for (j=0; j<CACHE_MAXSEGMENTS; j++) {
         cache->map[i].maxactive[j] = cache->map[0].maxactive[j];
         cache->map[i].lru[j] = NULL;
      }
   }
   getparam_int(parfile, "Cache line size (in blks)", &cache->linesize, 1, 0, 0);
   linesize = max(cache->linesize, 1);

   getparam_int(parfile, "Cache blocks per bit", &cache->atomsperbit, 1, 1, 0);
	/* Valid/dirty bit granularity must divide evenly into line size */
   ASSERT2((linesize % cache->atomsperbit) == 0, "linesize", linesize, "atomsperbit", cache->atomsperbit);

   getparam_int(parfile, "Cache lock granularity", &cache->lockgran, 1, 1, 0);
        /* Lock granularity must divide evenly into line size */
   ASSERT2((linesize % cache->lockgran) == 0, "linesize", linesize, "lockgran", cache->lockgran);

   getparam_int(parfile, "Cache shared read locks", &cache->sharedreadlocks, 3, 0, 1);
   getparam_int(parfile, "Cache max request size", &cache->maxreqsize, 1, 0, 0);
   getparam_int(parfile, "Cache replacement policy", &cache->replacepolicy, 3, CACHE_REPLACE_MIN, CACHE_REPLACE_MAX);
   getparam_int(parfile, "Cache allocation policy", &cache->allocatepolicy, 3, CACHE_ALLOCATE_MIN, CACHE_ALLOCATE_MAX);
   getparam_int(parfile, "Cache write scheme", &cache->writescheme, 3, CACHE_WRITE_MIN, CACHE_WRITE_MAX);
   getparam_int(parfile, "Cache flush policy", &cache->flush_policy, 3, CACHE_FLUSH_MIN, CACHE_FLUSH_MAX);
   getparam_double(parfile, "Cache flush period", &cache->flush_period, 1, (double) 0.0, (double) 0.0);
   getparam_double(parfile, "Cache flush idle delay", &cache->flush_idledelay, 1, (double) -1.0, (double) 0.0);
   if ((cache->flush_idledelay < 0.0) && (cache->flush_idledelay != -1.0)) {
      fprintf (stderr, "Illegal value for `Cache flush idle delay': %f\n", cache->flush_idledelay);
      exit (0);
   }
   getparam_int(parfile, "Cache flush max line cluster", &cache->flush_maxlinecluster, 1, 1, 0);
   getparam_int(parfile, "Cache prefetch type (read)", &cache->read_prefetch_type, 3, CACHE_PREFETCH_MIN, CACHE_PREFETCH_MAX);
   getparam_int(parfile, "Cache prefetch type (write)", &cache->writefill_prefetch_type, 3, CACHE_PREFETCH_MIN, CACHE_PREFETCH_MAX);
   getparam_int(parfile, "Cache line-by-line fetches", &cache->read_line_by_line, 3, 0, 1);
   getparam_int(parfile, "Cache scatter/gather max", &cache->maxscatgath, 1, 0, 0);
   ASSERT((cache->flush_maxlinecluster == 1) || ((!cache->read_line_by_line) && ((cache->maxscatgath == 0) || (cache->flush_maxlinecluster <= (cache->maxscatgath + 1)))));

   cache->prefetch_waitfor_locks = FALSE;
   cache->atomsize = 1;
   cache->startallflushes = TRUE;
   cache->write_line_by_line = cache->read_line_by_line;
   cache->no_write_allocate = FALSE;

   return(cache);
}

