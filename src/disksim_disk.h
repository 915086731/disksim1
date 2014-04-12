
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

#ifndef DISKSIM_DISK_H
#define DISKSIM_DISK_H

#include "disksim_stat.h"
#include "disksim_ioqueue.h"

#define MAXSLIPS	32
#define MAXDEFECTS	256

/* Seek types */

#define THREEPOINT_LINE         -1.0
#define THREEPOINT_CURVE        -2.0
#define HPL_SEEK_EQUATION	-3.0
#define FIRST10_PLUS_HPL_SEEK_EQUATION	-4.0
#define EXTRACTION_SEEK		-5.0

/* Latency types */

#define AVGROTATE               -1.0
#define SKEWED_FOR_TRACK_SWITCH -2.0
#define NON_SKEWED              -3.0

/* Disk states */

#define DISK_IDLE			1
#define DISK_TRANSFERING		2
#define DISK_WAIT_FOR_CONTROLLER	3

/* Disk buffer states (both internal and external) */

#define BUFFER_EMPTY		1
#define BUFFER_CLEAN		2
#define BUFFER_DIRTY		3
#define BUFFER_READING		4
#define BUFFER_WRITING		5
#define BUFFER_PREEMPT		6
#define BUFFER_IDLE		7
#define BUFFER_CONTACTING	8
#define BUFFER_TRANSFERING	9

/* Disk buffer content match types */

#define BUFFER_COLLISION -1
#define BUFFER_NOMATCH	  0
#define BUFFER_WHOLE	  1     /* read */
#define BUFFER_PARTIAL	  2     /* read */
#define BUFFER_PREPEND    3     /* write */
#define BUFFER_APPEND     4     /* write */

/* Extra disk buffer flag variable */
/* *** MAKE SURE THIS DOESN"T CONFLICT WITH ANY FLAGS IN global.h *** */

#define BUFFER_BACKGROUND	0x10000000

/* Disk buffer continuous read types */

#define BUFFER_NO_READ_AHEAD		0
#define BUFFER_READ_UNTIL_TRACK_END	1
#define BUFFER_READ_UNTIL_CYL_END	2
#define BUFFER_READ_UNTIL_SEG_END	3
#define BUFFER_DEC_PREFETCH_SCHEME      4

/* Disk sparing schemes (for replacing bad media) */

#define NO_SPARING		0
#define TRACK_SPARING		1
#define SECTPERCYL_SPARING	2
#define SECTPERTRACK_SPARING	3
#define MAXSPARESCHEME		3

/* Disk request flags */
#define SEG_OWNED			0x00000001
#define HDA_OWNED			0x00000002
#define EXTRA_WRITE_DISCONNECT          0x00000004
#define COMPLETION_SENT                 0x00000008
#define COMPLETION_RECEIVED             0x00000010
#define FINAL_WRITE_RECONNECTION_1      0x00000020
#define FINAL_WRITE_RECONNECTION_2      0x00000040

/* Disk preseeking levels */
#define NO_PRESEEK                      0
#define PRESEEK_DURING_COMPLETION	1
#define PRESEEK_BEFORE_COMPLETION       2        /* implies 1 as well */

/* Disk fastwrites levels */
#define NO_FASTWRITE			0
#define LIMITED_FASTWRITE		1
#define FULL_FASTWRITE			2

/* aliases */

#define ioreq_hold_disk tempptr1
#define ioreq_hold_diskreq tempptr2

typedef struct seg {
   double       time;
   int          state;
   struct seg  *next;
   struct seg  *prev;
   int          startblkno;
   int          endblkno;
   int          outstate;
   int		outbcount;
   int		minreadaheadblkno;      /* min prefetch blkno + 1 */
   int		maxreadaheadblkno;      /* max prefetch blkno + 1 */
   struct diskreq_t *diskreqlist;       /* sorted by ascendingly first blkno */
   int          size;
   ioreq_event *access;
   int		hold_blkno; 		/* used for prepending */
   int		hold_bcount;		/* sequential writes   */
   struct diskreq_t *recyclereq;        /* diskreq to recycle this seg */
} segment;

typedef struct diskreq_t {
   int			flags;
   ioreq_event 	       *ioreqlist;	/* sorted by ascending blkno */
   struct diskreq_t    *seg_next;
   struct diskreq_t    *bus_next;
   int			outblkno;
   int			inblkno;
   segment	       *seg;
   int          	watermark;
   int			hittype;
   double		overhead_done;
   char			space[20];
} diskreq;

typedef struct {
   double  seektime;
   double  latency;
   double  xfertime;
   int     seekdistance;
   int     zeroseeks;
   int     zerolatency;
   int     highblkno;
   statgen seekdiststats;
   statgen seektimestats;
   statgen rotlatstats;
   statgen xfertimestats;
   statgen acctimestats;
   int     writecombs;
   int     readmisses;
   int     writemisses;
   int     fullreadhits;
   int     appendhits;
   int     prependhits;
   int     readinghits;
   double  runreadingsize;
   double  remreadingsize;
   int     parthits;
   double  runpartsize;
   double  rempartsize;
   int     interfere[2];
   double  requestedbus;
   double  waitingforbus;
   int     numbuswaits;
} diskstat;

typedef struct {
   int    startcyl;
   int    endcyl;
   int    blkspertrack;
   int    deadspace;
   double firstblkno;
   double cylskew;
   double trackskew;
   int    blksinband;
   int    sparecnt;
   int    numslips;
   int    numdefects;
   int    slip[MAXSLIPS];
   int    defect[MAXDEFECTS];
   int    remap[MAXDEFECTS];
} band;

typedef struct {
   double       acctime;
   double       seektime;
   double       seekone;
   double       seekavg;
   double       seekfull;
   double	seekwritedelta;
   double	hpseek[6];
   double	first10seeks[10];
   int		extractseekcnt;
   int *	extractseekdists;
   double *	extractseektimes;
   double       headswitch;
   double	rotatetime;
   double	rpmerr;
   double       rpm;
   double	overhead;
   double	timescale;
   int		devno;
   int          syncset;
   int          numsurfaces;
   int          numblocks;
   int          numcyls;
   int          numbands;
   int		sparescheme;
   band        *bands;
   struct ioq  *queue;
   int		sectpercyl;     /* "Avg" value used in suboptimal schedulers */
   int		hold_bus_for_whole_read_xfer;
   int		hold_bus_for_whole_write_xfer;
   int		almostreadhits;
   int		sneakyfullreadhits;
   int		sneakypartialreadhits;
   int		sneakyintermediatereadhits;
   int		readhitsonwritedata;
   int		writeprebuffering;
   int		preseeking;
   int		neverdisconnect;
   int          qlen;
   int          maxqlen;
   int          busy;
   int		prev_readahead_min;
   int		write_hit_stop_readahead;
   int		read_direct_to_buffer;
   int		immedtrans_any_readhit;
   int		readanyfreeblocks;
   int		numsegs;
   int		numwritesegs;
   segment *	dedicatedwriteseg;
   int		segsize;
   double	writewater;
   double	readwater;
   int		reqwater;
   int		sectbysect;
   int		translatesectbysect;
   int		enablecache;
   int		contread;
   int		minreadahead;
   int		maxreadahead;
   int		keeprequestdata;
   int		readaheadifidle;
   int		fastwrites;
   int		numdirty;
   int		immedread;
   int		immedwrite;
   int		immed;
   int		writecomb;
   int		stopinsector;
   int		disconnectinseek;
   int		immedstart;
   int		immedend;
   segment     *seglist;
   double	overhead_command_readhit_afterread;
   double	overhead_command_readhit_afterwrite;
   double	overhead_command_readmiss_afterread;
   double	overhead_command_readmiss_afterwrite;
   double	overhead_command_write_afterread;
   double	overhead_command_write_afterwrite;
   double	overhead_complete_read;
   double	overhead_complete_write;
   double	overhead_data_prep;
   double	overhead_reselect_first;
   double	overhead_reselect_other;
   double	overhead_disconnect_read_afterread;
   double	overhead_disconnect_read_afterwrite;
   double	overhead_disconnect_write;
   struct diskreq_t *extradisc_diskreq;
   int		extra_write_disconnect;
   double	extradisc_command;
   double	extradisc_disconnect1;
   double	extradisc_inter_disconnect;
   double	extradisc_disconnect2;
   double	extradisc_seekdelta;
   double	minimum_seek_delay;
   int		firstblkontrack;
   int		endoftrack;
   int          currcylno;
   int          currsurface;
   double       currangle;
   double       currtime;
   int          lastgen;
   int		lastflags;
   int          printstats;
   double	starttrans;
   double	blktranstime;
   int		outstate;
   diskreq     *pendxfer;
   diskreq     *currenthda;
   diskreq     *effectivehda;
   diskreq     *currentbus;
   diskreq     *effectivebus;
   int		blksdone;
   int		busowned;
   ioreq_event *buswait;
   ioreq_event *outwait;
   int          numinbuses;
   int          inbuses[MAXINBUSES];
   int          depth[MAXINBUSES];
   int          slotno[MAXINBUSES];
   diskstat     stat;
} disk;

/* Print control variables */

extern int disk_printsizestats;
extern int disk_printseekstats;
extern int disk_printlatencystats;
extern int disk_printxferstats;
extern int disk_printacctimestats;
extern int disk_printinterferestats;
extern int disk_printbufferstats;

/* Other variables */

extern disk *disks;
extern int numdisks;

/* disksim_diskmech.c functions */

extern void disk_release_hda();
extern void disk_check_hda();
extern void disk_check_bus();
extern void disk_activate_read();
extern void disk_activate_write();
extern int diskspecialseektime();
extern int diskspecialaccesstime();
extern void disk_read_extracted_seek_curve();
extern double diskseektime();
extern double disklatency();
extern double diskxfertime();
extern double diskacctime();
extern void disk_acctimestats();

/* disksim_diskctlr.c functions */

extern double disk_buffer_estimate_acctime();
extern double disk_buffer_estimate_servtime();
extern double disk_buffer_firsttrans();
extern ioreq_event* disk_buffer_transfer_size();
extern void disk_buffer_seekdone();
extern void disk_buffer_trackacc_done();
extern void disk_buffer_sector_done();
extern void disk_got_remapped_sector();
extern void disk_goto_remapped_sector();
extern void disk_check_hda();
extern int disk_enablement_function();
extern void disk_buffer_stop_access();
extern int disk_buffer_stopable_access();

/* disksim_diskmap.c functions */

extern band * disk_translate_lbn_to_pbn();
extern int disk_translate_pbn_to_lbn();
extern double disk_map_pbn_skew();
extern void disk_get_lbn_boundaries_for_track();
extern void disk_check_numblocks();

/* disksim_diskcache.c functions */

extern segment * disk_buffer_select_segment();
extern segment * disk_buffer_recyclable_segment();
extern diskreq * disk_buffer_seg_owner();
extern int disk_buffer_attempt_seg_ownership();
extern int disk_buffer_get_max_readahead();
extern int disk_buffer_block_available();
extern int disk_buffer_reusable_segment_check();
extern int disk_buffer_overlap();
extern int disk_buffer_check_segments();
extern void disk_buffer_set_segment();
extern void disk_buffer_segment_wrap();
extern void disk_buffer_remove_from_seg();
extern void disk_interferestats();

#endif   /* DISKSIM_DISK_H */

