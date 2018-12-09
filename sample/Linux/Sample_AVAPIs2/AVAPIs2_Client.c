#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <signal.h>
#include "IOTCAPIs.h"
#include "AVAPIs.h"
#include "AVAPIs2.h"
#include "P2PCam/AVFRAMEINFO.h"
#include "P2PCam/AVIOCTRLDEFs.h"
#include "AVAPIs2_timeSync.h"

/******************************
 * Defined
 ******************************/
#define ENABLE_AUDIO_STREAM         1   //Enable Receive Audio Stream
#define ENABLE_SPEAKER_STREAM       0   //Enable Send Speaker Stream (Cycle test : Send 10 secs and then stop 10 secs)
#define ENABLE_TEST_CLEAN_BUFFER    0   //Test Clean Buffer function (Clean video audio buffer every 10 secs)
#define MAX_SERVER_NUMBER           4   //Maximum Server Number
//#define ENABLE_TIME_SYNC            1   //Enable Client Time Sync Function
#define AUDIO_BUF_SIZE 640

/******************************
 * Structure
 ******************************/

typedef enum
{
    STATUS_SPEAKER_THREAD_START		= 1<<0,
    STATUS_CLIENT_LOGIN_SUCCESS		= 1<<1,
    STATUS_READY_FOR_SEND			= STATUS_SPEAKER_THREAD_START | STATUS_CLIENT_LOGIN_SUCCESS,
} SpeakerStatus;

typedef struct _AV_Server_Info
{
    int nAVCanal;
    int nSpeakerAVCanal;
    int nSpeakerChannel;
    int bSpeakerRun;
    char szUID[32];
    char szPassword[32];

    int v_fpsCnt, v_bps, v_err, v_TotalCnt;
    int a_fpsCnt, a_bps, a_err, a_TotalCnt;
    struct timeval v_tv1, v_tv2;
    struct timeval a_tv1, a_tv2;
    struct timeval g_tv1, g_tv2;
    int firstVideo, firstAudio;
    unsigned int audiotime, videotime;

#if ENABLE_TIME_SYNC
    TIMESYNC_Info timeSyncInfo;
#endif
}AV_Server_Info;

/******************************
 * Global Variable
 ******************************/
char *gAVID = "admin";
int gAVServerNum = 0;
AV_Server_Info gAVServerInfo[MAX_SERVER_NUMBER];

/******************************
 * Print Error Code Function
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
		break;
	}
}

/******************************
 * AV_Server_Info Control Functions
 ******************************/
void AVServerInfo_Initialize()
{
	int i = 0;

	for(i = 0 ; i < MAX_SERVER_NUMBER ; i++){
		memset(&gAVServerInfo[i], 0, sizeof(AV_Server_Info));
		gAVServerInfo[i].nAVCanal = -1;
		gAVServerInfo[i].nSpeakerAVCanal = -1;
	}
}

void AVServerInfo_Clean(int index)
{
    if(index < 0 || index >= MAX_SERVER_NUMBER)
        return;

    memset(&gAVServerInfo[index], 0, sizeof(AV_Server_Info));
    gAVServerInfo[index].nAVCanal = -1;
    gAVServerInfo[index].nSpeakerAVCanal = -1;
}

int AVServerInfo_GetIndexByUID(char* szUID)
{
    int i = 0;
    
    for(i = 0 ; i < MAX_SERVER_NUMBER ; i++){
        if(strcmp(szUID, gAVServerInfo[i].szUID) == 0)
            return i;
    }

    return -1;
}

int AVServerInfo_GetIndexByCanal(int nAVCanal)
{
    int i = 0;

    for(i = 0 ; i < MAX_SERVER_NUMBER ; i++){
        if(nAVCanal == gAVServerInfo[i].nAVCanal)
            return i;
    }

    return -1;
}

int AVServerInfo_GetIndexBySpeakerCanal(int speakerAVCanal)
{
    int i = 0;

    for(i = 0 ; i < MAX_SERVER_NUMBER ; i++){
        if(speakerAVCanal == gAVServerInfo[i].nSpeakerAVCanal)
            return i;
    }

    return -1;
}

/******************************
 * Speaker Functions
 ******************************/
#if ENABLE_SPEAKER_STREAM
int Speaker_CanalStatusCB(int nAVCanal, int nError, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    int index = 0;
    printf("Speaker_CanalStatusCB : nAVCanal[%d] nError[%d] nChannelID[%u]\n", nAVCanal, nError, nChannelID);
    PrintErrHandling(nError);

    if(nError == AV_ER_SESSION_CLOSE_BY_REMOTE ||
        nError == AV_ER_REMOTE_TIMEOUT_DISCONNECT ||
        nError == AV_ER_IOTC_SESSION_CLOSED ||
        nError == AV_ER_SERVER_EXIT ||
        nError == AV_ER_INVALID_SID){
        index = AVServerInfo_GetIndexBySpeakerCanal(nAVCanal);
        if(index < 0){
            printf("Speaker_CanalStatusCB : nAVCanal[%d] AVServerInfo_GetIndexBySpeakerCanal Error[%d]\n", nAVCanal, index);
            return -1;
        }
        //Realease Channel
        AVAPI2_ReleaseChannelForSend(gAVServerInfo[index].nSpeakerAVCanal);
        gAVServerInfo[index].nSpeakerAVCanal = -1;

        //Stop Speaker_SendAudioThread
        gAVServerInfo[index].bSpeakerRun = 0;
    }
    else if(nError == AV_ER_CLIENT_NO_AVLOGIN){
        //Client login authentication error
        //Do AVAPI2_ServerExitCanal
        AVAPI2_ServerExitCanal(AVAPI2_GetSessionIDByAVCanal(nAVCanal), AVAPI2_GetChannelByAVCanal(nAVCanal));
    }
    else if(nError == AV_ER_MEM_INSUFF){
        printf("Speaker_CanalStatusCB : nAVCanal[%d] AV_ER_MEM_INSUFF\n", nAVCanal);
    }
    else if(nError == IOTC_ER_CH_NOT_ON){
        printf("Speaker_CanalStatusCB : nAVCanal[%d] IOTC_ER_CH_NOT_ON nChannelID[%d]\n", nAVCanal, nChannelID);
    }

    return 0;
}

int Speaker_ServerStatusCB(int nStatus, int nError, int nAVCanal, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    int index = 0;
    printf("Speaker_ServerStatusCB : nStatus[%d] nError[%d] nAVCanal[%d] nChannelID[%u]\n", nStatus, nError, nAVCanal, nChannelID);
    PrintErrHandling(nError);

    switch(nStatus){
		case AVAPI2_SERVER_STATUS_START_CANAL_FAILED :
		{
			printf("Speaker_ServerStatusCB AVAPI2_SERVER_STATUS_START_CANAL_FAILED\n");
			index = AVServerInfo_GetIndexBySpeakerCanal(nAVCanal);
			//Realease Channel
			AVAPI2_ReleaseChannelForSend(gAVServerInfo[index].nSpeakerAVCanal);
			gAVServerInfo[index].nSpeakerAVCanal = -1;

			//Stop Speaker_SendAudioThread
			gAVServerInfo[index].bSpeakerRun = 0;
		}
		break;

		case AVAPI2_SERVER_STATUS_CLIENT_LOGINED :
		{
			index = AVServerInfo_GetIndexBySpeakerCanal(nAVCanal);
			printf("Speaker_ServerStatusCB : Client logged in ok\n");
			gAVServerInfo[index].bSpeakerRun |= STATUS_CLIENT_LOGIN_SUCCESS;
		}
        break;

		default:
		break;
	}

    return 0;
}

void *Speaker_SendAudioThread(void *arg)
{
    int nRet = 0, size = 0, index = 0, nSpeakerCanal = 0;
	char buf[AUDIO_BUF_SIZE] = {0};
	int frameRate = 25;
	int sleepTick = 1000000/frameRate;
    unsigned char audioFlags = 0;
	FRAMEINFO_t frameInfo;

    nSpeakerCanal = *(int*)arg;
    index = AVServerInfo_GetIndexBySpeakerCanal(nSpeakerCanal);
    if(index < 0){
        printf("Speaker_SendAudioThread : nSpeakerCanal[%d] AVServerInfo_GetIndexBySpeakerCanal Error[%d]\n", nSpeakerCanal, index);
        return NULL;
    }

    memset(&frameInfo, 0, sizeof(frameInfo));

    //open audio file
    FILE *fp = fopen("audio_raw/8k_16bit_mono_rag.raw", "rb");
    if(fp == NULL){
        printf("Speaker_SendAudioThread : nSpeakerCanal[%d] fopen 8k_16bit_mono.raw error\n", nSpeakerCanal);
        return NULL;
    }

    printf("Speaker_SendAudioThread : nSpeakerCanal[%d] start\n", nSpeakerCanal);
	while(gAVServerInfo[index].bSpeakerRun)
    {
		if(gAVServerInfo[index].bSpeakerRun != STATUS_READY_FOR_SEND){
			sleep(1);
			continue;
		}

		size = fread(buf, 1, AUDIO_BUF_SIZE, fp);
		if(size <= 0){
			printf("rewind audio\n");
			rewind(fp);
			continue;
		}

        audioFlags = (AUDIO_SAMPLE_8K << 2) | (AUDIO_DATABITS_16 << 1) | AUDIO_CHANNEL_MONO;
        nRet = AVAPI2_SendAudioData(gAVServerInfo[index].nSpeakerAVCanal, MEDIA_CODEC_AUDIO_PCM, audioFlags, buf, AUDIO_BUF_SIZE);
        if(nRet == AV_ER_EXCEED_MAX_SIZE ||
            nRet == AV_ER_SOCKET_QUEUE_FULL ||
            nRet == AV_ER_MEM_INSUFF ||
            nRet == AV_ER_BUFPARA_MAXSIZE_INSUFF ||
            nRet == AV_ER_CLEANBUF_ALREADY_CALLED){
            //TODO : Handle Audio Buffer Full
            continue;
        }
        else if(nRet < 0){
            printf("Speaker_SendAudioThread : nSpeakerCanal[%d] AVAPI2_SendFrameData nRet[%d]\n", nSpeakerCanal, nRet);
            PrintErrHandling(nRet);
            break;
        }
		usleep(sleepTick);
	}
	fclose(fp);

    //Realease Channel
    if(gAVServerInfo[index].nSpeakerAVCanal >= 0){
        nRet = AVAPI2_ReleaseChannelForSend(gAVServerInfo[index].nSpeakerAVCanal);
        printf("Speaker_SendAudioThread : nSpeakerCanal[%d] AVAPI2_ReleaseChannelForSend nRet[%d]\n", nSpeakerCanal, nRet);
    }

    //Stop Speaker_SendAudioThread
    gAVServerInfo[index].nSpeakerAVCanal = -1;
    gAVServerInfo[index].nSpeakerChannel = 0;
    gAVServerInfo[index].bSpeakerRun = 0;

    printf("Speaker_SendAudioThread : nSpeakerCanal[%d] exit\n", nSpeakerCanal);

	return 0;
}

int Speaker_Start(int nAVCanal)
{
    int SID = 0, freeCH = 0, nRet = 0, index = 0;
    SMsgAVIoctrlAVStream ioMsg;
    pthread_t ThreadSpeaker_ID = 0;
    pthread_attr_t attr;

    index = AVServerInfo_GetIndexByCanal(nAVCanal);
    if(index < 0){
        printf("Speaker_Start : nAVCanal[%d] AVServerInfo_GetIndexByCanal Error[%d]\n", nAVCanal, index);
        return -1;
    }

    //Get Session ID
    SID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
    if(SID < 0){
        printf("Speaker_Start : nAVCanal[%d] AVAPI2_GetSessionIDByAVCanal Error[%d]\n", nAVCanal, SID);
        return -1;
    }

	//Get free IOTC channel
	freeCH = IOTC_Session_Get_Free_Channel(SID);
    if(freeCH < 0){
        printf("Speaker_Start : nAVCanal[%d] Get Free CH for Speaker Error[%d]\n", nAVCanal, freeCH);
        return -1;
    }
	printf("Speaker_Start : nAVCanal[%d] Get Free CH [%d] for Speaker\n", nAVCanal, freeCH);

    //Send IO Control IOTYPE_USER_IPCAM_SPEAKERSTART
    memset(&ioMsg, 0, sizeof(SMsgAVIoctrlAVStream));
    ioMsg.channel = freeCH;
    nRet = AVAPI2_SendIOCtrl(nAVCanal, IOTYPE_USER_IPCAM_SPEAKERSTART, (const char *)&ioMsg, sizeof(SMsgAVIoctrlAVStream));
    if(nRet < 0){
        printf("Speaker_Start : nAVCanal[%d] send Cmd IOTYPE_USER_IPCAM_SPEAKERSTART Error[%d]\n", nAVCanal, nRet);
        return -1;
    }
    printf("Speaker_Start : nAVCanal[%d] send Cmd IOTYPE_USER_IPCAM_SPEAKERSTART\n", nAVCanal);

    //Create Channel for send
    gAVServerInfo[index].nSpeakerAVCanal = AVAPI2_CreateChannelForSend(SID, 30, 0, freeCH, 0, Speaker_ServerStatusCB, Speaker_CanalStatusCB);
    if(gAVServerInfo[index].nSpeakerAVCanal < 0){
        printf("Speaker_Start : nAVCanal[%d] AVAPI2_CreateChannelForSend Error[%d]\n", nAVCanal, gAVServerInfo[index].nSpeakerAVCanal);
        return -1;
    }
    printf("Speaker_Start : nAVCanal[%d] Starting Speaker nSpeakerAVCanal[%d]\n", nAVCanal, gAVServerInfo[index].nSpeakerAVCanal);

    // Create Speaker thread
    nRet = pthread_attr_init(&attr);
    if(nRet != 0){
        printf("Speaker_Start : nAVCanal[%d] Attribute init failed\n", nAVCanal);
        return -1;
    }

    nRet = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if(nRet != 0){
        printf("Speaker_Start : nAVCanal[%d] Setting detached state failed\n", nAVCanal);
        return -1;
    }

    nRet = pthread_create(&ThreadSpeaker_ID, &attr, &Speaker_SendAudioThread, (void *)&gAVServerInfo[index].nSpeakerAVCanal);
    if(nRet != 0){
        printf("Speaker_Start : nAVCanal[%d] Create Speaker thread failed\n", nAVCanal);
        return -1;
    }

    gAVServerInfo[index].nSpeakerChannel = freeCH;
    gAVServerInfo[index].bSpeakerRun |= STATUS_SPEAKER_THREAD_START;

    return 0;
}

int Speaker_Stop(int nAVCanal)
{
    int index = AVServerInfo_GetIndexByCanal(nAVCanal);
    if(index < 0){
        printf("Speaker_Stop : AVServerInfo_GetIndexByCanal Error[%d]\n", index);
        return -1;
    }

    gAVServerInfo[index].bSpeakerRun = 0;

    //Send IO Control IOTYPE_USER_IPCAM_SPEAKERSTOP
    AVAPI2_SendIOCtrl(nAVCanal, IOTYPE_USER_IPCAM_SPEAKERSTOP, NULL, 0);
    printf("Speaker_Stop : nAVCanal[%d] nSpeakerAVCanal[%d] send Cmd IOTYPE_USER_IPCAM_SPEAKERSTOP\n", nAVCanal, gAVServerInfo[index].nSpeakerAVCanal);

    //if(gAVServerInfo[index].nSpeakerAVCanal >= 0){
    //    AVAPI2_ReleaseChannelForSend(gAVServerInfo[index].nSpeakerAVCanal);
    //    gAVServerInfo[index].nSpeakerAVCanal = -1;
    //}
    //printf("Speaker_Stop : nAVCanal[%d] nSpeakerAVCanal[%d] AVAPI2_ReleaseChannelForSend ok\n", nAVCanal, gAVServerInfo[index].nSpeakerAVCanal);

    return 0;
}
#endif

/******************************
 * AVAPI2 Client Call Back Functions
 ******************************/
static int Client_VideoRecvCB(int nAVCanal, int nError, char *pFrameData, int nActualFrameSize, int nExpectedFrameSize, char* pFrameInfo, int nFrameInfoSize, int frmNo, void* pUserData)
{
    FRAMEINFO_t* frmInfo = NULL;
    int index = 0;

    index = AVServerInfo_GetIndexByCanal(nAVCanal);
    if(index < 0){
        return 0;
    }

    if(nError < 0){
        printf("Client_VideoRecvCB : Receive frame nError:%d\n", nError);
        PrintErrHandling(nError);
        gAVServerInfo[index].v_err++;
        if(nError == AV_ER_LOSED_THIS_FRAME){
            printf("Client_VideoRecvCB : Video AV_ER_LOSED_THIS_FRAME frmNo[%d]\n", frmNo);
        }
        else{
            printf("Client_VideoRecvCB : Video receive nError[%d]\n", nError);
        }
        return 0;
    }

	if(pFrameData == NULL || nActualFrameSize < 0){
		printf("Client_VideoRecvCB : Invalid frame\n");
		return -1;
	}

    if(gAVServerInfo[index].firstVideo == 0){
        gettimeofday(&gAVServerInfo[index].g_tv2, NULL);
        int nTaketime = (gAVServerInfo[index].g_tv2.tv_sec-gAVServerInfo[index].g_tv1.tv_sec)*1000 + (gAVServerInfo[index].g_tv2.tv_usec-gAVServerInfo[index].g_tv1.tv_usec)/1000;
        printf("Client_VideoRecvCB : nAVCanal[%d] First Video nTaketime[%d]\n", nAVCanal, nTaketime);
        gAVServerInfo[index].firstVideo = 1;
    }

    frmInfo = (FRAMEINFO_t*)pFrameInfo;
    gAVServerInfo[index].videotime = frmInfo->timestamp;

    if(gAVServerInfo[index].v_TotalCnt == 0)
        gettimeofday(&gAVServerInfo[index].v_tv1, NULL);
    gAVServerInfo[index].v_fpsCnt++;
    gAVServerInfo[index].v_TotalCnt++;
    gAVServerInfo[index].v_bps += nActualFrameSize;

    gettimeofday(&gAVServerInfo[index].v_tv2, NULL);
    int msecDiff = msecDiff = (gAVServerInfo[index].v_tv2.tv_sec-gAVServerInfo[index].v_tv1.tv_sec)*1000 + (gAVServerInfo[index].v_tv2.tv_usec-gAVServerInfo[index].v_tv1.tv_usec)/1000;

    if(msecDiff > 10000){
        printf("===== Client_VideoRecvCB : nAVCanal[%d] pUserData[%s] Video FPS[%d] err[%d] TotalCnt[%d] LastFrameSize[%d Byte] Codec[%x] Flag[%d] bps[%d Kb] V/A TimeDiff[%d] =====\n", nAVCanal, pUserData != NULL ? (char*)pUserData : "NULL", gAVServerInfo[index].v_fpsCnt/10, gAVServerInfo[index].v_err, gAVServerInfo[index].v_TotalCnt, nActualFrameSize, frmInfo->codec_id, frmInfo->flags, ((gAVServerInfo[index].v_bps/1024)*8)/10, gAVServerInfo[index].videotime-gAVServerInfo[index].audiotime);
        gettimeofday(&gAVServerInfo[index].v_tv1, NULL);
        gAVServerInfo[index].v_fpsCnt = 0;
        gAVServerInfo[index].v_bps = 0;
    }

	#if 0
	static int frmCnt = 0;
	char fn[32];
	printf("The file will be saved.");
	if(frmInfo->flags == IPC_FRAME_FLAG_IFRAME)
		sprintf(fn, "I_%03d.bin", frmCnt);
	else
		sprintf(fn, "P_%03d.bin", frmCnt);
	frmCnt++;
	FILE *fp = fopen(fn, "wb+");
	fwrite(pFrameData, 1, nActualFrameSize, fp);
	fclose(fp);
	#endif

#if ENABLE_TIME_SYNC
    if(TimeSync_InsertVideo(&gAVServerInfo[index].timeSyncInfo, pFrameData, nActualFrameSize, frmNo, (FRAMEINFO_t*)pFrameInfo) < 0){
        printf("TimeSync_InsertVideo error\n");
    }
#endif

	return 0;
}

#if ENABLE_AUDIO_STREAM
static int Client_AudioRecvCB(int nAVCanal, int nError, char *pFrameData, int nFrameSize, char* pFrameInfo, int frmNo, void* pUserData)
{
    FRAMEINFO_t* frmInfo = NULL;
    int index = 0, i = 0, count = 0;
    float percent = 0;
    StatisticalDataSlot statisticalDataSlot;
    StatisticalClientDataSlot statisticalClientDataSlot;
    struct st_SInfo Sinfo;
    char *mode[] = {"P2P", "RLY", "LAN"};
    int SID = 0;

    index = AVServerInfo_GetIndexByCanal(nAVCanal);
    if(index < 0){
        return 0;
    }

    if(nError < 0){
        //printf("Client_AudioRecvCB : Receive audio nError:%d\n", nError);
        PrintErrHandling(nError);
        gAVServerInfo[index].a_err++;

        switch(nError)
        {
            case AV_ER_DATA_NOREADY:
                printf("[%s] AV_ER_DATA_NOREADY" , __func__);
                //msleep(500);
                break;
            case AV_ER_LOSED_THIS_FRAME:
                printf("***** AV_ER_LOSED_THIS_FRAME frmNo[%d] *****\n", frmNo);
                #if ENABLE_TIME_SYNC
                if(TimeSync_HandleAudioLost(&gAVServerInfo[index].timeSyncInfo, frmNo) < 0){
                    printf("TimeSync_HandleAudioLost error\n");
                }
                #endif
                break;
            default:
                printf("Client_AudioRecvCB : Audio receive nError[%d]\n", nError);
                break;
        }
        return 0;
    }

	if(pFrameData == NULL || nFrameSize < 0){
		printf("Client_AudioRecvCB : Invalid frame\n");
		return -1;
	}

    if(gAVServerInfo[index].firstAudio == 0){
        gettimeofday(&gAVServerInfo[index].g_tv2, NULL);
        int nTaketime = (gAVServerInfo[index].g_tv2.tv_sec-gAVServerInfo[index].g_tv1.tv_sec)*1000 + (gAVServerInfo[index].g_tv2.tv_usec-gAVServerInfo[index].g_tv1.tv_usec)/1000;
        printf("Client_AudioRecvCB : nAVCanal[%d] First Audio nTaketime[%d]\n", nAVCanal, nTaketime);
        gAVServerInfo[index].firstAudio = 1;
    }

    frmInfo = (FRAMEINFO_t*)pFrameInfo;
	//printf("Audio frame received via Canal #%d at time %u\n", nAVCanal, frmInfo->timestamp);
    gAVServerInfo[index].audiotime = frmInfo->timestamp;

    if(gAVServerInfo[index].a_TotalCnt == 0)
        gettimeofday(&gAVServerInfo[index].a_tv1, NULL);
    gAVServerInfo[index].a_fpsCnt++;
    gAVServerInfo[index].a_TotalCnt++;
    gAVServerInfo[index].a_bps += nFrameSize;

    gettimeofday(&gAVServerInfo[index].a_tv2, NULL);
    int msecDiff = msecDiff = (gAVServerInfo[index].a_tv2.tv_sec-gAVServerInfo[index].a_tv1.tv_sec)*1000 + (gAVServerInfo[index].a_tv2.tv_usec-gAVServerInfo[index].a_tv1.tv_usec)/1000;

    if(msecDiff > 10000){
        printf("===== Client_AudioRecvCB : nAVCanal[%d] pUserData[%s] Audio FPS[%d] err[%d] TotalCnt[%d] LastFrameSize[%d Byte] Codec[%x] Flag[%d] bps[%d Kb] =====\n", nAVCanal, pUserData != NULL ? (char*)pUserData : "NULL", gAVServerInfo[index].a_fpsCnt/10, gAVServerInfo[index].a_err, gAVServerInfo[index].a_TotalCnt, nFrameSize, frmInfo->codec_id, frmInfo->flags, ((gAVServerInfo[index].a_bps/1024)*8)/10);
        gettimeofday(&gAVServerInfo[index].a_tv1, NULL);
        gAVServerInfo[index].a_fpsCnt = 0;
        gAVServerInfo[index].a_bps = 0;

        SID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
            if(IOTC_Session_Check(SID, &Sinfo) == IOTC_ER_NoERROR){
                if( isdigit( Sinfo.RemoteIP[0] ))
                    printf("Device is from %s:%d[%s] Mode=%s NAT[%d] IOTCVersion[%X]\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID, mode[(int)Sinfo.Mode], Sinfo.NatType, Sinfo.IOTCVersion);
        }
#if 1
        //Print statistical data
        if(nAVCanal >= 0){
            AVAPI2_GetStatisticalData(nAVCanal, &statisticalDataSlot);
            printf("******************************\n");
            count = 0;
            while(count < statisticalDataSlot.usCount){
                count++;
                i = statisticalDataSlot.usIndex+count;
                i = i%statisticalDataSlot.usCount;
                if(i == statisticalDataSlot.usIndex)
                    continue;
                printf("timeStamp[%u] vSend[%7u] vData[%7u] vDrop[%7u] vResend[%7u] vFPS[%2u] vReqCnt[%4u] vCom[%2u] aCom[%2u] Packet Jitter[%u~%u]\n", statisticalDataSlot.m_Data[i].uTimeStamp/1000, statisticalDataSlot.m_Data[i].uVSendByte, statisticalDataSlot.m_Data[i].uVDataByte, statisticalDataSlot.m_Data[i].uVDropByte, statisticalDataSlot.m_Data[i].uVResendByte, statisticalDataSlot.m_Data[i].usVFPS, statisticalDataSlot.m_Data[i].uVResendReqCnt, statisticalDataSlot.m_Data[i].usVCompleteFPS, statisticalDataSlot.m_Data[i].usACompleteFPS, statisticalDataSlot.m_Data[i].usResponeMin, statisticalDataSlot.m_Data[i].usResponeMax);
            }

            AVAPI2_GetStatisticalClientData(nAVCanal, &statisticalClientDataSlot);
            printf("******************************\n");
            count = 0;
            while(count < statisticalClientDataSlot.usCount){
                count++;
                i = statisticalClientDataSlot.usIndex+count;
                i = i%statisticalClientDataSlot.usCount;
                if(i == statisticalClientDataSlot.usIndex)
                    continue;

                percent = 0.0;
                if(statisticalClientDataSlot.m_Data[i].uVRepeateByte > 0 && statisticalClientDataSlot.m_Data[i].uVResendByte > 0)
                    percent = ((float)statisticalClientDataSlot.m_Data[i].uVRepeateByte*100)/(statisticalClientDataSlot.m_Data[i].uVResendByte);
                printf("timeStamp[%u] vRecv[%7u] vUser[%7u] vDrop[%7u] vResend[%7u] vFPS[%2u] vReqCnt[%4u] vRepeate[%7u] [%f] Packet Jitter[%u~%u]\n", statisticalClientDataSlot.m_Data[i].uTimeStamp/1000, statisticalClientDataSlot.m_Data[i].uVRecvByte, statisticalClientDataSlot.m_Data[i].uVUserByte, statisticalClientDataSlot.m_Data[i].uVDropByte, statisticalClientDataSlot.m_Data[i].uVResendByte, statisticalClientDataSlot.m_Data[i].usVFPS, statisticalClientDataSlot.m_Data[i].uVResendReqCnt, statisticalClientDataSlot.m_Data[i].uVRepeateByte, percent, statisticalClientDataSlot.m_Data[i].usResponeMin, statisticalClientDataSlot.m_Data[i].usResponeMax);
            }
            printf("==============================\n");
        }
#endif
    }

#if ENABLE_TIME_SYNC
    if(TimeSync_InsertAudio(&gAVServerInfo[index].timeSyncInfo, pFrameData, nFrameSize, frmNo, (FRAMEINFO_t*)pFrameInfo) < 0){
        printf("TimeSync_InsertAudio Error\n");
    }
#endif

	return 0;
}
#endif

#if ENABLE_TIME_SYNC
int Client_addAudioDecodeQueue(void* pTimeSyncInfo, char* pFrameData, int nActualFrameSize, FRAMEINFO_t* pFrameInfo, int nFrameInfoSize, int frmNo)
{
    return 0;
}

int Client_addVideoDecodeQueue(void* pTimeSyncInfo, char* pFrameData, int nActualFrameSize, FRAMEINFO_t* pFrameInfo, int nFrameInfoSize, int frmNo)
{
    return 0;
}
#endif

static int Client_IOCtrlRecvCB(int nAVCanal, unsigned int nIoCtrlType, unsigned char *pIoCtrlBuf, unsigned int nIoCtrlBufLen, void* pUserData)
{
	printf("Client_IOCtrlRecvCB : nAVCanal[%d] nIoCtrlType[0x%x] nIoCtrlBufLen[%u]\n", nAVCanal, nIoCtrlType, nIoCtrlBufLen);
    if(pUserData != NULL)
        printf("Client_IOCtrlRecvCB : pUserData[%s]\n", (char*)pUserData);
	return 0;
}

static int Client_CanalStatusCB(int nAVCanal, int nError, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
	printf("Client_CanalStatusCB : nAVCanal[%d] error with error code [%d].\n", nAVCanal, nError);
    if(pUserData != NULL)
        printf("Client_CanalStatusCB : pUserData[%s]\n", (char*)pUserData);
	PrintErrHandling(nError);

    if(nError == AV_ER_SESSION_CLOSE_BY_REMOTE ||
        nError == AV_ER_REMOTE_TIMEOUT_DISCONNECT ||
        nError == AV_ER_IOTC_SESSION_CLOSED ||
        nError == AV_ER_CLIENT_EXIT ||
        nError == AV_ER_INVALID_SID){
        //Do client disconnect and close IOTC connection
        AVAPI2_ClientDisconnectAndCloseIOTC(nAVCanal);

        //Clean AV Server Info
        AVServerInfo_Clean(AVServerInfo_GetIndexByCanal(nAVCanal));
    }
    else if(nError == AV_ER_MEM_INSUFF){
        printf("Client_CanalStatusCB : AV_ER_MEM_INSUFF\n");
    }
    else if(nError == IOTC_ER_CH_NOT_ON){
        printf("Client_CanalStatusCB : IOTC_ER_CH_NOT_ON nChannelID[%d]\n", nChannelID);
    }

	return 0;
}

static int Client_ClientStatusCB(int nStatus, int nError, int nAVCanal, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    int nRet = 0, nTaketime = 0, index = 0;
    char *mode[] = {"P2P", "RLY", "LAN"}; 

    printf("Client_ClientStatusCB : nStatus[%d] nError[%d] nAVCanal[%d] UID[%s] nChannelID[%u]\n", nStatus, nError, nAVCanal, pStSInfo->UID, nChannelID);
    if(pUserData != NULL)
        printf("Client_ClientStatusCB : pUserData[%s]\n", (char*)pUserData);

    switch(nStatus){
        case AVAPI2_CLIENT_CONNECT_UID_ST_START :
        {
            printf("Client_ClientStatusCB : Client start connecting to a Device\n");
        }
        break;

        case AVAPI2_CLIENT_CONNECT_UID_ST_CONNECTING :
        {
            printf("Client_ClientStatusCB : Client is connecting a device\n");
        }
        break;

        case AVAPI2_CLIENT_CONNECT_UID_ST_CONNECTED :
        {
            printf("Client_ClientStatusCB : The connection is established between a Client and a Device\n");
            //Get IOTC Session Info
            if(pStSInfo != NULL){
                if(isdigit(pStSInfo->RemoteIP[0]))
                    printf("Client_ClientStatusCB : Device is from %s:%d[%s] Mode=%s NAT[%d] IOTCVersion[%X]\n", pStSInfo->RemoteIP, pStSInfo->RemotePort, pStSInfo->UID, mode[(int)pStSInfo->Mode], pStSInfo->NatType, pStSInfo->IOTCVersion);
            }

            // Get AV Server Info Index
            index = AVServerInfo_GetIndexByUID(pStSInfo->UID);
            if(index < 0){
                printf("Client_ClientStatusCB : Can't find UID[%s] in server info\n", pStSInfo->UID);
                break;
            }
            gettimeofday(&gAVServerInfo[index].g_tv2, NULL);
            nTaketime = (gAVServerInfo[index].g_tv2.tv_sec-gAVServerInfo[index].g_tv1.tv_sec)*1000 + (gAVServerInfo[index].g_tv2.tv_usec-gAVServerInfo[index].g_tv1.tv_usec)/1000;
            printf("Client_ClientStatusCB : UID[%s] nAVCanal[%d] Connect nTaketime[%d ms]\n", gAVServerInfo[index].szUID, nAVCanal, nTaketime);

            //Register receive IO control call back function
            gAVServerInfo[index].nAVCanal = nAVCanal;
            nRet = AVAPI2_RegRecvIoCtrlCB(gAVServerInfo[index].nAVCanal, Client_IOCtrlRecvCB);
            if(nRet < 0){
                printf("Client_ClientStatusCB : UID[%s] nAVCanal[%d] AVAPI2_RegRecvIoCtrlCB nRet[%d]\n", gAVServerInfo[index].szUID, nAVCanal, nRet);
                PrintErrHandling(nRet);
                AVAPI2_ClientDisconnectAndCloseIOTC(gAVServerInfo[index].nAVCanal);
                AVServerInfo_Clean(index);
                break;
            }

            //Start recveice video and register call back function using channel 0
        	nRet = AVAPI2_StartRecvFrame(gAVServerInfo[index].nAVCanal, 0, Client_VideoRecvCB);
            if(nRet < 0){
                printf("Client_ClientStatusCB : UID[%s] nAVCanal[%d] AVAPI2_StartRecvFrame nRet[%d]\n", gAVServerInfo[index].szUID, nAVCanal, nRet);
                PrintErrHandling(nRet);
                AVAPI2_ClientDisconnectAndCloseIOTC(gAVServerInfo[index].nAVCanal);
                AVServerInfo_Clean(index);
                break;
            }
        	printf("Client_ClientStatusCB : UID[%s] nAVCanal[%d] Start Receiving Frame\n", gAVServerInfo[index].szUID, nAVCanal);
            #if ENABLE_TIME_SYNC
            TimeSync_VideoEnable(&gAVServerInfo[index].timeSyncInfo, 1);
            #endif

#if ENABLE_AUDIO_STREAM
            //Start recveice audio and register call back function using channel 0
        	nRet = AVAPI2_StartRecvAudio(gAVServerInfo[index].nAVCanal, 0, Client_AudioRecvCB);
            if(nRet < 0){
                printf("Client_ClientStatusCB : UID[%s] nAVCanal[%d] AVAPI2_StartRecvAudio nRet[%d]\n", gAVServerInfo[index].szUID, nAVCanal, nRet);
                PrintErrHandling(nRet);
                AVAPI2_ClientDisconnectAndCloseIOTC(gAVServerInfo[index].nAVCanal);
                AVServerInfo_Clean(index);
                break;
            }
            printf("Client_ClientStatusCB : UID[%s] nAVCanal[%d] Start Receiving Audio\n", gAVServerInfo[index].szUID, nAVCanal);
            #if ENABLE_TIME_SYNC
            TimeSync_AudioEnable(&gAVServerInfo[index].timeSyncInfo, 1);
            #endif
#endif
        }
        break;

        case AVAPI2_CLIENT_STATUS_LOGINED :
        {
            printf("Client_ClientStatusCB : Login To AVAPI Server Success\n");
            index = AVServerInfo_GetIndexByUID(pStSInfo->UID);
            if(index < 0){
                printf("Client_ClientStatusCB : Can't find UID[%s] in server info\n", pStSInfo->UID);
                break;
            }

            gettimeofday(&gAVServerInfo[index].g_tv2, NULL);
            nTaketime = (gAVServerInfo[index].g_tv2.tv_sec-gAVServerInfo[index].g_tv1.tv_sec)*1000 + (gAVServerInfo[index].g_tv2.tv_usec-gAVServerInfo[index].g_tv1.tv_usec)/1000;
            printf("Client_ClientStatusCB : UID[%s] nAVCanal[%d] Connect Success nTaketime[%d]\n", gAVServerInfo[index].szUID, nAVCanal, nTaketime);
        }
        break;

        //Error Handle
        case AVAPI2_CLIENT_STATUS_LOGIN_FAILED :
        {
            printf("Client_ClientStatusCB : Login to server failed\n");
            if(nError == AV_ER_WRONG_VIEWACCorPWD){
                printf("Client_ClientStatusCB : AV_ER_WRONG_VIEWACCorPWD\n");
            }
            index = AVServerInfo_GetIndexByCanal(nAVCanal);
            if(index < 0){
                printf("Client_ClientStatusCB : Can't find nAVCanal[%d] in server info\n", nAVCanal);
                break;
            }

            AVAPI2_ClientDisconnectAndCloseIOTC(nAVCanal);
            AVServerInfo_Clean(index);
        }
        break;

        case AVAPI2_CLIENT_STATUS_LOGIN_TIMEOUT :
        {
            printf("Client_ClientStatusCB : Login to server timeout\n");
            index = AVServerInfo_GetIndexByCanal(nAVCanal);
            if(index < 0){
                printf("Client_ClientStatusCB : Can't find nAVCanal[%d] in server info\n", nAVCanal);
                break;
            }

            AVAPI2_ClientDisconnectAndCloseIOTC(nAVCanal);
            AVServerInfo_Clean(index);
        }
        break;

        case AVAPI2_CLIENT_STATUS_CLEAN_BUFFER_TIMEOUT :
        {
            printf("Client_ClientStatusCB : Clean buffer timeout\n");
            index = AVServerInfo_GetIndexByCanal(nAVCanal);
            if(index < 0){
                printf("Client_ClientStatusCB : Can't find nAVCanal[%d] in server info\n", nAVCanal);
                break;
            }

            AVAPI2_ClientDisconnectAndCloseIOTC(nAVCanal);
            AVServerInfo_Clean(index);
        }
        break;

        case AVAPI2_CLIENT_STATUS_RECV_FRAME_BLOCK :
        {
            printf("Client_ClientStatusCB : Took too long to handle received video frame in callback function , block %d ms\n", nError);
        }
        break;

        case AVAPI2_CLIENT_STATUS_RECV_AUDIO_BLOCK :
        {
            printf("Client_ClientStatusCB : Took too long to handle received audio data in callback function , block %d ms\n", nError);
        }
        break;

        case AVAPI2_CLIENT_CONNECT_UID_ST_FAILED:
        {
            printf("Client_ClientStatusCB : Client connects to a device failed\n");
            // Device Offline
            if(nError == IOTC_ER_DEVICE_OFFLINE){
                break;
            }

            //Handle Reconnect
            index = AVServerInfo_GetIndexByUID(pStSInfo->UID);
            if(index >= 0){
                printf("Client_ClientStatusCB : Try Reconnect UID[%s]\n", pStSInfo->UID);
                gettimeofday(&gAVServerInfo[index].g_tv1, NULL);
                nRet = AVAPI2_ClientConnectByUID(gAVServerInfo[index].szUID, gAVID, gAVServerInfo[index].szPassword, 30, 0, Client_CanalStatusCB, Client_ClientStatusCB, pUserData);
                if(nRet < 0){
                    printf("Client_ClientStatusCB : UID[%s] AVAPI2_ClientConnectByUID nRet[%d]\n", gAVServerInfo[index].szUID, nRet);
                    PrintErrHandling(nRet);
                    break;
                }
            }
        }
        break;

        default:
            break;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int nRet = 0, i = 0, ResetTime = 0;
    char szVersion[64] = {0};
    unsigned int nConnectTimeout = 30;
    struct timeval tv1, tv2;
#if ENABLE_TEST_CLEAN_BUFFER
    unsigned int nCleanBufferTimeout = 10;
    int bCleanServerBuffer = 1;
#endif
    char* avCanalStr = NULL;

    AVServerInfo_Initialize();

    // Get Server UID & Password From Input Argument
    if(argc < 3 || argc > MAX_SERVER_NUMBER*2 + 1){
        printf("Usage: %s [UID1] [Password1] [ [UID2] [Password2] ... ]\n", argv[0]);
        return -1;
    }
    else{
        gAVServerNum = (argc-1)/2;
        for (i = 0; i < gAVServerNum; i++){
            strncpy(gAVServerInfo[i].szUID, argv[i*2+1], 32);
            strncpy(gAVServerInfo[i].szPassword, argv[i*2+2], 32);
        }
    }
    if(gAVServerNum <= 0){
        printf("Argument Error : No Server UID\n");
        return -1;
    }

    //Initial IOTC & AVAPI2
    AVAPI2_SetCanalLimit(MAX_SERVER_NUMBER, 4);

    //Get Version
    AVAPI2_GetVersion(szVersion, 64);
    printf("%s\n", szVersion);

    //Connect to AVAPI2 Server
    //nAVCanal will return through Client_ClientStatusCB
    for (i = 0; i < gAVServerNum; i++){
        gettimeofday(&gAVServerInfo[i].g_tv1, NULL);
        printf("Connect to UID[%s] Password[%s]\n", gAVServerInfo[i].szUID, gAVServerInfo[i].szPassword);
        avCanalStr = (char*)malloc(16);
        sprintf(avCanalStr, "connection:%d", i);

        #if ENABLE_TIME_SYNC
        if(TimeSync_Initialize(&gAVServerInfo[i].timeSyncInfo, i, Client_addVideoDecodeQueue, Client_addAudioDecodeQueue) < 0){
            printf("TimeSync_Initialize error\n");
            continue;
        }
        #endif

        nRet = AVAPI2_ClientConnectByUID(gAVServerInfo[i].szUID, gAVID, gAVServerInfo[i].szPassword, nConnectTimeout, 0, Client_CanalStatusCB, Client_ClientStatusCB, avCanalStr);
        if(nRet < 0){
            printf("Connect to UID[%s] Password[%s] AVAPI2_ClientConnectByUID nRet[%d]\n", gAVServerInfo[i].szUID, gAVServerInfo[i].szPassword, nRet);
            PrintErrHandling (nRet);

            #if ENABLE_TIME_SYNC
            TimeSync_DeInitialize(&gAVServerInfo[i].timeSyncInfo);
            #endif
        }
    }

    gettimeofday(&tv1, NULL);
	while(1){
        gettimeofday(&tv2, NULL);
#if ENABLE_TEST_CLEAN_BUFFER
        if(tv2.tv_sec-tv1.tv_sec > 10){
            //Do Clean Buffer
            for (i = 0; i < gAVServerNum; i++){
                if(gAVServerInfo[i].nAVCanal >= 0){
                    //Client_ClientStatusCB will received status AVAPI2_CLIENT_STATUS_CLEAN_BUFFER_TIMEOUT when error occur.
                    nRet = AVAPI2_ClientCleanBuf(gAVServerInfo[i].nAVCanal, nCleanBufferTimeout, bCleanServerBuffer);
                    printf("UID[%s] nAVCanal[%d] AVAPI2_ClientCleanBuf nRet[%d]\n", gAVServerInfo[i].szUID, gAVServerInfo[i].nAVCanal, nRet);
                }
            }
            ResetTime = 1;
        }
#endif

#if ENABLE_SPEAKER_STREAM
        if(tv2.tv_sec-tv1.tv_sec > 10){
            for (i = 0; i < gAVServerNum; i++){
                if(gAVServerInfo[i].nAVCanal >= 0){
                    if(gAVServerInfo[i].bSpeakerRun == 0){
                        printf("UID[%s] nAVCanal[%d] Call Speaker_Start\n", gAVServerInfo[i].szUID, gAVServerInfo[i].nAVCanal);
                        Speaker_Start(gAVServerInfo[i].nAVCanal);
                    }
                    else{
                        printf("UID[%s] nAVCanal[%d] Call Speaker_Stop\n", gAVServerInfo[i].szUID, gAVServerInfo[i].nAVCanal);
                        Speaker_Stop(gAVServerInfo[i].nAVCanal);
                    }
                }
            }
            ResetTime = 1;
        }
#endif
        if(ResetTime){
            ResetTime = 0;
            gettimeofday(&tv1, NULL);
        }
        sleep(1);
    }

    AVAPI2_ClientStop();
    AVServerInfo_Initialize();

	return 0;
}


