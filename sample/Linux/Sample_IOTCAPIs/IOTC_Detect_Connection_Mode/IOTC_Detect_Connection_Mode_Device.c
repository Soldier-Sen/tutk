#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "IOTCAPIs.h"

#define MAX_SESSION 8

// Type declaration
typedef struct ThreadParam_t
{
	int nSID;
	unsigned char szChID;
} ThreadParam;


static void *thread_ForSessionReadWrite_Infinite(void *arg)
{
	ThreadParam *pParam = (ThreadParam *)arg;
	unsigned char ChID = pParam->szChID;
	int SID = pParam->nSID;

	free(pParam);

	int i = 0;
	int nWrite;
	char buf[IOTC_MAX_PACKET_SIZE];
	struct st_SInfo Sinfo;

	printf("Thread started...[%d]\n", ChID);

	IOTC_Session_Check(SID, &Sinfo);
	if(Sinfo.Mode ==0)
		printf("Client is from %s:%d[%s] Mode = P2P\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
	else if(Sinfo.Mode == 1)
		printf("Client is from %s:%d[%s] Mode = RLY\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
	else if(Sinfo.Mode == 2)
		printf("Client is from %s:%d[%s] Mode = LAN\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);

	printf("VPG [%d:%d:%d] Remote NAT[%d]\n", Sinfo.VID, Sinfo.PID, Sinfo.GID, Sinfo.NatType);

	IOTC_Session_Channel_ON(SID, ChID);
	sleep(3);

	// Write infinte
	for(i = 0; ; i++) {
		sprintf(buf, "Hello World[%d][%d]", ChID, i);
		nWrite = IOTC_Session_Write(SID, buf, strlen(buf), ChID);

		printf("Write Message[%d] : %s\n", ChID, buf);

		if(nWrite < 0) {
			printf("Write error[%d]->%d\n", ChID, nWrite);
			break;
		}

		sleep(1);
	}

	// check if remote site close this session
	if(nWrite == IOTC_ER_SESSION_CLOSE_BY_REMOTE)
		printf("remote site close this session, SID = %d\n", SID);
	else if(nWrite == IOTC_ER_REMOTE_TIMEOUT_DISCONNECT)
		printf("disconnected due to remote site has no any response after a 'timeout' period of time., SID = %d\n", SID);
	else if(nWrite < IOTC_ER_NoERROR)
		printf("Get an IOTC error - %d\n", nWrite);
	
	IOTC_Session_Close(SID);
	
	printf("Thread exit...[%d]\n", ChID);

	pthread_exit(0);
}


static void create_write_thread(int SID, int ChID) {
	// Read/Write data
	pthread_t threadID;
	ThreadParam *param = (ThreadParam*)malloc(sizeof(ThreadParam));
	memset(param, 0, sizeof(ThreadParam));

	param->nSID = SID;
	param->szChID = ChID;
	pthread_create(&threadID, NULL, &thread_ForSessionReadWrite_Infinite, param);
	pthread_detach(threadID);
}


static void *_thread_Login(void *arg)
{
	int ret;
	char *UID = (char *)arg;
	while(1)
	{
		ret = IOTC_Device_Login(UID, "AAAA0009", "12345678");
		printf("     Calling IOTC_Device_Login() ret = %d\n", ret);
		if(ret == IOTC_ER_NoERROR)
			break;
		else
			sleep(5);
	}

	pthread_exit(0);
}


int main(int argc, char *argv[])
{
	int ret;
	int i = 0;
	int SID = -1;
	char *UID = (char *)argv[1];
	pthread_t threadID_Login;

	if(argc < 2) {
		printf("No UID input!!!\n");
		printf("IOTCAPIs_Device [UID]\n");
		return 0;
	}

	// Must be first call before IOTC_Initialize(), but not a must
	IOTC_Set_Max_Session_Number(MAX_SESSION);

	printf("IOTCAPIs_Device start...\n");
	
	ret = IOTC_Initialize2(0);
	printf("Step 1: IOTC_Initialize() ret = %d\n", ret);
	if(ret != IOTC_ER_NoERROR) {
		printf("IOTCAPIs_Device exit...!!\n");
		return 0;
	}

	pthread_create(&threadID_Login, NULL, &_thread_Login, (void *)UID);
	pthread_detach(threadID_Login);

	while(1)
	{
		struct st_DeviceStInfo devStInfo;
		printf("Step 2: Checking device status...\n");
		
		ret = IOTC_Get_Device_Status(&devStInfo);
        if (ret == IOTC_ER_NoERROR) {
			if(devStInfo.state == IOTC_DEV_ST_CONNECTING || 
			   devStInfo.state == IOTC_DEV_ST_CONNECTED_WAITING)
			{
				if (devStInfo.connectedSessionNum >= MAX_SESSION) {
					printf("Exceed max session number\n");
					sleep(5);
					continue;
				}

				for (i = 0; i < devStInfo.newConnected; i++) {
					ret = IOTC_Accept(&SID);
					if (ret == IOTC_ER_NoERROR) {
						if (SID > -1) {
							printf("Step 3: IOTC_Accept() SID = %d\n", SID);
						}

						create_write_thread(SID, 0);
					} else {
						printf("IOTC_Accept failed error code = %d\n", ret);
						break;
					}
				}
			} else {
				printf("Device state = %d\n", devStInfo.state);
			}
        }

		if (ret < 0) {
			printf("Error occurs - %d\n", ret);
			break;
		}

		sleep(2);
	}

	IOTC_DeInitialize();

	return 0;
}
