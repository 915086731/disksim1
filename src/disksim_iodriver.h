
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

#ifndef DISKSIM_IODRIVER_H
#define DISKSIM_IODRIVER_H

#include "disksim_ioqueue.h"

/* Device driver types */

#define STANDALONE	1
/* & NOTSTANDALONE, which corresponds to a driver that is actually part of */
/* the process-flow model (i.e., a system-level simulation rather than a   */
/* standalone storage subsystem simulation). */

/* Device driver ctlr flags */

#define DRIVER_C700		0x2
#define DRIVER_CTLR_BUSY	0x4

/* Device driver computation times */

#define IODRIVER_IMMEDSCHED_TIME		0.0
#define IODRIVER_WRITE_DISCONNECT_TIME		0.0
#define IODRIVER_READ_DISCONNECT_TIME		0.0
#define IODRIVER_RECONNECT_TIME			0.0
#define IODRIVER_BASE_COMPLETION_TIME		0.0

#define IODRIVER_COMPLETION_RESPTODEV_TIME	0.0
#define IODRIVER_COMPLETION_RESPTOSCHED_TIME	0.0
#define IODRIVER_COMPLETION_SCHEDTOEND_TIME	0.0
#define IODRIVER_COMPLETION_SCHEDTOWAKEUP_TIME	0.0
#define IODRIVER_COMPLETION_WAKEUPTOEND_TIME	0.0
#define IODRIVER_COMPLETION_RESPTOWAKEUP_TIME	0.0
#define IODRIVER_COMPLETION_RESPTOREQ_TIME	0.0    /* e.g., Software RAID */

/* Special Constant Queue/Access times */

#define IODRIVER_TRACED_ACCESS_TIMES	-1.0
#define IODRIVER_TRACED_QUEUE_TIMES	-2.0
#define IODRIVER_TRACED_BOTH_TIMES	-3.0

typedef struct {
   int		type;
   int		usequeue;
   int		numoutbuses;
   int		outbus;
   double	consttime;     /* Use value if positive, or traced if neg. */
   double	scale;
   int		numdevices;
   int		numctlrs;
   struct ioq *	queue;
   device *	devices;
   ctlr *	ctlrs;
} iodriver;

#endif /* DISKSIM_IODRIVER_H */

