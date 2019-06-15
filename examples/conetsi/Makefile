CONTIKI_PROJECT = conetsi conetsi-server oam pmodule dummy-module
all: $(CONTIKI_PROJECT)

CONTIKI=../..

#WITH_IPV6 = 1

LDFLAGS += -lm
CONTIKI_TARGET_SOURCEFILES += oam.c conetsi.c pmodule.c dummy-module.c

include $(CONTIKI)/Makefile.include
