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
#define SERVTYPE_STREAM_SERVER  16
#define MAX_CLIENT_NUMBER       128
#define MAX_CH_NUMBER           16

/******************************
 * Structure
 ******************************/
typedef struct _AV_Client
{
    pthread_rwlock_t sLock;
    int avCanal;
    int pushAVCanal;
    int pushCh;
    unsigned char bEnablePush;
    unsigned char reserved[3];

    int v_fpsCnt, v_bps, v_err, v_TotalCnt;
    int a_fpsCnt, a_bps, a_err, a_TotalCnt;
    struct timeval v_tv1, v_tv2;
    struct timeval a_tv1, a_tv2;
    struct timeval g_tv1, g_tv2;
    int firstVideo, firstAudio;
    unsigned int audiotime, videotime;
}AV_PushClient;

/******************************
 * Global Variable
 * set AV server ID and password here
 ******************************/
char avID[] = "admin";
char avPass[] = "888888ii";
static AV_PushClient gClientInfo[MAX_CLIENT_NUMBER];
static char gUID[21] = {0};

/******************************
 * AV_PushClient Control Functions
 ******************************/
void InitAVPushClientInfo()
{
	int i = 0;

	for(i = 0 ; i < MAX_CLIENT_NUMBER ; i++){
		memset(&gClientInfo[i], 0, sizeof(AV_PushClient));
		gClientInfo[i].avCanal = -1;
        gClientInfo[i].pushAVCanal = -1;
		pthread_rwlock_init(&(gClientInfo[i].sLock), NULL);
	}
}

void DeInitAVPushClientInfo()
{
    int i = 0;

    for(i = 0 ; i < MAX_CLIENT_NUMBER ; i++){
        memset(&gClientInfo[i], 0, sizeof(AV_PushClient));
        gClientInfo[i].avCanal = -1;
        gClientInfo[i].pushAVCanal = -1;
        pthread_rwlock_destroy(&gClientInfo[i].sLock);
    }
}

void RegeditPushClient(int SID, int avCanal)
{
	AV_PushClient *p = &gClientInfo[SID];
	p->avCanal = avCanal;
}

void UnRegeditPushClient(int SID)
{
	AV_PushClient *p = &gClientInfo[SID];
    memset(p, 0, sizeof(AV_PushClient));
	p->avCanal = -1;
	p->pushAVCanal = -1;
}

/******************************
 * Push Streaming Servier Site Call Back Functions
 ******************************/
static int pushCanalStatusCB(int nAVCanal, int nError, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
	int nSID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
    int lock_ret = 0;

	if (nSID < 0) {
		printf("Invalid AV Canal.\n");
		return -1;
	}

	printf("pushCanalStatusCB nAVCanal[%d] Session [%d] error with error code [%d].\n", nAVCanal, nSID, nError);

    if(nError == AV_ER_SESSION_CLOSE_BY_REMOTE){
        //Do StopCanal
        AVAPI2_ReleaseChannelForReceive(nAVCanal);

        lock_ret = pthread_rwlock_wrlock(&gClientInfo[nSID].sLock);
        if(lock_ret)
            printf("Acquire SID %d rwlock error, ret = %d\n", nSID, lock_ret);
		gClientInfo[nSID].bEnablePush = 0;
        gClientInfo[nSID].pushAVCanal = -1;
		//release lock
		lock_ret = pthread_rwlock_unlock(&gClientInfo[nSID].sLock);
		if(lock_ret)
			printf("Release SID %d rwlock error, ret = %d\n", nSID, lock_ret);
    }
    else if(nError == AV_ER_REMOTE_TIMEOUT_DISCONNECT){
        //Do StopCanal
        AVAPI2_ReleaseChannelForReceive(nAVCanal);

        lock_ret = pthread_rwlock_wrlock(&gClientInfo[nSID].sLock);
        if(lock_ret)
            printf("Acquire SID %d rwlock error, ret = %d\n", nSID, lock_ret);
		gClientInfo[nSID].bEnablePush = 0;
        gClientInfo[nSID].pushAVCanal = -1;
		//release lock
		lock_ret = pthread_rwlock_unlock(&gClientInfo[nSID].sLock);
		if(lock_ret)
			printf("Release SID %d rwlock error, ret = %d\n", nSID, lock_ret);
    }

	return 0;
}

static int pushClientStatusCB(int nStatus, int nError, int nAVCanal, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    printf("pushClientStatusCB : nStatus[%d] nError[%d] nAVCanal[%d] nChannelID[%u]\n", nStatus, nError, nAVCanal, nChannelID);
    switch(nStatus){
        case AVAPI2_CLIENT_STATUS_LOGIN_FAILED :
            printf("pushClientStatusCB : Login to Server Failed\n");
            break;
        case AVAPI2_CLIENT_STATUS_LOGIN_TIMEOUT :
            printf("pushClientStatusCB : Login to Server Timeout\n");
            //Do StopCanal
            int nSID = 0, lock_ret = 0;
            nSID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
            AVAPI2_ReleaseChannelForReceive(nAVCanal);

            lock_ret = pthread_rwlock_wrlock(&gClientInfo[nSID].sLock);
            if(lock_ret)
                printf("Acquire SID %d rwlock error, ret = %d\n", nSID, lock_ret);
            gClientInfo[nSID].bEnablePush = 0;
            gClientInfo[nSID].pushAVCanal = -1;
            //release lock
            lock_ret = pthread_rwlock_unlock(&gClientInfo[nSID].sLock);
            if(lock_ret)
                printf("Release SID %d rwlock error, ret = %d\n", nSID, lock_ret);
            break;
        case AVAPI2_CLIENT_STATUS_LOGINED :
            printf("pushClientStatusCB : Login to Push Device ok, serverType = %x\n", nError);
            break;
        default:
            break;
    }

    return 0;
}

static int pushIOCtrlRecvCB(int nAVCanal, unsigned int nIoCtrlType, unsigned char *pIoCtrlBuf, unsigned int nIoCtrlBufLen, void* pUserData)
{
    printf("pushIOCtrlRecvCB : nAVCanal[%d] nIoCtrlType[0x%x] nIoCtrlBufLen[%u]\n", nAVCanal, nIoCtrlType, nIoCtrlBufLen);

    return 0;
}

static int pushVideoRecvCB(int nAVCanal, int nError, char *pFrameData, int nActualFrameSize, int nExpectedFrameSize, char* pFrameInfo, int nFrameInfoSize, int frmNo, void* pUserData)
{
    FRAMEINFO_t* frmInfo = NULL;
    int SID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);

    if(nError < 0){
        printf("Receive frame nError:%d\n", nError);
        gClientInfo[SID].v_err++;
        if(nError == AV_ER_LOSED_THIS_FRAME){
            printf("Video AV_ER_LOSED_THIS_FRAME frmNo[%d]\n", frmNo);
        }
        else{
            printf("Video receive nError[%d]\n", nError);
        }
        return 0;
    }

	if(pFrameData == NULL || nActualFrameSize < 0){
		printf("Invalid frame.\n");
		return -1;
	}

    if(gClientInfo[SID].firstVideo == 0){
        gettimeofday(&gClientInfo[SID].g_tv2, NULL);
        int taketime = (gClientInfo[SID].g_tv2.tv_sec-gClientInfo[SID].g_tv1.tv_sec)*1000 + (gClientInfo[SID].g_tv2.tv_usec-gClientInfo[SID].g_tv1.tv_usec)/1000;
        printf("First Video taketime[%d]\n", taketime);
        gClientInfo[SID].firstVideo = 1;
    }

    frmInfo = (FRAMEINFO_t*)pFrameInfo;
    gClientInfo[SID].videotime = frmInfo->timestamp;

    if(gClientInfo[SID].v_TotalCnt == 0)
        gettimeofday(&gClientInfo[SID].v_tv1, NULL);
    gClientInfo[SID].v_fpsCnt++;
    gClientInfo[SID].v_TotalCnt++;
    gClientInfo[SID].v_bps += nActualFrameSize;

    gettimeofday(&gClientInfo[SID].v_tv2, NULL);
    int msecDiff = msecDiff = (gClientInfo[SID].v_tv2.tv_sec-gClientInfo[SID].v_tv1.tv_sec)*1000 + (gClientInfo[SID].v_tv2.tv_usec-gClientInfo[SID].v_tv1.tv_usec)/1000;

    if(msecDiff > 10000){
        printf("[nAVCanal:%d] Video FPS=%d, err:%d, TotalCnt:%d, LastFrameSize:%d Byte, Codec:%d, Flag:%d, bps:%d Kbps V/A TimeDiff:%d\n", nAVCanal, gClientInfo[SID].v_fpsCnt/10, gClientInfo[SID].v_err, gClientInfo[SID].v_TotalCnt, nActualFrameSize, frmInfo->codec_id, frmInfo->flags, ((gClientInfo[SID].v_bps/1024)*8)/10, gClientInfo[SID].videotime-gClientInfo[SID].audiotime);
        gettimeofday(&gClientInfo[SID].v_tv1, NULL);
        gClientInfo[SID].v_fpsCnt = 0;
        gClientInfo[SID].v_bps = 0;
    }

	return 0;
}

static int pushAudioRecvCB(int nAVCanal, int nError, char *pFrameData, int nFrameSize, char* pFrameInfo, int frmNo, void* pUserData)
{
    FRAMEINFO_t* frmInfo = NULL;
    int SID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);

    if(nError < 0){
        printf("Receive audio nError:%d\n", nError);
        gClientInfo[SID].a_err++;
        if(nError == AV_ER_LOSED_THIS_FRAME){
            printf("Audio AV_ER_LOSED_THIS_FRAME frmNo[%d]\n", frmNo);
        }
        else{
            printf("Audio receive nError[%d]\n", nError);
        }
        return 0;
    }

	if(pFrameData == NULL || nFrameSize < 0){
		printf("Invalid frame.\n");
		return -1;
	}

    if(gClientInfo[SID].firstAudio == 0){
        gettimeofday(&gClientInfo[SID].g_tv2, NULL);
        int taketime = (gClientInfo[SID].g_tv2.tv_sec-gClientInfo[SID].g_tv1.tv_sec)*1000 + (gClientInfo[SID].g_tv2.tv_usec-gClientInfo[SID].g_tv1.tv_usec)/1000;
        printf("First Audio taketime[%d]\n", taketime);
        gClientInfo[SID].firstAudio = 1;
    }

    frmInfo = (FRAMEINFO_t*)pFrameInfo;
	//printf("Audio frame received via Canal #%d at time %u\n", nAVCanal, frmInfo->timestamp);
    gClientInfo[SID].audiotime = frmInfo->timestamp;

    if(gClientInfo[SID].a_TotalCnt == 0)
        gettimeofday(&gClientInfo[SID].a_tv1, NULL);
    gClientInfo[SID].a_fpsCnt++;
    gClientInfo[SID].a_TotalCnt++;
    gClientInfo[SID].a_bps += nFrameSize;

    gettimeofday(&gClientInfo[SID].a_tv2, NULL);
    int msecDiff = msecDiff = (gClientInfo[SID].a_tv2.tv_sec-gClientInfo[SID].a_tv1.tv_sec)*1000 + (gClientInfo[SID].a_tv2.tv_usec-gClientInfo[SID].a_tv1.tv_usec)/1000;

    if(msecDiff > 10000){
        printf("[nAVCanal:%d] Audio FPS=%d, err:%d, TotalCnt:%d, LastFrameSize:%d Byte, Codec:%d, Flag:%d, bps:%d Kbps\n", nAVCanal, gClientInfo[SID].a_fpsCnt/10, gClientInfo[SID].a_err, gClientInfo[SID].a_TotalCnt, nFrameSize, frmInfo->codec_id, frmInfo->flags, ((gClientInfo[SID].a_bps/1024)*8)/10);
        gettimeofday(&gClientInfo[SID].a_tv1, NULL);
        gClientInfo[SID].a_fpsCnt = 0;
        gClientInfo[SID].a_bps = 0;
    }

	return 0;
}

/******************************
 * Push Streaming Servier Site Functions
 * Start Push Streaming By Using "AVAPI2_CreateChannelForReceive"
 * Stop Push Streaming By Using  "AVAPI2_ReleaseChannelForReceive"
 ******************************/
static void Handle_IOCTRL_Cmd(int SID, int avCanal, unsigned char *buf, unsigned int type)
{
	printf("Handle CMD: ");
	switch(type)
	{
		case IOTYPE_USER_IPCAM_PUSH_STREAMING_START:
		{
			SMsgAVIoctrlPushStream *p = (SMsgAVIoctrlPushStream *)buf;
			printf("IOTYPE_USER_IPCAM_PUSH_STREAMING_START, ch:%d, deviceUID:%s, avCanal:%d\n\n", p->channel, p->szDeviceUID, avCanal);
			//get writer lock
			int lock_ret = pthread_rwlock_wrlock(&gClientInfo[SID].sLock);
			if(lock_ret)
				printf("Acquire SID %d rwlock error, ret = %d\n", SID, lock_ret);
			gClientInfo[SID].pushCh = p->channel;
			gClientInfo[SID].bEnablePush = 1;

            int ret = AVAPI2_CreateChannelForReceive(SID, gClientInfo[SID].pushCh, 1, 10, pushClientStatusCB, pushCanalStatusCB, pushIOCtrlRecvCB, pushVideoRecvCB, pushAudioRecvCB);
            if(ret < 0){
                printf("AVAPI2_CreateChannelForReceive gClientInfo[%d].pushCh[%d] Error ret[%d]\n", SID, gClientInfo[SID].pushCh, ret);
                gClientInfo[SID].bEnablePush = 0;
                gClientInfo[SID].pushAVCanal = -1;
            }
            else{
                gClientInfo[SID].pushAVCanal = ret;
                printf("AVAPI2_CreateChannelForReceive pushAVCanal[%d]\n", ret);
            }
			//release lock
			lock_ret = pthread_rwlock_unlock(&gClientInfo[SID].sLock);
			if(lock_ret)
				printf("Release SID %d rwlock error, ret = %d\n", SID, lock_ret);
			
			if(ret >= 0)
			{
				SMsgAVIoctrlAVStream ioMsg;
				ioMsg.channel = p->channel;
				printf("send IOTYPE_USER_IPCAM_PUSH_STREAMING_START_RESP avCanal[%d], pushAVCanal[%d]\n", avCanal, ret);
				if(AVAPI2_SendIOCtrl(avCanal, IOTYPE_USER_IPCAM_PUSH_STREAMING_START_RESP, (char *)&ioMsg, sizeof(SMsgAVIoctrlAVStream)) < 0)
					break;

			}
		}
		break;
		case IOTYPE_USER_IPCAM_PUSH_STREAMING_FINISH:
		{
			printf("IOTYPE_USER_IPCAM_PUSH_STREAMING_FINISH\n\n");
			SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)buf;
			p->channel = p->channel;
			printf("send IOTYPE_USER_IPCAM_PUSH_STREAMING_FINISH_RESP avCanal[%d]\n", avCanal);
			if(AVAPI2_SendIOCtrl(avCanal, IOTYPE_USER_IPCAM_PUSH_STREAMING_FINISH_RESP, (char *)p, sizeof(SMsgAVIoctrlAVStream)) < 0)
				printf("IOTYPE_USER_IPCAM_PUSH_STREAMING_FINISH_RESP error!\n");
			
			printf("AVAPI2_ReleaseChannelForReceive pushAVCanal(%d)\n", gClientInfo[SID].pushAVCanal);
			AVAPI2_ReleaseChannelForReceive(gClientInfo[SID].pushAVCanal);
			
			gClientInfo[SID].bEnablePush = 0;
            gClientInfo[SID].pushAVCanal = -1;
		}
		break;
		case IOTYPE_USER_IPCAM_PUSH_STREAMING_CLOSE:
		{
			
			SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)buf;
			printf("IOTYPE_USER_IPCAM_PUSH_STREAMING_CLOSE, channel = %d\n\n", p->channel);
			//get writer lock
			int lock_ret = pthread_rwlock_wrlock(&gClientInfo[SID].sLock);
			if(lock_ret)
				printf("Acquire SID %d rwlock error, ret = %d\n", SID, lock_ret);
          
			
			//release lock
			lock_ret = pthread_rwlock_unlock(&gClientInfo[SID].sLock);
			if(lock_ret)
				printf("Release SID %d rwlock error, ret = %d\n", SID, lock_ret);
			
			UnRegeditPushClient(SID);
			printf("AVAPI2_ServerStopCanal avCanal(%d)\n", avCanal);
			AVAPI2_ServerStopCanal(avCanal);
		}
		break;
		default:
		    printf("avCanal %d: non-handle type[%X]\n", avCanal, type);
		break;
	}
}

static int AuthCB(char *viewAcc,char *viewPwd)
{
	if(strcmp(viewAcc, avID) == 0 && strcmp(viewPwd, avPass) == 0)
		return 1;

	return 0;
}

static int CanalStatusCB(int nAVCanal, int nError, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    int SID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
    int pushAVCanal = 0;
    printf("CanalStatusCB : nAVCanal[%d] nError[%d] nChannelID[%u]\n", nAVCanal, nError, nChannelID);

    if(nError == AV_ER_SESSION_CLOSE_BY_REMOTE || nError == AV_ER_REMOTE_TIMEOUT_DISCONNECT || nError == AV_ER_CLIENT_NO_AVLOGIN){
        //get writer lock
        int lock_ret = pthread_rwlock_wrlock(&gClientInfo[SID].sLock);
        if(lock_ret)
            printf("Acquire SID %d rwlock error, ret = %d\n", SID, lock_ret);
        pushAVCanal = gClientInfo[SID].pushAVCanal;
        gClientInfo[SID].bEnablePush = 0;
        gClientInfo[SID].pushAVCanal = -1;
        //release lock
        lock_ret = pthread_rwlock_unlock(&gClientInfo[SID].sLock);
        if(lock_ret)
            printf("Release SID %d rwlock error, ret = %d\n", SID, lock_ret);

        //Do StopCanal
        printf("CanalStatusCB : Stop pushAVCanal[%d] nAVCanal[%d] SID[%d]\n", pushAVCanal, nAVCanal, SID);
        AVAPI2_ReleaseChannelForReceive(pushAVCanal);
        AVAPI2_ServerStopCanal(nAVCanal);
        UnRegeditPushClient(SID);
    }
    return 0;
}

int ServerStatusCB(int nStatus, int nError, int nAVCanal, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    int SID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
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
            RegeditPushClient(SID, nAVCanal);
            gettimeofday(&gClientInfo[SID].g_tv1, NULL);
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
            printf("ServerStatusCB : login failed\n");
            break;
        case AVAPI2_SERVER_STATUS_START_CANAL_FAILED :
            printf("ServerStatusCB : start canal failed\n");
            break;
        case AVAPI2_SERVER_STATUS_CREATE_SENDTASK_FAILED :
            printf("ServerStatusCB : create send task failed\n");
            break;
        case AVAPI2_SERVER_STATUS_CLEAN_BUFFER_TIMEOUT:
            printf("ServerStatusCB : Clean Buffer Timeout\n");
            break;
        default:
            break;
    }

    return 0;
}

int IOCtrlRecvCB(int nAVCanal, unsigned int nIoCtrlType, unsigned char *pIoCtrlBuf, unsigned int nIoCtrlBufLen, void* pUserData)
{
    int SID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
    printf("IOCtrlRecvCB : nAVCanal[%d] nIoCtrlType[0x%x] nIoCtrlBufLen[%u]\n", nAVCanal, nIoCtrlType, nIoCtrlBufLen);

    Handle_IOCTRL_Cmd(SID, nAVCanal, pIoCtrlBuf, nIoCtrlType);

    return 0;
}

int main(int argc, char *argv[])
{
    int ret = 0;

    if(argc < 2 || argc > 2){
        printf("Usage: %s [UID]\n", argv[0]);
        return -1;
    }

    strcpy(gUID, argv[1]);

    //AV Push Client Initial
    InitAVPushClientInfo();

    //AVAPI2 Initial
    ret = AVAPI2_SetCanalLimit(MAX_CLIENT_NUMBER, MAX_CH_NUMBER);
    if(ret < 0){
        printf("AVAPI2_SetCanalLimit ret=[%d]\n", ret);
        return -1;
    }

    //AVAPI2 Server Start
    ret = AVAPI2_ServerStart((char*)gUID, 0, SERVTYPE_STREAM_SERVER, AuthCB, ServerStatusCB, CanalStatusCB, IOCtrlRecvCB);
    if(ret < 0){
        printf("AVAPI2_ServerStart ret=[%d]\n", ret);
        return -1;
    }

    while(1){
        //Do anything you want
        sleep(10);
    }

    //AV Push Client De-Initial
    DeInitAVPushClientInfo();

    //AVAPI2 Server Stop
    AVAPI2_ServerStop();

    return 0;
}

