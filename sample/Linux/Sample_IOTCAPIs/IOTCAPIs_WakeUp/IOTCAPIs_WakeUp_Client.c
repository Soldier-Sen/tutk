/**====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
*
* IOTCAPIs_Wakeup_Client.c
*
* Copyright (c) by TUTK Co.LTD. All Rights Reserved.
*
* @brief   Examle of how to use IOTC WakeUp API at Client side
*
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "IOTCAPIs.h"
#include "IOTCWakeUp.h"


typedef struct _WakeupParam
{
	char UID[20];
	bool bWakeUp;
} WakeupParam_t;

static WakeupParam_t g_wuParam[2];


void *thread_WakeUpTask(void *arg)
{
    WakeupParam_t *pWU = (WakeupParam_t*)arg;

    int nRet = -1;
    while(!pWU->bWakeUp)
    {
        nRet = IOTC_WakeUp_WakeDevice(pWU->UID);
        if(nRet < 0)
        {
        	if(nRet == IOTC_ER_NOT_SUPPORT)
        		printf("[thread_WakeUpTask] - WakeUp API feature is not supported!\n");

        	break;
        }
        else
        	sleep(1);
    }

    if(pWU->bWakeUp)
    	printf("[thread_WakeUpTask] - Device is woken up!\n");

    pthread_exit(0);
}


pthread_t testIOTC_Wakeup(WakeupParam_t *pWU)
{
    pthread_t nTaskId = 0;

    int nRet = pthread_create(&nTaskId, NULL, thread_WakeUpTask, pWU);
    if(nRet != 0)
        printf("[testIOTC_Wakup] pthread_create for fail!\n");

    return nTaskId;
}


void connectDevice(WakeupParam_t *pWU)
{
	int nSID = -1;
	nSID = IOTC_Get_SessionID();
	if(nSID < 0)
	{
		printf("IOTC_Get_SessionID error code [%d]\n", nSID);
		return;
	}

	printf("IOTC_Connect_ByUID_Parallel\n");
	nSID = IOTC_Connect_ByUID_Parallel(pWU->UID, nSID);
	if(nSID >= 0)
	{
		printf("IOTC Connect OK SID[%d]\n", nSID);
	} else {
		if(nSID == IOTC_ER_DEVICE_IS_SLEEP)
		{
			printf("Device is in sleep and prepare to wake it up. UID = %s\n", pWU->UID);	

			// Wake Device Up
    		pthread_t nTaskId = testIOTC_Wakeup(pWU);

    		sleep(3);

    		nSID = IOTC_Connect_ByUID_Parallel(pWU->UID, nSID);
    		if(nSID >= 0)
    			pWU->bWakeUp = true;

    		pthread_join(nTaskId, 0);
		}
		printf("IOTC_Connect error code[%d]\n", nSID);
	}
	
	IOTC_Session_Close(nSID);
}


int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		printf("No UID input!!!\n");
		printf("IOTCAPIs_Client [UID]\n");
		return 0;
	}
	
    int nRet = -1;
    
	nRet = IOTC_Initialize2(0);
	if(nRet != IOTC_ER_NoERROR)
	{
		printf("IOTCAPIs_Client exit...!!\n");
		return 0;
	}

	int UIDcnt = argc -1; 
	
	memset(&g_wuParam, 0, sizeof(WakeupParam_t) * UIDcnt);
	
	for(int i = 0; i < UIDcnt; i++)
	{
		memcpy(&g_wuParam[i].UID, argv[i+1], 20);
		connectDevice(&g_wuParam[i]);
	}

    printf("Prepare IOTC_DeInitialize\n");
	IOTC_DeInitialize();

    return 0;
}
