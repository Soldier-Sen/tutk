include ../common.mk

ifeq ("$(ARCH)", "x86")
    CFLAGS=-m32
endif

all:
	@echo "=== Target Platform [$(ARCH)] ==="
	gcc -s $(CFLAGS) -O2 -Wall P2PTunnelServer.c -I./ -I../../../Include -L'../../../Lib/Linux/$(ARCH)' -lP2PTunnelAPIs -lRDTAPIs -lIOTCAPIs -lpthread -lrt -o P2PTunnelServer
	gcc -s $(CFLAGS) -O2 -Wall P2PTunnelAgent.c -I./ -I../../../Include -L'../../../Lib/Linux/$(ARCH)'  -lP2PTunnelAPIs -lRDTAPIs -lIOTCAPIs -lpthread -lrt -o P2PTunnelAgent


clean:
	rm -rf *.o *~ *.bak P2PTunnelServer P2PTunnelAgent
