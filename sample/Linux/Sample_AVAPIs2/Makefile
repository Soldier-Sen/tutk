include ../common.mk

ifeq ("$(ARCH)", "x86")
    CFLAGS=-m32
endif

CFLAGS += -O2 -Wall

# Run "HEX_AUDIO=[your audio file.c] make" to audio saved as C arrays. -- Edited on 1st June 2016 by Mono
ifneq ("$(HEX_AUDIO)", "")
    CFLAGS += -DHEX_AUDIO
endif

LDFLAGS = -I./ -I../../../Include -I./extension/emulater/ -I./extension/fq/ -I./extension/timeSync/ -L'../../../Lib/Linux/$(ARCH)' -lAVAPIs -lIOTCAPIs -lpthread

all:
	@echo "=== Target Platform [$(ARCH)] ==="
	$(CC) $(CFLAGS) AVAPIs2_Server.c extension/emulater/AVAPIs2_Emulater.c $(HEX_AUDIO) $(LDFLAGS) -o AVAPIs2_Server
	$(CC) $(CFLAGS) AVAPIs2_Client.c extension/fq/AVAPIs2_FQ.c extension/timeSync/AVAPIs2_timeSync.c $(LDFLAGS) -o AVAPIs2_Client
	$(CC) $(CFLAGS) AVAPIs2_Server_PushMode.c $(LDFLAGS) -o AVAPIs2_Server_PushMode
	$(CC) $(CFLAGS) AVAPIs2_Client_PushMode.c $(LDFLAGS) -o AVAPIs2_Client_PushMode
	$(CC) $(CFLAGS) AVAPIs2_Server_SendMeta.c $(LDFLAGS) -o AVAPIs2_Server_SendMeta
	$(CC) $(CFLAGS) AVAPIs2_Client_RecvMeta.c $(LDFLAGS) -o  AVAPIs2_Client_RecvMeta

clean:
	rm -rf *.o *~ *.bak AVAPIs2_Server AVAPIs2_Client AVAPIs2_Server_PushMode AVAPIs2_Client_PushMode AVAPIs2_Server_SendMeta AVAPIs2_Client_RecvMeta tags
