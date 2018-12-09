#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include "IOTCAPIs.h"
#include "RDTAPIs.h"
#include "common.h"

int gFd;
static char gFn[64];

struct channelSendData_s
{
	int SID;
	unsigned char CHID;
};

void *thread_channelSendData(void * arg)
{
	struct channelSendData_s * chSendArg = (struct channelSendData_s *) arg;
	int SID = chSendArg->SID;
	unsigned char CHID = chSendArg->CHID;
	struct st_SInfo sInfo;
	char buf[MAX_BUF_SIZE];

	FILE *fp = fopen(gFn, "rb");
	if(fp == NULL)
	{
		printf("FOpen file[%s] errno[%d]!\n", gFn, errno);
		pthread_exit(0);
	}

	IOTC_Session_Channel_ON(SID, CHID);

	IOTC_Session_Check(SID, &sInfo);
	int RDT_ID = RDT_Create(SID, RDT_WAIT_TIMEMS, CHID);
	if(RDT_ID < 0)
	{
		printf("RDT_Create failed[%d]!!\n", RDT_ID);
		IOTC_Session_Close(SID);
		fclose(fp);
		pthread_exit(0);
	}

	printf("RDT_ID = %d\n", RDT_ID);
#if 0
	int ret = RDT_Read(RDT_ID, buf, 1024, RDT_WAIT_TIMEMS);
	if(ret < 0)
	{
		printf("RDT rcv cmd error[%d]!\n", ret);
		fclose(fp);
		pthread_exit(0);
	}
	int requestCnt = atoi(buf);
#endif
	int ret = 0;
	struct stat fStat;
	fstat(gFd, &fStat);
	struct st_RDT_Status rdtStatus;
	printf("[%d]file size[%lu]\n", RDT_ID, fStat.st_size);
	sprintf(buf, "%lu", fStat.st_size);
	ret =  RDT_Write(RDT_ID, buf, strlen(buf));
	if(ret < 0)
	{
		printf("[%d]RDT send file size error[%d]!\n", RDT_ID, ret);
		fclose(fp);
		pthread_exit(0);
	}
	ret = RDT_Read(RDT_ID, buf, 1024, RDT_WAIT_TIMEMS);
	if(ret < 0 && strcmp(buf, START_STRING) != 0)
	{
		printf("[%d]RDT rcv start to send file error[%d]!\n", RDT_ID, ret);
		fclose(fp);
		pthread_exit(0);
	}

	while(1)
	{
		ret = fread(buf, 1, MAX_BUF_SIZE, fp);
		if(ret <= 0) break;
		ret = RDT_Write(RDT_ID, buf, ret);
		if(ret < 0)
		{
			printf("[%d]RDT send file data error[%d]!\n", RDT_ID, ret);
			pthread_exit(0);
		}

		if((ret = RDT_Status_Check(RDT_ID, &rdtStatus)) == RDT_ER_NoERROR)
		{
			if(sInfo.Mode == 2)
			{
				if(rdtStatus.BufSizeInSendQueue > 1024000)
					usleep(50000);
			}
			else
			{
				if(rdtStatus.BufSizeInSendQueue > 512000)
					usleep(100000);
				//sleep(1);
			}
		}
		else
		{
			printf("[%d]RDT status check error[%d]!\n", RDT_ID, ret);
			pthread_exit(0);
		}
	}

	fseek(fp, 0, SEEK_SET);

	printf("[%d]***RDT_Destroy enter\n", RDT_ID);
	RDT_Destroy(RDT_ID);
	fclose(fp);
	IOTC_Session_Channel_OFF(SID, CHID);
	
	printf("thread_Send exit[%d]\n", RDT_ID);

	pthread_exit(0);

}

void *thread_Send(void *arg)
{
	struct channelSendData_s argThread1;
	struct channelSendData_s argThread2;
	struct channelSendData_s argThread3;
	pthread_t channelSendDataThread1;
	pthread_t channelSendDataThread2;
	pthread_t channelSendDataThread3;
	int SID = *(int *)arg;
	free(arg);


	memset(&argThread1, 0, sizeof(struct channelSendData_s));
	memset(&argThread2, 0, sizeof(struct channelSendData_s));
	memset(&argThread3, 0, sizeof(struct channelSendData_s));

	argThread1.SID = argThread2.SID = argThread3.SID = SID;
	argThread1.CHID = 0;
	argThread2.CHID = 1;
	argThread3.CHID = 2;

	pthread_create(&channelSendDataThread1, NULL, &thread_channelSendData, (void *)&argThread1);
	pthread_create(&channelSendDataThread2, NULL, &thread_channelSendData, (void *)&argThread2);
	pthread_create(&channelSendDataThread3, NULL, &thread_channelSendData, (void *)&argThread3);

	pthread_join(channelSendDataThread1, NULL);
	pthread_join(channelSendDataThread2, NULL);
	pthread_join(channelSendDataThread3, NULL);
	
	while(1)
	{
		struct st_SInfo info;
		if( IOTC_Session_Check(SID, &info) < 0)
		{
			IOTC_Session_Close(SID);
			break;
		}
		sleep(1);
	}

	pthread_exit(0);
}

void *_thread_Login(void *arg)
{
	int ret;
	char *UID = (char *)arg;
	while(1)
	{
		ret = IOTC_Device_Login(UID, "TUTK", "1234");
		printf("     Calling IOTC_Device_Login() ret = %d\n", ret);
		if(ret == IOTC_ER_NoERROR)
			break;
		else
			sleep(60);
	}

	pthread_exit(0);
}

#define MAX_CLIENT_NUM 8

int main(int argc, char *argv[])
{
	if(argc < 4)
	{
		printf("Arg wrong!!!\n");
		printf("  --RDTServer [UID] [Filename] [MaxClientNum]\n");
		return 0;
	}
	printf("RDT Ver[%X]\n", RDT_GetRDTApiVer());

	int maxClientNum = atoi(argv[3]);
	strcpy(gFn, argv[2]);
	printf("%s\n", gFn);
	gFd = open(gFn, O_RDONLY);
	if(gFd < 0)
	{
		printf("Open file[%s] error code[%d], %d!!\n", gFn, gFd, errno);
		exit(0);
	}

	int ret = IOTC_Initialize2(0);
	if(ret != IOTC_ER_NoERROR)
	{
		printf("IOTC_Initialize error!!\n");
		return 0;
	}

	int rdtCh = RDT_Initialize();
	if(rdtCh <= 0)
	{
		printf("RDT_Initialize error!!\n");
		return 0;
	}

	char *UID = (char *)argv[1];
	pthread_t threadID_Login;
	pthread_create(&threadID_Login, NULL, &_thread_Login, (void *)UID);
	pthread_detach(threadID_Login);

	int SID = -1, clientCnt = 0;
	struct st_SInfo Sinfo;
	printf("start IOTC_Listen...\n");
	//unsigned long t;
	pthread_t Thread_ID[MAX_CLIENT_NUM];
	do {
		SID = IOTC_Listen(300000);

		if(SID > -1)
		{
            int *pSID = malloc(sizeof(int));
            if(pSID == NULL)
            {
                printf("[System error] - malloc failed\n");
                exit(0);
            }
			IOTC_Session_Check(SID, &Sinfo);
			char *mode[3] = {"P2P", "RLY", "LAN"};
			printf("Client from %s:%d Mode=%s\n",Sinfo.RemoteIP, Sinfo.RemotePort, mode[(int)Sinfo.Mode]);
			*pSID = SID;
			pthread_create(&Thread_ID[clientCnt], NULL, &thread_Send, (void *)pSID);
			//if(clientCnt == 0) t = IOTC_GetTickCount();
			clientCnt++;
		}
		if(clientCnt == maxClientNum)
			break;
	}while(1);

	int i;
	for(i=0;i<maxClientNum;i++)
		pthread_join(Thread_ID[i], NULL);

	//printf("RDT Send Data Cost[%lu ms]....\n", IOTC_GetTickCount()-t);
	RDT_DeInitialize();
	printf("RDT_DeInitialize OK\n");
	IOTC_DeInitialize();

	close(gFd);
	printf("Server exit!\n");

	return 0;
}


