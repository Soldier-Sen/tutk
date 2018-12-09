#ifndef _AVAPI2_TIMESYNC_H_
#define _AVAPI2_TIMESYNC_H_

#include <pthread.h>
#include "P2PCam/AVFRAMEINFO.h"
#include "AVAPIs2_FQ.h"

#define MAX_ANALYSIS_DATA_SLOT_NUMBER 10

#define DEFAULT_AUDIO_BUFFER_LOWEST 100
#define DEFAULT_AUDIO_BUFFER_LOWER  200
#define DEFAULT_AUDIO_BUFFER_MIDDLE 300
#define DEFAULT_AUDIO_BUFFER_UPPER  500

#define MAX_AUDIO_BUFFER_LOWER  1000
#define MAX_AUDIO_BUFFER_MIDDLE 2000
#define MAX_AUDIO_BUFFER_UPPER  3400

#define MAX_VIDEO_PENDING_TIME  3000
#define MAX_AUDIO_PENDING_TIME  3000

#define TIME_SYNC_TRIGGER_INTERVAL 50

typedef int (*addAudioDecodeQueue)(void* pTimeSyncInfo, char* pFrameData, int nActualFrameSize, FRAMEINFO_t* pFrameInfo, int nFrameInfoSize, int frmNo);
typedef int (*addVideoDecodeQueue)(void* pTimeSyncInfo, char* pFrameData, int nActualFrameSize, FRAMEINFO_t* pFrameInfo, int nFrameInfoSize, int frmNo);

typedef enum
{
    WALLCLOCK_INIT,
    WALLCLOCK_START,
    WALLCLOCK_PAUSE,
    WALLCLOCK_GET,
    WALLCLOCK_SPEED_1X,
    WALLCLOCK_SPEED_2X,
    WALLCLOCK_SPEED_HALFX,
    WALLCLOCK_SPEED_QUALX
} WallClockStatus;

typedef struct _WallClock_t_
{
    unsigned int ms;
    int speed;
    int slow;
    struct timeval sysTime;
    WallClockStatus status;
} WallClock_t;

typedef struct _AnalysisData
{
    unsigned int uTimeStamp;
    unsigned int uLastTimeStamp;
    unsigned int uAQMin;
    unsigned int uAQMax;
    unsigned int uVQMin;
    unsigned int uVQMax;
}AnalysisData;

typedef struct _AnalysisDataSlot
{
    unsigned short usCount;
    unsigned short usIndex;
    unsigned int uVersion;
    unsigned int uDataSize;
    AnalysisData m_Data[MAX_ANALYSIS_DATA_SLOT_NUMBER];
}AnalysisDataSlot;

typedef enum
{
    TIMESYNC_MODE_AUDIOFIRST,
    TIMESYNC_MODE_VIDEOFIRST,
    TIMESYNC_MODE_OTHERCLOCK
} TIMESYNC_MODE;

typedef enum
{
    DROPFRAME_MODE_PLAY_ALL,
    DROPFRAME_MODE_PLAY_IFRAME_ONLY,
    DROPFRAME_MODE_DROP_ALL,
    DROPFRAME_MODE_DROP_UNTIL_NEAR_IFRAME,
    DROPFRAME_MODE_DROP_UNTIL_NEXT_IFRAME,
} DROPFRAME_MODE;

typedef struct _TIMESYNC_Info
{
    int index;
    Frame_Queue video_queue;
    Frame_Queue audio_queue;
    WallClock_t clock;
    AnalysisDataSlot anaDataSlot;
    pthread_t ThreadTimeSync_ID;
    int isShowing;
    int isListening;
    int thread_status;
    int startRead;
    int useSystemTimestamp;
    int decodeITime;
    int decodePTime;
    long videoPendingTime;
    unsigned int waitAudioStartTime;
    unsigned int prevReadAudioTime;
    unsigned int prevReadVideoTime;
    unsigned int firstAudioTime;
    unsigned int firstVideoTime;
    unsigned int popVideo;
    unsigned int popAudio;
    unsigned int avgAudioTime;
    unsigned int firstTimeSync;
    unsigned int buffer_time_lower;
    unsigned int buffer_time_middle;
    unsigned int buffer_time_upper;

    addAudioDecodeQueue addAudio;
    addVideoDecodeQueue addVideo;
}TIMESYNC_Info;

int TimeSync_Initialize(TIMESYNC_Info *pTimeSyncInfo, int index, addVideoDecodeQueue pAddVideo, addVideoDecodeQueue pAddAudio);
void TimeSync_DeInitialize(TIMESYNC_Info *pTimeSyncInfo);
void TimeSync_VideoEnable(TIMESYNC_Info *pTimeSyncInfo, int enable);
void TimeSync_AudioEnable(TIMESYNC_Info *pTimeSyncInfo, int enable);
int TimeSync_InsertAudio(TIMESYNC_Info *pTimeSyncInfo, char* pFrameData, int nFrameSize, int frmNo, FRAMEINFO_t* pFrameInfo);
int TimeSync_HandleAudioLost(TIMESYNC_Info *pTimeSyncInfo, int frmNo);
int TimeSync_InsertVideo(TIMESYNC_Info *pTimeSyncInfo, char* pFrameData, int nActualFrameSize, int frmNo, FRAMEINFO_t* pFrameInfo);

#endif //_AVAPI2_TIMESYNC_H_

