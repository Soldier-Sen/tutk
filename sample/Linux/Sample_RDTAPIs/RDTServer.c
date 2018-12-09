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
#include "RDTPacketHelper.h"

#define MAX_CLIENT_NUM 8
#define RDT_CREATE_TIMEOUT 10000

typedef struct RDTClient_t
{
	int SID;
    int rdtCH;
    int inUse;
    int inProgress;
    pthread_t threadID;
}RDTClient;

RDTClient gRDTClient[MAX_CLIENT_NUM];
static int gProcess = 1;
static int gLoginRetry = 0;
static char gFn[256] = {0};

void *thread_RDTCreate(void *arg)
{
    RDTClient *pRDTClient = (RDTClient*)arg;
    FILE *fp = NULL;
    char buf[MAX_PACKET_SIZE], resp[128] = {0}, *clientCMD = NULL;
    int ret = 0, fileSize = 0, remainFileSize = 0, readSize = 0;

    //Open File For Read
    fp = fopen(gFn, "rb");
    if(fp == NULL)
    {
        printf("fopen file[%s] errno[%d]!\n", gFn, errno);
        goto THREAD_IOTCCLOSE;
    }

    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // RDT Create
	pRDTClient->rdtCH = RDT_Create(pRDTClient->SID, RDT_CREATE_TIMEOUT, 0);
	if(pRDTClient->rdtCH < 0)
	{
		printf("SID[%d] RDT_Create failed[%d]\n", pRDTClient->SID, pRDTClient->rdtCH);
        goto THREAD_IOTCCLOSE;
	}

	printf("[%s:%d] SID[%d] RDT_ID[%d] Start\n", __FUNCTION__, __LINE__, pRDTClient->SID, pRDTClient->rdtCH);

    while(pRDTClient->inProgress){
        ret = RDTPacketRead(pRDTClient->rdtCH, &clientCMD);
        printf("[%s:%d] client command [%s]\n", __FUNCTION__, __LINE__, clientCMD);
        if(ret > 0){
            //Handle command
            if(strncmp(clientCMD, "Get FileInfo", strlen("Get FileInfo")) == 0){
                RDTPacketRelease(&clientCMD);

                sprintf(resp, "FileInfo FileName %s FileSize %d", gFn, fileSize);
                if(RDTPacketWrite(pRDTClient->rdtCH, resp, strlen(resp)+1) < 0){
                    printf("RDTPacketWrite error ret[%d]\n", ret);
                    goto THREAD_FCLOSE;
                }
            }
            else if(strncmp(clientCMD, "Start", strlen("Start")) == 0){
                RDTPacketRelease(&clientCMD);
                //Send Data
                fseek(fp, 0, SEEK_SET);
                remainFileSize = fileSize;
                while(remainFileSize > 0 && pRDTClient->inProgress){
                    readSize = fread((void*)buf, 1, MAX_PACKET_SIZE, fp);
                    if(readSize == 0 && feof(fp)){
                        break;
                    }

                    if(RDTPacketWrite(pRDTClient->rdtCH, buf, readSize) < 0){
                        printf("RDTPacketWrite error ret[%d]\n", ret);
                        goto THREAD_FCLOSE;
                    }
                    remainFileSize -= readSize;
                    printf("\rrdtCH[%d] Remain [%10d / %10d]", pRDTClient->rdtCH, remainFileSize, fileSize);
                }
                printf("\n");
            }
            else if(strncmp(clientCMD, "Exit", strlen("Exit")) == 0){
                RDTPacketRelease(&clientCMD);
                break;
            }
        }
        else if(ret == PACKET_HELPER_TIMEOUT){
            //Wait next command
        }
        else{
            break;
        }
    }

THREAD_FCLOSE:
    fclose(fp);
	RDT_Destroy(pRDTClient->rdtCH);
THREAD_IOTCCLOSE:
	IOTC_Session_Close(pRDTClient->SID);
    pRDTClient->inUse = 0;
    pRDTClient->inProgress = 0;

    printf("[%s:%d] SID[%d] RDT_ID[%d] Exit\n", __FUNCTION__, __LINE__, pRDTClient->SID, pRDTClient->rdtCH);

	pthread_exit(0);
}

void *thread_Listen(void *arg)
{
    int SID = -1, index = 0;
    char *mode[3] = {"P2P", "RLY", "LAN"};
    struct st_SInfo Sinfo;

    printf("Start IOTC_Listen...\n");

    memset(gRDTClient, 0, sizeof(RDTClient)*MAX_CLIENT_NUM);

    while(1)
    {
        SID = IOTC_Listen(0);

		if(SID >= 0)
		{
			IOTC_Session_Check(SID, &Sinfo);
			printf("Client from [%s:%d] Mode[%s]\n",Sinfo.RemoteIP, Sinfo.RemotePort, mode[(int)Sinfo.Mode]);

			gRDTClient[SID].SID = SID;
            gRDTClient[SID].inUse = 1;
            gRDTClient[SID].inProgress = 1;

            //Create RDTCreate Thread
			pthread_create(&gRDTClient[SID].threadID, NULL, &thread_RDTCreate, (void *)&gRDTClient[SID]);
		}
        else if(SID == IOTC_ER_EXCEED_MAX_SESSION)
        {
            sleep(1);
            continue;
        }
        else if(SID == IOTC_ER_TIMEOUT)
        {
            sleep(1);
            continue;
        }
        else
        {
            printf("IOTC_Listen error ret[%d]\n", SID);
            break;
        }
	}

    // Wait All RDT Thread Stop
    for(index = 0 ; index < MAX_CLIENT_NUM ; index++){
        if(gRDTClient[index].inUse){
            gRDTClient[index].inProgress = 0;
            IOTC_Session_Close(gRDTClient[index].SID);
            pthread_join(gRDTClient[index].threadID, NULL);
        }
    }

    printf("[%s:%d] thread_Listen Exit\n", __FUNCTION__, __LINE__);

	pthread_exit(0);
}

static void LoginHandler(IOTCDeviceLoginState state, int errCode, void* pUserData)
{
    if(IOTC_DEVLOGIN_ST_LOGINED == state){
        printf("LoginHandler state[%d] error[%d] Login Success\n", state, errCode);
    }
    else if(IOTC_DEVLOGIN_ST_LOGINING == state){
        printf("LoginHandler state[%d] error[%d] Logining\n", state, errCode);
    }
    else if(IOTC_DEVLOGIN_ST_READY_FOR_LAN == state){
        printf("LoginHandler state[%d]. The device is able to be reached in LAN mode\n", state);
    }
    else if(IOTC_DEVLOGIN_ST_LOGIN_FAILED == state){
        printf("LoginHandler state[%d]. Login Failed, need retry login\n", state);
        gLoginRetry = 1;
    }
    else{
        printf("LoginHandler state[%d] error[%d] \n", state, errCode);
    }
}

int main(int argc, char *argv[])
{
    int ret = 0, rdtCh = 0, rdtVer = 0;
    unsigned int iotcVer = 0;
    char UID[24] = {0};
    pthread_t threadID_Listen;

    //Get Argument
    if(argc < 3)
    {
        printf("Arg wrong!!!\n");
        printf("  --RDTServer [UID] [Filename]\n");
        return 0;
    }

    strcpy(UID, argv[1]);
    strcpy(gFn, argv[2]);
    printf("Filename : [%s]\n", gFn);

    //Get IOTC,RDT Version
    IOTC_Get_Version(&iotcVer);
    rdtVer = RDT_GetRDTApiVer();
	printf("IOTC Ver[0x%x] RDT Ver[0x%X]\n", iotcVer, rdtVer);

    //IOTC Initialize
    IOTC_Set_Max_Session_Number(MAX_CLIENT_NUM);
    ret = IOTC_Initialize2(0);
    if(ret != IOTC_ER_NoERROR)
    {
        printf("IOTC_Initialize error!!\n");
        return 0;
    }

    //RDT Initialize
    RDT_Set_Max_Channel_Number(MAX_CLIENT_NUM);
    rdtCh = RDT_Initialize();
    if(rdtCh != MAX_CLIENT_NUM)
    {
        printf("RDT_Initialize error rdtCh[%d]\n", rdtCh);
        IOTC_DeInitialize();
        return 0;
    }

    //Start Login
    ret = IOTC_Device_LoginNB((const char *)UID, NULL, NULL, LoginHandler, NULL);
    if(ret != IOTC_ER_NoERROR)
    {
        printf("IOTC_Device_LoginNB error ret[%d]\n", ret);
        RDT_DeInitialize();
        IOTC_DeInitialize();
        return 0;
    }
    printf("Start login with UID [%s]\n\n", UID);

    //Create Listen Thread
    pthread_create(&threadID_Listen, NULL, &thread_Listen, (void *)UID);

	while(gProcess){
        if(gLoginRetry){
            ret = IOTC_Device_LoginNB((const char *)UID, NULL, NULL, LoginHandler, NULL);
            if(ret != IOTC_ER_NoERROR)
            {
                printf("IOTC_Device_LoginNB error ret[%d]\n", ret);
                //break;
            }
            gLoginRetry = 0;
        }
        sleep(1);
    }

    IOTC_Listen_Exit();
    pthread_join(threadID_Listen, NULL);

	RDT_DeInitialize();
	IOTC_DeInitialize();

	printf("Server exit!\n");

	return 0;
}

