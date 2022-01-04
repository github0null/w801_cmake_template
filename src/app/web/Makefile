TOP_DIR = ../../..
sinclude $(TOP_DIR)/tools/w800/conf.mk

EXCLUDES = fsdata_lwip.c \
           fsdata_lwip_basic.c \
           fsdata_lwip_ruigong.c \
           fsdata_lwip_russian.c
CSRCS = $(filter-out $(EXCLUDES), $(wildcard *.c))

ifndef PDIR
GEN_LIBS = libweb$(LIB_EXT)
endif

#DEFINES +=

sinclude $(TOP_DIR)/tools/w800/rules.mk

INCLUDES += -I ./
INCLUDES := $(INCLUDES) -I $(PDIR)include
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile