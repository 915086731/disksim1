
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

#ifndef DISKSIM_BUS_H
#define DISKSIM_BUS_H

#include "disksim_stat.h"

/* Bus types */

#define BUS_TYPE_MIN	1
#define EXCLUSIVE	1
#define INTERLEAVED	2
#define BUS_TYPE_MAX	2

/* Bus arbitration types */

#define BUS_ARB_TYPE_MIN	1
#define SLOTPRIORITY_ARB	1
#define FIFO_ARB		2
#define BUS_ARB_TYPE_MAX	2

/* Bus states */

#define BUS_FREE	1
#define BUS_OWNED	2

typedef struct {
   int    devtype;
   int    devno;
} slot;

typedef struct bus_ev {
   double time;
   int type;
   struct bus_ev *next;
   struct bus_ev *prev;
   int devno;
   int devtype;
   int busno;
   int slotno;
   ioreq_event *delayed_event;
   double wait_start;
} bus_event;

typedef struct {
   int          state;
   int          type;
   int          arbtype;
   bus_event   *owners;
   double       arbtime;
   double       readblktranstime;
   double       writeblktranstime;
   bus_event   *arbwinner;
   int		printstats;
   int          depth;
   int          numslots;
   slot        *slots;
   double	lastowned;
   double	runidletime;
   statgen	arbwaitstats;
   statgen	busidlestats;
} bus;

/* functions provided by users... */

extern void disk_bus_delay_complete();
extern void disk_bus_ownership_grant();
extern void controller_bus_delay_complete();
extern void controller_bus_ownership_grant();

#endif    /* DISKSIM_BUS_H */

