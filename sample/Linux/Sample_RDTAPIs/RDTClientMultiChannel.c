#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "IOTCAPIs.h"
#include "RDTAPIs.h"
#include "common.h"


struct channelReadData_s
{
	int SID;
	unsigned char CHID;
};

void *thread_channelReadData(void *arg)
{
	struct channelReadData_s * chSendArg = (struct channelReadData_s *) arg;
	int SID = chSendArg->SID;
	unsigned char CHID = chSendArg->CHID;
	int ret = 0;

	IOTC_Session_Channel_ON(SID, CHID);

	int nRDTIndex = RDT_Create(SID, RDT_WAIT_TIMEMS, CHID);
	if(nRDTIndex < 0)
	{
		printf("RDT_Create failed[%d]!!\n", nRDTIndex);
		IOTC_Session_Close(SID);
		pthread_exit(0);
	}
	printf("RDT_Create OK[%d]\n", nRDTIndex);
	char buf[MAX_BUF_SIZE];
#if 0
	ret = RDT_Write(nRDTIndex, buf, strlen(buf));
	if(ret < 0)
	{
		printf("RDT send cmd failed[%d]!!\n", ret);
		IOTC_Session_Close(SID);
		pthread_exit(0);
	}
#endif

	ret = RDT_Read(nRDTIndex, buf, 1024, RDT_WAIT_TIMEMS);
	if(ret < 0)
	{
		printf("[%d]RDT rcv file size failed[%d]!!\n", nRDTIndex, ret);
		IOTC_Session_Close(SID);
		pthread_exit(0);
	}
	int fileSize = atoi(buf), remainReadSize;

	strcpy(buf, START_STRING);
	ret = RDT_Write(nRDTIndex, buf, strlen(buf));
	if(ret < 0)
	{
		printf("[%d]RDT send start failed[%d]!!\n", nRDTIndex, ret);
		IOTC_Session_Close(SID);
		pthread_exit(0);
	}

	printf("[%d]fileSize[%d]\n", nRDTIndex, fileSize);
	//FILE *fp;
	char fn[32];
	struct st_RDT_Status rdtStatus;

	if(RDT_Status_Check(nRDTIndex, &rdtStatus) < 0)
	{
		printf("[%d]RDT_Status_Check error!\n", nRDTIndex);
		pthread_exit(0);
	}
	remainReadSize = fileSize;
	sprintf(fn, "%d.jpg",CHID);
#if 1
	FILE *fp = fopen(fn, "w+");
	if(fp == NULL)
	{
		printf("[%d]Open file[%s] error!!\n", nRDTIndex, fn);
		pthread_exit(0);
	}
#endif
	while(1)
	{
		if(remainReadSize < MAX_BUF_SIZE)
			ret = RDT_Read(nRDTIndex, buf, remainReadSize, RDT_WAIT_TIMEMS);
		else
			ret = RDT_Read(nRDTIndex, buf, MAX_BUF_SIZE, RDT_WAIT_TIMEMS);

		if(ret < 0 && ret != RDT_ER_TIMEOUT)
		{
			printf("[%d]RDT_Read data failed[%d]!!", nRDTIndex, ret);
			break;
		}

		if(ret > 0)
		{
			printf("[%d]RDT_Read[%d]\n", nRDTIndex, ret);
			remainReadSize -= ret;
		}
		fwrite(buf, 1, ret, fp);
		fflush(fp);
		if(remainReadSize == 0) break;
	}
	fclose(fp);
	printf("[%d]rcv file OK [%d]\n", nRDTIndex, CHID);

	//printf("RDT_Destroy calling....\n");
	RDT_Destroy(nRDTIndex);
	printf("[%d]RDT_Destroy calling....OK!\n", nRDTIndex);

	IOTC_Session_Channel_OFF(SID, CHID);

	pthread_exit(0);
}


int main(int argc, char *argv[])
{
	struct channelReadData_s argThread1;
	struct channelReadData_s argThread2;
	struct channelReadData_s argThread3;
	pthread_t channelReadDataThread1;
	pthread_t channelReadDataThread2;
	pthread_t channelReadDataThread3;

	if(argc < 2)
	{
		printf("No UID !!!\n");
		printf("RDTClient [UID]\n");
		return 0;
	}

	printf("RDT Version[%X]\n", RDT_GetRDTApiVer());

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

	int SID = 0;
	char *UID = (char *)argv[1];

	while(1)
	{
		SID = IOTC_Get_SessionID();
		if(SID == IOTC_ER_NOT_INITIALIZED)
		{
			printf("Not Initialize!!!\n");
			return 0;
		}
		else if (SID == IOTC_ER_EXCEED_MAX_SESSION)
		{
			printf("EXCEED MAX SESSION!!!\n");
			return 0;
		}
			
		ret = IOTC_Connect_ByUID_Parallel(UID, SID);
		printf("Step 2: call IOTC_Connect_ByUID_Parallel(%s) ret = %d\n", UID, ret);
		if(ret < 0)
		{
			printf("p2pAPIs_Client connect failed...!!\n");
			return 0;
		}
		else if(ret > -1)
			break;
	}

	memset(&argThread1, 0, sizeof(struct channelReadData_s));
	memset(&argThread2, 0, sizeof(struct channelReadData_s));
	memset(&argThread3, 0, sizeof(struct channelReadData_s));

	argThread1.SID = argThread2.SID = argThread3.SID = SID;
	argThread1.CHID = 0;
	argThread2.CHID = 1;
	argThread3.CHID = 2;

	pthread_create(&channelReadDataThread1, NULL, &thread_channelReadData, (void *)&argThread1);
	pthread_create(&channelReadDataThread2, NULL, &thread_channelReadData, (void *)&argThread2);
	pthread_create(&channelReadDataThread3, NULL, &thread_channelReadData, (void *)&argThread3);

	pthread_join(channelReadDataThread1, NULL);
	pthread_join(channelReadDataThread2, NULL);
	pthread_join(channelReadDataThread3, NULL);


	IOTC_Session_Close(SID);
	printf("IOTC_Session_Close OK!\n");
	RDT_DeInitialize();
	printf("RDT_DeInitialize OK!\n");
	IOTC_DeInitialize();
	printf("*****clien exit*****\n");

	return 0;
}

