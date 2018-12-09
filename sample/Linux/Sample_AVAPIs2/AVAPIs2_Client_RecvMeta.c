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

/******************************
 * Defined
 ******************************/
#define ENABLE_AUDIO 1              //Test Audio
#define ENABLE_SPEAKER 0            //Test Speaker
#define ENABLE_TEST_CLEAN_BUFFER 0  //Test Clean Buffer

#define AUDIO_BUF_SIZE 320
#define MAX_SERVER_NUMBER 4

/******************************
 * Structure
 ******************************/
typedef struct _AV_Server
{
    int avCanal;
    int speakerAVCanal;
    int recvMetaAVCanal;
    int speakerRun;
    int recvMetaRun;
    int speakerCh;
    char UID[32];
    char Password[32];

    int v_fpsCnt, v_bps, v_err, v_TotalCnt;
    int a_fpsCnt, a_bps, a_err, a_TotalCnt;
    struct timeval v_tv1, v_tv2;
    struct timeval a_tv1, a_tv2;
    struct timeval g_tv1, g_tv2;
    int firstVideo, firstAudio;
    unsigned int audiotime, videotime;
}AV_Server;

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
 ******************************/
char *avID = "admin";
int gServerNum = 0;
static AV_Server gServerInfo[MAX_SERVER_NUMBER];

/******************************
 * AV_Server Control Functions
 ******************************/
void InitAVServerInfo()
{
	int i = 0;

	for(i = 0 ; i < MAX_SERVER_NUMBER ; i++){
		memset(&gServerInfo[i], 0, sizeof(AV_Server));
		gServerInfo[i].avCanal = -1;
		gServerInfo[i].speakerAVCanal = -1;
	}
}

void DeInitAVServerInfo(int index)
{
    if(index < 0 || index >= MAX_SERVER_NUMBER)
        return;

    memset(&gServerInfo[index], 0, sizeof(AV_Server));
    gServerInfo[index].avCanal = -1;
    gServerInfo[index].speakerAVCanal = -1;
}

int GetAVServerIndexByUID(char* UID)
{
    int i = 0;
    
    for(i = 0 ; i < MAX_SERVER_NUMBER ; i++){
        if(strcmp(UID, gServerInfo[i].UID) == 0)
            return i;
    }

    return -1;
}

int GetAVServerIndexByCanal(int avCanal)
{
    int i = 0;

    for(i = 0 ; i < MAX_SERVER_NUMBER ; i++){
        if(avCanal == gServerInfo[i].avCanal)
            return i;
    }

    return -1;
}

int GetAVServerIndexByRecvMetaCanal(int avCanal)
{
    int i = 0;

    for(i = 0 ; i < MAX_SERVER_NUMBER ; i++){
        if(avCanal == gServerInfo[i].recvMetaAVCanal)
            return i;
    }

    return -1;
}

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

int recvMeta_Stop(int avCanal)
{
    int ret = 0, index = 0;
    SMsgAVIoctrlAVStream ioMsg;

    index = GetAVServerIndexByCanal(avCanal);
    if(index < 0){
        printf("GetAVServerIndexByCanal Error[%d]\n", index);
        return -1;
    }

    //Send IO Control IOTYPE_USER_IPCAM_SEND_META_START
    memset(&ioMsg, 0, sizeof(SMsgAVIoctrlAVStream));
    ret = AVAPI2_SendIOCtrl(avCanal, IOTYPE_USER_IPCAM_SEND_META_STOP, (const char *)&ioMsg, sizeof(SMsgAVIoctrlAVStream));
    if(ret < 0){
        printf("send Cmd: IOTYPE_USER_IPCAM_SEND_META_STOP Error[%d]\n", ret);
        return -1;
    }
    printf("send Cmd: IOTYPE_USER_IPCAM_SEND_META_STOP\n");

    //Release Channel for receive
    AVAPI2_ReleaseChannelForReceive(gServerInfo[index].recvMetaAVCanal);

    gServerInfo[index].recvMetaRun = 0;

    return 0;
}

static int recvMeta_MetaRecvCB(int nAVCanal, int nError, char *pFrameData, int nActualFrameSize, int nExpectedFrameSize, char* pFrameInfo, int nFrameInfoSize, int frmNo, void* pUserData)
{
    MetaDataInfo* metaDataInfo = NULL;

    if(nError < 0){
        printf("Receive meta nError:%d\n", nError);
        PrintErrHandling(nError);
        if(nError == AV_ER_LOSED_THIS_FRAME){
            printf("Video AV_ER_LOSED_THIS_FRAME frmNo[%d]\n", frmNo);
        }
        else{
            printf("Video receive nError[%d]\n", nError);
        }
        return 0;
    }

	if(pFrameData == NULL || nActualFrameSize < 0 || pFrameInfo == NULL){
		printf("Invalid frame.\n");
		return -1;
	}

    metaDataInfo = (MetaDataInfo*)pFrameInfo;
    printf("[%d:%d] [%d:%d]\n", metaDataInfo->nBeginSlice, metaDataInfo->nEndSlice, metaDataInfo->nSliceIdx, metaDataInfo->nSliceCount);

    if(metaDataInfo->nEndSlice == 1){
        recvMeta_Stop(GetAVServerIndexByRecvMetaCanal(nAVCanal));
    }
	return 0;
}

static int recvMeta_CanalStatusCB(int nAVCanal, int nError, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
	int nSID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);

	if (nSID < 0) {
		printf("Invalid AV Canal.\n");
		return -1;
	}

	printf("nAVCanal[%d] error with error code [%d].\n", nAVCanal, nError);
	PrintErrHandling(nError);

    if(nError == AV_ER_SESSION_CLOSE_BY_REMOTE){
        //Do StopCanal
        AVAPI2_ReleaseChannelForReceive(nAVCanal);
    }
    else if(nError == AV_ER_REMOTE_TIMEOUT_DISCONNECT){
        //Do StopCanal
        AVAPI2_ReleaseChannelForReceive(nAVCanal);
    }
    else if(nError == AV_ER_CLIENT_EXIT){
        //Do StopCanal
        AVAPI2_ReleaseChannelForReceive(nAVCanal);
    }

	return 0;
}

static int recvMeta_ClientStatusCB(int nStatus, int nError, int nAVCanal, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    int ret = 0, nSID = 0;
    struct st_SInfo Sinfo;
    char *mode[] = {"P2P", "RLY", "LAN"};

    printf("recvMeta_ClientStatusCB : nStatus[%d] nError[%d] nAVCanal[%d] nChannelID[%u]\n", nStatus, nError, nAVCanal, nChannelID);

    switch(nStatus){
        case AVAPI2_CLIENT_CONNECT_UID_ST_START :
            printf("recvMeta_ClientStatusCB : Client start connecting to a Device\n");
            break;
        case AVAPI2_CLIENT_CONNECT_UID_ST_CONNECTING :
            printf("recvMeta_ClientStatusCB : Client is connecting a device\n");
            break;
        case AVAPI2_CLIENT_CONNECT_UID_ST_CONNECTED :
            printf("recvMeta_ClientStatusCB : The connection are established between a Client and a Device\n");
            //Get IOTC Session Info
            memset(&Sinfo, 0, sizeof(struct st_SInfo));
            nSID = AVAPI2_GetChannelByAVCanal(nAVCanal);
            if(IOTC_Session_Check(nSID, &Sinfo) == IOTC_ER_NoERROR){
                if( isdigit( Sinfo.RemoteIP[0] ))
                    printf("Device is from %s:%d[%s] Mode=%s NAT[%d] IOTCVersion[%X]\n", Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID, mode[(int)Sinfo.Mode], Sinfo.NatType, Sinfo.IOTCVersion);
            }
            break;
        case AVAPI2_CLIENT_STATUS_LOGINED :
            printf("recvMeta_ClientStatusCB : Login Success\n");
            //register recv Meta call back function
            ret = AVAPI2_RegRecvMetaCB(nAVCanal, recvMeta_MetaRecvCB);
            if(ret < 0){
                printf("AVAPI2_RegRecvMetaCB ret[%d]\n", ret);
                AVAPI2_ReleaseChannelForReceive(nAVCanal);
                break;
            }
            break;

        //Error Handling
        case AVAPI2_CLIENT_STATUS_LOGIN_FAILED :
            printf("recvMeta_ClientStatusCB : Login Failed\n");
            break;
        case AVAPI2_CLIENT_STATUS_LOGIN_TIMEOUT :
            printf("recvMeta_ClientStatusCB : Login Timeout\n");
            break;
        case AVAPI2_CLIENT_STATUS_CLEAN_BUFFER_TIMEOUT :
            printf("recvMeta_ClientStatusCB : Clean Buffer Timeout\n");
            break;
        case AVAPI2_CLIENT_STATUS_RECV_FRAME_BLOCK:
            printf("recvMeta_ClientStatusCB : Took to long to handle received video frame in callback function\n");
            break;
        case AVAPI2_CLIENT_STATUS_RECV_AUDIO_BLOCK:
            printf("recvMeta_ClientStatusCB : Took to long to handle received audio data in callback function\n");
            break;
        case AVAPI2_CLIENT_CONNECT_UID_ST_FAILED:
            printf("recvMeta_ClientStatusCB : Client connects to a device failed\n");
            break;
    }
    return 0;
}

int recvMeta_Start(int avCanal)
{
    int SID = 0, freeCH = 0, ret = 0, index = 0;
    SMsgAVIoctrlAVStream ioMsg;

    index = GetAVServerIndexByCanal(avCanal);
    if(index < 0){
        printf("GetAVServerIndexByCanal Error[%d]\n", index);
        return -1;
    }

    //Get Session ID
    SID = AVAPI2_GetSessionIDByAVCanal(avCanal);
    if(SID < 0){
        printf("AVAPI2_GetSessionIDByAVCanal Error[%d]\n", SID);
        return -1;
    }

	//Get free IOTC channel
	freeCH = IOTC_Session_Get_Free_Channel(SID);
    if(freeCH < 0){
        printf("Get Free CH for Receive Meta Error[%d]\n", freeCH);
        return -1;
    }
	printf("Get Free CH [%d] for Receive Meta\n", freeCH);

    //Send IO Control IOTYPE_USER_IPCAM_SEND_META_START
    memset(&ioMsg, 0, sizeof(SMsgAVIoctrlAVStream));
    ioMsg.channel = freeCH;
    ret = AVAPI2_SendIOCtrl(avCanal, IOTYPE_USER_IPCAM_SEND_META_START, (const char *)&ioMsg, sizeof(SMsgAVIoctrlAVStream));
    if(ret < 0){
        printf("send Cmd: IOTYPE_USER_IPCAM_SEND_META_START Error[%d]\n", ret);
        return -1;
    }
    printf("send Cmd: IOTYPE_USER_IPCAM_SEND_META_START\n");

    //Create Channel for receive
    gServerInfo[index].recvMetaAVCanal = AVAPI2_CreateChannelForReceive(SID, freeCH, 1, 30, recvMeta_ClientStatusCB, recvMeta_CanalStatusCB, NULL, NULL, NULL);
    if(gServerInfo[index].recvMetaAVCanal < 0){
        printf("AVAPI2_CreateChannelForReceive Error[%d]\n", gServerInfo[index].recvMetaAVCanal);
        return -1;
    }
    printf("Starting recvMetaAVCanal[%d]\n", gServerInfo[index].recvMetaAVCanal);

    gServerInfo[index].recvMetaRun = 1;

    return 0;
}

/******************************
 * AVAPI2 Client Call Back Functions
 ******************************/
static int IOCtrlRecvCB(int nAVCanal, unsigned int nIoCtrlType, unsigned char *pIoCtrlBuf, unsigned int nIoCtrlBufLen, void* pUserData)
{
	printf("IOCtrlRecvCB : nAVCanal[%d] nIoCtrlType[0x%x] nIoCtrlBufLen[%u]\n", nAVCanal, nIoCtrlType, nIoCtrlBufLen);
	return 0;
}

static int CanalStatusCB(int nAVCanal, int nError, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
	int nSID = AVAPI2_GetSessionIDByAVCanal(nAVCanal);

	if (nSID < 0) {
		printf("Invalid AV Canal.\n");
		return -1;
	}

	printf("nAVCanal[%d] error with error code [%d].\n", nAVCanal, nError);
	PrintErrHandling(nError);

    if(nError == AV_ER_SESSION_CLOSE_BY_REMOTE){
        //Do StopCanal
        AVAPI2_ClientDisconnectAndCloseIOTC(nAVCanal);
        DeInitAVServerInfo(GetAVServerIndexByCanal(nAVCanal));
    }
    else if(nError == AV_ER_REMOTE_TIMEOUT_DISCONNECT){
        //Do StopCanal
        AVAPI2_ClientDisconnectAndCloseIOTC(nAVCanal);
        DeInitAVServerInfo(GetAVServerIndexByCanal(nAVCanal));
    }
    else if(nError == AV_ER_CLIENT_EXIT){
        //Do StopCanal
        AVAPI2_ClientDisconnectAndCloseIOTC(nAVCanal);
        DeInitAVServerInfo(GetAVServerIndexByCanal(nAVCanal));
    }

	return 0;
}

static int ClientStatusCB(int nStatus, int nError, int nAVCanal, unsigned char nChannelID, struct st_SInfo* pStSInfo, void* pUserData)
{
    int ret = 0, taketime = 0, index = 0;
    char *mode[] = {"P2P", "RLY", "LAN"};

    printf("clientStatusCB : nStatus[%d] nError[%d] nAVCanal[%d] UID[%s] nChannelID[%u]\n", nStatus, nError, nAVCanal, pStSInfo->UID, nChannelID);
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
            //Get IOTC Session Info
            if(pStSInfo != NULL){
                if( isdigit( pStSInfo->RemoteIP[0] ))
                    printf("Device is from %s:%d[%s] Mode=%s NAT[%d] IOTCVersion[%X]\n", pStSInfo->RemoteIP, pStSInfo->RemotePort, pStSInfo->UID, mode[(int)pStSInfo->Mode], pStSInfo->NatType, pStSInfo->IOTCVersion);
            }

            // Get AV Server Info Index
            index = GetAVServerIndexByUID(pStSInfo->UID);
            if(index < 0){
                printf("Can't find UID[%s] in server info\n", pStSInfo->UID);
                break;
            }
            gettimeofday(&gServerInfo[index].g_tv2, NULL);
            taketime = (gServerInfo[index].g_tv2.tv_sec-gServerInfo[index].g_tv1.tv_sec)*1000 + (gServerInfo[index].g_tv2.tv_usec-gServerInfo[index].g_tv1.tv_usec)/1000;
            printf("nAVCanal[%d] Connect taketime[%d]\n", nAVCanal, taketime);

            //Register recv IO control call back function
            gServerInfo[index].avCanal = nAVCanal;
            ret = AVAPI2_RegRecvIoCtrlCB(gServerInfo[index].avCanal, (ioCtrlRecvCB) IOCtrlRecvCB);
            if(ret < 0){
                printf("nAVCanal[%d] AVAPI2_RegRecvIoCtrlCB ret[%d]\n", nAVCanal, ret);
                PrintErrHandling (gServerInfo[index].avCanal);
                AVAPI2_ClientDisconnectAndCloseIOTC(gServerInfo[index].avCanal);
                DeInitAVServerInfo(index);
                break;
            }
            break;

        case AVAPI2_CLIENT_STATUS_LOGINED :
            printf("clientStatusCB : Login To AVAPI Server Success\n");
            index = GetAVServerIndexByUID(pStSInfo->UID);
            if(index < 0){
                printf("Can't find UID[%s] in server info\n", pStSInfo->UID);
            }

            gettimeofday(&gServerInfo[index].g_tv2, NULL);
            taketime = (gServerInfo[index].g_tv2.tv_sec-gServerInfo[index].g_tv1.tv_sec)*1000 + (gServerInfo[index].g_tv2.tv_usec-gServerInfo[index].g_tv1.tv_usec)/1000;
            printf("nAVCanal[%d] Login Success taketime[%d]\n", nAVCanal, taketime);
            break;

        //Error Handling
        case AVAPI2_CLIENT_STATUS_LOGIN_FAILED :
            printf("clientStatusCB : Login Failed\n");
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
            // Device Offline
            if(nError == IOTC_ER_DEVICE_OFFLINE){
                break;
            }

            //Handle Reconnect
            index = GetAVServerIndexByUID(pStSInfo->UID);
            if(index >= 0){
                printf("clientStatusCB : Try Reconnect UID[%s]\n", pStSInfo->UID);
                gettimeofday(&gServerInfo[index].g_tv1, NULL);
                ret = AVAPI2_ClientConnectByUID(gServerInfo[index].UID, avID, gServerInfo[index].Password, 30, 0, (canalStatusCB)CanalStatusCB, (clientStatusCB)ClientStatusCB, pUserData);
                if(ret < 0){
                    printf("UID[%s] AVAPI2_ClientConnectByUID ret[%d]\n", gServerInfo[index].UID, ret);
                    PrintErrHandling (ret);
                    break;
                }
            }
            break;
    }
    return 0;
}


int main(int argc, char *argv[])
{
    int ret = 0, i = 0;
    char szVersion[64] = {0};
    unsigned int timeout = 30;
#if ENABLE_SPEAKER
    struct timeval tv1, tv2;
#endif
    char *avCanalStr = NULL;

    InitAVServerInfo();

    // Get Server UID & Password From Input Argument
    if(argc < 3 || argc > MAX_SERVER_NUMBER*2 + 1){
        printf("Argument Error!!!\n");
        printf("Usage: %s [UID1] [Password1] [ [UID2] [Password2] ... ]\n", argv[0]);
        printf("\n");
        return -1;
    }
    else{
        gServerNum = (argc-1)/2;
        for (i = 0; i < gServerNum; i++){
            strncpy(gServerInfo[i].UID, argv[i*2+1], 32);
            strncpy(gServerInfo[i].Password, argv[i*2+2], 32);
            printf("[%s:%s]\n", gServerInfo[i].UID, gServerInfo[i].Password);
        }
    }
    if(gServerNum <= 0){
        printf("Argument Error : No Server UID\n");
        return -1;
    }

    //Initial IOTC & AVAPI2
    AVAPI2_SetCanalLimit(MAX_SERVER_NUMBER, 4);

    //Get Version
    AVAPI2_GetVersion(szVersion, 64);
    printf("%s\n", szVersion);

    //Connect to AVAPI2 Server
    //avCanal will return through ClientStatusCB
    for (i = 0; i < gServerNum; i++){
        gettimeofday(&gServerInfo[i].g_tv1, NULL);
        avCanalStr = (char*)malloc(16);
        sprintf(avCanalStr, "connection:%d", i);
        ret = AVAPI2_ClientConnectByUID(gServerInfo[i].UID, avID, gServerInfo[i].Password, timeout, 0, (canalStatusCB)CanalStatusCB, (clientStatusCB)ClientStatusCB, (void*)avCanalStr);
        if(ret < 0){
            printf("UID[%s] AVAPI2_ClientConnectByUID ret[%d]\n", gServerInfo[i].UID, ret);
            PrintErrHandling (ret);
            return 0;
        }
    }

	while(1){
        //Start Receive Meta
        sleep(3);
        for (i = 0; i < gServerNum; i++){
            if(gServerInfo[i].avCanal >= 0 && gServerInfo[i].recvMetaRun == 0){
                recvMeta_Start(gServerInfo[i].avCanal);
            }
        }
    }

    AVAPI2_ClientStop();
    InitAVServerInfo();

	return 0;
}


