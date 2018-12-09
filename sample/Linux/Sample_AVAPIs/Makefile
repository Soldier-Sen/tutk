include ../common.mk

ifeq ("$(ARCH)", "x86")
    CFLAGS=-m32
endif

CFLAGS += -O2 -Wall

LDFLAGS = -I./ -I../../../Include -I./extension/emulater/ -L'../../../Lib/Linux/$(ARCH)' -lAVAPIs -lIOTCAPIs -lpthread

all:
	@echo "=== Target Platform [$(ARCH)] ==="
	$(CC) $(CFLAGS) AVAPIs_Server.c extension/emulater/AVAPIs2_Emulater.c $(LDFLAGS) -o AVAPIs_Server
	$(CC) $(CFLAGS) AVAPIs_Client.c $(LDFLAGS) -o AVAPIs_Client

clean:
	rm -rf *.o *~ *.bak AVAPIs_Server AVAPIs_Client tags
