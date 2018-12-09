#ifndef _AVAPIs2_EMULATER_H_
#define _AVAPIs2_EMULATER_H_

#define EMULATER_STREAM_NUM 5

typedef enum
{
    EMULATER_MODE_SINGLESTREAM  = 0,
    EMULATER_MODE_MULTISTREAM   = 1
}EMULATER_MODE;

typedef int (*Emulater_SendVideoFunc)(int streamID, int timestamp, char* buf, int size, int frameType);

typedef int (*Emulater_SendAudioFunc)(int timestamp, char* buf, int size);

int Emulater_Initialize(int mode, int initStream, Emulater_SendVideoFunc sendVideoFunc, Emulater_SendAudioFunc sendAudioFunc);
int Emulater_DeInitialize();
int Emulater_ChangeStream(int selectStream);

#endif //_AVAPIs2_EMULATER_H_
