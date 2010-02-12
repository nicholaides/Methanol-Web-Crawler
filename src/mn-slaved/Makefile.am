bin_PROGRAMS = mn-slaved

AM_CFLAGS = -I/usr/include/mysql -I../libmetha/ -I../libmethaconfig/
AM_LDFLAGS = -lmysqlclient

mn_slaved_LDADD = ../libmethaconfig/libmethaconfig.la
mn_slaved_SOURCES = \
	main.c \
	client.c \
	../mn-masterd/daemon.c \
	../mn-masterd/daemon.h \
	master.c \
	client.h \
	hook.c \
	hook.h \
	slave.h

