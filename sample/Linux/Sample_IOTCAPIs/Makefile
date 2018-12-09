include ../common.mk

ifeq ("$(ARCH)", "x86")
    CFLAGS=-m32
endif

all:
	@echo "=== Target Platform [$(ARCH)] ==="
	gcc $(CFLAGS) -g -Wall IOTCAPIs_Client.c -I./ -I../../../Include -L'../../../Lib/Linux/$(ARCH)' -lIOTCAPIs -lpthread -o IOTCAPIs_Client
	gcc $(CFLAGS) -g -Wall IOTCAPIs_Device.c -I./ -I../../../Include -L'../../../Lib/Linux/$(ARCH)' -lIOTCAPIs -lpthread -o IOTCAPIs_Device
	
	gcc $(CFLAGS) -g -Wall IOTCAPIs_Client_Reliable.c -I./ -I../../../Include -L'../../../Lib/Linux/$(ARCH)' -lIOTCAPIs -lpthread -o IOTCAPIs_Client_Reliable
	gcc $(CFLAGS) -g -Wall IOTCAPIs_Device_Reliable.c -I./ -I../../../Include -L'../../../Lib/Linux/$(ARCH)' -lIOTCAPIs -lpthread -o IOTCAPIs_Device_Reliable

	gcc $(CFLAGS) -g -Wall IOTCAPIs_DevClient.c -I./ -I../../../Include -L'../../../Lib/Linux/$(ARCH)' -lIOTCAPIs -lpthread -o IOTCAPIs_DevClient

clean:
	rm -rf *.o *~ *.bak IOTCAPIs_Client IOTCAPIs_Device IOTCAPIs_Client_Reliable IOTCAPIs_Device_Reliable IOTCAPIs_DevClient