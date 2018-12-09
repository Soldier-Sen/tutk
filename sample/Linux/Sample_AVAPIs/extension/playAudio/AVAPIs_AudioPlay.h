#ifndef _AVAPI_AUDIOPLAY_H_
#define _AVAPI_AUDIOPLAY_H_

#define AVAPLAY void*
#define MAX_AUDIO_BLOCK_SIZE 640

AVAPLAY audioPlay_init(int nMaxSize, unsigned int nDelay);
void audioPlay_deinit(AVAPLAY pAVAPlay);
int audioPlay_put(AVAPLAY pAVAPlay, char* pBuf, int nDataSize, unsigned int nTimestamp, int nFrmNo);
int audioPlay_play(AVAPLAY pAVAPlay, char* pBuf, int nBufSize, unsigned int *nTimeStamp, int *nFrmNo);

#endif //_AVAPI_AUDIOPLAY_H_

