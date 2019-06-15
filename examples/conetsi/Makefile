CONTIKI_PROJECT = conetsi conetsi-server oam dummy-module
all: $(CONTIKI_PROJECT)

CONTIKI=../..

#WITH_IPV6 = 1

CONTIKI_TARGET_SOURCEFILES += oam.c conetsi.c pmodule.c
LDFLAGS += -lm

include $(CONTIKI)/Makefile.include
