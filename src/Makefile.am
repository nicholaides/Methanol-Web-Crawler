
# libmethaconfig will always be needed
SUBDIRS = libmethaconfig
if MASTER
SUBDIRS += mn-masterd
endif
if SLAVE
SUBDIRS += mn-slaved
endif
if CLIENT
  SUBDIRS += libmetha mb-client metha-config
if CLI
  SUBDIRS += methabot
endif
else
# no client but the cli
  SUBDIRS += libmetha methabot metha-config
endif
