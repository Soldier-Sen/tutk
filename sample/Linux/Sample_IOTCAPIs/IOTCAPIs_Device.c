#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "IOTCAPIs.h"

typedef struct _client
{
	int nSID;
	int ChID;
}ConnectClient;

static int gSID;

#define MAX_SESSION 8

void *thread_ForSessionReadWrite(void *arg)
{
	ConnectClient *pClient = (ConnectClient *)arg;
	unsigned char ChID = pClient->ChID;
	int SID = pClient->nSID;
	int i;
	int nRead;
	char buf[IOTC_MAX_PACKET_SIZE];
	struct st_SInfo Sinfo;

	printf("[thread_ForSessionReadWrite] Thread started...[%d]\n", ChID);

	IOTC_Session_Check(SID, &Sinfo);
	if(Sinfo.Mode ==0)
		printf("Client is from %s:%d[%s] Mode=P2P\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
	else if(Sinfo.Mode == 1)
		printf("Client is from %s:%d[%s] Mode=RLY\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
	else if(Sinfo.Mode == 2)
		printf("Client is from %s:%d[%s] Mode=LAN\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);

	printf("VPG [%d:%d:%d] Remote NAT[%d]\n", Sinfo.VID, Sinfo.PID, Sinfo.GID, Sinfo.NatType);

	IOTC_Session_Channel_ON(SID, ChID);
	sleep(3);

	for(i = 0; i < 3; i++)
	{
		sprintf(buf, "Hello World[%d][%d]", ChID, i);
		int ret = IOTC_Session_Write(SID,buf,strlen(buf), ChID);
		if(ret < 0)
			printf("Write error[%d]->%d\n", ChID, ret);
	}

	sprintf(buf, "end");
	IOTC_Session_Write(SID, buf, strlen(buf), ChID);

	while((nRead = IOTC_Session_Read(SID, buf, IOTC_MAX_PACKET_SIZE, 6000, ChID)) > -1)
	{
		buf[nRead] = 0;
		printf("[IOTC_Session_Read] SID=%d, ChID = %d, Size=%d, Data:%s\n",SID,ChID,nRead,buf);
	}
	// check if remote site close this session
	if(nRead == IOTC_ER_SESSION_CLOSE_BY_REMOTE)
	{
		printf("[thread_ForSessionReadWrite] %d remote site close this session, SID = %d\n", ChID, SID);
		IOTC_Session_Close(SID);
	}
	else if(nRead == IOTC_ER_REMOTE_TIMEOUT_DISCONNECT)
	{
		printf("[thread_ForSessionReadWrite] %d disconnected due to remote site has no any response after a 'timeout' period of time., SID = %d\n", ChID, SID);
		IOTC_Session_Close(SID);
	}
	else
	{
		printf("[thread_ForSessionReadWrite] %d, nRead = %d, SID = %d\n", ChID, nRead, SID);
		IOTC_Session_Close(SID);
	}

	printf("[thread_ForSessionRead] Thread exit...[%d]\n", ChID);
	free(pClient);
	pthread_exit(0);
}

void *_thread_Login(void *arg)
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
	unsigned char ChID = 0, ChID2 = 1;
	char *UID = (char *)argv[1];
	pthread_t threadID_Login;

	if(argc < 2)
	{
		printf("No UID input!!!\n");
		printf("IOTCAPIs_Device [UID]\n");
		return 0;
	}

	// Must be first call before IOTC_Initialize(), but not a must
	IOTC_Set_Max_Session_Number(MAX_SESSION);

	printf("IOTCAPIs_Device start...\n");
	ret = IOTC_Initialize2(0);
	printf("Step 1: IOTC_Initialize() ret = %d\n", ret);
	if(ret != IOTC_ER_NoERROR)
	{
		printf("IOTCAPIs_Device exit...!!\n");
		return 0;
	}

	pthread_create(&threadID_Login, NULL, &_thread_Login, (void *)UID);
	pthread_detach(threadID_Login);

	while(1)
	{
		struct st_DeviceStInfo devStInfo;
		pthread_t Thread_ID1, Thread_ID2;

		printf("Step 2: Checking device status...\n");
		ret = IOTC_Get_Device_Status(&devStInfo);
        if (ret == IOTC_ER_NoERROR)
        {
			if(devStInfo.state == IOTC_DEV_ST_CONNECTING || devStInfo.state == IOTC_DEV_ST_CONNECTED_WAITING)
			{
				if (devStInfo.connectedSessionNum >= MAX_SESSION)
				{
					printf("Exceed max session number\n");
					sleep(5);
					continue;
				}

				for (i = 0; i < devStInfo.newConnected; i++)
				{
					ret = IOTC_Accept(&gSID);
					if (ret == IOTC_ER_NoERROR)
					{
						if (gSID > -1)
						{
							ConnectClient *connectClient = (ConnectClient *)malloc(sizeof(ConnectClient));
							connectClient->nSID = gSID;
							connectClient->ChID = ChID;
							
							ConnectClient *connectClient1 = (ConnectClient *)malloc(sizeof(ConnectClient));
							connectClient1->nSID = gSID;
							connectClient1->ChID = ChID2;
					
							printf("Step 3: IOTC_Accept() SID = %d\n", gSID);
							printf("Step4: Create a Thread to handle data read / write from this session\n");
							
							IOTC_Session_Channel_ON(gSID, ChID2);
							pthread_create(&Thread_ID1, NULL, &thread_ForSessionReadWrite, (void*)connectClient);
							pthread_create(&Thread_ID2, NULL, &thread_ForSessionReadWrite, (void*)connectClient1);
							pthread_detach(Thread_ID1);
							pthread_detach(Thread_ID2);
						}
					}
					else
					{
						printf("IOTC_Accept failed error code = %d\n", ret);
						break;
					}
				}
			}
			else
			{
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
