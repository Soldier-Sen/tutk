#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include "AVAPIs_AudioPlay.h"

typedef struct _AV_AudioBlock
{
    unsigned int nTimestamp;
    int nDataSize;
    int nFrmNo;
    char data[MAX_AUDIO_BLOCK_SIZE];
}AV_AudioBlock;

typedef struct _AV_AudioRingBuffer
{
    AV_AudioBlock *block;
    int nMaxSize;
    int nNowSize;
    int nPutIndex;
    int nPrevFrmNo;
    unsigned int nPrevTimeStamp;
    unsigned int nLastPutTime;

    int nPlayIndex;
    int nPlayStart;
    unsigned int nPlayDelay;
    unsigned int nPlaySysTime;
    unsigned int nPlayRcvTime;

    int nWaitUntil;

    pthread_mutex_t mutex;
}AV_AudioRingBuffer;

static unsigned int audioPlay_GetTime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec*1000 + tv.tv_usec/1000);
}

AVAPLAY audioPlay_init(int nMaxSize, unsigned int nDelay)
{
    AV_AudioRingBuffer *pRingBuf = NULL;

    pRingBuf = malloc(sizeof(AV_AudioRingBuffer));
    if(pRingBuf == NULL)
        return NULL;

    memset(pRingBuf, 0, sizeof(AV_AudioRingBuffer));

    pRingBuf->nMaxSize = nMaxSize;
    pRingBuf->nPlayDelay = nDelay;
    pRingBuf->block = malloc(sizeof(AV_AudioBlock)*nMaxSize);
    if(pRingBuf->block == NULL){
        free(pRingBuf);
        return NULL;
    }

    pthread_mutex_init(&pRingBuf->mutex, NULL);

    return (AVAPLAY)pRingBuf;
}

void audioPlay_deinit(AVAPLAY pAVAPlay)
{
    AV_AudioRingBuffer *pRingBuf = (AV_AudioRingBuffer *)pAVAPlay;

    pthread_mutex_destroy(&pRingBuf->mutex);

    free(pRingBuf->block);
    pRingBuf->block = NULL;
    free(pRingBuf);
}

int audioPlay_put(AVAPLAY pAVAPlay, char* pBuf, int nDataSize, unsigned int nTimestamp, int nFrmNo)
{
    AV_AudioRingBuffer *pRingBuf = (AV_AudioRingBuffer*)pAVAPlay;
    unsigned int timeAvg = 0, nextTimeStamp = 0;
    int index = 0;

    if(pBuf == NULL || nDataSize <= 0)
        return -1;

    if(nDataSize > MAX_AUDIO_BLOCK_SIZE){
        printf("Error : MAX_AUDIO_BLOCK_SIZE(%d) smaller then nDataSize(%d)\n", MAX_AUDIO_BLOCK_SIZE, nDataSize);
        return -1;
    }

    pthread_mutex_lock(&pRingBuf->mutex);

    //Check Audio Lost
    if(pRingBuf->nNowSize > 0 && (nFrmNo - pRingBuf->nPrevFrmNo) > 1){
        index = nFrmNo - pRingBuf->nPrevFrmNo;
        timeAvg = (nTimestamp - pRingBuf->nPrevTimeStamp)/index;
        nextTimeStamp = pRingBuf->nPrevTimeStamp+timeAvg;

        index -= 1;
        for( ; index > 0 ; index--){
            //Queue Full, check playIndex before replace old data
            if(pRingBuf->nNowSize >= pRingBuf->nMaxSize && pRingBuf->nPutIndex == pRingBuf->nPlayIndex){
                printf("Warning : Ring buffer full, play too slow\n");
                pRingBuf->nPlayIndex = (pRingBuf->nPlayIndex+1)%pRingBuf->nMaxSize;
            }

            pRingBuf->block[pRingBuf->nPutIndex].nDataSize = nDataSize;
            pRingBuf->block[pRingBuf->nPutIndex].nTimestamp = nextTimeStamp;
            pRingBuf->block[pRingBuf->nPutIndex].nFrmNo = nFrmNo-index;
            memcpy(pRingBuf->block[pRingBuf->nPutIndex].data, pBuf, nDataSize);

            pRingBuf->nPutIndex = (pRingBuf->nPutIndex+1)%pRingBuf->nMaxSize;
            pRingBuf->nNowSize = (pRingBuf->nNowSize+1)%(pRingBuf->nMaxSize+1);
            pRingBuf->nLastPutTime = audioPlay_GetTime();

            nextTimeStamp += timeAvg;
        }
    }

    //Queue Full, check playIndex before replace old data
    if(pRingBuf->nNowSize >= pRingBuf->nMaxSize && pRingBuf->nPutIndex == pRingBuf->nPlayIndex){
        printf("Warning : Ring buffer full, play too slow\n");
        pRingBuf->nPlayIndex = (pRingBuf->nPlayIndex+1)%pRingBuf->nMaxSize;
    }

    pRingBuf->block[pRingBuf->nPutIndex].nDataSize = nDataSize;
    pRingBuf->block[pRingBuf->nPutIndex].nTimestamp = nTimestamp;
    pRingBuf->block[pRingBuf->nPutIndex].nFrmNo = nFrmNo;
    memcpy(pRingBuf->block[pRingBuf->nPutIndex].data, pBuf, nDataSize);

    pRingBuf->nPutIndex = (pRingBuf->nPutIndex+1)%pRingBuf->nMaxSize;
    pRingBuf->nNowSize = (pRingBuf->nNowSize+1)%(pRingBuf->nMaxSize+1);

    pRingBuf->nPrevTimeStamp = nTimestamp;
    pRingBuf->nPrevFrmNo = nFrmNo;
    pRingBuf->nLastPutTime = audioPlay_GetTime();

    //printf("nMaxSize[%d] nNowSize[%d] nPutIndex[%d] nPrevFrmNo[%d] nPrevTimeStamp[%u] nLastPutTime[%u]\n", pRingBuf->nMaxSize, pRingBuf->nNowSize, pRingBuf->nPutIndex, pRingBuf->nPrevFrmNo, pRingBuf->nPrevTimeStamp, pRingBuf->nLastPutTime);
    pthread_mutex_unlock(&pRingBuf->mutex);

    return 0;
}

int audioPlay_play(AVAPLAY pAVAPlay, char* pBuf, int nBufSize, unsigned int *nTimeStamp, int *nFrmNo)
{
    AV_AudioRingBuffer *pRingBuf = (AV_AudioRingBuffer*)pAVAPlay;
    int nLen = 0, nIndex = 0;
    unsigned int nBufDuration = 0, nNowTime = 0;

    if(pBuf == NULL || nBufSize < MAX_AUDIO_BLOCK_SIZE)
        return -1;

    pthread_mutex_lock(&pRingBuf->mutex);
    nNowTime = audioPlay_GetTime();

    //Check Buffer Duration
    if(pRingBuf->nPlayStart == 0){
        if(pRingBuf->nNowSize == 0){
            printf("audioPlay_play : Wait Buffer Duration[%d] > nPlayDelay[%d]\n", nBufDuration, pRingBuf->nPlayDelay);
            pthread_mutex_unlock(&pRingBuf->mutex);
            return 0;
        }

        nIndex = (pRingBuf->nPutIndex == 0) ? (pRingBuf->nMaxSize-1) : (pRingBuf->nPutIndex-1);
        nBufDuration = pRingBuf->block[nIndex].nTimestamp - pRingBuf->block[pRingBuf->nPlayIndex].nTimestamp;
        if(nBufDuration >= pRingBuf->nPlayDelay){
            printf("audioPlay_play : Start Play\n");
            pRingBuf->nPlayStart = 1;
            pRingBuf->nPlaySysTime = nNowTime;
            pRingBuf->nPlayRcvTime = pRingBuf->block[pRingBuf->nPlayIndex].nTimestamp;
        }
        else{
            printf("audioPlay_play : Wait Buffer Duration[%d] > nPlayDelay[%d]\n", nBufDuration, pRingBuf->nPlayDelay);
            pthread_mutex_unlock(&pRingBuf->mutex);
            return 0;
        }
    }

    if(pRingBuf->nNowSize > 0 && (pRingBuf->nNowSize >= pRingBuf->nWaitUntil)){
        //printf("sysDiff[%d] playDiff[%d]\n", nNowTime - pRingBuf->nPlaySysTime, pRingBuf->block[pRingBuf->nPlayIndex].nTimestamp - pRingBuf->nPlayRcvTime);
        if((nNowTime - pRingBuf->nPlaySysTime) >= (pRingBuf->block[pRingBuf->nPlayIndex].nTimestamp - pRingBuf->nPlayRcvTime)){

            memcpy(pBuf, pRingBuf->block[pRingBuf->nPlayIndex].data, pRingBuf->block[pRingBuf->nPlayIndex].nDataSize);
            nLen = pRingBuf->block[pRingBuf->nPlayIndex].nDataSize;
            *nTimeStamp = pRingBuf->block[pRingBuf->nPlayIndex].nTimestamp;
            *nFrmNo = pRingBuf->block[pRingBuf->nPlayIndex].nFrmNo;

            if(--pRingBuf->nNowSize < 0)
                pRingBuf->nNowSize = 0;
            //printf("audioPlay_play : Play Audio nFrmNo[%d] nDataSize[%d] nTimestamp[%u] nNowSize[%d]\n", pRingBuf->block[pRingBuf->nPlayIndex].nFrmNo, pRingBuf->block[pRingBuf->nPlayIndex].nDataSize, pRingBuf->block[pRingBuf->nPlayIndex].nTimestamp, pRingBuf->nNowSize);
            pRingBuf->nPlayIndex = (pRingBuf->nPlayIndex+1)%pRingBuf->nMaxSize;
            pRingBuf->nWaitUntil = 0;
        }
        //else{
            //printf("audioPlay_play : Wait SysTime[%d] > PlayTime[%d]\n", nNowTime - pRingBuf->nPlaySysTime, pRingBuf->block[pRingBuf->nPlayIndex].nTimestamp - pRingBuf->nPlayRcvTime);
        //}
    }
    else{
        printf("audioPlay_play : Do Audio Play, already pending [%d] ms\n", audioPlay_GetTime() - pRingBuf->nLastPutTime);
        pRingBuf->nWaitUntil = 1;
    }

    pthread_mutex_unlock(&pRingBuf->mutex);

    return nLen;
}

