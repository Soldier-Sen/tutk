
all:
	@echo "=== Target Platform [x86] ==="
	gcc -m32 -g -Wall IOTC_Detect_Connection_Mode_Client.c -I./ -I../../../../Include -L'../../../../Lib/Linux/x86' -lIOTCAPIs -lpthread -o IOTC_Detect_Connection_Mode_Client
	gcc -m32 -g -Wall IOTC_Detect_Connection_Mode_Device.c -I./ -I../../../../Include -L'../../../../Lib/Linux/x86' -lIOTCAPIs -lpthread -o IOTC_Detect_Connection_Mode_Device
	
clean:
	rm -rf *.o *~ *.bak IOTC_Detect_Connection_Mode_Client IOTC_Detect_Connection_Mode_Device