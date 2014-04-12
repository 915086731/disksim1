
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

#ifndef DISKSIM_IOSIM_H
#define DISKSIM_IOSIM_H

#include "disksim_ioface.h"

/* I/O Subsystem restrictions */

#define MAXDISKS	100
#define MAXDEPTH	4     /* Bus hierarchy can be up to four levels deep */
#define MAXINBUSES	1
#define MAXOUTBUSES	4
#define MAXSLOTS	15
#define MAXLOGORGS	100

/* I/O Event types */

/* #define IO_REQUEST_ARRIVE			100 *//* actually in ioface.h */
#define IO_ACCESS_ARRIVE			101
#define IO_INTERRUPT_ARRIVE			102
#define IO_RESPOND_TO_DEVICE			103
#define IO_ACCESS_COMPLETE			104
#define IO_INTERRUPT_COMPLETE			105
#define DISK_OVERHEAD_COMPLETE			106
#define DISK_PREPARE_FOR_DATA_TRANSFER		107
#define DISK_DATA_TRANSFER_COMPLETE		108
#define DISK_BUFFER_SEEKDONE			109
#define DISK_BUFFER_TRACKACC_DONE		110
#define DISK_BUFFER_SECTOR_DONE			111
#define DISK_GOT_REMAPPED_SECTOR		112
#define DISK_GOTO_REMAPPED_SECTOR		113
#define BUS_OWNERSHIP_GRANTED			114
#define BUS_DELAY_COMPLETE			115
#define CONTROLLER_DATA_TRANSFER_COMPLETE	116
#define TIMESTAMP_LOGORG			117
#define IO_TRACE_REQUEST_START			118
#define IO_QLEN_MAXCHECK			119

/* I/O Interrupt cause types */

#define COMPLETION		1
#define RECONNECT		2
#define DISCONNECT		3
#define READY_TO_TRANSFER	4

#ifndef IOFACE_H

/* Device types */

#define IODRIVER	2
#define DISK		4
#define CONTROLLER	5

#endif   /* IOFACE_H */

/* Call types for diskacctime() */

#define DISKACCESS	1
#define DISKACCTIME	2
#define DISKPOS		3
#define DISKPOSTIME	4
#define DISKSEEK	5
#define DISKSEEKTIME	6
#define DISKSERVTIME	7

/* LBN to PBN mapping types (also for getting cylinder mappings) */

#define	MAP_NONE		0
#define	MAP_IGNORESPARING	1
#define MAP_ZONEONLY		2
#define MAP_ADDSLIPS		3
#define MAP_FULL		4
#define	MAP_FROMTRACE		5
#define	MAP_AVGCYLMAP		6
#define	MAXMAPTYPE		6

/* Convenient to define here, so can be used both by controllers and drivers */
/* These are just simple structures used in higher-level components to track */
/* important state (e.g., access paths and outstanding requests).            */

typedef struct {
   int          ctlno;
   int          flags;
   intchar      slotpath;
   intchar      buspath;
   int          maxoutstanding;
   int		numoutstanding;
   int          maxreqsize;
   ioreq_event *pendio;
   ioreq_event *oversized;
} ctlr;

typedef struct {
   double       lastevent;
   int          flag;
   int          devno;
   int          busy;
   intchar      slotpath;
   intchar      buspath;
   int          maxoutstanding;
   int		queuectlr;
   ctlr         *ctl;
   struct ioq   *queue;
} device;

/* functions provided external to I/O subsystem */

extern event * io_done_notify();

/* Global I/O functions */

extern ioreq_event * ioreq_copy();

/* disksim_iodriver.c functions */

extern void    iodriver_read_toprints();
extern void    iodriver_read_specs();
extern void    iodriver_read_physorg();
extern void    iodriver_read_logorg();
extern void    iodriver_param_override();
extern void    iodriver_sysorg_param_override();
extern void    iodriver_initialize();
extern void    iodriver_resetstats();
extern void    iodriver_printstats();
extern void    iodriver_cleanstats();
extern event * iodriver_request();
extern void    iodriver_schedule();
extern double  iodriver_tick();
extern double  iodriver_raise_priority();
extern void    iodriver_interrupt_arrive();
extern void    iodriver_access_complete();
extern void    iodriver_respond_to_device();
extern void    iodriver_interrupt_complete();
extern void    iodriver_trace_request_start();
extern int *   iodriver_get_topbuses();
extern event * iodriver_event_arrive();

/* disksim_controller.c functions */

extern void    controller_read_toprints();
extern void    controller_read_specs();
extern void    controller_read_physorg();
extern void    controller_read_logorg();
extern void    controller_param_override();
extern void    controller_initialize();
extern void    controller_resetstats();
extern void    controller_printstats();
extern void    controller_cleanstats();
extern int     controller_C700_based();
extern int     controller_set_depth();
extern int     controller_get_numcontrollers();
extern int     controller_get_depth();
extern int     controller_get_bus_master();
extern int     controller_get_inbus();
extern int     controller_get_slotno();
extern int     controller_get_outslot();
extern int     controller_get_maxoutstanding();
extern void    controller_event_arrive();
extern int     controller_get_data_transfered();

/* disksim_bus.c functions */

extern void    bus_read_toprints();
extern void    bus_read_specs();
extern void    bus_read_physorg();
extern void    bus_param_override();
extern int     bus_ownership_get();
extern void    bus_ownership_release();
extern void    bus_remove_from_arbitration();
extern void    bus_delay();
extern void    bus_event_arrive();
extern double  bus_get_transfer_time();
extern int     bus_get_data_transfered();
extern int     bus_get_controller_slot();
extern void    bus_deliver_event();
extern void    bus_set_depths();
extern void    bus_set_to_zero_depth();
extern void    bus_initialize();
extern void    bus_resetstats();
extern void    bus_printstats();
extern void    bus_cleanstats();

/* disksim_disk.c functions */

extern void    disk_read_toprints();
extern void    disk_read_specs();
extern void    disk_read_syncsets();
extern void    disk_param_override();
extern void    disk_initialize();
extern void    disk_resetstats();
extern void    disk_printstats();
extern void    disk_printsetstats();
extern void    disk_cleanstats();
extern int     disk_set_depth();
extern int     disk_get_depth();
extern int     disk_get_inbus();
extern int     disk_get_busno();
extern int     disk_get_slotno();
extern int     disk_get_number_of_blocks();
extern int     disk_get_maxoutstanding();
extern int     disk_get_numdisks();
extern int     disk_get_numcyls();
extern double  disk_get_blktranstime();
extern int     disk_get_avg_sectpercyl();
extern void    disk_get_mapping();
extern void    disk_event_arrive();
extern void    disk_timestamp();
extern double  disk_get_servtime();
extern double  disk_get_acctime();

#endif   /* DISKSIM_IOSIM_H */

