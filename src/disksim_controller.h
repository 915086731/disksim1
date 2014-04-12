
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

#ifndef DISKSIM_CONTROLLER_H
#define DISKSIM_CONTROLLER_H

#include "disksim_ioqueue.h"
#include "disksim_cache.h"

/* Controller types */

#define MINCTLTYPE		1
#define CTLR_PASSTHRU		1
#define CTLR_BASED_ON_53C700	2
#define CTLR_SMART		3
#define MAXCTLTYPE		3

/* Controller states */

#define FREE			1
#define READ_DATA_TRANSFER	2
#define WRITE_DATA_TRANSFER	3
#define COMPLETION_PENDING	4
#define DISCONNECT_PENDING	5
#define RECONNECTING		6
#define REQUEST_PENDING		7

/* Directions for buswaitdir */

#define UP	1
#define DOWN	2


typedef struct {
   int		ctlno;
   int          type;
   int          state;
   int          printstats;
   struct ioq *	queue;
   struct cache_def *cache;
   int		numdevices;
   device *	devices;
   double	timescale;
   double	blktranstime;
   double	ovrhd_disk_request;
   double	ovrhd_ready;
   double	ovrhd_disk_ready;
   double	ovrhd_disconnect;
   double	ovrhd_disk_disconnect;
   double	ovrhd_reconnect;
   double	ovrhd_disk_reconnect;
   double	ovrhd_complete;
   double	ovrhd_disk_complete;
   double	ovrhd_reset;
   ioreq_event *connections;
   ioreq_event *datatransfers;
   int		hosttransfer;
   ioreq_event *hostwaiters;
   int          maxoutstanding;
   int		maxdiskqsize;
   int		inbusowned;
   int		outbusowned;
   ioreq_event *buswait;
   int          numinbuses;
   int          inbuses[MAXINBUSES];
   int          depth[MAXINBUSES];
   int          slotno[MAXINBUSES];
   int          numoutbuses;
   int          outbuses[MAXOUTBUSES];
   int          outslot[MAXOUTBUSES];
   double	waitingforbus;
} controller;


/* disksim_ctlrdumb.c functions */

extern void controller_passthru_event_arrive();
extern void controller_passthru_printstats();
extern void controller_53c700_event_arrive();
extern void controller_53c700_printstats();

/* disksim_ctlrsmart.c functions */

extern void controller_smart_event_arrive();
extern void controller_smart_initialize();
extern void controller_smart_resetstats();
extern void controller_smart_read_specs();
extern void controller_smart_printstats();

/* controller.c functions */

extern void controller_send_event_up_path();
extern void controller_send_event_down_path();
extern void controller_remove_type_from_buswait();
extern int  controller_get_downward_busno();


#endif    /* DISKSIM_CONTROLLER_H */

