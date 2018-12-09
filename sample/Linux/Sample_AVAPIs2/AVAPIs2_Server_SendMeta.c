#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <ctype.h>
#include "IOTCAPIs.h"
#include "AVAPIs.h"
#include "AVAPIs2.h"
#include "P2PCam/AVFRAMEINFO.h"
#include "P2PCam/AVIOCTRLDEFs.h"

/******************************
 * Defined
 ******************************/
#define SERVTYPE_STREAM_SERVER  16  // Server Type , Using at AVAPI2_ServerStart
#define MAX_CLIENT_NUMBER       128 // Maxinum client number, Using at AVAPI2_SetCanalLimit
#define MAX_CH_NUMBER           16  // Maxinum channel namber of a client, Using at AVAPI2_SetCanalLimit

#define META_BUF_SIZE           (1024 * 10)    // Meta buffer size , Using at SendMeta_SendMetaThread

/******************************
 * Structure
 ******************************/
typedef struct _AV_Client_Info
{
    int nAVCanal;
    int nSendMetaAVCanal;
    unsigned char bEnableMeta;
    int nFileLen;
    int nSendIndex;
    int nMaxIndex;
    int nSendDone;
    FILE *fpFile;
    pthread_rwlock_t sLock;
}AV_Client_Info;

typedef struct _MetaDataInfo {
    unsigned int nDataType;         //
    unsigned int nSliceIdx;         // slice no. sequence
    unsigned int nSliceCount;
    unsigned char nBeginSlice;      // means this is Begin frame
    unsigned char nEndSlice;        // means this is End frame
    unsigned char resreved[2];
} MetaDataInfo;

/******************************
 * Global Variable
 * set AV server ID and password here
 ******************************/
char gAVID[] = "admin";
char gAVPass[] = "888888ii";
static AV_Client_Info gAVClientInfo[MAX_CLIENT_NUMBER];

static char gUID[21];               //argv[1] of Usage: Usage: ./AVAPIs2_Server_SendMeta [UID] [MetaFile]
static char gMetaFilename[128];     //argv[2] of Usage: Usage: ./AVAPIs2_Server_SendMeta [UID] [MetaFile]
static int gProcessRun = 1;

/******************************
 * Error Handling Function
 ******************************/
void PrintErrHandling (int nErr)
{
    switch (nErr)
    {
        case IOTC_ER_SERVER_NOT_RESPONSE :
            //-1 IOTC_ER_SERVER_NOT_RESPONSE
            printf ("[Error code : %d]\n", IOTC_ER_SERVER_NOT_RESPONSE );
            printf ("Master doesn't respond.\n");
            printf ("Please check the network wheather it could connect to the Internet.\n");
            break;
        case IOTC_ER_FAIL_RESOLVE_HOSTNAME :
            //-2 IOTC_ER_FAIL_RESOLVE_HOSTNAME
            printf ("[Error code : %d]\n", IOTC_ER_FAIL_RESOLVE_HOSTNAME);
            printf ("Can't resolve hostname.\n");
            break;
        case IOTC_ER_ALREADY_INITIALIZED :
            //-3 IOTC_ER_ALREADY_INITIALIZED
            printf ("[Error code : %d]\n", IOTC_ER_ALREADY_INITIALIZED);
            printf ("Already initialized.\n");
            break;
        case IOTC_ER_FAIL_CREATE_MUTEX :
            //-4 IOTC_ER_FAIL_CREATE_MUTEX
            printf ("[Error code : %d]\n", IOTC_ER_FAIL_CREATE_MUTEX);
            printf ("Can't create mutex.\n");
            break;
        case IOTC_ER_FAIL_CREATE_THREAD :
            //-5 IOTC_ER_FAIL_CREATE_THREAD
            printf ("[Error code : %d]\n", IOTC_ER_FAIL_CREATE_THREAD);
            printf ("Can't create thread.\n");
            break;
        case IOTC_ER_UNLICENSE :
            //-10 IOTC_ER_UNLICENSE
            printf ("[Error code : %d]\n", IOTC_ER_UNLICENSE);
            printf ("This UID is unlicense.\n");
            printf ("Check your UID.\n");
            break;
        case IOTC_ER_NOT_INITIALIZED :
            //-12 IOTC_ER_NOT_INITIALIZED
            printf ("[Error code : %d]\n", IOTC_ER_NOT_INITIALIZED);
            printf ("Please initialize the IOTCAPI first.\n");
            break;
        case IOTC_ER_TIMEOUT :
            //-13 IOTC_ER_TIMEOUT
            break;
        case IOTC_ER_INVALID_SID :
            //-14 IOTC_ER_INVALID_SID
            printf ("[Error code : %d]\n", IOTC_ER_INVALID_SID);
            printf ("This SID is invalid.\n");
            printf ("Please check it again.\n");
            break;
        case IOTC_ER_EXCEED_MAX_SESSION :
            //-18 IOTC_ER_EXCEED_MAX_SESSION
            printf ("[Error code : %d]\n", IOTC_ER_EXCEED_MAX_SESSION);
            printf ("[Warning]\n");
            printf ("The amount of session reach to the maximum.\n");
            printf ("It cannot be connected unless the session is released.\n");
            break;
        case IOTC_ER_CAN_NOT_FIND_DEVICE :
            //-19 IOTC_ER_CAN_NOT_FIND_DEVICE
            printf ("[Error code : %d]\n", IOTC_ER_CAN_NOT_FIND_DEVICE);
            printf ("Device didn't register on server, so we can't find device.\n");
            printf ("Please check the device again.\n");
            printf ("Retry...\n");
            break;
        case IOTC_ER_SESSION_CLOSE_BY_REMOTE :
            //-22 IOTC_ER_SESSION_CLOSE_BY_REMOTE
            printf ("[Error code : %d]\n", IOTC_ER_SESSION_CLOSE_BY_REMOTE);
            printf ("Session is closed by remote so we can't access.\n");
            printf ("Please close it or establish session again.\n");
            break;
        case IOTC_ER_REMOTE_TIMEOUT_DISCONNECT :
            //-23 IOTC_ER_REMOTE_TIMEOUT_DISCONNECT
            printf ("[Error code : %d]\n", IOTC_ER_REMOTE_TIMEOUT_DISCONNECT);
            printf ("We can't receive an acknowledgement character within a TIMEOUT.\n");
            printf ("It might that the session is disconnected by remote.\n");
            printf ("Please check the network wheather it is busy or not.\n");
            printf ("And check the device and user equipment work well.\n");
            break;
        case IOTC_ER_DEVICE_NOT_LISTENING :
            //-24 IOTC_ER_DEVICE_NOT_LISTENING
            printf ("[Error code : %d]\n", IOTC_ER_DEVICE_NOT_LISTENING);
            printf ("Device doesn't listen or the sessions of device reach to maximum.\n");
            printf ("Please release the session and check the device wheather it listen or not.\n");
            break;
        case IOTC_ER_CH_NOT_ON :
            //-26 IOTC_ER_CH_NOT_ON
            printf ("[Error code : %d]\n", IOTC_ER_CH_NOT_ON);
            printf ("Channel isn't on.\n");
            printf ("Please open it by IOTC_Session_Channel_ON() or IOTC_Session_Get_Free_Channel()\n");
            printf ("Retry...\n");
            break;
        case IOTC_ER_SESSION_NO_FREE_CHANNEL :
            //-31 IOTC_ER_SESSION_NO_FREE_CHANNEL
            printf ("[Error code : %d]\n", IOTC_ER_SESSION_NO_FREE_CHANNEL);
            printf ("All channels are occupied.\n");
            printf ("Please release some channel.\n");
            break;
        case IOTC_ER_TCP_TRAVEL_FAILED :
            //-32 IOTC_ER_TCP_TRAVEL_FAILED
            printf ("[Error code : %d]\n", IOTC_ER_TCP_TRAVEL_FAILED);
            printf ("Device can't connect to Master.\n");
            printf ("Don't let device use proxy.\n");
            printf ("Close firewall of device.\n");
            printf ("Or open device's TCP port 80, 443, 8080, 8000, 21047.\n");
            break;
        case IOTC_ER_TCP_CONNECT_TO_SERVER_FAILED :
            //-33 IOTC_ER_TCP_CONNECT_TO_SERVER_FAILED
            printf ("[Error code : %d]\n", IOTC_ER_TCP_CONNECT_TO_SERVER_FAILED);
            printf ("Device can't connect to server by TCP.\n");
            printf ("Don't let server use proxy.\n");
            printf ("Close firewall of server.\n");
            printf ("Or open server's TCP port 80, 443, 8080, 8000, 21047.\n");
            printf ("Retry...\n");
            break;
        case IOTC_ER_NO_PERMISSION :
            //-40 IOTC_ER_NO_PERMISSION
            printf ("[Error code : %d]\n", IOTC_ER_NO_PERMISSION);
            printf ("This UID's license doesn't support TCP.\n");
            break;
        case IOTC_ER_NETWORK_UNREACHABLE :
            //-41 IOTC_ER_NETWORK_UNREACHABLE
            printf ("[Error code : %d]\n", IOTC_ER_NETWORK_UNREACHABLE);
            printf ("Network is unreachable.\n");
            printf ("Please check your network.\n");
            printf ("Retry...\n");
            break;
        case IOTC_ER_FAIL_SETUP_RELAY :
            //-42 IOTC_ER_FAIL_SETUP_RELAY
            printf ("[Error code : %d]\n", IOTC_ER_FAIL_SETUP_RELAY);
            printf ("Client can't connect to a device via Lan, P2P, and Relay mode\n");
            break;
        case IOTC_ER_NOT_SUPPORT_RELAY :
            //-43 IOTC_ER_NOT_SUPPORT_RELAY
            printf ("[Error code : %d]\n", IOTC_ER_NOT_SUPPORT_RELAY);
            printf ("Server doesn't support UDP relay mode.\n");
            printf ("So client can't use UDP relay to connect to a device.\n");
            break;

        default :
            printf ("[Error code : %d]\n", nErr);
            printf ("Please reference to AVAPIs.h and IOTCAPIs.h\n");
            break;
    }
}

/******************************
 * AV_Client_Info Control Functions
 ******************************/
void AVClientInfo_Initialize()
{
    int i = 0;

    for(i = 0 ; i < MAX_CLIENT_NUMBER ; i++){
        memset(&gAVClientInfo[i], 0, sizeof(AV_Client_Info));
        gAVClientInfo[i].nAVCanal = -1;
        gAVClientInfo[i].nSendMetaAVCanal = -1;
        pthread_rwlock_init(&(gAVClientInfo[i].sLock), NULL);
    }
}

void AVClientInfo_DeInitialize()
{
    int i = 0;

    for(i = 0 ; i < MAX_CLIENT_NUMBER ; i++){
        memset(&gAVClientInfo[i], 0, sizeof(AV_Client_Info));
        gAVClientInfo[i].nAVCanal = -1;
        gAVClientInfo[i].nSendMetaAVCanal = -1;
        pthread_rwlock_destroy(&gAVClientInfo[i].sLock);
    }
}

void AVClientInfo_RegeditMeta(int nSID, int nAVCanal)
{
    AV_Client_Info *p = NULL;

    if(nSID < 0 || nSID >= MAX_CLIENT_NUMBER)
        return;

    p = &gAVClientInfo[nSID];

    //get writer lock
    int lock_ret = pthread_rwlock_wrlock(&gAVClientInfo[nSID].sLock);
	if(lock_ret){
		printf("Acquire nSID[%d] rwlock error ret[%d]\n", nSID, lock_ret);
        return;
    }

    p->nSendMetaAVCanal = nAVCanal;
    p->bEnableMeta = 1;
    p->nSendDone = 0;

	//release lock
	pthread_rwlock_unlock(&gAVClientInfo[nSID].sLock);
}

void AVClientInfo_UnRegeditMeta(int nSID)
{
    AV_Client_Info *p = NULL;

    if(nSID < 0 || nSID >= MAX_CLIENT_NUMBER)
        return;

    p = &gAVClientInfo[nSID];

    //get writer lock
    int lock_ret = pthread_rwlock_wrlock(&gAVClientInfo[nSID].sLock);
	if(lock_ret){
		printf("Acquire nSID[%d] rwlock error ret[%d]\n", nSID, lock_ret);
        return;
    }

    p->nSendMetaAVCanal = -1;
    p->bEnableMeta = 0;
    p->nSendDone = 1;

	//release lock
	pthread_rwlock_unlock(&gAVClientInfo[nSID].sLock);
}

int AVClientInfo_GetIndexBySendMetaCanal(int sendMetaAVCanal)
{
    int i = 0;

    for(i = 0 ; i < MAX_CLIENT_NUMBER ; i++){
        if(sendMetaAVCanal == gAVClientInfo[i].nSendMetaAVCanal)
            return i;
    }

    return -1;
}

int AVClientInfo_GetSendMetaCanal(int nSID)
{
    AV_Client_Info *p = NULL;
    int nSendMetaCanal = -1;

    if(nSID < 0 || nSID >= MAX_CLIENT_NUMBER)
        return -1;

    p = &gAVClientInfo[nSID];

    //get writer lock
    int lock_ret = pthread_rwlock_rdlock(&gAVClientInfo[nSID].sLock);
	if(lock_ret){
		printf("Acquire nSID[%d] rdlock error ret[%d]\n", nSID, lock_ret);
        return -1;
    }

    if(p->bEnableMeta)
        nSendMetaCanal = p->nSendMetaAVCanal;

    //release lock
	pthread_rwlock_unlock(&gAVClientInfo[nSID].sLock);

    return nSendMetaCanal;
}

/******************************
 * Send Meta Functions
 * SendMeta_SendMetaThread - Send Meta data to all client
 ******************************/
int SendMeta_Stop(int nSID)
{
    int nSendMetaAVCanal = 0;

    if(nSID < 0 || nSID >= MAX_CLIENT_NUMBER)
        return -1;

    nSendMetaAVCanal = AVClientInfo_GetSendMetaCanal(nSID);
    if(nSendMetaAVCanal >= 0){
        AVClientInfo_UnRegeditMeta(nSID);
        printf("SendMeta_Stop : AVAPI2_ReleaseChannelForSend nSendMetaAVCanal[%d]\n", nSendMetaAVCanal);
        AVAPI2_ReleaseChannelForSend(nSendMetaAVCanal);
    }

    return 0;
}

void *SendMeta_SendMetaThread(void *arg)
{
    int i = 0, nRet = 0, size = 0;
    char buf[META_BUF_SIZE];
    MetaDataInfo metaDataInfo;

    printf("SendMeta_SendMetaThread start\n");

    while(gProcessRun)
    {
        for(i = 0 ; i < MAX_CLIENT_NUMBER; i++)
        {
            //get reader lock
            int lock_ret = pthread_rwlock_rdlock(&gAVClientInfo[i].sLock);
            if(lock_ret)
                printf("Acquire SID[%d] rdlock error ret[%d]\n", i, lock_ret);

            if(gAVClientInfo[i].nSendMetaAVCanal < 0 || gAVClientInfo[i].bEnableMeta == 0 || gAVClientInfo[i].nSendDone == 1){
                //release reader lock
                lock_ret = pthread_rwlock_unlock(&gAVClientInfo[i].sLock);
                if(lock_ret)
                    printf("Release SID[%d] rwlock error ret[%d]\n", i, lock_ret);
                continue;
            }

            //Open Meta File
            if(gAVClientInfo[i].fpFile == NULL){
                gAVClientInfo[i].fpFile = fopen(gMetaFilename, "rb");
                if(gAVClientInfo[i].fpFile == NULL){
                    printf("SendMeta_SendMetaThread : [%d] Meta File \'%s\' open error!!\n", i, gMetaFilename);
                    //release reader lock
                    lock_ret = pthread_rwlock_unlock(&gAVClientInfo[i].sLock);
                    if(lock_ret)
                        printf("Release SID[%d] rwlock error lock_ret[%d]\n", i, lock_ret);
                    continue;
                }

                fseek(gAVClientInfo[i].fpFile, 0, SEEK_END);
                gAVClientInfo[i].nFileLen = ftell(gAVClientInfo[i].fpFile);
                fseek(gAVClientInfo[i].fpFile, 0, SEEK_SET);
                gAVClientInfo[i].nMaxIndex = gAVClientInfo[i].nFileLen/META_BUF_SIZE + (gAVClientInfo[i].nFileLen%META_BUF_SIZE > 0 ? 1 : 0);
                gAVClientInfo[i].nSendIndex = 0;
                printf("SendMeta_SendMetaThread : [%d] Send Meta File \'%s\' nFileLen[%d] nSendIndex[%d] nMaxIndex[%d]\n", i, gMetaFilename, gAVClientInfo[i].nFileLen, gAVClientInfo[i].nSendIndex, gAVClientInfo[i].nMaxIndex);
            }

            //Read Data from Meta File
            if(gAVClientInfo[i].fpFile != NULL){
                fseek(gAVClientInfo[i].fpFile, gAVClientInfo[i].nSendIndex*META_BUF_SIZE, SEEK_SET);
                size = fread(buf, 1, META_BUF_SIZE, gAVClientInfo[i].fpFile);
                if(size <= 0){
                    if(feof(gAVClientInfo[i].fpFile)){
                        printf("SendMeta_SendMetaThread : [%d] Send Meta File \'%s\' done\n", i, gMetaFilename);
                    }
                    else{
                        printf("SendMeta_SendMetaThread : [%d] Meta File \'%s\' read error!!\n", i, gMetaFilename);
                    }

                    fclose(gAVClientInfo[i].fpFile);
                    gAVClientInfo[i].fpFile = NULL;
                    gAVClientInfo[i].nFileLen = 0;
                    gAVClientInfo[i].nMaxIndex = 0;
                    gAVClientInfo[i].nSendIndex = 0;

                    gAVClientInfo[i].nSendDone = 1;

                    //release reader lock
                    lock_ret = pthread_rwlock_unlock(&gAVClientInfo[i].sLock);
                    if(lock_ret)
                        printf("Release SID[%d] rwlock error lock_ret[%d]\n", i, lock_ret);
                    continue;
                }
            }

            // Send Meta Frame to av-idx and know how many time it takes
            memset(&metaDataInfo, 0, sizeof(MetaDataInfo));
            metaDataInfo.nSliceCount = gAVClientInfo[i].nMaxIndex;
            metaDataInfo.nSliceIdx = gAVClientInfo[i].nSendIndex;
            if(metaDataInfo.nSliceIdx == 0){
                metaDataInfo.nBeginSlice = 1;
                if(metaDataInfo.nSliceCount == 1){
                    metaDataInfo.nEndSlice = 1;
                }
                else{
                    metaDataInfo.nEndSlice = 0;
                }
            }
            else if(metaDataInfo.nSliceIdx == metaDataInfo.nSliceCount-1){
                metaDataInfo.nBeginSlice = 0;
                metaDataInfo.nEndSlice = 1;
            }
            else{
                metaDataInfo.nBeginSlice = 0;
                metaDataInfo.nEndSlice = 0;
            }

            nRet = AVAPI2_SendMetaData(gAVClientInfo[i].nSendMetaAVCanal, buf, size, (const void *)&metaDataInfo, sizeof(MetaDataInfo));

            //release reader lock
            lock_ret = pthread_rwlock_unlock(&gAVClientInfo[i].sLock);
            if(lock_ret)
                printf("Release SID[%d] rwlock error lock_ret[%d]\n", i, lock_ret);

            if(nRet == AV_ER_EXCEED_MAX_SIZE ||
                nRet == AV_ER_SOCKET_QUEUE_FULL ||
                nRet == AV_ER_MEM_INSUFF ||
                nRet == AV_ER_WAIT_KEY_FRAME ||
                nRet == AV_ER_CLEANBUF_ALREADY_CALLED ||
                nRet == AV_ER_CLIENT_NO_AVLOGIN){
                //TODO : Handle Video Buffer Full
                continue;
            }
            else if(nRet < 0){
                printf("SendMeta_SendMetaThread : SID[%d] nSendMetaAVCanal[%d] AVAPI2_SendFrameData nRet[%d] UnRegedit\n", i, gAVClientInfo[i].nSendMetaAVCanal, nRet);
                PrintErrHandling(nRet);
                AVClientInfo_UnRegeditMeta(i);
                continue;
            }

            gAVClientInfo[i].nSendIndex++;
        }

        usleep(30);
    }

    pthread_exit(0);
}

void SendMeta_CreateThread()
{
    int nRet;
    pthread_t ThreadMetaData_ID;

    if((nRet = pthread_create(&ThreadMetaData_ID, NULL, &SendMeta_SendMetaThread, NULL))){
        printf("pthread_create SendMeta_SendMetaThread nRet[%d]\n", nRet);
        exit(-1);
    }
    pthread_detach(ThreadMetaData_ID);
}

int SendMeta_CanalStatusCB(int nAVCanal, int nError, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    int nSID = 0;
    printf("SendMeta_CanalStatusCB : nAVCanal[%d] nError[%d] nChannelID[%u]\n", nAVCanal, nError, nChannelID);
    PrintErrHandling(nError);

    if(nError == AV_ER_SESSION_CLOSE_BY_REMOTE ||
        nError == AV_ER_REMOTE_TIMEOUT_DISCONNECT ||
        nError == AV_ER_IOTC_SESSION_CLOSED ||
        nError == AV_ER_SERVER_EXIT ||
        nError == AV_ER_INVALID_SID){
        nSID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
        SendMeta_Stop(nSID);
    }
    else if(nError == AV_ER_CLIENT_NO_AVLOGIN){
        //Client login authentication error
        //Do AVAPI2_ServerExitCanal
        AVAPI2_ServerExitCanal(AVAPI2_GetSessionIDByAVCanal(nAVCanal), AVAPI2_GetChannelByAVCanal(nAVCanal));
    }
    else if(nError == AV_ER_MEM_INSUFF){
        printf("SendMeta_CanalStatusCB : nAVCanal[%d] AV_ER_MEM_INSUFF\n", nAVCanal);
    }
    else if(nError == IOTC_ER_CH_NOT_ON){
        printf("SendMeta_CanalStatusCB : nAVCanal[%d] IOTC_ER_CH_NOT_ON nChannelID[%d]\n", nAVCanal, nChannelID);
    }

    return 0;
}

int SendMeta_ServerStatusCB(int nStatus, int nError, int nAVCanal, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    printf("SendMeta_ServerStatusCB : nStatus[%d] nError[%d] nAVCanal[%d] nChannelID[%u]\n", nStatus, nError, nAVCanal, nChannelID);

    switch(nStatus){
		case AVAPI2_SERVER_STATUS_START_CANAL_FAILED :
		{
			printf("SendMeta_ServerStatusCB AVAPI2_SERVER_STATUS_START_CANAL_FAILED\n");
			AVAPI2_ReleaseChannelForSend(nAVCanal);
		}
		break;

		case AVAPI2_SERVER_STATUS_CLIENT_LOGINED :
		{
			printf("SendMeta_ServerStatusCB : Client Logined ok\n");
		}
        break;

		default:
		break;
	}

    return 0;
}

int SendMeta_Start(int nSID, int nChannel)
{
    int nSendMetaAVCanal = 0;
    int nTimeout = 10;

    if(nSID < 0 || nChannel < 0)
        return -1;

    nSendMetaAVCanal = AVAPI2_CreateChannelForSend(nSID, nTimeout, SERVTYPE_STREAM_SERVER, nChannel, 1, SendMeta_ServerStatusCB, SendMeta_CanalStatusCB);
    if(nSendMetaAVCanal >= 0){
        avServSetResendSize(nSendMetaAVCanal, 128);
        AVAPI2_ServerSetCongestionCtrlMode(nSendMetaAVCanal, AVAPI2_CONGESTION_CTRL_META);
    }
    else{
        printf("SendMeta_Start : nSID[%d] nChannel[%d] AVAPI2_CreateChannelForSend nRet[%d]\n", nSID, nChannel, nSendMetaAVCanal);
        return -1;
    }
    AVClientInfo_RegeditMeta(nSID, nSendMetaAVCanal);

    return 0;
}

/******************************
 * AVAPI2 Server Call Back Functions
 ******************************/
int Server_AuthCB(char *viewAcc,char *viewPwd)
{
    //Check login account & password, return true if success.
    if(strcmp(viewAcc, gAVID) == 0 && strcmp(viewPwd, gAVPass) == 0)
        return 1;

    printf("Server_AuthCB : viewAcc[%s] viewPwd[%s] incorrect\n", viewAcc, viewPwd);

    return 0;
}

int Server_ServerStatusCB(int nStatus, int nError, int nAVCanal, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    int nSID = 0;

    //Handle Server Status Change
    printf("ServerStatusCB : nStatus[%d] nError[%d] nAVCanal[%d] nChannelID[%u]\n", nStatus, nError, nAVCanal, nChannelID);
    switch(nStatus){
        case AVAPI2_SERVER_STATUS_START :
            printf("ServerStatusCB : Server Start Listen\n");
            break;
        case AVAPI2_SERVER_STATUS_LOGINED :
            printf("ServerStatusCB : Server Login to P2P Server ok\n");
            break;
        case AVAPI2_SERVER_STATUS_NEW_CANAL :
            printf("ServerStatusCB : New Canal Connection\n");
            avServSetResendSize(nAVCanal, 128);
            break;
        case AVAPI2_SERVER_STATUS_WAIT_CLIENT :
            printf("ServerStatusCB : Wait Client Login\n");
            break;
        case AVAPI2_SERVER_STATUS_CLIENT_LOGINED :
            printf("ServerStatusCB : Client Logined ok\n");
            break;
        case AVAPI2_SERVER_STATUS_CLEAN_BUFFER_SUCCESS :
            printf("ServerStatusCB : Server Clean Buffer Success\n");
            break;
        case AVAPI2_SERVER_STATUS_CLEAN_VIDEOBUF_REQUEST :
            printf("ServerStatusCB : Client Request Clean Server Video Buffeer Success\n");
            break;
        case AVAPI2_SERVER_STATUS_CLEAN_AUDIOBUF_REQUEST :
            printf("ServerStatusCB : Client Request Clean Server Audio Buffeer Success\n");
            break;

        //Error Handling
        case AVAPI2_SERVER_STATUS_LOGIN_FAILED :
            printf("ServerStatusCB : login to P2PServer failed\n");
            PrintErrHandling(nError);
            //return 0 to keep retry login, return -1 to give up
            return 0;
        case AVAPI2_SERVER_STATUS_START_CANAL_FAILED :
            printf("ServerStatusCB : start canal failed\n");
            PrintErrHandling(nError);
            break;
        case AVAPI2_SERVER_STATUS_CREATE_SENDTASK_FAILED :
            printf("ServerStatusCB : create send task failed\n");
            AVAPI2_ServerStopCanal(nAVCanal);
            break;
        case AVAPI2_SERVER_STATUS_CLEAN_BUFFER_TIMEOUT:
            printf("ServerStatusCB : Clean Buffer Timeout\n");
            //Stop SendMeta and UnRegedit
            nSID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
            SendMeta_Stop(nSID);
            AVClientInfo_UnRegeditMeta(nSID);

            //Stop Canal
            AVAPI2_ServerStopCanal(nAVCanal);
            break;
        default:
            break;
    }

    return 0;
}

int Server_CanalStatusCB(int nAVCanal, int nError, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    //Canal Error Handling Function
    int nSID = 0;
    printf("Server_CanalStatusCB : nAVCanal[%d] nError[%d] nChannelID[%u]\n", nAVCanal, nError, nChannelID);
    PrintErrHandling(nError);

    if(nError == AV_ER_SESSION_CLOSE_BY_REMOTE ||
        nError == AV_ER_REMOTE_TIMEOUT_DISCONNECT ||
        nError == AV_ER_IOTC_SESSION_CLOSED ||
        nError == AV_ER_SERVER_EXIT ||
        nError == AV_ER_INVALID_SID){
        //Stop SendMeta and UnRegedit
        nSID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
        SendMeta_Stop(nSID);

        //Stop Canal
        AVAPI2_ServerStopCanal(nAVCanal);
    }
    else if(nError == AV_ER_CLIENT_NO_AVLOGIN){
        //Client login authentication error
        //Do AVAPI2_ServerExitCanal
        AVAPI2_ServerExitCanal(AVAPI2_GetSessionIDByAVCanal(nAVCanal), AVAPI2_GetChannelByAVCanal(nAVCanal));
    }
    else if(nError == AV_ER_MEM_INSUFF){
        printf("Server_CanalStatusCB : AV_ER_MEM_INSUFF\n");
    }
    else if(nError == IOTC_ER_CH_NOT_ON){
        printf("Server_CanalStatusCB : IOTC_ER_CH_NOT_ON nChannelID[%d]\n", nChannelID);
    }

    return 0;
}

//Handle IOCtrl
//Send Meta : IOTYPE_USER_IPCAM_SEND_META_START, IOTYPE_USER_IPCAM_SEND_META_STOP
void Server_Handle_IOCTRL_Cmd(int nSID, int nAVCanal, unsigned char *buf, unsigned int type)
{
	switch(type)
	{
		case IOTYPE_USER_IPCAM_SEND_META_START:
		{
			SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)buf;
			printf("Server_Handle_IOCTRL_Cmd : nSID[%d] nAVCanal[%d] IOTYPE_USER_IPCAM_SEND_META_START CH[%d]\n", nSID, nAVCanal, p->channel);
            SendMeta_Start(nSID, p->channel);
		}
		break;
		case IOTYPE_USER_IPCAM_SEND_META_STOP:
		{
			SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)buf;
			printf("Server_Handle_IOCTRL_Cmd : nSID[%d] nAVCanal[%d] IOTYPE_USER_IPCAM_SEND_META_STOP CH[%d]\n", nSID, nAVCanal, p->channel);
            SendMeta_Stop(nSID);
		}
		break;
		default:
        {
            printf("Server_Handle_IOCTRL_Cmd : nAVCanal [%d] non-handle type[%X]\n", nAVCanal, type);
        }
		break;
	}
}

int Server_IOCtrlRecvCB(int nAVCanal, unsigned int nIoCtrlType, unsigned char *pIoCtrlBuf, unsigned int nIoCtrlBufLen, void* pUserData)
{
    printf("Server_IOCtrlRecvCB : nAVCanal[%d] nIoCtrlType[0x%x] nIoCtrlBufLen[%u]\n", nAVCanal, nIoCtrlType, nIoCtrlBufLen);

    Server_Handle_IOCTRL_Cmd(AVAPI2_GetSessionIDByAVCanal(nAVCanal), nAVCanal, pIoCtrlBuf, nIoCtrlType);

    return 0;
}

int main(int argc, char *argv[])
{
    int nRet = 0;
    int nServerInitChannel = 0;
    int nUDPPort = 0;
    char szVersion[64] = {0};

    if(argc < 3 || argc > 4){
        printf("Argument Error!!!\n");
        printf("Usage: ./AVAPIs2_Server_SendMeta [UID] [MetaFile]\n");
        return -1;
    }

    strcpy(gUID, argv[1]);
    strcpy(gMetaFilename, argv[2]);

    //Initial AV Client Info Structure
    AVClientInfo_Initialize();

    //Create Thread to read video/audio stream from files
    SendMeta_CreateThread();

    //AVAPI2 Initial : Random UDP port
    nRet = AVAPI2_ServerInitial(MAX_CLIENT_NUMBER, MAX_CH_NUMBER, nUDPPort);
    if(nRet < 0){
        printf("AVAPI2_SetCanalLimit nRet[%d]\n", nRet);
        PrintErrHandling(nRet);
        return -1;
    }

    //Get Version
    AVAPI2_GetVersion(szVersion, 64);
    printf("%s\n", szVersion);

    //AVAPI2 Server Start : Handle Login To P2PServer, Listen, Accept, Authentication
    nRet = AVAPI2_ServerStart((char*)gUID, nServerInitChannel, SERVTYPE_STREAM_SERVER, Server_AuthCB, Server_ServerStatusCB, Server_CanalStatusCB, Server_IOCtrlRecvCB);
    if(nRet < 0){
        printf("AVAPI2_ServerStart nRet[%d]\n", nRet);
        PrintErrHandling(nRet);
        return -1;
    }

    while(1){
        //Do anything you want
        sleep(1);
    }

    //AVAPI2 Server Stop
    AVAPI2_ServerStop();

    return 0;
}

