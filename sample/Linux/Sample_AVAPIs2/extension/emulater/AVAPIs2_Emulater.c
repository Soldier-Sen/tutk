#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#include "P2PCam/AVFRAMEINFO.h"
#include "P2PCam/AVIOCTRLDEFs.h"
#include "AVAPIs2_Emulater.h"

static char gEmulaterVideoStreamFile[EMULATER_STREAM_NUM][128] = {
    "video_multi/beethoven_1080.multi",
    "video_multi/beethoven_720.multi",
    "video_multi/beethoven_480.multi",
    "video_multi/beethoven_360.multi",
    "video_multi/beethoven_240.multi"};

static char gEmulaterAudioStreamFile[128] = {
    "audio_raw/beethoven_8k_16bit_mono.raw"};

#define VIDEO_BUF_SIZE	(1024 * 400)        // Video buffer size , Using at Streamout_VideoThread
#define AUDIO_BUF_SIZE	(1024)              // Audio buffer size , Using at Streamout_AudioThread
#define AUDIO_FRAME_SIZE 640
#define AUDIO_FPS 25

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

typedef struct _EmulaterVideoStream
{
    char stream_bin[128];
    char stream_info[128];
    FILE *streamBin_fp;
    FILE *streamInfo_fp;
    int fps;
}EmulaterVideoStream;

typedef struct _EmulaterAudioStream
{
    char stream_bin[128];
    FILE *streamBin_fp;
    int fps;
}EmulaterAudioStream;

typedef struct _EmulaterInfo
{
    int mode;
    int selectStream;
    int selectStreamOld;
    int selectChange;
    int threadRun;
    WallClock_t clock;
    EmulaterVideoStream videoStream[EMULATER_STREAM_NUM];
    EmulaterAudioStream audioStream;

    Emulater_SendVideoFunc sendVideoFunc;
    Emulater_SendAudioFunc sendAudioFunc;
}EmulaterInfo;


static int gInit = 0;
static EmulaterInfo gEmulaterInfo;

int getTimevalDiff(struct timeval x , struct timeval y)
{
    int x_ms , y_ms , diff;

    x_ms = x.tv_sec*1000 + x.tv_usec/1000;
    y_ms = y.tv_sec*1000 + y.tv_usec/1000;

    diff = y_ms - x_ms;

    return diff;
}

unsigned int WallClock(WallClock_t *clock, WallClockStatus status)
{
    struct timeval now;
    unsigned int pauseTime = 0;
    gettimeofday(&now, NULL);
    
    if(status == WALLCLOCK_INIT){
        memset(clock, 0, sizeof(WallClock_t));
        clock->status = WALLCLOCK_INIT;
        clock->speed = 1;
        clock->slow = 1;
    }
    else if(status == WALLCLOCK_START){
        if(clock->status == WALLCLOCK_INIT){
            memcpy(&clock->sysTime, &now, sizeof(struct timeval));
            clock->status = WALLCLOCK_START;
            clock->ms = 0;
        }
        else if(clock->status == WALLCLOCK_PAUSE){
            pauseTime = (unsigned int)getTimevalDiff(clock->sysTime, now);
            memcpy(&clock->sysTime, &now, sizeof(struct timeval));
            clock->status = WALLCLOCK_START;
            return pauseTime;
        }
    }
    else if(status == WALLCLOCK_PAUSE){
        clock->ms += (((unsigned int)getTimevalDiff(clock->sysTime, now)*clock->speed))/clock->slow;
        memcpy(&clock->sysTime, &now, sizeof(struct timeval));
        clock->status = WALLCLOCK_PAUSE;
        return clock->ms;
    }
    else if(status == WALLCLOCK_GET){
        if(clock->status == WALLCLOCK_START){
            clock->ms += (((unsigned int)getTimevalDiff(clock->sysTime, now)*clock->speed))/clock->slow;
            memcpy(&clock->sysTime, &now, sizeof(struct timeval));
        }
        return clock->ms;
    }
    else if(status == WALLCLOCK_SPEED_1X){
        clock->speed = 1;
        clock->slow = 1;
    }
    else if(status == WALLCLOCK_SPEED_2X){
        clock->speed = 2;
        clock->slow = 1;
    }
    else if(status == WALLCLOCK_SPEED_HALFX){
        clock->speed = 1;
        clock->slow = 2;
    }
    else if(status == WALLCLOCK_SPEED_QUALX){
        clock->speed = 1;
        clock->slow = 4;
    }
    return 0;
}

int Emulater_Open(EmulaterInfo *pEmulaterInfo, int mode)
{
    int index = 0;
    char line[128] = {0}, *read = NULL;

    memset(pEmulaterInfo, 0, sizeof(EmulaterInfo));
    WallClock(&pEmulaterInfo->clock, WALLCLOCK_INIT);

    pEmulaterInfo->mode = mode;

    for(index = 0 ; index < EMULATER_STREAM_NUM ; index++){
        sprintf(pEmulaterInfo->videoStream[index].stream_bin, "%s/frames.bin", gEmulaterVideoStreamFile[index]);
        sprintf(pEmulaterInfo->videoStream[index].stream_info, "%s/frames.info", gEmulaterVideoStreamFile[index]);

        pEmulaterInfo->videoStream[index].streamBin_fp = fopen(pEmulaterInfo->videoStream[index].stream_bin, "rb");
        if(pEmulaterInfo->videoStream[index].streamBin_fp == NULL){
            printf("[%s:%d]: Video Bin \'%s\' open error!!\n", __FUNCTION__, __LINE__, pEmulaterInfo->videoStream[index].stream_bin);
            goto EMULATER_OPEN_ERROR;
        }
        pEmulaterInfo->videoStream[index].streamInfo_fp = fopen(pEmulaterInfo->videoStream[index].stream_info, "rb");
        if(pEmulaterInfo->videoStream[index].streamInfo_fp == NULL){
            printf("[%s:%d]: Video Info \'%s\' open error!!\n", __FUNCTION__, __LINE__, pEmulaterInfo->videoStream[index].stream_info);
            goto EMULATER_OPEN_ERROR;
        }

        if((read = fgets(line, sizeof(line), pEmulaterInfo->videoStream[index].streamInfo_fp)) == NULL){
            printf("[%s:%d]: Read Video Info \'%s\' error!!\n", __FUNCTION__, __LINE__, pEmulaterInfo->videoStream[index].stream_info);
            goto EMULATER_OPEN_ERROR;
        }
        sscanf(line, "FPS %d\n", &pEmulaterInfo->videoStream[index].fps);
    }

    sprintf(pEmulaterInfo->audioStream.stream_bin, "%s", gEmulaterAudioStreamFile);
    pEmulaterInfo->audioStream.streamBin_fp = fopen(pEmulaterInfo->audioStream.stream_bin, "rb");
    if(pEmulaterInfo->audioStream.streamBin_fp == NULL){
        printf("[%s:%d]: Audio Bin \'%s\' open error!!\n", __FUNCTION__, __LINE__, pEmulaterInfo->audioStream.stream_bin);
        goto EMULATER_OPEN_ERROR;
    }

    return 0;

EMULATER_OPEN_ERROR:
    for(index = 0 ; index < EMULATER_STREAM_NUM ; index++){
        if(pEmulaterInfo->videoStream[index].streamBin_fp != NULL){
            fclose(pEmulaterInfo->videoStream[index].streamBin_fp);
            pEmulaterInfo->videoStream[index].streamBin_fp = NULL;
        }
        if(pEmulaterInfo->videoStream[index].streamInfo_fp != NULL){
            fclose(pEmulaterInfo->videoStream[index].streamInfo_fp);
            pEmulaterInfo->videoStream[index].streamInfo_fp = NULL;
        }
    }
    if(pEmulaterInfo->audioStream.streamBin_fp != NULL){
        fclose(pEmulaterInfo->audioStream.streamBin_fp);
        pEmulaterInfo->audioStream.streamBin_fp = NULL;
    }
    return -1;
}

int Emulater_Close(EmulaterInfo *pEmulaterInfo)
{
    int index = 0;

    for(index = 0 ; index < EMULATER_STREAM_NUM ; index++){
        if(pEmulaterInfo->videoStream[index].streamBin_fp != NULL){
            fclose(pEmulaterInfo->videoStream[index].streamBin_fp);
            pEmulaterInfo->videoStream[index].streamBin_fp = NULL;
        }
        if(pEmulaterInfo->videoStream[index].streamInfo_fp != NULL){
            fclose(pEmulaterInfo->videoStream[index].streamInfo_fp);
            pEmulaterInfo->videoStream[index].streamInfo_fp = NULL;
        }
    }
    if(pEmulaterInfo->audioStream.streamBin_fp != NULL){
        fclose(pEmulaterInfo->audioStream.streamBin_fp);
        pEmulaterInfo->audioStream.streamBin_fp = NULL;
    }

    return -1;
}

int Emulater_Rewind(EmulaterInfo *pEmulaterInfo)
{
    int index = 0;
    char line[128] = {0}, *read = NULL;

    for(index = 0 ; index < EMULATER_STREAM_NUM ; index++){
        rewind(pEmulaterInfo->videoStream[index].streamBin_fp);
        rewind(pEmulaterInfo->videoStream[index].streamInfo_fp);

        if((read = fgets(line, sizeof(line), pEmulaterInfo->videoStream[index].streamInfo_fp)) == NULL){
            printf("[%s:%d]: Read Video Info \'%s\' error!!\n", __FUNCTION__, __LINE__, pEmulaterInfo->videoStream[index].stream_info);
            return -1;
        }
        sscanf(line, "FPS %d\n", &pEmulaterInfo->videoStream[index].fps);
    }
    rewind(pEmulaterInfo->audioStream.streamBin_fp);

    return 0;
}

int Emulater_ReadVideo(EmulaterInfo *pEmulaterInfo, int streamID, char* buf, int size, int* frameType)
{
    EmulaterVideoStream* pVideoStream = &(pEmulaterInfo->videoStream[streamID]);
    char line[128] = {0}, *read = NULL, frame_type[2] = {0};
    int frame_pos = 0, frame_size = 0, nRet = 0;

    if((read = fgets(line, sizeof(line), pVideoStream->streamInfo_fp)) == NULL) {
        //TODO : Rewind
        return 0;
    }

    sscanf(line, "%c %d %d\n", frame_type, &frame_pos, &frame_size);

    if(frame_size > size){
        return -1;
    }
    *frameType = (strcmp(frame_type, "I") == 0 ? IPC_FRAME_FLAG_IFRAME : IPC_FRAME_FLAG_PBFRAME);

    fseek(pVideoStream->streamBin_fp, frame_pos*sizeof(char), SEEK_SET);
    nRet = fread(buf, 1, frame_size, pVideoStream->streamBin_fp);

    return nRet;
}

int Emulater_ReadVideoOnlyInfo(EmulaterInfo *pEmulaterInfo, int streamID, int* frameType)
{
    EmulaterVideoStream* pVideoStream = &(pEmulaterInfo->videoStream[streamID]);
    char line[128] = {0}, *read = NULL, frame_type[2] = {0};
    int frame_pos = 0, frame_size = 0;

    if((read = fgets(line, sizeof(line), pVideoStream->streamInfo_fp)) == NULL) {
        //TODO : Rewind
        return 0;
    }

    sscanf(line, "%c %d %d\n", frame_type, &frame_pos, &frame_size);
    *frameType = (strcmp(frame_type, "I") == 0 ? IPC_FRAME_FLAG_IFRAME : IPC_FRAME_FLAG_PBFRAME);

    return 1;
}

int Emulater_ReadAudio(EmulaterInfo *pEmulaterInfo, char* buf, int size)
{
    EmulaterAudioStream* pAudioStream = &(pEmulaterInfo->audioStream);
    int nRet = 0;

    if(feof(pAudioStream->streamBin_fp))
        return 0;

    nRet = fread(buf, 1, size, pAudioStream->streamBin_fp);

    return nRet;
}

void *Emulater_StreamoutThread(void *arg)
{
    EmulaterInfo *pEmulaterInfo = (EmulaterInfo *)arg;
    struct timeval tv, tv2;
    int recycle = 1, index = 0, mode = pEmulaterInfo->mode, sleepTime = 0, roundtime = 0, frameType = 0, ret = 0;
    int readVideo = 0, readAudio = 0, audioTimestamp = 0, videoTimestamp = 0, playTime = 0, videofps = 0, audiofps = 0;
    int audioSleepTime = 0, videoSleepTime = 0, selectStreamOld = 0;
    char vBuf[VIDEO_BUF_SIZE] = {0};
    char aBuf[AUDIO_BUF_SIZE] = {0};

    videofps = pEmulaterInfo->videoStream[0].fps;
    audiofps = AUDIO_FPS;

    //printf("Emulater_StreamoutThread: Start\n");
    WallClock(&pEmulaterInfo->clock, WALLCLOCK_SPEED_1X);
    WallClock(&pEmulaterInfo->clock, WALLCLOCK_START);

    while(pEmulaterInfo->threadRun)
    {
        gettimeofday(&tv, NULL);
        if(recycle){
            printf("[%s:%d] Emulater_Rewind playTime[%d]\n", __FUNCTION__, __LINE__, playTime);
            if(Emulater_Rewind(pEmulaterInfo) < 0){
                printf("[%s:%d] rewind error, exit thread\n", __FUNCTION__, __LINE__);
                goto EXIT_STREAMOUT_EMULATER;
            }
            WallClock(&pEmulaterInfo->clock, WALLCLOCK_INIT);
            WallClock(&pEmulaterInfo->clock, WALLCLOCK_SPEED_1X);
            WallClock(&pEmulaterInfo->clock, WALLCLOCK_START);
            audioTimestamp = 0;
            videoTimestamp = 0;
            recycle = 0;
        }

        playTime = WallClock(&pEmulaterInfo->clock, WALLCLOCK_GET);
        //printf("playTime[%d] videoTimestamp[%d] audioTimestamp[%d]\n", playTime, videoTimestamp, audioTimestamp);
        readVideo = readAudio = 0;
        if(videoTimestamp <= playTime){
            readVideo = 1;
            videoTimestamp += (1000/videofps);
        }
        videoSleepTime = (videoTimestamp - playTime);
        
        if(audioTimestamp <= playTime){
            readAudio = 1;
            audioTimestamp += (1000/audiofps);
        }
        audioSleepTime = (audioTimestamp - playTime);

        sleepTime = audioSleepTime <= videoSleepTime ? audioSleepTime : videoSleepTime;

        if(readVideo){
            //Read Video
            if(mode == EMULATER_MODE_MULTISTREAM){
                for(index = 0; index < EMULATER_STREAM_NUM ; index++){
                    ret = Emulater_ReadVideo(pEmulaterInfo, index, vBuf, VIDEO_BUF_SIZE, &frameType);
                    if(ret == 0){
                        recycle = 1;
                    }
                    else if(ret == -1){
                        printf("[%s:%d] video buffer too small\n", __FUNCTION__, __LINE__);
                    }
                    else if(ret > 0){
                        //Send Video
                        if(pEmulaterInfo->sendVideoFunc != NULL)
                            pEmulaterInfo->sendVideoFunc(index, videoTimestamp, vBuf, ret, frameType);
                    }
                    else{
                        printf("[%s:%d] read error ret[%d], exit thread\n", __FUNCTION__, __LINE__, ret);
                        goto EXIT_STREAMOUT_EMULATER;
                    }
                }
            }
            else{
                for(index = 0; index < EMULATER_STREAM_NUM ; index++){
                    if(pEmulaterInfo->selectStream != index && pEmulaterInfo->selectStreamOld != index){
                        ret = Emulater_ReadVideoOnlyInfo(pEmulaterInfo, index, &frameType);
                        if(ret == 0){
                            recycle = 1;
                        }
                        continue;
                    }

                    ret = Emulater_ReadVideo(pEmulaterInfo, index, vBuf, VIDEO_BUF_SIZE, &frameType);
                    if(ret == 0){
                        recycle = 1;
                    }
                    else if(ret == -1){
                        printf("[%s:%d] video buffer too small\n", __FUNCTION__, __LINE__);
                    }
                    else if(ret > 0){
                        if(pEmulaterInfo->selectChange){
                            if(frameType == IPC_FRAME_FLAG_IFRAME){
                                selectStreamOld = pEmulaterInfo->selectStreamOld;
                                pEmulaterInfo->selectStreamOld = pEmulaterInfo->selectStream;
                                pEmulaterInfo->selectChange = 0;

                                if(index == selectStreamOld)
                                    continue;
                            }
                            else{
                                if(index == pEmulaterInfo->selectStream)
                                    continue;
                            }
                        }

                        //Send Video
                        if(pEmulaterInfo->sendVideoFunc != NULL)
                            pEmulaterInfo->sendVideoFunc(index, videoTimestamp, vBuf, ret, frameType);
                    }
                    else{
                        printf("[%s:%d] read error ret[%d], exit thread\n", __FUNCTION__, __LINE__, ret);
                        goto EXIT_STREAMOUT_EMULATER;
                    }
                }
            }
        }

        //Read Audio
        if(readAudio){
            ret = Emulater_ReadAudio(pEmulaterInfo, aBuf, AUDIO_FRAME_SIZE);
            if(ret == 0){
                recycle = 1;
            }
            else if(ret > 0){
                //Send Audio
                if(pEmulaterInfo->sendAudioFunc != NULL)
                    pEmulaterInfo->sendAudioFunc(audioTimestamp, aBuf, ret);
            }
            else{
                printf("[%s:%d] read error ret[%d], exit thread\n", __FUNCTION__, __LINE__, ret);
                goto EXIT_STREAMOUT_EMULATER;
            }
        }
        gettimeofday(&tv2, NULL);

        roundtime = getTimevalDiff(tv, tv2);
        sleepTime -= roundtime;
        if(sleepTime > 0){
            usleep((sleepTime-1)*1000);
        }
    }

EXIT_STREAMOUT_EMULATER:
    pEmulaterInfo->threadRun = 2;
    pthread_exit(0);
}

int Emulater_Initialize(int mode, int initStream, Emulater_SendVideoFunc sendVideoFunc, Emulater_SendAudioFunc sendAudioFunc)
{
    int nRet = 0;
    pthread_t ThreadEmulater_ID;

    if(mode < EMULATER_MODE_SINGLESTREAM || mode > EMULATER_MODE_MULTISTREAM)
        return -1;

    if(gInit)
        return 0;

    //Open File for Read
    if(Emulater_Open(&gEmulaterInfo, mode) < 0){
        printf("[%s:%d] Emulater_Open error!!\n", __FUNCTION__, __LINE__);
        return -1;
    }
    gEmulaterInfo.mode = mode;
    gEmulaterInfo.selectStream = initStream;
    gEmulaterInfo.selectStreamOld = initStream;
    gEmulaterInfo.selectChange = 0;
    gEmulaterInfo.sendVideoFunc = sendVideoFunc;
    gEmulaterInfo.sendAudioFunc = sendAudioFunc;

    gEmulaterInfo.threadRun = 1;
    if((nRet = pthread_create(&ThreadEmulater_ID, NULL, &Emulater_StreamoutThread, (void*)&gEmulaterInfo))){
        printf("[%s:%d] pthread_create ThreadEmulater_ID nRet[%d]\n", __FUNCTION__, __LINE__, nRet);
        return -1;
    }
    pthread_detach(ThreadEmulater_ID);

    gInit = 1;

    return 0;
}

int Emulater_DeInitialize()
{
    if(gInit)
        return 0;

    gEmulaterInfo.threadRun = 0;
    while(gEmulaterInfo.threadRun != 2){
        usleep(100000);
    }

    Emulater_Close(&gEmulaterInfo);

    gInit = 0;

    return 0;
}

int Emulater_ChangeStream(int selectStream)
{
    gEmulaterInfo.selectStreamOld = gEmulaterInfo.selectStream;
    gEmulaterInfo.selectStream = selectStream;
    gEmulaterInfo.selectChange = 1;

    return 0;
}

