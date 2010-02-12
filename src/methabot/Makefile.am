
bin_PROGRAMS = \
	mb

mb_SOURCES = \
	main.c \
	help.c \
	sig.c  \
	dump.c \
	methabot.h

AM_CFLAGS = -DMETHA_CONFIG_DIR=\"$(datadir)/metha\" \
		    -DMETHA_SCRIPT_DIR=\"$(datadir)/metha/scripts\" \
		    -DMETHA_MODULE_DIR=\"$(libdir)/metha/modules\" \
			-Wall -I../libmethaconfig/ -I../libmetha/

mb_LDADD = ../libmetha/libmetha.la


