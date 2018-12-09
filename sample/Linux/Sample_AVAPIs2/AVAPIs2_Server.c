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
#include "AVAPIs2_Emulater.h"

/******************************
 * Defined
 ******************************/
#define SERVTYPE_STREAM_SERVER  16  // Server Type , Using at AVAPI2_ServerStart
#define MAX_CLIENT_NUMBER       128 // Maxinum client number, Using at AVAPI2_SetCanalLimit
#define MAX_CH_NUMBER           16  // Maxinum channel namber of a client, Using at AVAPI2_SetCanalLimit
//#define FPS                     25  // Video FPS : Using at Streamout_VideoThread
#define GOP                     30  // Video GOP : Using at Streamout_VideoThread
//#define ENABLE_EMULATER         1
                                    // Using at Streamout_AudioThread
#define AUDIO_FORMAT_PCM            // Modify this define for test which audio format

#ifdef AUDIO_FORMAT_PCM
#define AUDIO_FRAME_SIZE 640
#define AUDIO_FPS 25
#define AUDIO_CODEC 0x8C

#elif defined (AUDIO_FORMAT_ADPCM)
#define AUDIO_FRAME_SIZE 160
#define AUDIO_FPS 25
#define AUDIO_CODEC 0x8B

#elif defined (AUDIO_FORMAT_SPEEX)
#define AUDIO_FRAME_SIZE 38
#define AUDIO_FPS 56
#define AUDIO_CODEC 0x8D

#elif defined (AUDIO_FORMAT_MP3)
#define AUDIO_FRAME_SIZE 380
#define AUDIO_FPS 32
#define AUDIO_CODEC 0x8E

#elif defined (AUDIO_FORMAT_SPEEX_ENC)
#define AUDIO_FRAME_SIZE 160
#define AUDIO_ENCODED_SIZE 160
#define AUDIO_FPS 56
#define AUDIO_CODEC 0x8D

#elif defined (AUDIO_FORMAT_G726_ENC)
#define AUDIO_FRAME_SIZE 320
#define AUDIO_ENCODED_SIZE 40
#define AUDIO_FPS 50
#define AUDIO_CODEC 0x8F

#elif defined (AUDIO_FORMAT_G726)
#define AUDIO_FRAME_SIZE 40
#define AUDIO_FPS 50
#define AUDIO_CODEC 0x8F
#endif

#define VIDEO_BUF_SIZE	(1024 * 400)        // Video buffer size , Using at Streamout_VideoThread
#define AUDIO_BUF_SIZE	(1024)              // Audio buffer size , Using at Streamout_AudioThread

/******************************
 * Structure
 ******************************/
//AV_Client_Info : Save client infomation
typedef struct _AV_Client_Info
{
    int nAVCanal;
    int nSpeakerAVCanal;
    int nSpeakerIOTCChannel;
    int nVideoQuality;
    unsigned char bEnableAudio;
    unsigned char bEnableVideo;
    unsigned char bEnableSpeaker;
    unsigned char waitKeyFrame;
    unsigned char bSendCache;
    pthread_rwlock_t sLock;
}AV_Client_Info;

/******************************
 * Global Variable
 * Set AVAPI2 server ID and Password here
 ******************************/
char gAVID[] = "admin";
char gAVPass[] = "888888ii";

int FPS = 30; // Should be read from the .info files.

#ifdef HEX_AUDIO
extern char g_audio_hex[];
extern const int g_audio_hex_size;
#endif

static AV_Client_Info gClientInfo[MAX_CLIENT_NUMBER];

static char gUID[21] = {0};             //argv[1] of Usage: ./AVAPIs2_Server [UID] [VideoFile] [AudioFile]
static char gVideoFilename[128] = {0};  //argv[2] of Usage: ./AVAPIs2_Server [UID] [VideoFile] [AudioFile]
static char gAudioFilename[128] = {0};  //argv[3] of Usage: ./AVAPIs2_Server [UID] [VideoFile] [AudioFile]
static int gCongestionControlMode = 0;
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
        memset(&gClientInfo[i], 0, sizeof(AV_Client_Info));
        gClientInfo[i].nAVCanal = -1;
        gClientInfo[i].nSpeakerAVCanal = -1;
        pthread_rwlock_init(&(gClientInfo[i].sLock), NULL);
    }
}

void AVClientInfo_DeInitialize()
{
    int i = 0;

    for(i = 0 ; i < MAX_CLIENT_NUMBER ; i++){
        memset(&gClientInfo[i], 0, sizeof(AV_Client_Info));
        gClientInfo[i].nAVCanal = -1;
        gClientInfo[i].nSpeakerAVCanal = -1;
        pthread_rwlock_destroy(&gClientInfo[i].sLock);
    }
}

void AVClientInfo_RegeditVideo(int nSID, int nAVCanal)
{
	AV_Client_Info *p = NULL;

    if(nSID < 0 || nSID >= MAX_CLIENT_NUMBER)
        return;

    p = &gClientInfo[nSID];

    //get writer lock
    int lock_ret = pthread_rwlock_wrlock(&gClientInfo[nSID].sLock);
	if(lock_ret){
		printf("Acquire nSID[%d] rwlock error ret[%d]\n", nSID, lock_ret);
        return;
    }

	p->nAVCanal = nAVCanal;
	p->bEnableVideo = 1;
	p->bSendCache = 1;

	//release lock
	pthread_rwlock_unlock(&gClientInfo[nSID].sLock);
}

void AVClientInfo_UnRegeditVideo(int nSID)
{
	AV_Client_Info *p = NULL;

    if(nSID < 0 || nSID >= MAX_CLIENT_NUMBER)
        return;

    p = &gClientInfo[nSID];

    //get writer lock
    int lock_ret = pthread_rwlock_wrlock(&gClientInfo[nSID].sLock);
	if(lock_ret){
		printf("Acquire nSID[%d] rwlock error ret[%d]\n", nSID, lock_ret);
        return;
    }

	p->bEnableVideo = 0;
    p->nAVCanal = -1;

    //release lock
	pthread_rwlock_unlock(&gClientInfo[nSID].sLock);
}

void AVClientInfo_RegeditAudio(int nSID, int nAVCanal)
{
	AV_Client_Info *p = NULL;

    if(nSID < 0 || nSID >= MAX_CLIENT_NUMBER)
        return;

    p = &gClientInfo[nSID];

    //get writer lock
    int lock_ret = pthread_rwlock_wrlock(&gClientInfo[nSID].sLock);
	if(lock_ret){
		printf("Acquire nSID[%d] rwlock error ret[%d]\n", nSID, lock_ret);
        return;
    }

	p->bEnableAudio = 1;

    //release lock
	pthread_rwlock_unlock(&gClientInfo[nSID].sLock);
}

void AVClientInfo_UnRegeditAudio(int nSID)
{
	AV_Client_Info *p = NULL;

    if(nSID < 0 || nSID >= MAX_CLIENT_NUMBER)
        return;

    p = &gClientInfo[nSID];

    //get writer lock
    int lock_ret = pthread_rwlock_wrlock(&gClientInfo[nSID].sLock);
	if(lock_ret){
		printf("Acquire nSID[%d] rwlock error ret[%d]\n", nSID, lock_ret);
        return;
    }

	p->bEnableAudio = 0;

    //release lock
    pthread_rwlock_unlock(&gClientInfo[nSID].sLock);

}

void AVClientInfo_RegeditSpeaker(int nSID, int nSpeakerAVCanal, int nSpeakerChannel)
{
	AV_Client_Info *p = NULL;

    if(nSID < 0 || nSID >= MAX_CLIENT_NUMBER)
        return;

    p = &gClientInfo[nSID];

    //get writer lock
    int lock_ret = pthread_rwlock_wrlock(&gClientInfo[nSID].sLock);
	if(lock_ret){
		printf("Acquire nSID[%d] rwlock error ret[%d]\n", nSID, lock_ret);
        return;
    }

    p->nSpeakerAVCanal = nSpeakerAVCanal;
    p->nSpeakerIOTCChannel = nSpeakerChannel;
    p->bEnableSpeaker = 1;

    //release lock
	pthread_rwlock_unlock(&gClientInfo[nSID].sLock);
}

void AVClientInfo_UnRegeditSpeaker(int nSID)
{
	AV_Client_Info *p = NULL;

    if(nSID < 0 || nSID >= MAX_CLIENT_NUMBER)
        return;

    p = &gClientInfo[nSID];

    //get writer lock
    int lock_ret = pthread_rwlock_wrlock(&gClientInfo[nSID].sLock);
	if(lock_ret){
		printf("Acquire nSID[%d] rwlock error ret[%d]\n", nSID, lock_ret);
        return;
    }

    p->nSpeakerAVCanal = -1;
    p->nSpeakerIOTCChannel = -1;
    p->bEnableSpeaker = 0;

    //release lock
	pthread_rwlock_unlock(&gClientInfo[nSID].sLock);
}

int AVClientInfo_GetSpeakerCanal(int nSID)
{
	AV_Client_Info *p = NULL;
    int nSpeakerCanal = -1;

    if(nSID < 0 || nSID >= MAX_CLIENT_NUMBER)
        return -1;

    p = &gClientInfo[nSID];

    //get writer lock
    int lock_ret = pthread_rwlock_rdlock(&gClientInfo[nSID].sLock);
	if(lock_ret){
		printf("Acquire nSID[%d] rdlock error ret[%d]\n", nSID, lock_ret);
        return -1;
    }

    if(p->bEnableSpeaker)
        nSpeakerCanal = p->nSpeakerAVCanal;

    //release lock
	pthread_rwlock_unlock(&gClientInfo[nSID].sLock);

    return nSpeakerCanal;
}

void AVClientInfo_SetVideoQuality(int nSID, int level)
{
    AV_Client_Info *p = NULL;

    if(nSID < 0 || nSID >= MAX_CLIENT_NUMBER)
        return;

    p = &gClientInfo[nSID];

    //get writer lock
    int lock_ret = pthread_rwlock_rdlock(&gClientInfo[nSID].sLock);
	if(lock_ret){
		printf("Acquire nSID[%d] rdlock error ret[%d]\n", nSID, lock_ret);
        return;
    }

    if(p->nVideoQuality != level)
        p->waitKeyFrame = 1;
    p->nVideoQuality = level;

    //release lock
	pthread_rwlock_unlock(&gClientInfo[nSID].sLock);
}

/******************************
 * Speaker Call Back Functions
 ******************************/
 int Speaker_Stop(int nSID)
{
    int nSpeakerAVCanal = 0;

    if(nSID < 0 || nSID >= MAX_CLIENT_NUMBER)
        return -1;

    nSpeakerAVCanal = AVClientInfo_GetSpeakerCanal(nSID);
    if(nSpeakerAVCanal >= 0){
        AVClientInfo_UnRegeditSpeaker(nSID);
        printf("Speaker_Stop : AVAPI2_ReleaseChannelForReceive nSpeakerAVCanal[%d]\n", nSpeakerAVCanal);
        AVAPI2_ReleaseChannelForReceive(nSpeakerAVCanal);
    }

    return 0;
}

int Speaker_CanalStatusCB(int nAVCanal, int nError, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    int nSID = 0;

	printf("Speaker_CanalStatusCB nAVCanal[%d] nSID[%d] error with error code [%d].\n", nAVCanal, nSID, nError);
    if(pUserData != NULL)
        printf("Speaker_CanalStatusCB : pUserData[%s]\n", (char*)pUserData);
	PrintErrHandling(nError);

    if(nError == AV_ER_SESSION_CLOSE_BY_REMOTE ||
        nError == AV_ER_REMOTE_TIMEOUT_DISCONNECT ||
        nError == AV_ER_IOTC_SESSION_CLOSED ||
        nError == AV_ER_CLIENT_EXIT ||
        nError == AV_ER_INVALID_SID){
        nSID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
        Speaker_Stop(nSID);
    }
    else if(nError == AV_ER_MEM_INSUFF){
        printf("Speaker_CanalStatusCB : AV_ER_MEM_INSUFF\n");
    }
    else if(nError == IOTC_ER_CH_NOT_ON){
        printf("Speaker_CanalStatusCB : IOTC_ER_CH_NOT_ON nChannelID[%d]\n", nChannelID);
    }

	return 0;
}

int Speaker_ClientStatusCB(int nStatus, int nError, int nAVCanal, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    int nSID = 0;
    printf("Speaker_ClientStatusCB : nStatus[%d] nError[%d] nAVCanal[%d] nChannelID[%u]\n", nStatus, nError, nAVCanal, nChannelID);
    if(pUserData != NULL)
        printf("Speaker_ClientStatusCB : pUserData[%s]\n", (char*)pUserData);

    switch(nStatus){
        case AVAPI2_CLIENT_CONNECT_UID_ST_START :
            printf("Speaker_ClientStatusCB : Start connect to device\n");
            break;
        case AVAPI2_CLIENT_CONNECT_UID_ST_CONNECTING :
            printf("Speaker_ClientStatusCB : Connecting to device\n");
            break;
        case AVAPI2_CLIENT_CONNECT_UID_ST_CONNECTED :
            printf("Speaker_ClientStatusCB : The connection is established\n");
            break;
        case AVAPI2_CLIENT_STATUS_LOGINED :
            printf("Speaker_ClientStatusCB : Login to speaker server success\n");
            break;

        //Error Handle
        case AVAPI2_CLIENT_STATUS_LOGIN_FAILED :
        {
            printf("Speaker_ClientStatusCB : Login to server failed\n");
            if(nError == AV_ER_WRONG_VIEWACCorPWD){
                printf("Speaker_ClientStatusCB : AV_ER_WRONG_VIEWACCorPWD\n");
            }
        }
        break;
        case AVAPI2_CLIENT_STATUS_LOGIN_TIMEOUT :
        {
            printf("Speaker_ClientStatusCB : Login to speaker server timeout\n");
            nSID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
            Speaker_Stop(nSID);
        }
        break;
        case AVAPI2_CLIENT_STATUS_CLEAN_BUFFER_TIMEOUT :
        {
            printf("Speaker_ClientStatusCB : Clean buffer timeout\n");
            nSID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
            Speaker_Stop(nSID);
        }
        break;
        case AVAPI2_CLIENT_STATUS_RECV_FRAME_BLOCK :
        {
            printf("Speaker_ClientStatusCB : Took too long to handle received video frame in callback function , block %d ms\n", nError);
        }
        break;
        case AVAPI2_CLIENT_STATUS_RECV_AUDIO_BLOCK :
        {
            printf("Speaker_ClientStatusCB : Took too long to handle received audio data in callback function , block %d ms\n", nError);
        }
        break;
        case AVAPI2_CLIENT_CONNECT_UID_ST_FAILED :
        {
            printf("Speaker_ClientStatusCB : Connects to device failed\n");
        }
        break;
        default:
            break;
    }

    return 0;
}

int Speaker_IOCtrlRecvCB(int nAVCanal, unsigned int nIoCtrlType, unsigned char *pIoCtrlBuf, unsigned int nIoCtrlBufLen, void* pUserData)
{
    printf("Speaker_IOCtrlRecvCB : nAVCanal[%d] nIoCtrlType[0x%x] nIoCtrlBufLen[%u]\n", nAVCanal, nIoCtrlType, nIoCtrlBufLen);
    if(pUserData != NULL)
        printf("Speaker_IOCtrlRecvCB : pUserData[%s]\n", (char*)pUserData);
    return 0;
}

int Speaker_AudioRecvCB(int nAVCanal, int nError, char *pFrameData, int nFrameSize, char* pFrameInfo, int nFrmNo, void* pUserData)
{
    printf("Speaker_AudioRecvCB : nAVCanal[%d] nError[%d] nFrameSize[%d] nFrmNo[%d]\n", nAVCanal, nError, nFrameSize, nFrmNo);
    if(pUserData != NULL)
        printf("Speaker_AudioRecvCB : pUserData[%s]\n", (char*)pUserData);
    return 0;
}

int Speaker_Start(int nSID, int nSpeakerChannel)
{
    int nSpeakerAVCanal = 0;

    if(nSID < 0 || nSID >= MAX_CLIENT_NUMBER)
        return -1;

    AVClientInfo_RegeditSpeaker(nSID, -1, nSpeakerChannel);

    nSpeakerAVCanal = AVAPI2_CreateChannelForReceive(nSID, nSpeakerChannel, 0, 10, Speaker_ClientStatusCB, Speaker_CanalStatusCB, Speaker_IOCtrlRecvCB, NULL, Speaker_AudioRecvCB);
    if(nSpeakerAVCanal < 0){
        printf("AVAPI2_CreateChannelForReceive nSID[%d] nSpeakerChannel[%d] Error[%d]\n", nSID, nSpeakerChannel, nSpeakerAVCanal);
        AVClientInfo_UnRegeditSpeaker(nSID);
        return -1;
    }

    printf("AVAPI2_CreateChannelForReceive nSpeakerAVCanal[%d]\n", nSpeakerAVCanal);
    AVClientInfo_RegeditSpeaker(nSID, nSpeakerAVCanal, nSpeakerChannel);

    return 0;
}

/******************************
 * Streamout Functions
 * Streamout_AudioThread - Send audio data to all client
 * Streamout_VideoThread - Send video data to all client
 ******************************/
void Streamout_StatisticalPrint(int nAVCanal)
{
    int nRet = 0, i = 0;
    StatisticalDataSlot statisticalDataSlot;
    StatisticalClientDataSlot statisticalClientDataSlot;
    unsigned int serSendVBitrate = 0, serSendVFPS = 0, serSendABitrate = 0, serSendAFPS = 0, serDataVBitrate = 0, serDataABitrate = 0;
    unsigned int cliRecvVBitrate = 0, cliRecvVFPS = 0, cliRecvABitrate = 0, cliRecvAFPS = 0, cliUserVBitrate = 0, cliUserABitrate = 0;

    if(nAVCanal < 0)
        return;

    nRet = AVAPI2_GetStatisticalData(nAVCanal, &statisticalDataSlot);
    if(nRet < 0)
        return;

    printf("==========================================================================================\n");
    printf("Streamout_StatisticalPrint : nAVCanal[%d]\n", nAVCanal);
    //printf("[%d] StatisticalDataSlot version[0x%x] count[%d] index[%d] dataSize[%d]\n", gClientInfo[index].nAVCanal, statisticalDataSlot.uVersion, statisticalDataSlot.usCount, statisticalDataSlot.usIndex, statisticalDataSlot.uDataSize);
    //printf("******************************\n");
    for (i = 0 ; i < statisticalDataSlot.usCount ; i++){
        //printf("Data[%d] timeStamp[%u]\n", i, statisticalDataSlot.m_Data[i].uTimeStamp);
        //printf("vDataByte[%u] vSendByte[%u] vDropByte[%u] vResendByte[%u] vFPS[%u]\n", statisticalDataSlot.m_Data[i].uVDataByte, statisticalDataSlot.m_Data[i].uVSendByte, statisticalDataSlot.m_Data[i].uVDropByte, statisticalDataSlot.m_Data[i].uVResendByte, statisticalDataSlot.m_Data[i].usVFPS);
        //printf("aDataByte[%u] aSendByte[%u] aDropByte[%u] aResendByte[%u] aFPS[%u]\n", statisticalDataSlot.m_Data[i].uADataByte, statisticalDataSlot.m_Data[i].uASendByte, statisticalDataSlot.m_Data[i].uADropByte, statisticalDataSlot.m_Data[i].uAResendByte, statisticalDataSlot.m_Data[i].usAFPS);
        //printf("******************************\n");

        if(i == statisticalDataSlot.usIndex)
            continue;
        serDataVBitrate += statisticalDataSlot.m_Data[i].uVDataByte;
        serSendVBitrate += statisticalDataSlot.m_Data[i].uVSendByte;
        serSendVFPS += statisticalDataSlot.m_Data[i].usVFPS;
        serDataABitrate += statisticalDataSlot.m_Data[i].uADataByte;
        serSendABitrate += statisticalDataSlot.m_Data[i].uASendByte;
        serSendAFPS += statisticalDataSlot.m_Data[i].usAFPS;
    }
    serDataVBitrate = serDataVBitrate/(statisticalDataSlot.usCount-1);
    serSendVBitrate = serSendVBitrate/(statisticalDataSlot.usCount-1);
    serSendVFPS = serSendVFPS/(statisticalDataSlot.usCount-1);
    serDataABitrate = serDataABitrate/(statisticalDataSlot.usCount-1);
    serSendABitrate = serSendABitrate/(statisticalDataSlot.usCount-1);
    serSendAFPS = serSendAFPS/(statisticalDataSlot.usCount-1);
    printf("Server Send : Video DataBitrate[%u Kb] SendBitrate[%u Kb] FPS[%u] Audio DataBitrate[%u Kb] SendBitrate[%u Kb] FPS[%u]\n", serDataVBitrate/128, serSendVBitrate/128, serSendVFPS, serDataABitrate/128, serSendABitrate/128, serSendAFPS);

    nRet = AVAPI2_GetStatisticalClientData(nAVCanal, &statisticalClientDataSlot);
    if(nRet < 0)
        return;

    //printf("==============================\n");
    //printf("[%d] StatisticalClientDataSlot version[0x%x] count[%d] index[%d] dataSize[%d]\n", gClientInfo[index].nAVCanal, statisticalClientDataSlot.uVersion, statisticalClientDataSlot.usCount, statisticalClientDataSlot.usIndex, statisticalClientDataSlot.uDataSize);
    //printf("******************************\n");
    for (i = 0 ; i < statisticalClientDataSlot.usCount ; i++){
        //printf("Data[%d] timeStamp[%u]\n", i, statisticalClientDataSlot.m_Data[i].uTimeStamp);
        //printf("vRecvByte[%u] vUserByte[%u] vDropByte[%u] vResendByte[%u] vResendReqCnt[%u] vFPS[%u]\n", statisticalClientDataSlot.m_Data[i].uVRecvByte, statisticalClientDataSlot.m_Data[i].uVUserByte, statisticalClientDataSlot.m_Data[i].uVDropByte, statisticalClientDataSlot.m_Data[i].uVResendByte, statisticalClientDataSlot.m_Data[i].uVResendReqCnt, statisticalClientDataSlot.m_Data[i].usVFPS);
        //printf("aRecvByte[%u] aUserByte[%u] aDropCnt [%u] aResendByte[%u] aResendReqCnt[%u] aFPS[%u]\n", statisticalClientDataSlot.m_Data[i].uARecvByte, statisticalClientDataSlot.m_Data[i].uAUserByte, statisticalClientDataSlot.m_Data[i].uADropCnt , statisticalClientDataSlot.m_Data[i].uAResendByte, statisticalClientDataSlot.m_Data[i].uAResendReqCnt, statisticalClientDataSlot.m_Data[i].usAFPS);
        //printf("******************************\n");
        if(i == statisticalClientDataSlot.usIndex)
            continue;

        cliRecvVBitrate += statisticalClientDataSlot.m_Data[i].uVRecvByte;
        cliUserVBitrate += statisticalClientDataSlot.m_Data[i].uVUserByte;
        cliRecvVFPS += statisticalClientDataSlot.m_Data[i].usVFPS;
        cliRecvABitrate += statisticalClientDataSlot.m_Data[i].uARecvByte;
        cliUserABitrate += statisticalClientDataSlot.m_Data[i].uAUserByte;
        cliRecvAFPS += statisticalClientDataSlot.m_Data[i].usAFPS;
    }
    cliRecvVBitrate = cliRecvVBitrate/(statisticalClientDataSlot.usCount-1);
    cliUserVBitrate = cliUserVBitrate/(statisticalClientDataSlot.usCount-1);
    cliRecvVFPS = cliRecvVFPS/(statisticalClientDataSlot.usCount-1);
    cliRecvABitrate = cliRecvABitrate/(statisticalClientDataSlot.usCount-1);
    cliUserABitrate = cliUserABitrate/(statisticalClientDataSlot.usCount-1);
    cliRecvAFPS = cliRecvAFPS/(statisticalClientDataSlot.usCount-1);
    printf("Client Recv : Video UserBitrate[%u Kb] RecvBitrate[%u Kb] FPS[%u] Audio UserBitrate[%u Kb] RecvBitrate[%u Kb] FPS[%u]\n", cliUserVBitrate/128, cliRecvVBitrate/128, cliRecvVFPS, cliUserABitrate/128, cliRecvABitrate/128, cliRecvAFPS);
}

void *Streamout_AudioThread(void *arg)
{
    FILE *fp = NULL;
    char buf[AUDIO_BUF_SIZE];
    int sleepTick = 1000000/AUDIO_FPS;
    int i = 0, nRet = 0, size = 0;
#ifdef HEX_AUDIO
    int fptr = 0;
#endif

    if(gAudioFilename[0]=='\0'){
        printf("[Audio] is DISABLED!!\n");
        pthread_exit(0);
    }

    //open audio file
    fp = fopen(gAudioFilename, "rb");
    if(fp == NULL){
        printf("Streamout_AudioThread: Audio File \'%s\' open error!!\n", gAudioFilename);
        printf("Streamout_AudioThread: exit\n");
        pthread_exit(0);
    }

    printf("Streamout_AudioThread Start\n");

    while(gProcessRun)
    {

#ifndef HEX_AUDIO
        size = fread(buf, 1, AUDIO_FRAME_SIZE, fp);
        if(size <= 0){
            //printf("rewind audio\n");
            rewind(fp);
            continue;
        }
#else
        size = (g_audio_hex_size / AUDIO_FRAME_SIZE) - 1;
        memcpy(buf, g_audio_hex + fptr * AUDIO_FRAME_SIZE, AUDIO_FRAME_SIZE);
        if(fptr >= size){
            //printf("rewind audio\n");
            fptr = 0;
            continue;
        }
        fptr++;
        /*
        for (int j = 0; j < AUDIO_BUF_SIZE; j++) {
            printf("%02x", buf[j] & 0xff);
        }
        */
#endif

        for(i = 0 ; i < MAX_CLIENT_NUMBER; i++)
        {
            //get reader lock
            int lock_ret = pthread_rwlock_rdlock(&gClientInfo[i].sLock);
            if(lock_ret)
                printf("Acquire SID[%d] rdlock error ret[%d]\n", i, lock_ret);
            if(gClientInfo[i].nAVCanal < 0 || gClientInfo[i].bEnableAudio == 0){
                //release reader lock
                lock_ret = pthread_rwlock_unlock(&gClientInfo[i].sLock);
                if(lock_ret)
                    printf("Release SID[%d] rwlock error ret[%d]\n", i, lock_ret);
                continue;
            }

            // send audio data to nAVCanal
            nRet = AVAPI2_SendAudioData(gClientInfo[i].nAVCanal, AUDIO_CODEC, (AUDIO_SAMPLE_8K << 2) | (AUDIO_DATABITS_16 << 1) | AUDIO_CHANNEL_MONO, buf, AUDIO_FRAME_SIZE);

            //release reader lock
            lock_ret = pthread_rwlock_unlock(&gClientInfo[i].sLock);
            if(lock_ret)
                printf("Acquire SID[%d] rdlock error ret[%d]\n", i, lock_ret);

            if(nRet == AV_ER_EXCEED_MAX_SIZE ||
                    nRet == AV_ER_SOCKET_QUEUE_FULL ||
                    nRet == AV_ER_MEM_INSUFF ||
                    nRet == AV_ER_BUFPARA_MAXSIZE_INSUFF ||
                    nRet == AV_ER_CLEANBUF_ALREADY_CALLED){
                //TODO : Handle Video Buffer Full
                continue;
            }
            else if(nRet < 0){
                printf("Streamout_AudioThread : SID[%d] nAVCanal[%d] AVAPI2_SendFrameData nRet[%d] UnRegedit\n", i, gClientInfo[i].nAVCanal, nRet);
                PrintErrHandling(nRet);
                AVClientInfo_UnRegeditAudio(i);
                continue;
            }
        }
        usleep(sleepTick);
    }

#ifndef HEX_AUDIO
    fclose(fp);
#endif

    printf("[Streamout_AudioThread] exit\n");

    pthread_exit(0);
}

const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

void *Streamout_VideoThread(void *arg)
{
    int fpsCnt = 0, i = 0, nRet = 0, size = 0, gop = FPS, videoQuality = 0;
    int sleepTick = 1000000/FPS;
    char buf[VIDEO_BUF_SIZE];
    struct timeval tv, tv2;

    char frames_file[128];
    char frames_info[128];
    int motion_input = 0;

    FILE *fp_bin;
    FILE *fp_info;

    char line[256];
    int frame_loc;
    char frame_type[2] = {0};
    char* read = NULL;

    if (strcmp(get_filename_ext(gVideoFilename), "multi") == 0) {
        printf("Input video file includes multiple frames.\n");
        sprintf(frames_file, "%s/frames.bin", gVideoFilename);
        sprintf(frames_info, "%s/frames.info", gVideoFilename);
        strcpy(gVideoFilename, frames_file);
        motion_input = 1;

        fp_info = fopen(frames_info, "rb");
        if(fp_info == NULL){
            printf("Streamout_VideoThread: Video Info \'%s\' open error!!\n", frames_info);
            printf("Streamout_VideoThread: exit\n");
            pthread_exit(0);
        }
        read = fgets(line, sizeof(line), fp_info);

        sscanf(line, "FPS %d\n", &FPS);
    }

    // open video file
    fp_bin = fopen(gVideoFilename, "rb");
    if(fp_bin == NULL){
        printf("Streamout_VideoThread: Video File \'%s\' open error!!\n", gVideoFilename);
        printf("Streamout_VideoThread: exit\n");
        pthread_exit(0);
    }

    if (motion_input == 0) {
        // input file only one I frame for test
        size = fread(buf, 1, VIDEO_BUF_SIZE, fp_bin);
        fclose(fp_bin);
        if(size <= 0){
            printf("Streamout_VideoThread: Video File \'%s\' read error!!\n", gVideoFilename);
            printf("Streamout_VideoThread: exit\n");
            pthread_exit(0);
        }
    }

    printf("Streamout_VideoThread: Start\n");
    gettimeofday(&tv, NULL);
    while(gProcessRun)
    {
        gettimeofday(&tv2, NULL);
        int timeDiff = (tv2.tv_sec-tv.tv_sec)*1000 + (tv2.tv_usec-tv.tv_usec)/1000;
        if(timeDiff > 10000){
            fpsCnt = 0;
            gettimeofday(&tv, NULL);
            for(i = 0 ; i < MAX_CLIENT_NUMBER; i++){
                Streamout_StatisticalPrint(gClientInfo[i].nAVCanal);
            }
        }

        if (motion_input == 1) {
            if((read = fgets(line, sizeof(line), fp_info)) == NULL) {
                rewind(fp_info);
                read = fgets(line, sizeof(line), fp_info);
            }
            if(strncmp (line, "FPS", 3) == 0) {
                read = fgets(line, sizeof(line), fp_info);
            }

            sscanf(line, "%c %d %d\n", frame_type, &frame_loc, &size);
        }

        for(i = 0 ; i < MAX_CLIENT_NUMBER; i++)
        {
            //get reader lock
            int lock_ret = pthread_rwlock_rdlock(&gClientInfo[i].sLock);
            if(lock_ret)
                printf("Acquire SID[%d] rdlock error ret[%d]\n", i, lock_ret);

            if(gClientInfo[i].nAVCanal < 0 || gClientInfo[i].bEnableVideo == 0){
                //release reader lock
                lock_ret = pthread_rwlock_unlock(&gClientInfo[i].sLock);
                if(lock_ret)
                    printf("Release SID[%d] rwlock error ret[%d]\n", i, lock_ret);
                continue;
            }

            videoQuality = 0;
            if(gCongestionControlMode == 3){
                if(gClientInfo[i].nVideoQuality > videoQuality)
                    videoQuality = gClientInfo[i].nVideoQuality;
            }

        if (motion_input == 0) {
            if(gCongestionControlMode == 3){
                nRet = AVAPI2_SendFrameData(gClientInfo[i].nAVCanal, MEDIA_CODEC_VIDEO_H264, (fpsCnt%gop == 0) ? IPC_FRAME_FLAG_IFRAME : IPC_FRAME_FLAG_PBFRAME, buf, (fpsCnt%gop == 0) ? VIDEO_BUF_SIZE/2 : size);
                //printf("Send Frame fpsCnt[%d] gop[%d] nRet[%d]\n", fpsCnt, gop, nRet);
            }
            else{
                // Send Video Frame to nAVCanal and know how many time it takes
                nRet = AVAPI2_SendFrameData(gClientInfo[i].nAVCanal, MEDIA_CODEC_VIDEO_H264, fpsCnt%GOP == 0 ? IPC_FRAME_FLAG_IFRAME : IPC_FRAME_FLAG_PBFRAME, buf, size);
            }
        } else {
            fseek(fp_bin, frame_loc * sizeof(char), SEEK_SET);
            nRet = fread(buf, size, 1, fp_bin);
            nRet = AVAPI2_SendFrameData(gClientInfo[i].nAVCanal, MEDIA_CODEC_VIDEO_H264, strcmp(frame_type, "I") == 0 ? IPC_FRAME_FLAG_IFRAME : IPC_FRAME_FLAG_PBFRAME, buf, size); 
            fseek(fp_bin, 0, SEEK_SET);           
        }
            //release reader lock
            lock_ret = pthread_rwlock_unlock(&gClientInfo[i].sLock);
            if(lock_ret)
                printf("Release SID[%d] rwlock error ret[%d]\n", i, lock_ret);

            if(nRet == AV_ER_EXCEED_MAX_SIZE ||
                nRet == AV_ER_SOCKET_QUEUE_FULL ||
                nRet == AV_ER_MEM_INSUFF ||
                nRet == AV_ER_WAIT_KEY_FRAME ||
                nRet == AV_ER_CLEANBUF_ALREADY_CALLED){
                //TODO : Handle Video Buffer Full
                continue;
            }
            else if(nRet < 0){
                printf("Streamout_VideoThread : SID[%d] nAVCanal[%d] AVAPI2_SendFrameData nRet[%d] UnRegedit\n", i, gClientInfo[i].nAVCanal, nRet);
                PrintErrHandling(nRet);
                AVClientInfo_UnRegeditVideo(i);
                continue;
            }
        }

        if(gCongestionControlMode == 3){
            if(videoQuality == AV_DASA_LEVEL_QUALITY_HIGH){
                gop = FPS;
            }
            else if(videoQuality == AV_DASA_LEVEL_QUALITY_BTWHIGHNORMAL){
                gop = (FPS*4)/5;
            }
            else if(videoQuality == AV_DASA_LEVEL_QUALITY_NORMAL){
                gop = (FPS*3)/5;
            }
            else if(videoQuality == AV_DASA_LEVEL_QUALITY_BTWNORMALLOW){
                gop = (FPS*2)/5;
            }
            else if(videoQuality == AV_DASA_LEVEL_QUALITY_LOW){
                gop = FPS/5;
            }
            sleepTick = 1000000/gop;
        }
        fpsCnt++;
        usleep(sleepTick);
    }

    pthread_exit(0);
}

void Streamout_CreateThread()
{
    int nRet = 0;
    pthread_t ThreadVideoFrameData_ID;
    pthread_t ThreadAudioFrameData_ID;

    if((nRet = pthread_create(&ThreadVideoFrameData_ID, NULL, &Streamout_VideoThread, NULL))){
        printf("pthread_create Streamout_VideoThread nRet[%d]\n", nRet);
        exit(-1);
    }
    pthread_detach(ThreadVideoFrameData_ID);

    if((nRet = pthread_create(&ThreadAudioFrameData_ID, NULL, &Streamout_AudioThread, NULL))){
        printf("pthread_create Streamout_AudioThread nRet[%d]\n", nRet);
        exit(-1);
    }
    pthread_detach(ThreadAudioFrameData_ID);
}

#ifdef ENABLE_EMULATER
int gCacheSize = 0;
char gIFrameCache[VIDEO_BUF_SIZE] = {0};

int Streamout_SetCacheIFrame(char* buf, int size)
{
    gCacheSize = size;
    memcpy(gIFrameCache, buf, size);

    return 0;
}

int Streamout_GetCacheIFrame(char* buf)
{
    if(gCacheSize == 0)
        return -1;

    buf = gIFrameCache;

    return gCacheSize;
}

int Streamout_SendVideoFunc(int streamID, int timestamp, char* buf, int size, int frameType)
{
    //printf("Video streamID[%d] timestamp[%d] frameType[%s] size[%d]\n", streamID, timestamp, frameType == IPC_FRAME_FLAG_IFRAME ? "I" : "P", size);
    int i = 0, videoQuality = 0, waitKeyFrame = 0, nRet = 0, videoSize = size;
    char *videoBuf = buf;

    if(streamID == AV_DASA_LEVEL_QUALITY_LOW && frameType == IPC_FRAME_FLAG_IFRAME){
        Streamout_SetCacheIFrame(buf, size);
    }

    for(i = 0 ; i < MAX_CLIENT_NUMBER; i++)
    {
        //get reader lock
        int lock_ret = pthread_rwlock_rdlock(&gClientInfo[i].sLock);
        if(lock_ret)
            printf("Acquire SID[%d] rdlock error ret[%d]\n", i, lock_ret);

        if(gClientInfo[i].nAVCanal < 0 || gClientInfo[i].bEnableVideo == 0){
            //release reader lock
            lock_ret = pthread_rwlock_unlock(&gClientInfo[i].sLock);
            if(lock_ret)
                printf("Release SID[%d] rwlock error ret[%d]\n", i, lock_ret);
            continue;
        }

        //Handle Multi-Stream & DASA
        videoQuality = 0;
        if(gCongestionControlMode == 3)
            videoQuality = gClientInfo[i].nVideoQuality;

        if(videoQuality != streamID){
            //release reader lock
            lock_ret = pthread_rwlock_unlock(&gClientInfo[i].sLock);
            if(lock_ret)
                printf("Release SID[%d] rwlock error ret[%d]\n", i, lock_ret);
            continue;
        }

        //Handle wait key frame
        waitKeyFrame = gClientInfo[i].waitKeyFrame;
        if(waitKeyFrame){
            if(frameType == IPC_FRAME_FLAG_IFRAME){
               gClientInfo[i].waitKeyFrame = 0;
               waitKeyFrame = 0;
            }
            else{
                if(gClientInfo[i].bSendCache){
                    videoSize = Streamout_GetCacheIFrame(videoBuf);
                    printf("Streamout_GetCacheIFrame videoSize[%d]\n", videoSize);
                    if(videoSize < 0){
                        //release reader lock
                        lock_ret = pthread_rwlock_unlock(&gClientInfo[i].sLock);
                        if(lock_ret)
                            printf("Release SID[%d] rwlock error ret[%d]\n", i, lock_ret);
                        continue;
                    }
                    gClientInfo[i].bSendCache = 0;
                }
                else{
                    //release reader lock
                    lock_ret = pthread_rwlock_unlock(&gClientInfo[i].sLock);
                    if(lock_ret)
                        printf("Release SID[%d] rwlock error ret[%d]\n", i, lock_ret);
                    continue;
                }
            }
        }

        nRet = AVAPI2_SendFrameData(gClientInfo[i].nAVCanal, MEDIA_CODEC_VIDEO_H264, frameType, videoBuf, videoSize); 

        //release reader lock
        lock_ret = pthread_rwlock_unlock(&gClientInfo[i].sLock);
        if(lock_ret)
            printf("Release SID[%d] rwlock error ret[%d]\n", i, lock_ret);

        if(nRet == AV_ER_EXCEED_MAX_SIZE ||
            nRet == AV_ER_SOCKET_QUEUE_FULL ||
            nRet == AV_ER_MEM_INSUFF ||
            nRet == AV_ER_WAIT_KEY_FRAME ||
            nRet == AV_ER_CLEANBUF_ALREADY_CALLED){
            // Handle Video Buffer Full
            gClientInfo[i].waitKeyFrame = 1;
            continue;
        }
        else if(nRet < 0){
            printf("Streamout_VideoThread : SID[%d] nAVCanal[%d] AVAPI2_SendFrameData nRet[%d] UnRegedit\n", i, gClientInfo[i].nAVCanal, nRet);
            PrintErrHandling(nRet);
            AVClientInfo_UnRegeditVideo(i);
            continue;
        }
    }

    return 0;
}

int Streamout_SendAudioFunc(int timestamp, char* buf, int size)
{
    //printf("Audio timestamp[%d] size[%d]\n", timestamp, size);
    int i = 0, nRet = 0;

    for(i = 0 ; i < MAX_CLIENT_NUMBER; i++)
    {
        //get reader lock
        int lock_ret = pthread_rwlock_rdlock(&gClientInfo[i].sLock);
        if(lock_ret)
            printf("Acquire SID[%d] rdlock error ret[%d]\n", i, lock_ret);
        if(gClientInfo[i].nAVCanal < 0 || gClientInfo[i].bEnableAudio == 0){
            //release reader lock
            lock_ret = pthread_rwlock_unlock(&gClientInfo[i].sLock);
            if(lock_ret)
                printf("Release SID[%d] rwlock error ret[%d]\n", i, lock_ret);
            continue;
        }

        // send audio data to nAVCanal
        nRet = AVAPI2_SendAudioData(gClientInfo[i].nAVCanal, 0x8C, (AUDIO_SAMPLE_8K << 2) | (AUDIO_DATABITS_16 << 1) | AUDIO_CHANNEL_MONO, buf, size);

        //release reader lock
        lock_ret = pthread_rwlock_unlock(&gClientInfo[i].sLock);
        if(lock_ret)
            printf("Acquire SID[%d] rdlock error ret[%d]\n", i, lock_ret);

        if(nRet == AV_ER_EXCEED_MAX_SIZE ||
                nRet == AV_ER_SOCKET_QUEUE_FULL ||
                nRet == AV_ER_MEM_INSUFF ||
                nRet == AV_ER_BUFPARA_MAXSIZE_INSUFF ||
                nRet == AV_ER_CLEANBUF_ALREADY_CALLED){
            //Handle Audio Buffer Full
            //Just Drop it
            continue;
        }
        else if(nRet < 0){
            printf("Streamout_AudioThread : SID[%d] nAVCanal[%d] AVAPI2_SendFrameData nRet[%d] UnRegedit\n", i, gClientInfo[i].nAVCanal, nRet);
            PrintErrHandling(nRet);
            AVClientInfo_UnRegeditAudio(i);
            continue;
        }
    }

    return 0;
}
#endif

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
    int nSID = 0, ret = 0;
    char* avCanalStr = NULL;

    //Handle Server Status Change
    printf("ServerStatusCB : nStatus[%d] nError[%d] nAVCanal[%d] nChannelID[%u]\n", nStatus, nError, nAVCanal, nChannelID);
    if(pUserData != NULL)
        printf("ServerStatusCB : pUserData[%s]\n", (char*)pUserData);

    switch(nStatus){
        case AVAPI2_SERVER_STATUS_START :
            printf("ServerStatusCB : Server Start Listen\n");
            break;
        case AVAPI2_SERVER_STATUS_LOGINED :
            printf("ServerStatusCB : Server Login to P2P Server ok\n");
            break;
        case AVAPI2_SERVER_STATUS_NEW_CANAL :
            printf("ServerStatusCB : New Canal Connection\n");
            //avServSetResendSize(nAVCanal, 128);
            AVAPI2_ServerSetIoCtrlBufSize(nAVCanal, 8);
            AVAPI2_ServerSetAudioPreBufSize(nAVCanal, 32);
            AVAPI2_ServerSetVideoPreBufSize(nAVCanal, 128);
            if(gCongestionControlMode == 0){
                AVAPI2_ServerSetCongestionCtrlMode(nAVCanal, AVAPI2_CONGESTION_CTRL_AUDIO_FIRST);
                printf("ServerStatusCB : Set nAVCanal [%d] CongestionCtrlMode to AVAPI2_CONGESTION_CTRL_AUDIO_FIRST\n", nAVCanal);
            }
            else if(gCongestionControlMode == 1){
                AVAPI2_ServerSetCongestionCtrlMode(nAVCanal, AVAPI2_CONGESTION_CTRL_VIDEO_FIRST);
                printf("ServerStatusCB : Set nAVCanal [%d] CongestionCtrlMode to AVAPI2_CONGESTION_CTRL_VIDEO_FIRST\n", nAVCanal);
            }
            else if(gCongestionControlMode == 2){
                AVAPI2_ServerSetCongestionCtrlMode(nAVCanal, AVAPI2_CONGESTION_CTRL_PLAYBACK);
                printf("ServerStatusCB : Set nAVCanal [%d] CongestionCtrlMode to AVAPI2_CONGESTION_CTRL_PLAYBACK\n", nAVCanal);
            }
            else{
                int nCleanBufferCondition = 5, nCleanBufferRatio = 70, nAdjustmentKeepTime = 5, nIncreaseQualityCond = 10, nDecreaseRatio = 50;
                AVAPI2_ServerSetVideoPreBufSize(nAVCanal, 1024);
                avServSetResendSize(nAVCanal, 1024*4);
                ret = AVAPI2_ServerSetDASAEnable(nAVCanal, AVAPI2_CONGESTION_CTRL_DASA, nCleanBufferCondition, nCleanBufferRatio, nAdjustmentKeepTime, nIncreaseQualityCond, nDecreaseRatio, AV_DASA_LEVEL_QUALITY_LOW);

                nSID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
                AVClientInfo_SetVideoQuality(nSID, AV_DASA_LEVEL_QUALITY_LOW);

                printf("ServerStatusCB : Set nAVCanal [%d] CongestionCtrlMode to AVAPI2_CONGESTION_CTRL_DASA ret[%d]\n", nAVCanal, ret);
            }
            avCanalStr = (char*)malloc(16);
            sprintf(avCanalStr, "avCanal:%d", nAVCanal);
            AVAPI2_RegUserData(nAVCanal, avCanalStr);
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
        case AVAPI2_SERVER_STATUS_DASA_LEVEL_CHANGE :
            printf("ServerStatusCB : Dynamic Adaptive Streaming over AVAPI Change Level to [%d]\n", nError);
            nSID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
            AVClientInfo_SetVideoQuality(nSID, nError);
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
            //Stop Speaker and UnRegedit
            nSID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
            Speaker_Stop(nSID);
            AVClientInfo_UnRegeditAudio(nSID);
            AVClientInfo_UnRegeditVideo(nSID);

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
    if(pUserData != NULL)
        printf("Server_CanalStatusCB : pUserData[%s]\n", (char*)pUserData);
    PrintErrHandling(nError);

    if(nError == AV_ER_SESSION_CLOSE_BY_REMOTE ||
        nError == AV_ER_REMOTE_TIMEOUT_DISCONNECT ||
        nError == AV_ER_IOTC_SESSION_CLOSED ||
        nError == AV_ER_SERVER_EXIT ||
        nError == AV_ER_INVALID_SID){
        //Stop Speaker and UnRegedit
        nSID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
        Speaker_Stop(nSID);
        AVClientInfo_UnRegeditAudio(nSID);
        AVClientInfo_UnRegeditVideo(nSID);

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
//Video Streaming : IOTYPE_USER_IPCAM_START, IOTYPE_USER_IPCAM_STOP
//Audio Streaming : IOTYPE_USER_IPCAM_AUDIOSTART, IOTYPE_USER_IPCAM_AUDIOSTOP
//Speaker Streaming : IOTYPE_USER_IPCAM_SPEAKERSTART, IOTYPE_USER_IPCAM_SPEAKERSTOP
void Server_Handle_IOCTRL_Cmd(int SID, int nAVCanal, unsigned char *buf, unsigned int type)
{
    printf("Handle CMD: ");
    switch(type)
    {
        case IOTYPE_USER_IPCAM_START:
        {
            SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)buf;
            printf("nAVCanal[%d] IOTYPE_USER_IPCAM_START CH[%d]\n", nAVCanal, p->channel);
            AVClientInfo_RegeditVideo(SID, nAVCanal);
            if(gCongestionControlMode == 3){
                AVAPI2_ServerSetDASAReset(nAVCanal, AV_DASA_LEVEL_QUALITY_LOW);
                AVClientInfo_SetVideoQuality(SID, AV_DASA_LEVEL_QUALITY_LOW);
            }
        }
        break;
        case IOTYPE_USER_IPCAM_STOP:
        {
            SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)buf;
            printf("nAVCanal[%d] IOTYPE_USER_IPCAM_STOP CH[%d]\n", nAVCanal, p->channel);
            AVClientInfo_UnRegeditVideo(SID);
        }
        break;
        case IOTYPE_USER_IPCAM_AUDIOSTART:
        {
            SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)buf;
            printf("nAVCanal[%d] IOTYPE_USER_IPCAM_AUDIOSTART CH[%d]\n", nAVCanal, p->channel);
            AVClientInfo_RegeditAudio(SID, nAVCanal);
        }
        break;
        case IOTYPE_USER_IPCAM_AUDIOSTOP:
        {
            SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)buf;
            printf("nAVCanal[%d] IOTYPE_USER_IPCAM_AUDIOSTOP CH[%d]\n", nAVCanal, p->channel);
            AVClientInfo_UnRegeditAudio(SID);
        }
        break;
        case IOTYPE_USER_IPCAM_SPEAKERSTART:
        {
            SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)buf;
            printf("nAVCanal[%d] IOTYPE_USER_IPCAM_SPEAKERSTART CH[%d]\n", nAVCanal, p->channel);
            Speaker_Start(SID, p->channel);
        }
        break;
        case IOTYPE_USER_IPCAM_SPEAKERSTOP:
        {
            printf("nAVCanal[%d] IOTYPE_USER_IPCAM_SPEAKERSTOP\n", nAVCanal);
            Speaker_Stop(SID);
        }
        break;
        default:
        printf("nAVCanal %d: non-handle type[%X]\n", nAVCanal, type);
        break;
	}
}

int Server_IOCtrlRecvCB(int nAVCanal, unsigned int nIoCtrlType, unsigned char *pIoCtrlBuf, unsigned int nIoCtrlBufLen, void* pUserData)
{
    printf("IOCtrlRecvCB : nAVCanal[%d] nIoCtrlType[0x%x] nIoCtrlBufLen[%u]\n", nAVCanal, nIoCtrlType, nIoCtrlBufLen);
    if(pUserData != NULL)
        printf("IOCtrlRecvCB : pUserData[%s]\n", (char*)pUserData);

    Server_Handle_IOCTRL_Cmd(AVAPI2_GetSessionIDByAVCanal(nAVCanal), nAVCanal, pIoCtrlBuf, nIoCtrlType);

    return 0;
}

int main(int argc, char *argv[])
{
    int nRet = 0;
    int nServerInitChannel = 0;
    int nUDPPort = 0;
    char szVersion[64] = {0};

#ifdef ENABLE_EMULATER
    //Get parameter of testing
    if(argc < 2){
        printf("Usage: ./AVAPIs2_Server [UID]\n");
        return -1;
    }
    strcpy(gUID, argv[1]);
    gCongestionControlMode = 3;
#else
    //Get parameter of testing
    if(argc < 3 || argc > 5){
        printf("Usage: ./AVAPIs2_Server [UID] [VideoFile] [AudioFile] [Congestion Control Mode : 0 for Audio First, 1 for Video First, 2 For Playback, 3 For DASA]\n");
        return -1;
    }

    strcpy(gUID, argv[1]);
    strcpy(gVideoFilename, argv[2]);
    memset(gAudioFilename, 0, sizeof(gAudioFilename));
    if(argc >= 4)
        strcpy(gAudioFilename, argv[3]);
    if(argc >= 5){
        gCongestionControlMode = atoi(argv[4]);
        if(gCongestionControlMode < 0 || gCongestionControlMode > 3){
            printf("Usage: ./AVAPIs_Server [UID] [VideoFile] [AudioFile] [Congestion Control Mode : 0 for Audio First, 1 for Video First, 2 For Playback, 3 For DASA]\n");
            return -1;
        }
    }
#endif

    //Initial AV Client Info Structure
    AVClientInfo_Initialize();

    //Create Thread to read video/audio stream from files
#ifdef ENABLE_EMULATER
    nRet = Emulater_Initialize(EMULATER_MODE_MULTISTREAM, 0, Streamout_SendVideoFunc, Streamout_SendAudioFunc);
    if(nRet < 0){
        printf("Emulater_Initialize nRet[%d]\n", nRet);
        return -1;
    }
#else
    Streamout_CreateThread();
#endif

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
        sleep(15);
#if 0
        //Print statistical data
        int index = 0, i = 0;
        StatisticalDataSlot statisticalDataSlot;
        StatisticalClientDataSlot statisticalClientDataSlot;
        for(index = 0 ; index < MAX_CLIENT_NUMBER ; index++){
            if(gClientInfo[index].nAVCanal >= 0){
                AVAPI2_GetStatisticalData(gClientInfo[index].nAVCanal, &statisticalDataSlot);
                printf("==============================\n");
                printf("[%d] StatisticalDataSlot version[0x%x] count[%d] index[%d] dataSize[%d]\n", gClientInfo[index].nAVCanal, statisticalDataSlot.uVersion, statisticalDataSlot.usCount, statisticalDataSlot.usIndex, statisticalDataSlot.uDataSize);
                printf("******************************\n");
                for (i = 0 ; i < statisticalDataSlot.usCount ; i++){
                    printf("Data[%d] timeStamp[%u]\n", i, statisticalDataSlot.m_Data[i].uTimeStamp);
                    printf("vDataByte[%u] vSendByte[%u] vDropByte[%u] vResendByte[%u] vFPS[%u]\n", statisticalDataSlot.m_Data[i].uVDataByte, statisticalDataSlot.m_Data[i].uVSendByte, statisticalDataSlot.m_Data[i].uVDropByte, statisticalDataSlot.m_Data[i].uVResendByte, statisticalDataSlot.m_Data[i].usVFPS);
                    printf("aDataByte[%u] aSendByte[%u] aDropByte[%u] aResendByte[%u] aFPS[%u]\n", statisticalDataSlot.m_Data[i].uADataByte, statisticalDataSlot.m_Data[i].uASendByte, statisticalDataSlot.m_Data[i].uADropByte, statisticalDataSlot.m_Data[i].uAResendByte, statisticalDataSlot.m_Data[i].usAFPS);
                    printf("******************************\n");
                }
                printf("==============================\n");

                AVAPI2_GetStatisticalClientData(gClientInfo[index].nAVCanal, &statisticalClientDataSlot);
                printf("==============================\n");
                printf("[%d] StatisticalClientDataSlot version[0x%x] count[%d] index[%d] dataSize[%d]\n", gClientInfo[index].nAVCanal, statisticalClientDataSlot.uVersion, statisticalClientDataSlot.usCount, statisticalClientDataSlot.usIndex, statisticalClientDataSlot.uDataSize);
                printf("******************************\n");
                for (i = 0 ; i < statisticalClientDataSlot.usCount ; i++){
                    printf("Data[%d] timeStamp[%u]\n", i, statisticalClientDataSlot.m_Data[i].uTimeStamp);
                    printf("vRecvByte[%u] vUserByte[%u] vDropByte[%u] vResendByte[%u] vResendReqCnt[%u] vFPS[%u]\n", statisticalClientDataSlot.m_Data[i].uVRecvByte, statisticalClientDataSlot.m_Data[i].uVUserByte, statisticalClientDataSlot.m_Data[i].uVDropByte, statisticalClientDataSlot.m_Data[i].uVResendByte, statisticalClientDataSlot.m_Data[i].uVResendReqCnt, statisticalClientDataSlot.m_Data[i].usVFPS);
                    printf("aRecvByte[%u] aUserByte[%u] aDropCnt [%u] aResendByte[%u] aResendReqCnt[%u] aFPS[%u]\n", statisticalClientDataSlot.m_Data[i].uARecvByte, statisticalClientDataSlot.m_Data[i].uAUserByte, statisticalClientDataSlot.m_Data[i].uADropCnt , statisticalClientDataSlot.m_Data[i].uAResendByte, statisticalClientDataSlot.m_Data[i].uAResendReqCnt, statisticalClientDataSlot.m_Data[i].usAFPS);
                    printf("******************************\n");
                }
                printf("==============================\n");
            }
        }
#endif
    }

    //AVAPI2 Server Stop
    AVAPI2_ServerStop();

    return 0;
}

