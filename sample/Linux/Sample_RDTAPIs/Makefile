include ../common.mk

ifeq ("$(ARCH)", "x86")
    CFLAGS=-m32
endif

all:
	@echo "=== Target Platform [$(ARCH)] ==="
	gcc $(CFLAGS) -O2 -Wall extension/RDTPacketHelper/RDTPacketHelper.c RDTServer.c -I./ -I../../../Include -I./extension/RDTPacketHelper/ -L'../../../Lib/Linux/$(ARCH)' -lpthread -lIOTCAPIs -lRDTAPIs -o RDTServer
	gcc $(CFLAGS) -O2 -Wall extension/RDTPacketHelper/RDTPacketHelper.c RDTClient.c -I./ -I../../../Include -I./extension/RDTPacketHelper/ -L'../../../Lib/Linux/$(ARCH)' -lpthread -lIOTCAPIs -lRDTAPIs -o RDTClient
	gcc $(CFLAGS) -O2 -Wall RDTServerMultiChannel.c -I./ -I../../../Include -L'../../../Lib/Linux/$(ARCH)' -lpthread -lIOTCAPIs -lRDTAPIs -o RDTServerMultiChannel
	gcc $(CFLAGS) -O2 -Wall RDTClientMultiChannel.c -I./ -I../../../Include -L'../../../Lib/Linux/$(ARCH)' -lpthread -lIOTCAPIs -lRDTAPIs -o RDTClientMultiChannel
clean:
	rm -rf *.o *~ *.bak RDTServer RDTClient RDTServerMultiChannel RDTClientMultiChannel
