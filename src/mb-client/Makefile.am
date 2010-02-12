bin_PROGRAMS=mb-client

mb_client_SOURCES = client.h \
					 ../mn-masterd/daemon.c \
					 ../mn-masterd/daemon.h \
					 slave.c \
					 main.c

AM_CFLAGS = -I../libmethaconfig/ -I../libmetha/

mb_client_LDADD = ../libmetha/libmetha.la ../libmethaconfig/libmethaconfig.la
