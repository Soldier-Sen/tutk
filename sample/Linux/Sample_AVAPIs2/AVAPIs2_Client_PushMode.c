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

typedef enum
{
    STATUS_SEND_PUSH_START			= 1<<0,
    STATUS_RECEIVE_PUSH_START_RESP	= 1<<1,
    STATUS_CLIENT_LOGIN_SUCCESS		= 1<<2,
    STATUS_READY_FOR_PUSH			= STATUS_SEND_PUSH_START | STATUS_RECEIVE_PUSH_START_RESP | STATUS_CLIENT_LOGIN_SUCCESS,
} PushStreamStatus;

typedef struct _PushClientStruct
{
	int avCanal;
	int iotcChannel;
	int pushStart;
	int pushAvCanal;
	int pushStartTime;
	int pushInterval;
}PushClientStruct;

static PushClientStruct gPushClient;

/******************************
 * Global Variable
 ******************************/
char *avID = "admin";
char gUID[32] = {0};
char gClientUID[32] = {0};
char gPassword[32] = {0};
static char gVideoFn[128];
static char gAudioFn[128];
int g_nCanal = -1;
int g_pushStreamingRun = 0;
struct timeval g_tv1, g_tv2;

/******************************
 * Push Streaming Functions
 ******************************/
//PCM
#define AUDIO_FRAME_SIZE    640
#define AUDIO_FPS           25
#define AUDIO_CODEC         0x8C

void *pushStreaming_AudioThread(void *arg)
{
    FILE *fp = NULL;
    char buf[AUDIO_FRAME_SIZE];
    int frameRate = AUDIO_FPS;
    int sleepTick = 1000000/frameRate;
    int ret = 0, size = 0, bSendFrameOut = 0, fpsCnt = 0, round = 0;
    struct timeval tv, tv2;

    //open audio file
    fp = fopen(gAudioFn, "rb");
    if(fp == NULL){
        printf("%s: Audio file \'%s\' open error!!\n", __FUNCTION__, gAudioFn);
        pthread_exit(0);
    }

    printf("%s: start\n", __FUNCTION__);

    while(g_pushStreamingRun)
    {
		if(STATUS_READY_FOR_PUSH != gPushClient.pushStart)
		{
			usleep(10000);
			continue;
		}
        size = fread(buf, 1, AUDIO_FRAME_SIZE, fp);
        if(size <= 0){
            rewind(fp);
            continue;
        }

        if(fpsCnt == 0)
            gettimeofday(&tv, NULL);
		
        bSendFrameOut = 0;
        // send audio data
        ret = AVAPI2_SendAudioData(gPushClient.pushAvCanal, AUDIO_CODEC, (AUDIO_SAMPLE_8K << 2) | (AUDIO_DATABITS_16 << 1) | AUDIO_CHANNEL_MONO, buf, size);
        if(ret == AV_ER_SESSION_CLOSE_BY_REMOTE){
            printf("%s: AV_ER_SESSION_CLOSE_BY_REMOTE exit\n", __FUNCTION__);
            pthread_exit(0);
        }
        else if(ret == AV_ER_REMOTE_TIMEOUT_DISCONNECT){
            printf("%s: AV_ER_REMOTE_TIMEOUT_DISCONNECT exit\n", __FUNCTION__);
            pthread_exit(0);
        }
        else if(ret == IOTC_ER_INVALID_SID){
            printf("%s: Session cant be used anymore\n", __FUNCTION__);
            continue;
        }
        else if(ret == AV_ER_CLEANBUF_ALREADY_CALLED){
            printf("%s: Wait Clean Audio Buffer Done\n", __FUNCTION__);
            continue;
        }
        else if(ret == AV_ER_EXCEED_MAX_SIZE){
            printf("%s: Exceed max size\n", __FUNCTION__);
            continue;
        }
        else if(ret == AV_ER_SOCKET_QUEUE_FULL){
            printf("%s: socket queue full\n", __FUNCTION__);
            continue;
        }
        else if(ret == AV_ER_CLIENT_NO_AVLOGIN){
            printf("%s: wait push server ready\n", __FUNCTION__);
            usleep(100000);
            continue;
        }
        else if(ret < 0){
            printf("%s: AVAPI2_SendAudioData error[%d]\n", __FUNCTION__, ret);
            continue;
        }
        bSendFrameOut = 1;

        if(bSendFrameOut){
            fpsCnt++;

            gettimeofday(&tv2, NULL);
            int timeDiff = (tv2.tv_sec-tv.tv_sec)*1000 + (tv2.tv_usec-tv.tv_usec)/1000;
            if(timeDiff > 10000){
                round++;
                printf("Audio FPS[%d] R[%d]\n", fpsCnt/10, round);
                fpsCnt = 0;
                gettimeofday(&tv, NULL);
            }
        }
        usleep(sleepTick);
    }

    fclose(fp);

    printf("%s: exit\n", __FUNCTION__);
    pthread_exit(0);
}

#define VIDEO_BUF_SIZE (128*1024)
#define VIDEO_FPS 30

void *pushStreaming_VideoThread(void *arg)
{
    unsigned int totalCnt = 0;
    int fpsCnt = 0, round = 0, ret = 0, size = 0, bSendFrameOut = 0;
    int frameRate = VIDEO_FPS;
    int sleepTick = 1000000/frameRate;
    long takeSec = 0, takeUSec = 0, sendFrameRoundTick = 0;
    char buf[VIDEO_BUF_SIZE];
    struct timeval tv, tv2, tStart, tEnd;

    // open video file
    FILE *fp = fopen(gVideoFn, "rb");
    if(fp == NULL){
        printf("%s: Video File \'%s\' open error!!\n", __FUNCTION__, gVideoFn);
        pthread_exit(0);
    }

    // input file only one I frame for test
    size = fread(buf, 1, VIDEO_BUF_SIZE, fp);
    fclose(fp);
    if(size <= 0){
        printf("%s: Video File \'%s\' read error!!\n", __FUNCTION__, gVideoFn);
        pthread_exit(0);
    }

    printf("%s: start\n", __FUNCTION__);

    while(g_pushStreamingRun)
    {
		if(STATUS_READY_FOR_PUSH != gPushClient.pushStart)
		{
			usleep(10000);
			continue;
		}
        if(fpsCnt == 0)
            gettimeofday(&tv, NULL);

        bSendFrameOut = 0;
        // Send Video Frame to av-idx and know how many time it takes
        gettimeofday(&tStart, NULL);
        ret = AVAPI2_SendFrameData(gPushClient.pushAvCanal, MEDIA_CODEC_VIDEO_H264, fpsCnt%30 == 0 ? IPC_FRAME_FLAG_IFRAME : IPC_FRAME_FLAG_PBFRAME, buf, size);
        gettimeofday(&tEnd, NULL);

        takeSec = tEnd.tv_sec-tStart.tv_sec, takeUSec = tEnd.tv_usec-tStart.tv_usec;
        if(takeUSec < 0){
            takeSec--;
            takeUSec += 1000000;
        }
        sendFrameRoundTick += takeUSec;

        totalCnt++;

        if(ret == AV_ER_EXCEED_MAX_SIZE){ // means data not write to queue, send too slow, I want to skip it
            //usleep(10000);
            continue;
        }
        else if(ret == AV_ER_SESSION_CLOSE_BY_REMOTE){
            printf("%s: AV_ER_SESSION_CLOSE_BY_REMOTE exit\n", __FUNCTION__);
            pthread_exit(0);
        }
        else if(ret == AV_ER_REMOTE_TIMEOUT_DISCONNECT){
            printf("%s: AV_ER_REMOTE_TIMEOUT_DISCONNECT exit\n", __FUNCTION__);
            pthread_exit(0);
        }
        else if(ret == IOTC_ER_INVALID_SID){
            printf("%s: IOTC_ER_INVALID_SID exit\n", __FUNCTION__);
            pthread_exit(0);
        }
        else if(ret == AV_ER_WAIT_KEY_FRAME){
            //printf("%s: Wait Next Key Frame\n", __FUNCTION__);
            continue;
        }
        else if(ret == AV_ER_CLEANBUF_ALREADY_CALLED){
            printf("%s: Wait Clean Video Buffer Done\n", __FUNCTION__);
            continue;
        }
        else if(ret == AV_ER_SOCKET_QUEUE_FULL){
            printf("%s: socket queue full\n", __FUNCTION__);
            continue;
        }
        else if(ret == AV_ER_CLIENT_NO_AVLOGIN){
            printf("%s: wait push server ready\n", __FUNCTION__);
            usleep(100000);
            continue;
        }
        else if(ret < 0){
            printf("%s: AVAPI2_SendFrameData error[%d]\n", __FUNCTION__, ret);
            continue;
        }
        bSendFrameOut = 1;

        fpsCnt++;
        if(bSendFrameOut){

            gettimeofday(&tv2, NULL);
            int timeDiff = (tv2.tv_sec-tv.tv_sec)*1000 + (tv2.tv_usec-tv.tv_usec)/1000;
            if(timeDiff > 10000){
                round++;
                printf("Video FPS[%d] R[%d]\n", fpsCnt/10, round);
                fpsCnt = 0;
                gettimeofday(&tv, NULL);
            }
        }
        usleep(sleepTick);
    }

    printf("%s: exit\n", __FUNCTION__);
    pthread_exit(0);
}

int pushStreamingCreateThread()
{
    int ret;
    pthread_t ThreadVideoFrameData_ID;
    pthread_t ThreadAudioFrameData_ID;

    if((ret = pthread_create(&ThreadVideoFrameData_ID, NULL, &pushStreaming_VideoThread, NULL))){
        printf("pthread_create ret=%d\n", ret);
        return -1;
    }
    pthread_detach(ThreadVideoFrameData_ID);

    if((ret = pthread_create(&ThreadAudioFrameData_ID, NULL, &pushStreaming_AudioThread, NULL))){
        printf("pthread_create ret=%d\n", ret);
        return -1;
    }
    pthread_detach(ThreadAudioFrameData_ID);

    return 0;
}

int pushStreamingCanalStatusCB(int nAVCanal, int nError, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    printf("pushStreamingCanalStatusCB : nAVCanal[%d] nError[%d] nChannelID[%u]\n", nAVCanal, nError, nChannelID);

    if(nError == AV_ER_SESSION_CLOSE_BY_REMOTE){
        //Realease Channel
        AVAPI2_ReleaseChannelForSend(nAVCanal);

        //Stop Push Streaming Thread
        g_pushStreamingRun = 0;
    }
    else if(nError == AV_ER_REMOTE_TIMEOUT_DISCONNECT){
        //Realease Channel
        AVAPI2_ReleaseChannelForSend(nAVCanal);

        //Stop Push Streaming Thread
        g_pushStreamingRun = 0;
    }

    return 0;
}

int pushStreamingServerStatusCB(int nStatus, int nError, int nAVCanal, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
	//int SID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
    printf("pushStreamingServerStatusCB : nStatus[%d] nError[%d] nAVCanal[%d] nChannelID[%u]\n", nStatus, nError, nAVCanal, nChannelID);

    switch(nStatus){
		case AVAPI2_SERVER_STATUS_START_CANAL_FAILED :
		{
			printf("pushStreamingServerStatusCB AVAPI2_SERVER_STATUS_START_CANAL_FAILED\n");
			AVAPI2_ReleaseChannelForSend(nAVCanal);
			gPushClient.pushStart = 0;
		}
		break;

		case AVAPI2_SERVER_STATUS_CLIENT_LOGINED :
		{
			gPushClient.pushStart |= STATUS_CLIENT_LOGIN_SUCCESS;
			printf("pushStreamingServerStatusCB : Client logged in ok\n");
			if(gPushClient.pushStart == STATUS_READY_FOR_PUSH)
			{
				struct timeval tv;
				gettimeofday(&tv, NULL);
				gPushClient.pushStartTime = tv.tv_sec;
			}
		}
        break;

		default:
		break;
	}

    return 0;
}


int pushStreamingStop(PushClientStruct *client)
{
    int ret = 0;

    if(client->avCanal < 0){
        return -1;
    }
	client->pushStart = 0;
	
	while(avResendBufUsageRate(client->pushAvCanal) != 0)
	{
		usleep(1000);
	}
	
	SMsgAVIoctrlAVStream ioMsg;
	memset(&ioMsg, 0, sizeof(SMsgAVIoctrlAVStream));
	ioMsg.channel = client->iotcChannel;
	ret = AVAPI2_SendIOCtrl(client->avCanal, IOTYPE_USER_IPCAM_PUSH_STREAMING_FINISH, (char *)&ioMsg, sizeof(ioMsg));
    if(ret < 0){
        printf("send Cmd: IOTYPE_USER_IPCAM_PUSH_STREAMING_FINISH failed[%d]\n", ret);
        return ret;
    } else {
		printf("send Cmd: IOTYPE_USER_IPCAM_PUSH_STREAMING_FINISH\n");
	}
	
    AVAPI2_ReleaseChannelForSend(client->pushAvCanal);
	
	client->pushInterval = 0;
	client->pushAvCanal = -1;

    return 0;
}

int pushStreamingStart(PushClientStruct *client)
{
    SMsgAVIoctrlPushStream ioMsg;
    int SID = -1, freeCH = 0, ret = -1;

    if(client->avCanal < 0){
        return -1;
    }

    //Get Session ID
    SID = AVAPI2_GetSessionIDByAVCanal(client->avCanal);
    if(SID < 0){
        printf("AVAPI2_GetSessionIDByAVCanal SID[%d]\n", SID);
        return SID;
    }

	//Get free IOTC channel
	freeCH = IOTC_Session_Get_Free_Channel(SID);
    if(freeCH < 0){
        printf("Get Free CH for Push Streaming Error[%d]\n", freeCH);
        return freeCH;
    } else {
		printf("Get Free CH [%d] for Push Streaming\n", freeCH);
		client->iotcChannel = freeCH;
	}
	

    //Send IO Control IOTYPE_USER_IPCAM_SPEAKERSTART
    memset(&ioMsg, 0, sizeof(SMsgAVIoctrlPushStream));
    ioMsg.channel = freeCH;
	ioMsg.event   = AVIOCTRL_EVENT_PIR;
	memcpy(ioMsg.szDeviceUID, gClientUID, 20);
	
    ret = AVAPI2_SendIOCtrl(client->avCanal, IOTYPE_USER_IPCAM_PUSH_STREAMING_START, (const char *)&ioMsg, sizeof(SMsgAVIoctrlPushStream));
    if(ret < 0){
        printf("send Cmd: IOTYPE_USER_IPCAM_PUSH_STREAMING_START failed[%d]\n", ret);
        return ret;
    }else{
		client->pushStart |= STATUS_SEND_PUSH_START;
		printf("send Cmd: IOTYPE_USER_IPCAM_PUSH_STREAMING_START\n");
	}
    

    //Create Channel for send
    int pushCanal = AVAPI2_CreateChannelForSend(SID, 30, 0, freeCH, 1, pushStreamingServerStatusCB, pushStreamingCanalStatusCB);
    if(pushCanal < 0){
        printf("AVAPI2_CreateChannelForSend failed[%d]\n", pushCanal);
        return pushCanal;
    } else {
		printf("AVAPI2_CreateChannelForSend pushCanal[%d]\n", pushCanal);
		client->pushAvCanal = pushCanal;
	}

    return 0;
}

/******************************
 * AVAPI2 Call Back Functions
 ******************************/
static int IOCtrlRecvCB(int nAVCanal, unsigned int nIoCtrlType, unsigned char *pIoCtrlBuf, unsigned int nIoCtrlBufLen, void* pUserData)
{
    printf("IOCtrlRecvCB : nAVCanal[%d] nIoCtrlType[0x%x] nIoCtrlBufLen[%u]\n", nAVCanal, nIoCtrlType, nIoCtrlBufLen);
	switch(nIoCtrlType)
	{
		case IOTYPE_USER_IPCAM_PUSH_STREAMING_START_RESP:
		{
			SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)pIoCtrlBuf;
			int SID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
			printf("receive Cmd: IOTYPE_USER_IPCAM_PUSH_STREAMING_START_RESP[%x], SID:%d, ch:%d, nAVCanal:%d\n", IOTYPE_USER_IPCAM_PUSH_STREAMING_START_RESP, SID, p->channel, nAVCanal);
			gPushClient.pushStart |= STATUS_RECEIVE_PUSH_START_RESP;
			if(gPushClient.pushStart == STATUS_READY_FOR_PUSH)
			{
				struct timeval tv;
				gettimeofday(&tv, NULL);
				gPushClient.pushStartTime = tv.tv_sec;
			}
		}
		break;
		case IOTYPE_USER_IPCAM_PUSH_STREAMING_FINISH_RESP:
		{
			SMsgAVIoctrlAVStream *p = (SMsgAVIoctrlAVStream *)pIoCtrlBuf;
			int SID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);
			printf("receive Cmd: IOTYPE_USER_IPCAM_PUSH_STREAMING_FINISH_RESP[%x], SID:%d, ch:%d, nAVCanal:%d\n", IOTYPE_USER_IPCAM_PUSH_STREAMING_FINISH_RESP, SID, p->channel, nAVCanal);
		}
		break;
		default:
		    printf("avCanal %d: non-handle type[%X]\n", nAVCanal, nIoCtrlType);
		break;
	}
    return 0;
}

static int ClientStatusCB(int nStatus, int nError, int nAVCanal, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    int ret = 0, taketime = 0, nSID = 0;
    struct st_SInfo Sinfo;
    char *mode[] = {"P2P", "RLY", "LAN"};

    printf("clientStatusCB : nStatus[%d] nError[%d] nAVCanal[%d] nChannelID[%u]\n", nStatus, nError, nAVCanal, nChannelID);
    if(pUserData != NULL){
        printf("clientStatusCB : pUserData[%s]\n", (char*)pUserData);
    }

    switch(nStatus){
        case AVAPI2_CLIENT_CONNECT_UID_ST_START :
            printf("clientStatusCB : Client start connecting to a Device\n");
            break;
        case AVAPI2_CLIENT_CONNECT_UID_ST_CONNECTING :
            printf("clientStatusCB : Client is connecting a device\n");
            break;
        case AVAPI2_CLIENT_CONNECT_UID_ST_CONNECTED :
            printf("clientStatusCB : The connection are established between a Client and a Device\n");
            gettimeofday(&g_tv2, NULL);
            taketime = (g_tv2.tv_sec-g_tv1.tv_sec)*1000 + (g_tv2.tv_usec-g_tv1.tv_usec)/1000;
            printf("Connect taketime[%d]\n", taketime);

            //Get IOTC Session Info
            memset(&Sinfo, 0, sizeof(struct st_SInfo));
            nSID = AVAPI2_GetChannelByAVCanal(nAVCanal);
            if(IOTC_Session_Check(nSID, &Sinfo) == IOTC_ER_NoERROR){
                if( isdigit( Sinfo.RemoteIP[0] ))
                    printf("Device is from %s:%d[%s] Mode=%s NAT[%d] IOTCVersion[%X]\n", Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID, mode[(int)Sinfo.Mode], Sinfo.NatType, Sinfo.IOTCVersion);
            }
            break;
        case AVAPI2_CLIENT_STATUS_LOGINED :
            printf("clientStatusCB : Login Success\n");
            gettimeofday(&g_tv2, NULL);
            taketime = (g_tv2.tv_sec-g_tv1.tv_sec)*1000 + (g_tv2.tv_usec-g_tv1.tv_usec)/1000;
            printf("Login Success taketime[%d]\n", taketime);

			//register recv IO control call back function
            ret = AVAPI2_RegRecvIoCtrlCB(nAVCanal, (ioCtrlRecvCB) IOCtrlRecvCB);
            if(ret < 0){
                printf("AVAPI2_RecvIoCtrl ret[%d]\n", ret);
                AVAPI2_ClientDisconnectAndCloseIOTC(nAVCanal);
				g_nCanal = -1;
				gPushClient.avCanal = -1;
                break;
            }
			g_nCanal = nAVCanal;
			gPushClient.avCanal = nAVCanal;
            break;

        //Error Handling
        case AVAPI2_CLIENT_STATUS_LOGIN_FAILED :
            printf("clientStatusCB : Login Failed\n");
			AVAPI2_ClientDisconnectAndCloseIOTC(nAVCanal);
			g_nCanal = -1;
			gPushClient.avCanal = -1;
            break;
        case AVAPI2_CLIENT_STATUS_LOGIN_TIMEOUT :
            printf("clientStatusCB : Login Timeout\n");
            break;
        case AVAPI2_CLIENT_STATUS_CLEAN_BUFFER_TIMEOUT :
            printf("clientStatusCB : Clean Buffer Timeout\n");
            break;
        case AVAPI2_CLIENT_STATUS_RECV_FRAME_BLOCK:
            printf("clientStatusCB : Took to long to handle received video frame in callback function\n");
            break;
        case AVAPI2_CLIENT_STATUS_RECV_AUDIO_BLOCK:
            printf("clientStatusCB : Took to long to handle received audio data in callback function\n");
            break;
        case AVAPI2_CLIENT_CONNECT_UID_ST_FAILED:
            printf("clientStatusCB : Client connects to a device failed\n");
            break;
    }
    return 0;
}


static int CanalStatusCB(int nAVCanal, int nError, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
	int nSID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);

	if (nSID < 0) {
		printf("Invalid AV Canal.\n");
		return -1;
	}

	printf("Session [%d] error with error code [%d].\n", nSID, nError);

    if(nError == AV_ER_SESSION_CLOSE_BY_REMOTE){
        //Stop Push Streaming
        if(gPushClient.pushStart == STATUS_READY_FOR_PUSH && gPushClient.pushAvCanal >= 0){
            pushStreamingStop(&gPushClient);
        }

        //Do StopCanal
        AVAPI2_ClientDisconnectAndCloseIOTC(nAVCanal);
        g_nCanal = -1;
    }
    else if(nError == AV_ER_REMOTE_TIMEOUT_DISCONNECT){
        //Stop Push Streaming
        if(gPushClient.pushStart == STATUS_READY_FOR_PUSH && gPushClient.pushAvCanal >= 0){
            pushStreamingStop(&gPushClient);
        }

        //Do StopCanal
        AVAPI2_ClientDisconnectAndCloseIOTC(nAVCanal);
        g_nCanal = -1;
    }
    else if(nError == AV_ER_CLIENT_EXIT){
        //Stop Push Streaming
        if(gPushClient.pushStart == STATUS_READY_FOR_PUSH && gPushClient.pushAvCanal >= 0){
            pushStreamingStop(&gPushClient);
        }

        //Do StopCanal
        AVAPI2_ClientDisconnectAndCloseIOTC(nAVCanal);
        g_nCanal = -1;
    }

	return 0;
}

int main(int argc, char *argv[])
{
    int ret = 0;
    unsigned int timeout = 30;
    char szVersion[64] = {0};
    struct timeval tv;
    char *avCanalStr = NULL;

    if(argc < 6 || argc > 6){
        printf("Argument Error!!!\n");
        printf("Usage: %s [Server UID] [Client UID] [Password] [VideoFile] [AudioFile]\n", argv[0]);
        return -1;
    }

    strcpy(gUID, argv[1]);
	strcpy(gClientUID, argv[2]);
    strcpy(gPassword, argv[3]);
    strcpy(gVideoFn, argv[4]);
    strcpy(gAudioFn, argv[5]);

    //Initial IOTC & AVAPI2
    AVAPI2_SetCanalLimit(16, 4);

    //Get Version
    AVAPI2_GetVersion(szVersion, 64);
    printf("%s\n", szVersion);
	
	g_pushStreamingRun = 1;
    if(pushStreamingCreateThread() < 0){
        return -1;
    }

    gettimeofday(&g_tv1, NULL);
    //Connect to AVAPI2 Server
    //avCanal will return through ClientStatusCB
    avCanalStr = (char*)malloc(16);
    sprintf(avCanalStr, "connection:0");
    ret = AVAPI2_ClientConnectByUID(gUID, avID, gPassword, timeout, 0, (canalStatusCB)CanalStatusCB, (clientStatusCB)ClientStatusCB, (void*)avCanalStr);
	printf("AVAPI2_ClientConnectByUID ret[%d]\n", ret);
    if(ret < 0){
        return 0;
    }
	
	int i = 0;
	while(i < 5){
        if(g_nCanal < 0){
            sleep(1);
            continue;
        }

        if(0 == gPushClient.pushStart){
			//Stop push streaming after 30 secs
			gPushClient.pushInterval = 30;
            ret = pushStreamingStart(&gPushClient);
			if(0 < ret)
				break;
			sleep(5);
        }
        else if(STATUS_READY_FOR_PUSH == gPushClient.pushStart)
		{
            gettimeofday(&tv, NULL);
            if(tv.tv_sec - gPushClient.pushStartTime > gPushClient.pushInterval){
                pushStreamingStop(&gPushClient);
				i++;
				sleep(5);
            }
        }
        usleep(500000);
    }
	
	g_pushStreamingRun = 0;
	
	SMsgAVIoctrlAVStream ioMsg;
	memset(&ioMsg, 0, sizeof(SMsgAVIoctrlAVStream));
    ioMsg.channel = g_nCanal;
	
	ret = AVAPI2_SendIOCtrl(g_nCanal, IOTYPE_USER_IPCAM_PUSH_STREAMING_CLOSE, (char *)&ioMsg, sizeof(ioMsg));
    if(ret < 0){
        printf("send Cmd: IOTYPE_USER_IPCAM_PUSH_STREAMING_CLOSE failed[%d]\n", ret);
    } else {
		printf("send Cmd: IOTYPE_USER_IPCAM_PUSH_STREAMING_CLOSE\n");
	}
    
	
	AVAPI2_ClientDisconnectAndCloseIOTC(g_nCanal);
	g_nCanal = -1;
	printf("%s exit...\n", argv[0]);

	return 0;
}


