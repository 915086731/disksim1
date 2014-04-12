LDFLAGS = -lm
HP_FAST_OFLAGS = +O4
NCR_FAST_OFLAGS = -O4 -Hoff=BEHAVED
DEBUG_OFLAGS = -g -DASSERTS
PROF_OFLAGS = -g -DASSERTS -p
GPROF_OFLAGS = -g -DASSERTS -G
CFLAGS = ${DEBUG_OFLAGS}
#CC = cc
CC = gcc -Wall
DISKSIM_OBJ = disksim_intr.o disksim_cache.o disksim_pfsim.o disksim_pfdisp.o\
	disksim_synthio.o disksim_iotrace.o disksim_iosim.o disksim_logorg.o\
	disksim_redun.o disksim_ioqueue.o disksim_iodriver.o disksim_bus.o\
	disksim_controller.o disksim_ctlrdumb.o disksim_ctlrsmart.o\
	disksim_disk.o disksim_diskctlr.o disksim_diskcache.o disksim_diskmap.o\
	disksim_diskmech.o disksim_stat.o

all : disksim syssim rms hplcomb

clean :
	rm -f *.o disksim syssim rms hplcomb core

rms : rms.c
	$(CC) rms.c -lm -o rms

hplcomb : hplcomb.c
	$(CC) hplcomb.c -o hplcomb

disksim : disksim.o $(DISKSIM_OBJ)
	${CC} ${CFLAGS} -o disksim disksim.o $(DISKSIM_OBJ) $(LDFLAGS)

syssim : syssim_driver.o disksim_main.o disksim_interface.o $(DISKSIM_OBJ)
	${CC} ${CFLAGS} -o syssim syssim_driver.o disksim_main.o disksim_interface.o $(DISKSIM_OBJ) $(LDFLAGS)

disksim_stat.o : disksim_stat.c disksim_stat.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_stat.c

disksim_diskmech.o : disksim_diskmech.c disksim_disk.h disksim_stat.h disksim_iosim.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_diskmech.c

disksim_diskmap.o : disksim_diskmap.c disksim_disk.h disksim_stat.h disksim_iosim.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_diskmap.c

disksim_diskcache.o : disksim_diskcache.c disksim_disk.h disksim_stat.h disksim_iosim.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_diskcache.c

disksim_diskctlr.o : disksim_diskctlr.c disksim_disk.h disksim_stat.h disksim_iosim.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_diskctlr.c

disksim_disk.o : disksim_disk.c disksim_disk.h disksim_stat.h disksim_ioqueue.h disksim_iosim.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_disk.c

disksim_ctlrsmart.o : disksim_ctlrsmart.c disksim_controller.h disksim_cache.h disksim_ioqueue.h disksim_orgface.h disksim_iosim.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_ctlrsmart.c

disksim_ctlrdumb.o : disksim_ctlrdumb.c disksim_controller.h disksim_iosim.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_ctlrdumb.c

disksim_controller.o : disksim_controller.c disksim_controller.h disksim_orgface.h disksim_iosim.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_controller.c

disksim_bus.o : disksim_bus.c disksim_bus.h disksim_iosim.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_bus.c

disksim_iodriver.o : disksim_iodriver.c disksim_iodriver.h disksim_ioqueue.h disksim_orgface.h disksim_iosim.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_iodriver.c

disksim_redun.o : disksim_redun.c disksim_logorg.h disksim_orgface.h disksim_iosim.h disksim_stat.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_redun.c

disksim_logorg.o : disksim_logorg.c disksim_logorg.h disksim_orgface.h disksim_iosim.h disksim_stat.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_logorg.c

disksim_ioqueue.o : disksim_ioqueue.c disksim_ioqueue.h disksim_iosim.h disksim_stat.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_ioqueue.c

disksim_iosim.o : disksim_iosim.c disksim_iosim.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_iosim.c

disksim_iotrace.o : disksim_iotrace.c disksim_iosim.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_iotrace.c

disksim_synthio.o : disksim_synthio.c disksim_pfsim.h disksim_synthio.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_synthio.c

disksim_pfdisp.o : disksim_pfdisp.c disksim_pfsim.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_pfdisp.c

disksim_pfsim.o : disksim_pfsim.c disksim_ioface.h disksim_pfsim.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_pfsim.c

disksim_cache.o : disksim_cache.c disksim_cache.h disksim_iosim.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_cache.c

disksim_intr.o : disksim_intr.c disksim_ioface.h disksim_pfface.h disksim_global.h
	${CC} -c ${CFLAGS} disksim_intr.c

disksim.o : disksim.c disksim_ioface.h disksim_pfface.h disksim_global.h
	${CC} -c ${CFLAGS} disksim.c

disksim_main.o : disksim.c disksim_ioface.h disksim_pfface.h disksim_global.h
	${CC} -c -o disksim_main.o ${CFLAGS} -DEXTERNAL_MAIN disksim.c

disksim_interface.o: disksim_interface.c disksim_global.h disksim_ioface.h syssim_driver.h
	${CC} -c ${CFLAGS} disksim_interface.c

syssim_driver.o: syssim_driver.c syssim_driver.h
	${CC} -c ${CFLAGS} syssim_driver.c

