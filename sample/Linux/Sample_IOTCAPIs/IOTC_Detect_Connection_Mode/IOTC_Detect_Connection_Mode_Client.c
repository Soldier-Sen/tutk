#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "IOTCAPIs.h"

enum IOTC_CONN_MODE {
	IOTC_CONN_LAN_MODE,
	IOTC_CONN_P2P_MODE,
	IOTC_CONN_UDP_RLY_MODE,
	IOTC_CONN_TCP_RLY_MODE
};

// Type declaration
typedef struct ThreadParam_t
{
	int nSID;
	unsigned char szChID;
} ThreadParam;


static struct timeval start_time;


static unsigned int DiffTimeResult(struct timeval *start_time, struct timeval *end_time)
{
	if (start_time == NULL || end_time == NULL)
		return 0;

	return  (end_time->tv_sec - start_time->tv_sec) * 1000000 + end_time->tv_usec - start_time->tv_usec;
}


static void start_calc_conn_time() {
	gettimeofday(&start_time, NULL);
}


static void end_calc_conn_time() {
	struct timeval end_time;
	unsigned int conn_time = 0;

	gettimeofday(&end_time, NULL);
	conn_time = DiffTimeResult(&start_time, &end_time);

	printf("connection time (us): %d\n", conn_time);	
}


static void connect_mode_change_callback(int nIOTCSessionID, unsigned int nConnMode) {
	switch(nConnMode)
	{
	case IOTC_CONN_LAN_MODE:
		printf("Connection Mode is changed! LAN Mode!\n");
		break;
	case IOTC_CONN_P2P_MODE:
		printf("Connection Mode is changed! P2P Mode!\n");
		break;
	case IOTC_CONN_UDP_RLY_MODE:
		printf("Connection Mode is changed! UDP RLY Mode!\n");
		break;
	case IOTC_CONN_TCP_RLY_MODE:
		printf("Connection Mode is changed! TCP RLY Mode!\n");
		break;
	}

	end_calc_conn_time();
}


static void *thread_ForSessionRead(void *arg)
{
	ThreadParam *pParam = (ThreadParam *)arg;
	unsigned char ChID = pParam->szChID;
	int SID = pParam->nSID;
	
	free(pParam);

	int nRead;
	char buf[IOTC_MAX_PACKET_SIZE];
	struct st_SInfo Sinfo;	
	
	printf("Thread started...[%d]\n", ChID);
	
	IOTC_Session_Check(SID, &Sinfo);
	if(Sinfo.Mode == 0)
		printf("Client is from %s:%d[%s] Mode = P2P\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
	else if(Sinfo.Mode == 1)
		printf("Client is from %s:%d[%s] Mode = RLY\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
	else if(Sinfo.Mode == 2)
		printf("Client is from %s:%d[%s] Mode = LAN\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);

	printf("VPG [%d:%d:%d] Remote NAT[%d]\n", Sinfo.VID, Sinfo.PID, Sinfo.GID, Sinfo.NatType);
	
	IOTC_Session_Channel_ON(SID, ChID);
	sleep(3);
	
	while((nRead = IOTC_Session_Read(SID, buf, IOTC_MAX_PACKET_SIZE, 6000, ChID)) > -1) {
		buf[nRead] = 0;
		printf("[IOTC_Session_Read] SID=%d, ChID = %d, Size=%d, Data:%s\n",SID,ChID,nRead,buf);
		IOTC_Session_Write(SID, "Hello", strlen("Hello"), ChID);
	}

	// check if remote site close this session
	if(nRead == IOTC_ER_SESSION_CLOSE_BY_REMOTE) {
		printf("remote site close this session, SID = %d\n", SID);
		//IOTC_Session_Close(SID);
	} else if(nRead == IOTC_ER_REMOTE_TIMEOUT_DISCONNECT) {
		printf("disconnected due to remote site has no any response after a 'timeout' period of time., SID = %d\n", SID);
		//IOTC_Session_Close(SID);
	}
	
	printf("Thread exit...[%d]\n", ChID);
	
	pthread_exit(0);
}


static void create_read_thread(int SID, int ChID) {
	pthread_t Thread_ID;
	ThreadParam *tParam1 = (ThreadParam*)malloc(sizeof(ThreadParam));	
	
	tParam1->nSID = SID;
	tParam1->szChID = ChID;

	pthread_create(&Thread_ID, NULL, &thread_ForSessionRead, tParam1);
	pthread_detach(Thread_ID);
}


int main(int argc, char *argv[])
{
	int ret;
	int SID = -1;

	if(argc < 2) {
		printf("No UID input!!!\n");
		printf("IOTCAPIs_Client [UID]\n");
		return 0;
	}

	// Register connection mode change callback
	IOTC_ConnModeChange_CallBack(connect_mode_change_callback);

	// port 0 means to get a random port
	ret = IOTC_Initialize2(0);
	printf("Step 1: call IOTC_Initialize(), ret = %d\n", ret);
	if(ret != IOTC_ER_NoERROR) {
		printf("IOTCAPIs_Client exit...!!\n");
		return 0;
	}

	char *UID = (char *)argv[1];

	SID = IOTC_Get_SessionID();
	if(SID < 0) {
		printf("IOTC_Get_SessionID error code [%d]\n", SID);
		return 0;
	}

	start_calc_conn_time();
	ret = IOTC_Connect_ByUID_Parallel(UID, SID);
	printf("Step 2: call IOTC_Connect_ByUID_Parallel(%s).......\n", UID);

	struct st_SInfo Sinfo;
	IOTC_Session_Check(SID, &Sinfo);
	if(Sinfo.Mode == 0)
		printf("Device info %s:%d[%s] Mode=P2P\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
	else if(Sinfo.Mode == 1)
		printf("Device info %s:%d[%s] Mode=RLY\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);
	else if(Sinfo.Mode == 2)
		printf("Device info %s:%d[%s] Mode=LAN\n",Sinfo.RemoteIP, Sinfo.RemotePort, Sinfo.UID);

	end_calc_conn_time();

	if(ret >= 0) {
		printf("IOTC Connect OK SID[%d]\n", SID);
		create_read_thread(SID, 0);
	}
	else
		printf("IOTC_Connect error code[%d]\n", ret);

	int count = 0;
	while(count++ < 60) {
		sleep(1);
	}

	IOTC_Session_Close(SID);

	IOTC_DeInitialize();

	return 0;
}
