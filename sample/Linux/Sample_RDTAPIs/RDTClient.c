#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "IOTCAPIs.h"
#include "RDTAPIs.h"
#include "common.h"
#include "RDTPacketHelper.h"

#define RDT_CREATE_TIMEOUT 10000
#define RDT_READ_TIMEOUT 1000

typedef struct _RDTDeviceInfo
{
    int inProgress;
    int nSID;
    int nChannelID;
    int rdtCH;
    unsigned int timeout;
    char szUID[24];
    pthread_t threadID;
}RDTDeviceInfo;

void *thread_RDTCreate(void *arg)
{
    RDTDeviceInfo* pRDTDeviceInfo = (RDTDeviceInfo*)arg;
    FILE *fp = NULL;
    char req[128] = {0}, filename[128] = {0}, *resp = NULL;
    int ret = 0, fileSize = 0, remainFileSize = 0;

    // RDT Create
	pRDTDeviceInfo->rdtCH = RDT_Create(pRDTDeviceInfo->nSID, RDT_CREATE_TIMEOUT, pRDTDeviceInfo->nChannelID);
	if(pRDTDeviceInfo->rdtCH < 0)
	{
		printf("SID[%d] RDT_Create failed[%d]\n", pRDTDeviceInfo->nSID, pRDTDeviceInfo->rdtCH);
        goto THREAD_IOTCCLOSE;
	}

	printf("SID[%d] RDT_ID[%d]\n", pRDTDeviceInfo->nSID, pRDTDeviceInfo->rdtCH);

    //Get FileInfo
    sprintf(req, "Get FileInfo");
    if((ret = RDTPacketWrite(pRDTDeviceInfo->rdtCH, req, strlen(req)+1)) < 0){
        printf("RDTPacketWrite error ret[%d]\n", ret);
        goto THREAD_RDTDESTROY;
    }

    if((ret = RDTPacketRead(pRDTDeviceInfo->rdtCH, &resp)) < 0){
        printf("RDTPacketRead error ret[%d]\n", ret);
        goto THREAD_RDTDESTROY;
    }
    sscanf(resp,"FileInfo FileName %s FileSize %d", filename, &fileSize);
    printf("filename[%s] fileSize[%d]\n", filename, fileSize);
    srandom(time(NULL));
    sprintf(filename, "%s_bak_%ld", filename, random()%1000);
    RDTPacketRelease(&resp);

    //Open File for Write
    fp = fopen(filename, "w");
    if(fp == NULL){
        printf("fopen error filename[%s]\n", filename);
        goto THREAD_RDTDESTROY;
    }

    //Start
    sprintf(req, "Start");
    if((ret = RDTPacketWrite(pRDTDeviceInfo->rdtCH, req, strlen(req)+1)) < 0){
        printf("RDTPacketWrite error ret[%d]\n", ret);
        goto THREAD_FCLOSE;
    }

    remainFileSize = fileSize;
    while(remainFileSize > 0){
        ret = RDTPacketRead(pRDTDeviceInfo->rdtCH, &resp);
        if(ret > 0){
            fwrite(resp, 1, ret, fp);
            RDTPacketRelease(&resp);
            remainFileSize -= ret;
            printf("\rrdtCH[%d] Remain [%10d / %10d]", pRDTDeviceInfo->rdtCH, remainFileSize, fileSize);
        }
        else if(ret == 0){
            usleep(100*1000);
        }
        else{
            printf("RDTPacketRead error ret[%d]\n", ret);
            break;
        }
    }
    printf("\n");

    //Recv Done
    if(remainFileSize == 0){
        printf("filename[%s] Recv Done\n", filename);
        sprintf(req, "Exit");
        if((ret = RDTPacketWrite(pRDTDeviceInfo->rdtCH, req, strlen(req)+1)) < 0){
            printf("RDTPacketWrite error ret[%d]\n", ret);
            goto THREAD_FCLOSE;
        }
    }

THREAD_FCLOSE:
    fclose(fp);
THREAD_RDTDESTROY:
	RDT_Destroy(pRDTDeviceInfo->rdtCH);
THREAD_IOTCCLOSE:
	IOTC_Session_Close(pRDTDeviceInfo->nSID);
    pRDTDeviceInfo->inProgress = 0;
	pthread_exit(0);
}

static void ConnectStateHandler(IOTCConnectState state, int errCode, void* pUserData)
{
    RDTDeviceInfo* pRDTDeviceInfo = (RDTDeviceInfo*)pUserData;
    struct st_SInfo psSessionInfo;
    char *mode[3] = {"P2P", "RLY", "LAN"};

    switch(state){
        case IOTC_CONNECT_UID_ST_FAILED:    // A Client connects to a device failed.
            if(pRDTDeviceInfo != NULL){
                printf("%s UID[%s] IOTC_CONNECT_UID_ST_FAILED, connects to a device failed\n", __FUNCTION__, pRDTDeviceInfo->szUID);
                IOTC_Session_Close(pRDTDeviceInfo->nSID);
                pRDTDeviceInfo->inProgress = 0;
            }
            break;
        case IOTC_CONNECT_UID_ST_START:     // A Client start connecting to a Device.
            if(pRDTDeviceInfo != NULL){
                printf("%s UID[%s] IOTC_CONNECT_UID_ST_START, start connecting to a Device\n", __FUNCTION__, pRDTDeviceInfo->szUID);
            }
            break;
        case IOTC_CONNECT_UID_ST_CONNECTING: // A Client is connecting a device.
            if(pRDTDeviceInfo != NULL){
                printf("%s UID[%s] IOTC_CONNECT_UID_ST_CONNECTING, connecting to a device\n", __FUNCTION__, pRDTDeviceInfo->szUID);
            }
            break;
        case IOTC_CONNECT_UID_ST_CONNECTED: // The connection are established between a Client and a Device.
            if(pRDTDeviceInfo != NULL){
                printf("%s UID[%s] IOTC_CONNECT_UID_ST_CONNECTED, The connection are established between a Client and a Device\n", __FUNCTION__, pRDTDeviceInfo->szUID);
                IOTC_Session_Check(pRDTDeviceInfo->nSID, &psSessionInfo);
                printf("Device from [%s:%d] Mode[%s]\n",psSessionInfo.RemoteIP, psSessionInfo.RemotePort, mode[(int)psSessionInfo.Mode]);

                //Create RDTCreate Thread
                pthread_create(&pRDTDeviceInfo->threadID, NULL, &thread_RDTCreate, (void *)pRDTDeviceInfo);
                pthread_detach(pRDTDeviceInfo->threadID);
            }
            break;
        default:
            break;
    }
}

int StartConnect(RDTDeviceInfo* gRDTDeviceInfo)
{
    int nSID = -1, ret = 0;

    // Get Session ID
    nSID = IOTC_Get_SessionID();
    if(nSID < 0){
        return nSID;
    }

    // Fill RDTDeviceInfo
    gRDTDeviceInfo->nSID = nSID;

    // Connect to Device
    ret = IOTC_Connect_ByUID_ParallelNB((const char *)gRDTDeviceInfo->szUID, gRDTDeviceInfo->nSID, ConnectStateHandler, (void *)gRDTDeviceInfo);
    if(ret < 0){
        printf("IOTC_Connect_ByUID_ParallelNB failed ret[%d]\n", ret);
        IOTC_Session_Close(nSID);
        return nSID;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int ret = 0, rdtVer = 0;
    unsigned int iotcVer = 0;
    RDTDeviceInfo sRDTDeviceInfo;

    if(argc < 2)
    {
        printf("No UID!!!\n");
        printf("RDTClient [UID]\n");
        return 0;
    }

	//Get IOTC,RDT Version
    IOTC_Get_Version(&iotcVer);
    rdtVer = RDT_GetRDTApiVer();
	printf("IOTC Ver[0x%x] RDT Ver[0x%X]\n", iotcVer, rdtVer);

    //IOTC Initilize
	ret = IOTC_Initialize2(0);
	if(ret != IOTC_ER_NoERROR)
	{
		printf("IOTC_Initialize error!!\n");
		return 0;
	}

    //RDT Initilize
	ret = RDT_Initialize();
	if(ret <= 0)
	{
		printf("RDT_Initialize error!!\n");
		return 0;
	}

    //Start Connect to RDT Server
    memset(&sRDTDeviceInfo, 0, sizeof(RDTDeviceInfo));
    sRDTDeviceInfo.nChannelID = 0;
    strncpy(sRDTDeviceInfo.szUID, (char *)argv[1], 20);
    sRDTDeviceInfo.inProgress = 1;
    ret = StartConnect(&sRDTDeviceInfo);
    if(ret < 0)
    {
        printf("StartConnect error!!\n");
        RDT_DeInitialize();
        IOTC_DeInitialize();
		return 0;
    }

	while(sRDTDeviceInfo.inProgress)
	{
		sleep(1);
	}

    RDT_DeInitialize();
    IOTC_DeInitialize();

	return 0;
}

