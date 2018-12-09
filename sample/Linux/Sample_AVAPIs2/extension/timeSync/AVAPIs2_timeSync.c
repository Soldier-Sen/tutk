#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include "AVAPIs2_timeSync.h"

int H264FindNaluTypeNoSize(const char *data, int size, unsigned char type)
{
    char nalu[4] = {0x00, 0x00, 0x00, 0x01};
    int count = 0;
    unsigned char naluType = 0;

    if(data == NULL || size < 4){
        printf("[%s:%d] data without video\n", __FUNCTION__, __LINE__);
        return -1;
    }

    //separate h.264 frame to nalu & replace nalu header (00 00 00 01) to nalu size
    while(count < size-4){
        if(data[count] == nalu[0] && data[count+1] == nalu[1] && data[count+2] == nalu[2] && data[count+3] == nalu[3]){
            naluType = (unsigned char)(data[count+4]&0x1f);
            //printf("NALU [%d] from %d\n", naluType, count);
            if(naluType == type){
                return 1;
            }
        }
        count++;
    }

    return 0;
}

long getSystemTimeMillis()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return (time.tv_sec * 1000) + (time.tv_usec / 1000);//ms
}

long getTimevalDiff(struct timeval x , struct timeval y)
{
    long x_ms , y_ms , diff;
    
    x_ms = x.tv_sec*1000 + x.tv_usec/1000;
    y_ms = y.tv_sec*1000 + y.tv_usec/1000;
    
    diff = y_ms - x_ms;
    
    return diff;
}

int checkIFrame(FRAMEINFO_t * frameInfo)
{
    if(frameInfo->codec_id == MEDIA_CODEC_VIDEO_MJPEG)
        return 1;

    return ((frameInfo->flags & IPC_FRAME_FLAG_IFRAME) == IPC_FRAME_FLAG_IFRAME);
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

void TimeSync_FrameNodeFree(Frame_Node *node)
{
    if(node != NULL){
        node->prev_frame = NULL;
        node->next_frame = NULL;
        if(node->frame_data != NULL)
            free(node->frame_data);
        if(node->frame_info != NULL)
            free(node->frame_info);
        free(node);
    }
}

int TimeSync_AVCaluMinMax(AnalysisDataSlot *dataSlot, unsigned int *AMin, unsigned int *AMax, unsigned int *VMin, unsigned int *VMax)
{
    unsigned int index = 0;
    
    *AMin = 0;
    *AMax = 0;
    *VMin = 0;
    *VMax = 0;
    
    if(dataSlot->uDataSize == 0)
        return 0;
    
    for(index = 0 ; index < MAX_ANALYSIS_DATA_SLOT_NUMBER ; index++){
        if(index == dataSlot->usIndex)
            continue;
        
        if(*AMin == 0)
            *AMin = dataSlot->m_Data[index].uAQMin;
        if(*AMax == 0)
            *AMax = dataSlot->m_Data[index].uAQMax;
        if(*VMin == 0)
            *VMin = dataSlot->m_Data[index].uVQMin;
        if(*VMax == 0)
            *VMax = dataSlot->m_Data[index].uVQMax;
        
        if(dataSlot->m_Data[index].uAQMin > 0 && dataSlot->m_Data[index].uAQMin <= *AMin){
            *AMin = dataSlot->m_Data[index].uAQMin;
        }
        if(dataSlot->m_Data[index].uAQMax > 0 && dataSlot->m_Data[index].uAQMax > *AMax){
            *AMax = dataSlot->m_Data[index].uAQMax;
        }
        
        if(dataSlot->m_Data[index].uVQMin > 0 && dataSlot->m_Data[index].uVQMin <= *VMin){
            *VMin = dataSlot->m_Data[index].uVQMin;
        }
        if(dataSlot->m_Data[index].uVQMax > 0 && dataSlot->m_Data[index].uVQMax > *VMax){
            *VMax = dataSlot->m_Data[index].uVQMax;
        }
    }
    
    return 1;
}

int TimeSync_AVCaluStatistics(AnalysisDataSlot *dataSlot, unsigned int AQueueDuration, unsigned int VQueueDuration, unsigned int *AMin, unsigned int *AMax, unsigned int *VMin, unsigned int *VMax)
{
    unsigned short index = 0;
    unsigned int nowTime = 0;
    int ret = 0;
    
    if(dataSlot == NULL)
        return ret;
    
    if(dataSlot->usCount == 0){
        //initial
        dataSlot->usCount = MAX_ANALYSIS_DATA_SLOT_NUMBER;
        dataSlot->uVersion = 0;
        dataSlot->usIndex = 0;
        dataSlot->uDataSize = 0;
    }
    
    index = dataSlot->usIndex;
    if(dataSlot->m_Data[index].uTimeStamp == 0){
        dataSlot->m_Data[index].uTimeStamp = (unsigned int)getSystemTimeMillis();
    }
    else{
        nowTime = (unsigned int)getSystemTimeMillis();
        if(nowTime - dataSlot->m_Data[index].uTimeStamp >= 1000){
            dataSlot->m_Data[index].uLastTimeStamp = nowTime;
            if(++index >= MAX_ANALYSIS_DATA_SLOT_NUMBER){
                dataSlot->uDataSize++;
                index = 0;
            }

            if(dataSlot->uDataSize > 0){
                ret = TimeSync_AVCaluMinMax(dataSlot, AMin, AMax, VMin, VMax);
            }
            
            dataSlot->usIndex = index;
            memset(&dataSlot->m_Data[index], 0, sizeof(AnalysisData));
            dataSlot->m_Data[index].uTimeStamp = nowTime;
        }
        else{
            if(dataSlot->m_Data[index].uAQMin == 0)
                dataSlot->m_Data[index].uAQMin = AQueueDuration;
            if(dataSlot->m_Data[index].uVQMin == 0)
                dataSlot->m_Data[index].uVQMin = VQueueDuration;
            
            if(AQueueDuration > 0 && AQueueDuration <= dataSlot->m_Data[index].uAQMin)
                dataSlot->m_Data[index].uAQMin = AQueueDuration;
            if(AQueueDuration > 0 && AQueueDuration >= dataSlot->m_Data[index].uAQMax)
                dataSlot->m_Data[index].uAQMax = AQueueDuration;
            
            if(VQueueDuration > 0 && VQueueDuration <= dataSlot->m_Data[index].uVQMin)
                dataSlot->m_Data[index].uVQMin = VQueueDuration;
            if(VQueueDuration > 0 && VQueueDuration >= dataSlot->m_Data[index].uVQMax)
                dataSlot->m_Data[index].uVQMax = VQueueDuration;
        }
    }
    
    return ret;
}

int TimeSync_DoTimeSync(TIMESYNC_Info *pTimeSyncInfo)
{
    //config
    int time_sync_mode = TIMESYNC_MODE_AUDIOFIRST;
    int trigger_sync_interval = TIME_SYNC_TRIGGER_INTERVAL; // ms
    int drop_frame_mode = DROPFRAME_MODE_PLAY_ALL;
    unsigned int audio_buffer_time = 100;  // ms

    //local variable
    unsigned int audioQueueDuration = 0, audioFirstTimeStamp = 0;
    unsigned int /*videoQueueDuration = 0, */videoFirstTimeStamp = 0;
    unsigned int audioPlayTimeStamp = 0;
    Frame_Node *videoFrame = NULL;
    Frame_Node *audioFrame = NULL;

    switch(time_sync_mode){
        case TIMESYNC_MODE_AUDIOFIRST:
        {
            // Step 1 : Check Audio Queue Duration
            audioQueueDuration = kalay_queue_duration(&pTimeSyncInfo->audio_queue);
            audioFirstTimeStamp = kalay_queue_head_timestamp(&pTimeSyncInfo->audio_queue);

            //videoQueueDuration = kalay_queue_duration(&pTimeSyncInfo->video_queue);
            videoFirstTimeStamp = kalay_queue_head_timestamp(&pTimeSyncInfo->video_queue);

            // Check Audio Buffer Duration Time
            if(audioQueueDuration >= audio_buffer_time){
                pTimeSyncInfo->waitAudioStartTime = 0;

                if(videoFirstTimeStamp == 0){
                    if(pTimeSyncInfo->videoPendingTime == 0){
                        pTimeSyncInfo->videoPendingTime = getSystemTimeMillis();
                    }
                    if(getSystemTimeMillis() - pTimeSyncInfo->videoPendingTime > MAX_VIDEO_PENDING_TIME){
                        //printf("No Video to Sync, Already Pending [%ld]ms\n", getSystemTimeMillis() - pTimeSyncInfo->videoPendingTime);
                    }
                    break;
                }
                else{
                    pTimeSyncInfo->videoPendingTime = 0;
                }

                if(audioFirstTimeStamp == 0){
                    printf("No Audio to Sync\n");
                    break;
                }

                if(videoFirstTimeStamp < audioFirstTimeStamp && (audioFirstTimeStamp - videoFirstTimeStamp) >= trigger_sync_interval){
                    if(pTimeSyncInfo->prevReadAudioTime > 0 && videoFirstTimeStamp > 0 && pTimeSyncInfo->prevReadAudioTime > videoFirstTimeStamp+100){
                        //pTimeSyncInfo->buffer_time_lower += 100;
                        pTimeSyncInfo->buffer_time_middle += 100;
                        pTimeSyncInfo->buffer_time_upper += 100;
                        if(pTimeSyncInfo->buffer_time_middle > MAX_AUDIO_BUFFER_MIDDLE)
                            pTimeSyncInfo->buffer_time_middle = MAX_AUDIO_BUFFER_MIDDLE;
                        if(pTimeSyncInfo->buffer_time_upper > MAX_AUDIO_BUFFER_UPPER)
                            pTimeSyncInfo->buffer_time_upper = MAX_AUDIO_BUFFER_UPPER;
                        //printf("%u %u %u\n", pTimeSyncInfo->buffer_time_lower, pTimeSyncInfo->buffer_time_middle, pTimeSyncInfo->buffer_time_upper);
                    }
                    //Video Behind, Need Speed up Video
                    switch(drop_frame_mode){
                        case DROPFRAME_MODE_PLAY_ALL:
                            //Play All Video Frame Before audioQueueDuration
                            if(pTimeSyncInfo->prevReadAudioTime > 0 && videoFirstTimeStamp >= pTimeSyncInfo->prevReadAudioTime)
                                break;
                            //printf("===== Not Sync : Play All Video TimeDiff[%u] [%u ~ %u] =====\n", pTimeSyncInfo->prevReadAudioTime-videoFirstTimeStamp, videoFirstTimeStamp, pTimeSyncInfo->prevReadAudioTime);
                            while(1){
                                videoFirstTimeStamp = kalay_queue_head_timestamp(&pTimeSyncInfo->video_queue);
                                if(videoFirstTimeStamp == 0)
                                    break;
                                if(videoFirstTimeStamp >= pTimeSyncInfo->prevReadAudioTime)
                                    break;
                                videoFrame = kalay_queue_pop(&pTimeSyncInfo->video_queue, pTimeSyncInfo->video_queue.head, "#001");
                                if(videoFrame != NULL){
                                    pTimeSyncInfo->prevReadVideoTime = videoFirstTimeStamp;
                                    pTimeSyncInfo->popVideo = (unsigned int)getSystemTimeMillis();
                                    if(checkIFrame(videoFrame->frame_info)){
                                        if(pTimeSyncInfo->addVideo != NULL && pTimeSyncInfo->addVideo(pTimeSyncInfo, videoFrame->frame_data, videoFrame->frame_size, (FRAMEINFO_t *)videoFrame->frame_info, sizeof(FRAMEINFO_t), videoFrame->frame_no) == 0){
                                            videoFrame->frame_data = NULL;
                                            videoFrame->frame_info = NULL;
                                        }
                                    }
                                    TimeSync_FrameNodeFree(videoFrame);
                                }
                            }
                            pTimeSyncInfo->firstVideoTime = pTimeSyncInfo->firstAudioTime;
                            pTimeSyncInfo->firstTimeSync = 1;
                            break;
                        case DROPFRAME_MODE_PLAY_IFRAME_ONLY :
                            //Play All I Frame Before audioQueueDuration
                            printf("Play All I Frame Before audioFirstTimeStamp[%u]\n", audioFirstTimeStamp);
                            break;
                        case DROPFRAME_MODE_DROP_ALL :
                            //Drop All Video Frame Before audioQueueDuration
                            printf("Drop All Video Frame Before audioFirstTimeStamp[%u]\n", audioFirstTimeStamp);
                            break;
                        case DROPFRAME_MODE_DROP_UNTIL_NEAR_IFRAME :
                            //Drop All Video Frame Until I Frame before audioQueueDuration
                            printf("Drop All Video Frame Until I Frame before audioFirstTimeStamp[%u]\n", audioFirstTimeStamp);
                            break;
                        case DROPFRAME_MODE_DROP_UNTIL_NEXT_IFRAME:
                            //Drop All Video Frame Until I Frame after audioQueueDuration
                            printf("Drop All Video Frame Until I Frame after audioFirstTimeStamp[%u]\n", audioFirstTimeStamp);
                            break;
                        default:
                            break;
                    }
                }
                else if(audioFirstTimeStamp < videoFirstTimeStamp && videoFirstTimeStamp >= audioFirstTimeStamp+trigger_sync_interval){
                    //Video Ahead, Need Speed up Audio
                    audioPlayTimeStamp = audioFirstTimeStamp+(pTimeSyncInfo->prevReadVideoTime - audioFirstTimeStamp);
                    //printf("===== Not Sync : Play All Audio TimeDiff[%u] [%u ~ %u] =====\n", pTimeSyncInfo->prevReadVideoTime - audioFirstTimeStamp, audioFirstTimeStamp, audioPlayTimeStamp);
                    while(1){
                        audioFirstTimeStamp = kalay_queue_head_timestamp(&pTimeSyncInfo->audio_queue);
                        if(audioFirstTimeStamp > audioPlayTimeStamp || audioFirstTimeStamp == 0)
                            break;
                        audioFrame = kalay_queue_pop(&pTimeSyncInfo->audio_queue, pTimeSyncInfo->audio_queue.head, "#001");
                        if(pTimeSyncInfo->addAudio != NULL && pTimeSyncInfo->addAudio(pTimeSyncInfo, (char*)audioFrame->frame_data, audioFrame->frame_size, (FRAMEINFO_t*)audioFrame->frame_info, sizeof(FRAMEINFO_t), audioFrame->frame_no) == 0){
                            audioFrame->frame_data = NULL;
                            audioFrame->frame_info = NULL;
                        }
                        if(audioFrame != NULL){
                            pTimeSyncInfo->prevReadAudioTime = audioFirstTimeStamp;
                            TimeSync_FrameNodeFree(audioFrame);
                        }
                        else{
                            break;
                        }
                    }
                    pTimeSyncInfo->firstVideoTime = pTimeSyncInfo->firstAudioTime;
                    pTimeSyncInfo->firstTimeSync = 1;
                }
                else{
                    //Video, Audio Already Time Sync
                    //printf("===== Time Sync Diff [%d] ms a[%u] v[%u] [ %u %u %u ] =====\n", videoFirstTimeStamp >= audioFirstTimeStamp ? videoFirstTimeStamp - audioFirstTimeStamp : audioFirstTimeStamp - videoFirstTimeStamp, audioQueueDuration, videoQueueDuration, pTimeSyncInfo->buffer_time_lower, pTimeSyncInfo->buffer_time_middle, pTimeSyncInfo->buffer_time_upper);
                }
            }
            else{
                if(pTimeSyncInfo->waitAudioStartTime == 0){
                    pTimeSyncInfo->waitAudioStartTime = (unsigned int)getSystemTimeMillis();
                }
                else if((unsigned int)getSystemTimeMillis() - pTimeSyncInfo->waitAudioStartTime >= MAX_AUDIO_PENDING_TIME){
                    pTimeSyncInfo->waitAudioStartTime = 0;
                    //printf("===== Audio Delay over MAX_AUDIO_PENDING_TIME[%d] videoQueueDuration[%d] audioQueueDuration[%d] =====\n", MAX_AUDIO_PENDING_TIME, videoQueueDuration, audioQueueDuration);
                    break;
                }
            }
        }
        break;

        case TIMESYNC_MODE_VIDEOFIRST:
        {
        }
        break;

        case TIMESYNC_MODE_OTHERCLOCK:
        {
        }
        break;

        default:
            break;
    }
    
    return 0;
}

unsigned int TimeSync_ReadStream(TIMESYNC_Info *pTimeSyncInfo)
{
    //config
    unsigned int buffer_time_lower = pTimeSyncInfo->buffer_time_lower;    // ms
    unsigned int buffer_time_middle = pTimeSyncInfo->buffer_time_middle;  // ms
    unsigned int buffer_time_upper = pTimeSyncInfo->buffer_time_upper;    // ms

    //local variable
    unsigned int audioQueueDuration = 0, audioFirstTimeStamp = 0;
    unsigned int videoQueueDuration = 0, videoFirstTimeStamp = 0;
    unsigned int playTime = 0, audioSleepTime = 1000, videoSleepTime = 1000, sleepTime = 0, pauseTime = 0;
    unsigned int aMin = 0, aMax = 0, vMin = 0, vMax = 0;
    int readVideo = 0, readAudio = 0;
    Frame_Node *audioFrame = NULL;
    Frame_Node *videoFrame = NULL;

    audioQueueDuration = kalay_queue_duration(&pTimeSyncInfo->audio_queue);
    audioFirstTimeStamp = kalay_queue_head_timestamp(&pTimeSyncInfo->audio_queue);

    videoQueueDuration = kalay_queue_duration(&pTimeSyncInfo->video_queue);
    videoFirstTimeStamp = kalay_queue_head_timestamp(&pTimeSyncInfo->video_queue);

    if(TimeSync_AVCaluStatistics(&pTimeSyncInfo->anaDataSlot, audioQueueDuration, videoQueueDuration, &aMin, &aMax, &vMin, &vMax) == 1){
        printf("$$$$$ Audio/Video Jitter[%4u /%4u ] dur[%4u /%4u ] buffer[ %u %u %u ] $$$$$\n", aMax-aMin, vMax-vMin, audioQueueDuration, videoQueueDuration, pTimeSyncInfo->buffer_time_lower, pTimeSyncInfo->buffer_time_middle, pTimeSyncInfo->buffer_time_upper);
        pTimeSyncInfo->buffer_time_middle = ((aMax-aMin)/100)*100+((aMax-aMin)%100 >= 50 ? 100 : 0);
        if(pTimeSyncInfo->buffer_time_middle < DEFAULT_AUDIO_BUFFER_MIDDLE)
            pTimeSyncInfo->buffer_time_middle = DEFAULT_AUDIO_BUFFER_MIDDLE;
        if(pTimeSyncInfo->buffer_time_middle > MAX_AUDIO_BUFFER_MIDDLE)
            pTimeSyncInfo->buffer_time_middle = MAX_AUDIO_BUFFER_MIDDLE;

        if(aMax-aMin > 100)
            pTimeSyncInfo->buffer_time_upper = pTimeSyncInfo->buffer_time_middle+(DEFAULT_AUDIO_BUFFER_UPPER - DEFAULT_AUDIO_BUFFER_MIDDLE);
        else
            pTimeSyncInfo->buffer_time_upper = pTimeSyncInfo->buffer_time_middle;

        buffer_time_lower = pTimeSyncInfo->buffer_time_lower;    // ms
        buffer_time_middle = pTimeSyncInfo->buffer_time_middle;  // ms
        buffer_time_upper = pTimeSyncInfo->buffer_time_upper;    // ms
    }

    // Check Audio Buffer Duration Time
    if(pTimeSyncInfo->startRead == 0){
        if(audioQueueDuration >= buffer_time_middle){
            pTimeSyncInfo->startRead = 1;
            WallClock(&pTimeSyncInfo->clock, WALLCLOCK_SPEED_1X);
            pauseTime = WallClock(&pTimeSyncInfo->clock, WALLCLOCK_START);
            printf("***** Audio Resume pauseTime[%u] *****\n", pauseTime);
        }
    }
    else if(pTimeSyncInfo->startRead == 1){
        if(audioQueueDuration < DEFAULT_AUDIO_BUFFER_LOWEST){
            pTimeSyncInfo->startRead = 0;
            WallClock(&pTimeSyncInfo->clock, WALLCLOCK_PAUSE);
            WallClock(&pTimeSyncInfo->clock, WALLCLOCK_SPEED_1X);
            pTimeSyncInfo->buffer_time_middle += 100;
            pTimeSyncInfo->buffer_time_upper += 100;
            if(pTimeSyncInfo->buffer_time_middle > MAX_AUDIO_BUFFER_MIDDLE)
                pTimeSyncInfo->buffer_time_middle = MAX_AUDIO_BUFFER_MIDDLE;
            if(pTimeSyncInfo->buffer_time_upper > MAX_AUDIO_BUFFER_UPPER)
                pTimeSyncInfo->buffer_time_upper = MAX_AUDIO_BUFFER_UPPER;

            //Audio Delay
            sleepTime = buffer_time_middle - audioQueueDuration;
            printf("***** Audio Delay audioQueueDuration[%u] sleepTime[%u] *****\n", audioQueueDuration, sleepTime);
        }
        else if(audioQueueDuration < buffer_time_lower){
            WallClock(&pTimeSyncInfo->clock, WALLCLOCK_SPEED_QUALX);
        }
        else if(audioQueueDuration > buffer_time_upper){
            //speed up play
            WallClock(&pTimeSyncInfo->clock, WALLCLOCK_SPEED_2X);
        }
        else{
            //speed up normal
            WallClock(&pTimeSyncInfo->clock, WALLCLOCK_SPEED_1X);
        }
    }

    if(pTimeSyncInfo->startRead){
        playTime = WallClock(&pTimeSyncInfo->clock, WALLCLOCK_GET);
        audioFirstTimeStamp = kalay_queue_head_timestamp(&pTimeSyncInfo->audio_queue);
        videoFirstTimeStamp = kalay_queue_head_timestamp(&pTimeSyncInfo->video_queue);

        if(pTimeSyncInfo->firstAudioTime == 0){
            pTimeSyncInfo->firstAudioTime = audioFirstTimeStamp;
            pTimeSyncInfo->prevReadAudioTime = audioFirstTimeStamp;
            readAudio = 1;
        }
        else{
            if(audioFirstTimeStamp <= pTimeSyncInfo->firstAudioTime + playTime){
                if(audioFirstTimeStamp > 0){
                    pTimeSyncInfo->prevReadAudioTime = audioFirstTimeStamp;
                    readAudio = 1;
                }
                else{
                    readAudio = 0;
                }
            }
            else{
                audioSleepTime = audioFirstTimeStamp - pTimeSyncInfo->firstAudioTime - playTime;
                readAudio = 0;
            }
        }

        if(readAudio){
            audioFrame = kalay_queue_pop(&pTimeSyncInfo->audio_queue, pTimeSyncInfo->audio_queue.head, "#001");
            if(audioFrame != NULL){
                pTimeSyncInfo->popAudio = (unsigned int)getSystemTimeMillis();
                if(pTimeSyncInfo->addAudio != NULL && pTimeSyncInfo->addAudio(pTimeSyncInfo, (char*)audioFrame->frame_data, audioFrame->frame_size, (FRAMEINFO_t*)audioFrame->frame_info, sizeof(FRAMEINFO_t), audioFrame->frame_no) == 0){
                    audioFrame->frame_data = NULL;
                    audioFrame->frame_info = NULL;
                }
                TimeSync_FrameNodeFree(audioFrame);
            }
            else{
                readAudio = 0;
            }

            if(kalay_queue_head_timestamp(&pTimeSyncInfo->audio_queue) > 0){
                audioSleepTime = kalay_queue_head_timestamp(&pTimeSyncInfo->audio_queue) - audioFirstTimeStamp;
            }
        }

        if(pTimeSyncInfo->firstVideoTime == 0){
            pTimeSyncInfo->firstVideoTime = videoFirstTimeStamp;
            pTimeSyncInfo->prevReadVideoTime = videoFirstTimeStamp;
            readVideo = 1;
        }
        else{
            if(videoFirstTimeStamp <= pTimeSyncInfo->firstVideoTime + playTime){
                if(videoFirstTimeStamp > 0){
                    pTimeSyncInfo->prevReadVideoTime = videoFirstTimeStamp;
                    readVideo = 1;
                }
                else{
                    readVideo = 0;
                }
            }
            else{
                videoSleepTime = videoFirstTimeStamp - (pTimeSyncInfo->firstVideoTime + playTime);
                readVideo = 0;
            }
        }

        if(readVideo){
            videoFrame = kalay_queue_pop(&pTimeSyncInfo->video_queue, pTimeSyncInfo->video_queue.head, "#001");
            if(videoFrame != NULL){
                pTimeSyncInfo->popVideo = (unsigned int)getSystemTimeMillis();
                if(pTimeSyncInfo->addVideo != NULL && pTimeSyncInfo->addVideo(pTimeSyncInfo, videoFrame->frame_data, videoFrame->frame_size, (FRAMEINFO_t *)videoFrame->frame_info, sizeof(FRAMEINFO_t), videoFrame->frame_no) == 0){
                    videoFrame->frame_data = NULL;
                    videoFrame->frame_info = NULL;
                }
                TimeSync_FrameNodeFree(videoFrame);
            }
            else{
                readVideo = 0;
            }

            if(kalay_queue_head_timestamp(&pTimeSyncInfo->video_queue) > 0){
                videoSleepTime = kalay_queue_head_timestamp(&pTimeSyncInfo->video_queue) - videoFirstTimeStamp;
            }
        }
        sleepTime = audioSleepTime <= videoSleepTime ? audioSleepTime : videoSleepTime;
    }

    return (sleepTime > 5) ? (sleepTime-5)*1000 : 3*1000;
}

unsigned int TimeSync_ReadStreamWithoutSync(TIMESYNC_Info *pTimeSyncInfo)
{
    //local variable
    unsigned int /*audioQueueDuration = 0, */audioFirstTimeStamp = 0;
    unsigned int /*videoQueueDuration = 0, */videoFirstTimeStamp = 0;
    unsigned int audioSleepTime = 1000, videoSleepTime = 1000, sleepTime = 0;
    int readVideo = 0, readAudio = 0;
    Frame_Node *audioFrame = NULL;
    Frame_Node *videoFrame = NULL;

    //audioQueueDuration = kalay_queue_duration(&pTimeSyncInfo->audio_queue);
    audioFirstTimeStamp = kalay_queue_head_timestamp(&pTimeSyncInfo->audio_queue);

    //videoQueueDuration = kalay_queue_duration(&pTimeSyncInfo->video_queue);
    videoFirstTimeStamp = kalay_queue_head_timestamp(&pTimeSyncInfo->video_queue);

    if(audioFirstTimeStamp > 0){
        if(pTimeSyncInfo->firstAudioTime == 0)
            pTimeSyncInfo->firstAudioTime = audioFirstTimeStamp;
        pTimeSyncInfo->prevReadAudioTime = audioFirstTimeStamp;
        readAudio = 1;
    }
    else{
        readAudio = 0;
        audioSleepTime = 25;
    }

    if(readAudio){
        audioFrame = kalay_queue_pop(&pTimeSyncInfo->audio_queue, pTimeSyncInfo->audio_queue.head, "#001");
        if(audioFrame != NULL){
            pTimeSyncInfo->popAudio = (unsigned int)getSystemTimeMillis();
            if(pTimeSyncInfo->addAudio != NULL && pTimeSyncInfo->addAudio(pTimeSyncInfo, (char*)audioFrame->frame_data, audioFrame->frame_size, (FRAMEINFO_t*)audioFrame->frame_info, sizeof(FRAMEINFO_t), audioFrame->frame_no) == 0){
                audioFrame->frame_data = NULL;
                audioFrame->frame_info = NULL;
            }
            TimeSync_FrameNodeFree(audioFrame);
        }
        else{
            readAudio = 0;
        }

        if(kalay_queue_head_timestamp(&pTimeSyncInfo->audio_queue) > 0){
            audioSleepTime = kalay_queue_head_timestamp(&pTimeSyncInfo->audio_queue) - audioFirstTimeStamp;
        }
    }

    if(videoFirstTimeStamp > 0){
        if(pTimeSyncInfo->firstVideoTime == 0){
            pTimeSyncInfo->firstVideoTime = videoFirstTimeStamp;
        }
        pTimeSyncInfo->prevReadVideoTime = videoFirstTimeStamp;
        readVideo = 1;
    }
    else{
        readVideo = 0;
        videoSleepTime = 25;
    }

    if(readVideo){
        videoFrame = kalay_queue_pop(&pTimeSyncInfo->video_queue, pTimeSyncInfo->video_queue.head, "#001");
        if(videoFrame != NULL){
            pTimeSyncInfo->popVideo = (unsigned int)getSystemTimeMillis();
            if(pTimeSyncInfo->addVideo != NULL && pTimeSyncInfo->addVideo(pTimeSyncInfo, videoFrame->frame_data, videoFrame->frame_size, (FRAMEINFO_t *)videoFrame->frame_info, sizeof(FRAMEINFO_t), videoFrame->frame_no) == 0){
                videoFrame->frame_data = NULL;
                videoFrame->frame_info = NULL;
            }
            TimeSync_FrameNodeFree(videoFrame);
        }
        else{
            readVideo = 0;
        }

        if(kalay_queue_head_timestamp(&pTimeSyncInfo->video_queue) > 0){
            videoSleepTime = kalay_queue_head_timestamp(&pTimeSyncInfo->video_queue) - videoFirstTimeStamp;
        }
    }
    sleepTime = audioSleepTime <= videoSleepTime ? audioSleepTime : videoSleepTime;

    return (sleepTime > 5) ? (sleepTime-5)*1000 : 3*1000;
}

// Audio Base
void *TimeSync_Thread(void *arg)
{
    TIMESYNC_Info *pTimeSyncInfo = NULL;

    //config
    int check_sync = 300;      // ms
    int doTimeSync = 0, doReadStream = 0;

    //local variable
    struct timeval waitTime;
    unsigned int prevCheckTimeSync = 0;
    unsigned int timeToNextSample = 5000;

    pthread_detach(pthread_self());

    if(arg == NULL)
        return NULL;
    pTimeSyncInfo = (TIMESYNC_Info*)arg;

    WallClock(&pTimeSyncInfo->clock, WALLCLOCK_INIT);
    memset(&pTimeSyncInfo->anaDataSlot, 0, sizeof(AnalysisDataSlot));

    pTimeSyncInfo->buffer_time_lower = DEFAULT_AUDIO_BUFFER_LOWER;
    pTimeSyncInfo->buffer_time_middle = DEFAULT_AUDIO_BUFFER_MIDDLE;
    pTimeSyncInfo->buffer_time_upper = DEFAULT_AUDIO_BUFFER_UPPER;

    while(pTimeSyncInfo->thread_status){
        waitTime.tv_sec = 0;
        waitTime.tv_usec = timeToNextSample;
        select(1, NULL, NULL, NULL, &waitTime);

        if(pTimeSyncInfo->isShowing || pTimeSyncInfo->isListening){
            if(!doReadStream){
                printf("[%s] Start Read Stream\n", __FUNCTION__);
            }
            doReadStream = 1;
        }
        else{
            if(doReadStream){
                printf("[%s] Stop Read Stream\n", __FUNCTION__);
            }
            doReadStream = 0;
        }

        if(pTimeSyncInfo->isShowing && pTimeSyncInfo->isListening){
            if(!doTimeSync){
                printf("[%s] Start Time Sync, Clean all\n", __FUNCTION__);
                prevCheckTimeSync = 0;

                WallClock(&pTimeSyncInfo->clock, WALLCLOCK_INIT);
                memset(&pTimeSyncInfo->anaDataSlot, 0, sizeof(AnalysisDataSlot));

                pTimeSyncInfo->waitAudioStartTime = 0;
                pTimeSyncInfo->startRead = 0;
                pTimeSyncInfo->prevReadAudioTime = 0;
                pTimeSyncInfo->prevReadVideoTime = 0;
                pTimeSyncInfo->firstAudioTime = 0;
                pTimeSyncInfo->firstVideoTime = 0;
                pTimeSyncInfo->popVideo = 0;
                pTimeSyncInfo->popAudio = 0;
                pTimeSyncInfo->avgAudioTime = 0;
                pTimeSyncInfo->firstTimeSync = 0;
                pTimeSyncInfo->buffer_time_lower = DEFAULT_AUDIO_BUFFER_LOWER;
                pTimeSyncInfo->buffer_time_middle = DEFAULT_AUDIO_BUFFER_MIDDLE;
                pTimeSyncInfo->buffer_time_upper = DEFAULT_AUDIO_BUFFER_UPPER;
                pTimeSyncInfo->videoPendingTime = 0;
            }
            doTimeSync = 1;
        }
        else{
            if(doTimeSync){
                printf("[%s] Stop Time Sync\n", __FUNCTION__);
            }
            doTimeSync = 0;
        }

        if(doReadStream){
            if(doTimeSync){
                //Do Time Sync
                if(prevCheckTimeSync == 0){
                    prevCheckTimeSync = (unsigned int)getSystemTimeMillis();
                }
                if((unsigned int)getSystemTimeMillis() - prevCheckTimeSync >= check_sync){
                    prevCheckTimeSync = (unsigned int)getSystemTimeMillis();
                    TimeSync_DoTimeSync(pTimeSyncInfo);
                }

                //Do Read Stream
                timeToNextSample = TimeSync_ReadStream(pTimeSyncInfo);
            }
            else{
                timeToNextSample = TimeSync_ReadStreamWithoutSync(pTimeSyncInfo);
            }
        }
        else{
            timeToNextSample = 100;
        }
    }
    
    pTimeSyncInfo->thread_status = 2;

    return NULL;
}

int TimeSync_Initialize(TIMESYNC_Info *pTimeSyncInfo, int index, addVideoDecodeQueue pAddVideo, addVideoDecodeQueue pAddAudio)
{
    int nRet = 0;
    pthread_t ThreadTimeSync_ID;

    memset(pTimeSyncInfo, 0, sizeof(TIMESYNC_Info));

    if(pAddVideo == NULL || pAddAudio == NULL){
        printf("[%s:%d] parameter error\n", __FUNCTION__, __LINE__);
        return -1;
    }

    pTimeSyncInfo->index = index;
    pTimeSyncInfo->addVideo = pAddVideo;
    pTimeSyncInfo->addAudio = pAddAudio;

    kalay_queue_init(&pTimeSyncInfo->video_queue);
    kalay_queue_init(&pTimeSyncInfo->audio_queue);

    pTimeSyncInfo->thread_status = 1;
    nRet = pthread_create(&ThreadTimeSync_ID, NULL, (void*)&TimeSync_Thread, (void*)pTimeSyncInfo);
    if(nRet != 0){
        printf("[%s:%d] pthread_create nRet[%d]\n", __FUNCTION__, __LINE__, nRet);
        kalay_queue_free(&pTimeSyncInfo->video_queue);
        kalay_queue_free(&pTimeSyncInfo->audio_queue);
        return -1;
    }
    pTimeSyncInfo->ThreadTimeSync_ID = ThreadTimeSync_ID;
    pthread_detach(ThreadTimeSync_ID);

    return 0;
}

void TimeSync_DeInitialize(TIMESYNC_Info *pTimeSyncInfo)
{
    pTimeSyncInfo->thread_status = 0;
    while(1){
        if(pTimeSyncInfo->thread_status == 2)
            break;
        usleep(100*1000);
    }

    pTimeSyncInfo->addVideo = NULL;
    pTimeSyncInfo->addAudio = NULL;

    kalay_queue_free(&pTimeSyncInfo->video_queue);
    kalay_queue_free(&pTimeSyncInfo->audio_queue);
}

void TimeSync_VideoEnable(TIMESYNC_Info *pTimeSyncInfo, int enable)
{
    pTimeSyncInfo->isShowing = enable;
}

void TimeSync_AudioEnable(TIMESYNC_Info *pTimeSyncInfo, int enable)
{
    pTimeSyncInfo->isListening = enable;
}

int TimeSync_InsertAudio(TIMESYNC_Info *pTimeSyncInfo, char* pFrameData, int nFrameSize, int frmNo, FRAMEINFO_t* pFrameInfo)
{
    Frame_Node *node = NULL;

    node = (Frame_Node *) malloc(sizeof(Frame_Node));
    if(node == NULL){
        printf("[%s:%d] malloc error\n", __FUNCTION__, __LINE__);
        return -1;
    }

    node->frame_size = nFrameSize;
    node->frame_no = frmNo;
    node->prev_frame = NULL;
    node->next_frame = NULL;
    
    node->frame_data = (char *)malloc(node->frame_size);
    node->frame_info = (FRAMEINFO_t *)malloc(sizeof(FRAMEINFO_t));
    
    memcpy(node->frame_data, pFrameData, node->frame_size);
    memcpy(node->frame_info, pFrameInfo, sizeof(FRAMEINFO_t));
    if(node->frame_info->timestamp == 0){
        node->frame_info->timestamp = (unsigned int)getSystemTimeMillis();
        pTimeSyncInfo->useSystemTimestamp = 1;
    }

    if(kalay_queue_tail_timestamp(&pTimeSyncInfo->audio_queue) > 0){
        pTimeSyncInfo->avgAudioTime = pFrameInfo->timestamp - kalay_queue_tail_timestamp(&pTimeSyncInfo->audio_queue);
        //printf("avgAudioTime[%u]\n", pTimeSyncInfo->avgAudioTime);
    }

    if(kalay_queue_insert(&pTimeSyncInfo->audio_queue, node) < 0){
        printf("[%s:%d] kalay_queue_insert error\n", __FUNCTION__, __LINE__);
        if(node->frame_data != NULL)
            free(node->frame_data);
        if(node->frame_info != NULL)
            free(node->frame_info);
        if(node != NULL)
            free(node);
        return -1;
    }

    return 0;
}

int TimeSync_HandleAudioLost(TIMESYNC_Info *pTimeSyncInfo, int frmNo)
{
    Frame_Node *audioFrame = NULL;
    Frame_Node *node = NULL;

    //Substituted audio data
    audioFrame = kalay_queue_rpop(&pTimeSyncInfo->audio_queue, pTimeSyncInfo->audio_queue.tail, "#001");
    if(audioFrame != NULL){
        node = (Frame_Node *) malloc(sizeof(Frame_Node));

        node->frame_size = audioFrame->frame_size;
        node->frame_no = frmNo;
        node->prev_frame = NULL;
        node->next_frame = NULL;

        node->frame_data = (char *)malloc(node->frame_size);
        node->frame_info = (FRAMEINFO_t *)malloc(sizeof(FRAMEINFO_t));

        memcpy(node->frame_data, audioFrame->frame_data, node->frame_size);
        memcpy(node->frame_info, audioFrame->frame_info, sizeof(FRAMEINFO_t));
        node->frame_info->timestamp += pTimeSyncInfo->avgAudioTime;

        if(kalay_queue_insert(&pTimeSyncInfo->audio_queue, audioFrame) < 0){
            printf("[%s:%d] kalay_queue_insert error\n", __FUNCTION__, __LINE__);
            TimeSync_FrameNodeFree(audioFrame);
            TimeSync_FrameNodeFree(node);
            return -1;
        }
        if(kalay_queue_insert(&pTimeSyncInfo->audio_queue, node) < 0){
            printf("[%s:%d] kalay_queue_insert error\n", __FUNCTION__, __LINE__);
            TimeSync_FrameNodeFree(audioFrame);
            TimeSync_FrameNodeFree(node);
            return -1;
        }
    }

    return 0;
}

int TimeSync_InsertVideo(TIMESYNC_Info *pTimeSyncInfo, char* pFrameData, int nActualFrameSize, int frmNo, FRAMEINFO_t* pFrameInfo)
{
    Frame_Node *node = NULL;

    node = (Frame_Node *)malloc(sizeof(Frame_Node));
    if(node == NULL){
        printf("[%s:%d] malloc error\n", __FUNCTION__, __LINE__);
        return 0;
    }

    node->frame_size = nActualFrameSize;
    node->frame_no = frmNo;
    node->prev_frame = NULL;
    node->next_frame = NULL;
    node->frame_data = NULL;
    node->frame_info = NULL;

    if(checkIFrame(pFrameInfo)){
        if(node->frame_size < 500){
            //Add By Jeff at 2016.10.03, For handle sps,pps and I Slice separate -- start
            if(H264FindNaluTypeNoSize(pFrameData, nActualFrameSize, 5) == 0){
                //printf("skip I Frame without I Slice\n");
                TimeSync_FrameNodeFree(node);
                return 0;
            }
        }

        node->frame_data = (char *)malloc(node->frame_size);
        if(node->frame_data == NULL){
            printf("[%s:%d] malloc error\n", __FUNCTION__, __LINE__);
            TimeSync_FrameNodeFree(node);
            return -1;
        }
        node->frame_info = (FRAMEINFO_t *)malloc(sizeof(FRAMEINFO_t));
        if(node->frame_info == NULL){
            printf("[%s:%d] malloc error\n", __FUNCTION__, __LINE__);
            TimeSync_FrameNodeFree(node);
            return -1;
        }

        memcpy(node->frame_data, pFrameData, node->frame_size);
        memcpy(node->frame_info, pFrameInfo, sizeof(FRAMEINFO_t));

        if(node->frame_info->timestamp == 0){
            node->frame_info->timestamp = (unsigned int)getSystemTimeMillis();
            pTimeSyncInfo->useSystemTimestamp = 1;
        }
        node->frame_info->timestamp -= pTimeSyncInfo->decodeITime;
    }
    else{
        node->frame_data = (char *)malloc(node->frame_size);
        if(node->frame_data == NULL){
            printf("[%s:%d] malloc error\n", __FUNCTION__, __LINE__);
            TimeSync_FrameNodeFree(node);
            return -1;
        }
        node->frame_info = (FRAMEINFO_t *)malloc(sizeof(FRAMEINFO_t));
        if(node->frame_info == NULL){
            printf("[%s:%d] malloc error\n", __FUNCTION__, __LINE__);
            TimeSync_FrameNodeFree(node);
            return -1;
        }

        memcpy(node->frame_data, pFrameData, node->frame_size);
        memcpy(node->frame_info, pFrameInfo, sizeof(FRAMEINFO_t));
        if(node->frame_info->timestamp == 0){
            node->frame_info->timestamp = (unsigned int)getSystemTimeMillis();
            pTimeSyncInfo->useSystemTimestamp = 1;
        }

        node->frame_info->timestamp -= pTimeSyncInfo->decodePTime;
    }
    //printf("[%s:%d] frmNo[%d] timestamp[%u] decode timestamp[%u]\n", __FUNCTION__, __LINE__, frmNo, ((FRAMEINFO_t *)pFrameInfo)->timestamp, node->frame_info->timestamp);

    if(kalay_queue_insert(&pTimeSyncInfo->video_queue, node) < 0){
        printf("[%s:%d] kalay_queue_insert error\n", __FUNCTION__, __LINE__);
        TimeSync_FrameNodeFree(node);
        return -1;
    }

    return 0;
}

