/**====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
*
* IOTCAPIs_WakeUp_Device.c
*
* Copyright (c) by TUTK Co.LTD. All Rights Reserved.
*
* @brief   Examle of how to use IOTC WakeUp API at Device side
*
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "IOTCAPIs.h"
#include "IOTCWakeUp.h"


// Macro definition
#define MAX_BUF_SIZE 1500

// Structure declaration
typedef struct _WakeupParam
{
    int                 nSkt;
    unsigned long       ulLoginAddr;
    unsigned short      usLoginPort;
    char                *pWakeupPattern;
    int 				nWakeupPatternLength;
} WakeupParam;


typedef struct _AliveParam
{
    int 			nSkt;
    IOTC_WakeUpData *pWUData;
    int 			nDataCnt;
} AliveParam;


// Variables
static bool g_bWakeUp = false;


static int createSocket()
{
    int nSkt = -1;

    nSkt = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    printf("create socket = %d!\n", nSkt);

    return nSkt;
}


static void SetupSocketSelectParam(int nSkt, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *time, uint8_t nMaxTimeout)
{
    if(readfds)
    {
        FD_ZERO(readfds);
        FD_SET(nSkt, readfds);
    }

    if(writefds)
    {
        FD_ZERO(writefds);
        FD_SET(nSkt, writefds);
    }

    if(exceptfds)
    {
        FD_ZERO(exceptfds);
        FD_SET(nSkt, exceptfds);
    }

    if(time)
    {
        memset(time, 0, sizeof(struct timeval));
        time->tv_sec = nMaxTimeout;
    }
}


static bool isWakeupPattern(const char *pData, const char *pPattern, const int nPatternLength)
{
    bool bPattern = true;

    for(int i = 0; i < nPatternLength; i++)
    {
        if(pPattern[i])
        {
            if(pData[i] != pPattern[i])
            {
                bPattern = false;
                break;
            }
        }
    }

    return bPattern;
}


static void packetParser(WakeupParam *pParam)
{
    int nRecvSize = 0;
    char buf[MAX_BUF_SIZE];
    char transBuf[MAX_BUF_SIZE];

    socklen_t socklength = 0;
    int nSkt = pParam->nSkt;

    //printf("[packetParser] - Prepare to receive socket header. skt = %d\n", nSkt);

    struct sockaddr_in remote;
    memset(&remote, 0, sizeof(struct sockaddr_in));
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = pParam->ulLoginAddr;
    remote.sin_port = pParam->usLoginPort;

    nRecvSize = recvfrom(nSkt, &buf, MAX_BUF_SIZE, 0, (struct sockaddr *)&remote, &socklength);

    // Compare WakeUp pattern
    if(isWakeupPattern(buf, pParam->pWakeupPattern, pParam->nWakeupPatternLength))
        g_bWakeUp = true;
}


static void *thread_WakeUpTask(void *arg)
{
    fd_set readfds;
    struct timeval timeout;
    WakeupParam *pParam = (WakeupParam*)arg;

    int nSkt = pParam->nSkt;

    printf("[thread_WakeUpTask] - skt = %d\n", nSkt);

    int nVal = 0;
    int nMaxFds = nSkt + 1;
    while(1)
    {
        SetupSocketSelectParam(nSkt, &readfds, NULL, NULL, &timeout, 1);
        nVal = select(nMaxFds, &readfds, NULL, NULL, &timeout);
        if(nVal > 0)
        {
            if(FD_ISSET(nSkt, &readfds))
            {
                //printf("[thread_WakeUpTask] - FD_ISSET(pParam->nSkt, &readfds), skt = %d\n", nSkt);
                packetParser(pParam);
                //break;
            }
        }
        else if(nVal == 0)
        {
            //printf("[thread_WakeUpTask] - select timeout, skt = %d\n", nSkt);
        }

        if(g_bWakeUp)
        {
            printf("[thread_WakeUpTask] - Device is WakeUp!\n");
            break;
        }
    }

    printf("[testIOTC_Wakup] - thread_WakeUpTask exit!\n");
    pthread_exit(0);
}


static void *_thread_Login(void *arg)
{
    int nRet;
    char *UID = (char *)arg;

    while(1)
    {
        nRet = IOTC_Device_Login(UID, "AAAA0009", "12345678");
        printf("Calling IOTC_Device_Login() ret = %d\n", nRet);

        if(nRet == IOTC_ER_NoERROR)
            break;
        else
            sleep(2);
    }

    pthread_exit(0);
}


static void sendKeepAlivePacket(int nSkt, IOTC_WakeUpData *pIOTCAliveData, int nDataCnt)
{
    for(int i = 0; i < nDataCnt; i++)
    {
        struct sockaddr_in remote;
        memset(&remote, 0, sizeof(struct sockaddr_in));
        remote.sin_addr.s_addr = pIOTCAliveData[i].ulLoginAddr;
        remote.sin_port = pIOTCAliveData[i].usLoginPort;

        printf("[sendKeepAlivePacket] - %s : %d, size = %d\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port), pIOTCAliveData[i].nLoginPacketLength);

        int ret = sendto(nSkt, pIOTCAliveData[i].pszLoginPacket, pIOTCAliveData[i].nLoginPacketLength, 0, (struct sockaddr *)&remote, sizeof(struct sockaddr_in));
    }

    //sleep(pIOTCAliveData[0].nLoginInterval);
    sleep(3);
}


static void duplicateWakeUpData(IOTC_WakeUpData *pDest, IOTC_WakeUpData* pSrc, int nDataCnt)
{
    if(pDest && pSrc)
    {
        for(int i = 0; i < nDataCnt; i++)
        {
            pDest[i].ulLoginAddr = pSrc[i].ulLoginAddr;
            pDest[i].usLoginPort = pSrc[i].usLoginPort;
            pDest[i].nLoginInterval = pSrc[i].nLoginInterval;
            pDest[i].nLoginPacketLength = pSrc[i].nLoginPacketLength;
            pDest[i].nWakeupPatternLength = pSrc[i].nWakeupPatternLength;

            // copy Login packet
            pDest[i].pszLoginPacket = malloc(pSrc[i].nLoginPacketLength);
            memcpy(pDest[i].pszLoginPacket, pSrc[i].pszLoginPacket, pSrc[i].nLoginPacketLength);

            // copy wake up pattern
            pDest[i].pszWakeupPattern = malloc(pSrc[i].nWakeupPatternLength);
            memcpy(pDest[i].pszWakeupPattern, pSrc[i].pszWakeupPattern, pSrc[i].nWakeupPatternLength);
        }
    }
}


static void releaseWakeUpData(IOTC_WakeUpData *pData, int nDataCnt)
{
    if(pData)
    {
        for(int i = 0; i < nDataCnt; i++)
        {
            free(pData[i].pszLoginPacket);
            free(pData[i].pszWakeupPattern);
        }
        free(pData);
    }
}


static void *thread_AliveTask(void *arg)
{
    AliveParam *ap = (AliveParam*)arg;

    while(1)
    {
        if(!g_bWakeUp)
            sendKeepAlivePacket(ap->nSkt, ap->pWUData, ap->nDataCnt);
        else
            break;
    }

    pthread_exit(0);
}


int main(int argc, char *argv[])
{
    // ++++++ Devicde at normal mode ++++++
    if(argc < 2)
    {
        printf("No UID input!!!\n");
        printf("IOTCAPIs_Device [UID]\n");
        return 0;
    }

    printf("IOTCAPIs_Device start...\n");

    int nRet;
    char *UID = (char *)argv[1];

    nRet = IOTC_Initialize2(0);

    printf("Step 1: IOTC_Initialize() ret = %d\n", nRet);
    if(nRet != IOTC_ER_NoERROR)
    {
        printf("IOTCAPIs_Device exit...!!\n");
        return 0;
    }

    // Use Wake up feature
    IOTC_WakeUp_Init();
    // Login
    pthread_t threadID_Login;
    pthread_create(&threadID_Login, NULL, &_thread_Login, (void *)UID);
    pthread_detach(threadID_Login);

    sleep(5);

    IOTC_WakeUpData *pAliveData = NULL;
    unsigned int nDataCnt = 0;

    // IOTC_WakeUp_Get_KeepAlivePacket "Must" be used after login
    IOTC_WakeUp_Get_KeepAlivePacket(&pAliveData, &nDataCnt);

    // Device is going to sleep, duplicate IOTC_WakeUpData
    IOTC_WakeUpData *pIOTCAliveData = NULL;
    pIOTCAliveData = (IOTC_WakeUpData*)malloc(sizeof(IOTC_WakeUpData) * nDataCnt);
    if(pIOTCAliveData)
        duplicateWakeUpData(pIOTCAliveData, pAliveData, nDataCnt);
    else
        printf("malloc error!\n");

    // Free pAliveData memory
    IOTC_WakeUp_DeInit(pAliveData);
    // DeInit IOTC
    IOTC_DeInitialize();
    // ------ Devicde at normal mode ------


    // ++++++ Device enter sleep mode ++++++
    printf("Device enter sleep mode!\n");
    // Driver will create a receive thread and keep receive data
    int nSkt = createSocket();

    WakeupParam *pParam = (WakeupParam*)malloc(sizeof(WakeupParam) * nDataCnt);

    for(int i = 0; i < nDataCnt; i++)
    {
        pthread_t nTaskId;

        pParam[i].nSkt = nSkt;
        pParam[i].ulLoginAddr = pIOTCAliveData[i].ulLoginAddr;
        pParam[i].usLoginPort = pIOTCAliveData[i].usLoginPort;
        pParam[i].pWakeupPattern = pIOTCAliveData[i].pszWakeupPattern;
        pParam[i].nWakeupPatternLength = pIOTCAliveData[i].nWakeupPatternLength;

        nRet = pthread_create(&nTaskId, NULL, thread_WakeUpTask, &pParam[i]);

        if(nRet != 0)
            printf("pthread_create for fail!\n");
    }

    // Driver will create a send thread and keep send IOTC login
    pthread_t nAliveTaskId;

    AliveParam ap;
    ap.nSkt = nSkt;
    ap.pWUData = pIOTCAliveData;
    ap.nDataCnt = nDataCnt;

    nRet = pthread_create(&nAliveTaskId, NULL, thread_AliveTask, &ap);

    pthread_join(nAliveTaskId, 0);

    // Device is woken up
    close(nSkt);
    // Don't forget to free memory
    free(pParam);
    releaseWakeUpData(pIOTCAliveData, nDataCnt);
    // ------ Device enter sleep mode ------

    printf("Device enter normal mode!\n");

    nRet = IOTC_Initialize2(0);

    pthread_create(&threadID_Login, NULL, &_thread_Login, (void *)UID);

    printf("Device prepare to listen!\n");

    int nSID = -1;
    while(1)
    {
        nSID = IOTC_Listen(0);
        if(nSID > -1)
            break;
        else
            sleep(1);
    }

    printf("Device accept connection\n");

    IOTC_DeInitialize();

    return 0;
}
