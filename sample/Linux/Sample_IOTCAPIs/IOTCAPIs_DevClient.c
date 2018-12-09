#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "IOTCAPIs.h"

#define MAX_SESSION 8

typedef struct threadArgs_s
{
    int sid;
    unsigned char chid;
} threadArgs_t;

void *thread_ForSessionRead(void *arg)
{
    threadArgs_t * inArg = (threadArgs_t *)arg;
    unsigned char ChID = inArg->chid;
    int SID = inArg->sid;
    int nRead = 0;
    char buf[IOTC_MAX_PACKET_SIZE];

    while((nRead = IOTC_Session_Read(SID, buf, IOTC_MAX_PACKET_SIZE, 6000, ChID)) > -1)
    {
        buf[nRead] = 0;
        printf("[IOTC_Session_Read] SID=%d, ChID = %d, Size=%d, Data:%s\n",SID,ChID,nRead,buf);
    }
    // check if remote site close this session
    if(nRead == IOTC_ER_SESSION_CLOSE_BY_REMOTE)
    {
        printf("[thread_ForSessionReadWrite] remote site close this session, SID = %d\n", SID);
        IOTC_Session_Close(SID);
    }
    else if(nRead == IOTC_ER_REMOTE_TIMEOUT_DISCONNECT)
    {
        printf("[thread_ForSessionReadWrite] disconnected due to remote site has no any response after a 'timeout' period of time., SID = %d\n", SID);
        IOTC_Session_Close(SID);
    }
    else
    {
        printf("[thread_ForSessionReadWrite] [%d] Closing SID %d\n", nRead, SID);
        IOTC_Session_Close(SID);
    }

    printf("[thread_ForSessionRead] Thread exit...[%d] nRead = %d\n", ChID, nRead);

    return NULL;
}

void *thread_ForSessionWrite(void *arg)
{
    threadArgs_t * inArg = (threadArgs_t *)arg;
    unsigned char ChID = inArg->chid;
    int SID = inArg->sid;
    int i;
    int ret = 0;
    char buf[IOTC_MAX_PACKET_SIZE];
    struct st_SInfo Sinfo;
    pthread_t ReadThread;

    printf("[thread_ForSessionReadWrite] Thread started...SID [%d] CID[%d]\n", SID, ChID);

    IOTC_Session_Check(SID, &Sinfo);
    if(Sinfo.CorD == 1)
    {
        if(Sinfo.Mode ==0)
            printf("Client is from %s:%d[%s] Mode=P2P\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
        else if(Sinfo.Mode == 1)
            printf("Client is from %s:%d[%s] Mode=RLY\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
        else if(Sinfo.Mode == 2)
            printf("Client is from %s:%d[%s] Mode=LAN\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
    }
    else
    {
        if(Sinfo.Mode ==0)
            printf("Device is from %s:%d[%s] Mode=P2P\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
        else if(Sinfo.Mode == 1)
            printf("Device is from %s:%d[%s] Mode=RLY\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
        else if(Sinfo.Mode == 2)
            printf("Device is from %s:%d[%s] Mode=LAN\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
    }

    printf("VPG [%d:%d:%d] Remote NAT[%d]\n", Sinfo.VID, Sinfo.PID, Sinfo.GID, Sinfo.NatType);

    IOTC_Session_Channel_ON(SID, ChID);
    //sleep(3);

    pthread_create(&ReadThread, NULL, &thread_ForSessionRead, (void*)arg);

    for(i = 0; i < 30; i++)
    {
        if (Sinfo.CorD == 1)
        {
            sprintf(buf, "Hello Client... [%d][%d]", ChID, i);
        }
        else
        {
            sprintf(buf, "Hello Device... [%d][%d]", ChID, i);
        }
        ret = IOTC_Session_Write(SID,buf,strlen(buf), ChID);
        if(ret < 0)
            printf("Write error[%d]->%d\n", ChID, ret);

        sleep(1);
    }

    sprintf(buf, "end");
    IOTC_Session_Write(SID, buf, strlen(buf), ChID);

#if 0
    while((nRead = IOTC_Session_Read(SID, buf, IOTC_MAX_PACKET_SIZE, 6000, ChID)) > -1)
    {
        buf[nRead] = 0;
        printf("[IOTC_Session_Read] SID=%d, ChID = %d, Size=%d, Data:%s\n",SID,ChID,nRead,buf);
    }
    // check if remote site close this session
    if(nRead == IOTC_ER_SESSION_CLOSE_BY_REMOTE)
    {
        printf("[thread_ForSessionReadWrite] remote site close this session, SID = %d\n", SID);
        IOTC_Session_Close(SID);
    }
    else if(nRead == IOTC_ER_REMOTE_TIMEOUT_DISCONNECT)
    {
        printf("[thread_ForSessionReadWrite] disconnected due to remote site has no any response after a 'timeout' period of time., SID = %d\n", SID);
        IOTC_Session_Close(SID);
    }
    else
    {
        printf("[thread_ForSessionReadWrite] [%d] Closing SID %d\n", nRead, SID);
        IOTC_Session_Close(SID);
    }
#endif

    pthread_join(ReadThread, NULL);

    printf("[thread_ForSessionWrite] Thread exit...SID[%d] CID[%d] ret = %d\n", SID, ChID, ret);

    pthread_exit(0);
}

void *_thread_Login(void *arg)
{
    int ret;
    char *UID = (char *)arg;
    printf("myUID is [%s]\n", UID);
    while(1)
    {
        ret = IOTC_Device_Login(UID, "AAAA0009", "12345678");
        printf("     Calling IOTC_Device_Login() ret = %d\n", ret);
        if(ret == IOTC_ER_NoERROR)
            break;
        else
            sleep(5);
    }

    pthread_exit(0);
}

void * clientWorks(void * arg)
{
    int clientSId = 0;
    int ret = IOTC_ER_NoERROR;
    char UID[21] = {0};
    pthread_t WriteThread1;
    pthread_t WriteThread2;
    threadArgs_t arg1;
    threadArgs_t arg2;

    memset(UID, 0, sizeof(UID));

    memcpy(UID, arg, 20);

    sleep(5);

    clientSId = IOTC_Get_SessionID();
    if(clientSId < 0)
    {
        printf("IOTC_Get_SessionID error code [%d]\n", clientSId);
        pthread_exit(0);
    }
    printf("Client Step 2: call IOTC_Connect_ByUID_Parallel[%d](%s).......\n", clientSId, UID);
    ret = IOTC_Connect_ByUID_Parallel(UID, clientSId);
    printf("IOTC_Connect_ByUID_Parallel return %d\n", ret);

#if 0
    struct st_SInfo Sinfo;
    IOTC_Session_Check(clientSId, &Sinfo);
    if(Sinfo.Mode ==0)
        printf("Connected Device info %s:%d[%s] Mode=P2P\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
    else if(Sinfo.Mode == 1)
        printf("Connected Device info %s:%d[%s] Mode=RLY\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
    else if(Sinfo.Mode == 2)
        printf("Connected Device info %s:%d[%s] Mode=LAN\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
#endif

    arg1.chid = 0;
    arg1.sid = clientSId;
    pthread_create(&WriteThread1, NULL, &thread_ForSessionWrite, (void*)&arg1);

    arg2.chid = 1;
    arg2.sid = clientSId;
    pthread_create(&WriteThread2, NULL, &thread_ForSessionWrite, (void*)&arg2);
    pthread_join(WriteThread1, NULL);
    pthread_join(WriteThread2, NULL);

    pthread_exit(0);
}

int main(int argc, char *argv[])
{
    int ret;
    int i = 0;
    threadArgs_t arg1;
    threadArgs_t arg2;
    int SID = 0;
    pthread_t devWriteThread1;
    pthread_t devWriteThread2;
    struct st_DeviceStInfo devStInfo;
    pthread_t threadID_Login;
    pthread_t clientThread;
    char *myUID = (char *)argv[1];
    char *connectingUID = (char *)argv[2];


    if(argc < 3)
    {
        printf("No UID input!!!\n");
        printf("IOTCAPIs_Device [myUID] [connectingDeviceUID]\n");
        return 0;
    }

    // Must be first call before IOTC_Initialize(), but not a must
    IOTC_Set_Max_Session_Number(MAX_SESSION);

    printf("IOTCAPIs_Device start...\n");
    ret = IOTC_Initialize2(0);
    printf("Step 1: IOTC_Initialize() ret = %d\n", ret);
    if(ret != IOTC_ER_NoERROR)
    {
        printf("IOTCAPIs_Device exit...!!\n");
        return 0;
    }

    pthread_create(&threadID_Login, NULL, &_thread_Login, (void *)myUID);
    pthread_detach(threadID_Login);

    pthread_create(&clientThread, NULL, &clientWorks, (void *)connectingUID);
    pthread_detach(clientThread);


    while(1)
    {

        printf("Step 2: Checking device status...\n");
        ret = IOTC_Get_Device_Status(&devStInfo);
        if (ret == IOTC_ER_NoERROR)
        {
            if(devStInfo.state == IOTC_DEV_ST_CONNECTING || devStInfo.state == IOTC_DEV_ST_CONNECTED_WAITING)
            {
                if (devStInfo.connectedSessionNum >= MAX_SESSION)
                {
                    printf("Exceed max session number\n");
                    sleep(5);
                    continue;
                }

                for (i = 0; i < devStInfo.newConnected; i++)
                {
                    ret = IOTC_Accept(&SID);
                    if (ret == IOTC_ER_NoERROR)
                    {
                        if (SID > -1)
                        {
                            printf("+D Step 3: IOTC_Accept() SID = %d\n", SID);
                            printf("+D Step4: Create a Thread to handle data read / write from this session\n");

                            arg1.chid = 0;
                            arg1.sid = SID;
                            pthread_create(&devWriteThread1, NULL, &thread_ForSessionWrite, (void*)&arg1);

                            arg2.chid = 1;
                            arg2.sid = SID;
                            pthread_create(&devWriteThread2, NULL, &thread_ForSessionWrite, (void*)&arg2);

                            pthread_detach(devWriteThread1);
                            pthread_detach(devWriteThread2);
                        }
                    }
                    else
                    {
                        printf("IOTC_Accept failed error code = %d\n", ret);
                        break;
                    }
                }
            }
            else
            {
                printf("Device state = %d\n", devStInfo.state);
            }
        }

        if (ret < 0)
        {
            printf("Error occurs - %d\n", ret);
            break;
        }

        sleep(2);
    }

    IOTC_DeInitialize();

    return 0;
}

