
ifeq ("$(platform)","")
        platform=x64
endif

ifeq ("$(platform)","x86")
        CFLAGS=-m32
endif


all:
	@echo === Target Platform [$(platform)] === 
	gcc $(CFLAGS) -std=c99 -g -O2 -Wall IOTCAPIs_WakeUp_Device.c -I./ -I../../../../Include -L'../../../../Lib/Linux/$(platform)' -lIOTCAPIs -lpthread -o IOTCAPIs_WakeUp_Device	
	gcc $(CFLAGS) -std=c99 -g -O2 -Wall IOTCAPIs_WakeUp2_Device.c -I./ -I../../../../Include -L'../../../../Lib/Linux/$(platform)' -lIOTCAPIs -lpthread -o IOTCAPIs_WakeUp2_Device	
	gcc $(CFLAGS) -std=c99 -g -O2 -Wall IOTCAPIs_WakeUp_Client.c -I./ -I../../../../Include -L'../../../../Lib/Linux/$(platform)' -lIOTCAPIs -lpthread -o IOTCAPIs_WakeUp_Client
	gcc $(CFLAGS) -std=c99 -g -O2 -Wall IOTCAPIs_WakeUp_AutoWakeUp_Client.c -I./ -I../../../../Include -L'../../../../Lib/Linux/$(platform)' -lIOTCAPIs -lpthread -o IOTCAPIs_WakeUp_AutoWakeUp_Client

clean:
	rm -rf *.o *~ *.bak IOTCAPIs_WakeUp_Device IOTCAPIs_WakeUp2_Device IOTCAPIs_WakeUp_Client IOTCAPIs_WakeUp_AutoWakeUp_Client
