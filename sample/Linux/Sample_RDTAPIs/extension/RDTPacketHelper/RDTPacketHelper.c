/**====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
*
* RDTPacketHelper.c
*
* Copyright (c) by TUTK Co.LTD. All Rights Reserved.
*
* \brief       RDTPacketHelper Implementation
*
* \description RDTPacketHelper Implementation
*              
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "IOTCAPIs.h" 
#include "RDTAPIs.h"
#include "RDTPacketHelper.h"

#define RDT_PACKET_MAXIMUM_DATA_SIZE	1280

typedef struct _RDTPacketHeader {
    unsigned short packet_length;
} RDTPacketHeader;

static int read_rdt_packet_length(int rdt_id, int timeout)
{
    int read_len = 0;
    RDTPacketHeader header;
    int header_remain_size = 0;
    char *remain_data = NULL;

    header_remain_size = sizeof(RDTPacketHeader);
    remain_data = (char*)&header;

    while(header_remain_size > 0){
        // try to read the remain byte of RDT packet length
        read_len = RDT_Read(rdt_id, remain_data, header_remain_size, timeout);
        //printf("[%s:%d] RDT_Read rdt_id[%d] read_len[%d]\n", __FUNCTION__, __LINE__, rdt_id, read_len);
        if(read_len > 0){
            header_remain_size -= read_len;
            remain_data += read_len;
        }
        else if (read_len == 0 || read_len == RDT_ER_TIMEOUT){
            // Still can't read the data, maybe network is busy
            // Wait 1ms and try again
            usleep(1000);
        }
        else{
            break;
        }
    }

    if(header_remain_size == 0){
        //printf("[%s:%d] RDT_Read rdt_id[%d] packet_length[%d]\n", __FUNCTION__, __LINE__, rdt_id, (int)header.packet_length);
        return (int)header.packet_length;
    }
    else{
        // return RDT error code
        return read_len;
    }
}

static int read_rdt_packet_data(int rdt_id, char *buf, int packet_length, int timeout)
{
    int read_len = 0, remain_data_len = 0;
    char *remain_data = NULL;

    remain_data_len = packet_length;
    remain_data = buf;

    while(remain_data_len > 0){
        read_len = RDT_Read(rdt_id, remain_data, remain_data_len, timeout);
        //printf("[%s:%d] RDT_Read rdt_id[%d] read_len[%d]\n", __FUNCTION__, __LINE__, rdt_id, read_len);
        if(read_len > 0){
            remain_data_len -= read_len;
            remain_data += read_len;
        }
        else if (read_len == 0 || read_len == RDT_ER_TIMEOUT){
            // Still can't recv complete Packet, sleep 1 ms
            // 1. wait for data arrives
            // 2. avoid CPU loading high
            usleep(1000);
        }
        else{
            // Error happens, stop reading data
            break;
        }
    }

    if(remain_data_len == 0){
        // The real packet length we had read.
        read_len = packet_length;
    }

    return read_len;
}

static int drop_rdt_packet_data(int rdt_id, int dropSize, int timeout)
{
    char dropData[RDT_PACKET_MAXIMUM_DATA_SIZE] = {0};
    int remain_data_len = dropSize, read_len = 0;

    while(remain_data_len > 0){
        read_len = RDT_Read(rdt_id, dropData, remain_data_len > RDT_PACKET_MAXIMUM_DATA_SIZE ? RDT_PACKET_MAXIMUM_DATA_SIZE : remain_data_len, timeout);
        //printf("[%s:%d] RDT_Read read_len[%d]\n", __FUNCTION__, __LINE__, read_len);
        if(read_len > 0){
            remain_data_len -= read_len;
        }
        else if (read_len == 0 || read_len == RDT_ER_TIMEOUT){
            // Still can't recv complete Packet, sleep 1 ms
            // 1. wait for data arrives
            // 2. avoid CPU loading high
            usleep(1000);
        }
        else{
            // Error happens, stop reading data
            break;
        }
    }

    return remain_data_len;
}

void RDTPacketRelease(char** buf)
{
    if(*buf != NULL){
        free(*buf);
        *buf = NULL;
    }
}

int RDTPacketRead(int rdt_id, char** buf)
{
    int read_len = 0;
    int packet_length = 0;
    int timeout = 1000;

    if(rdt_id < 0)
        return PACKET_HELPER_PARAMETER_ERROR;

    packet_length = read_rdt_packet_length(rdt_id, timeout);
    if(packet_length > 0){
        *buf = (char*)malloc(packet_length);
        if(*buf == NULL){
            drop_rdt_packet_data(rdt_id, packet_length, timeout);
            return PACKET_HELPER_MALLOC_ERROR;
        }

        read_len = read_rdt_packet_data(rdt_id, *buf, packet_length, timeout);
    }
    else{
        read_len = packet_length;
    }

    if(read_len > 0) {
        return read_len;
    }

    switch(read_len){
        case 0: 
        case RDT_ER_TIMEOUT:
            printf("[%s:%d] rdt_id[%d] error[%d] packet_length[%d]\n", __FUNCTION__, __LINE__, rdt_id, read_len, packet_length);
            read_len = PACKET_HELPER_TIMEOUT;
            break;
        case RDT_ER_RCV_DATA_END:
        case RDT_ER_REMOTE_ABORT:
        case RDT_ER_LOCAL_ABORT:
        case RDT_ER_REMOTE_EXIT:
        case RDT_ER_RDT_DESTROYED:
        case IOTC_ER_REMOTE_TIMEOUT_DISCONNECT:
        default:
            printf("[%s:%d] rdt_id[%d] error[%d] packet_length[%d], need RDT Destroy\n", __FUNCTION__, __LINE__, rdt_id, read_len, packet_length);
            //RDT_Destroy(rdt_id);
            read_len = PACKET_HELPER_RDTDESTROY;
            break;
    }

    return read_len;
}

int RDTPacketWrite(int rdt_id, char* buf, int length)
{
	int write_len = 0, remain_data_size = 0;
    RDTPacketHeader header;
    char *write_pos = NULL;

	if(length == 0 || buf == NULL || length > 0xFFFF)
        return PACKET_HELPER_PARAMETER_ERROR;

    //Write RDTPacketHeader
    header.packet_length = length;
    remain_data_size = sizeof(RDTPacketHeader);
    write_pos = (char*)&header;
    while(remain_data_size > 0){
        write_len = RDT_Write(rdt_id, write_pos, remain_data_size);
        if(write_len > 0){
            remain_data_size -= write_len;
            write_pos += write_len;
        }
        else if(write_len == RDT_ER_SEND_BUFFER_FULL){
            // Buffer Full Can't send packet, sleep 1 ms
            // 1. wait for data send
            // 2. avoid CPU loading high
            usleep(1000);
        }
        else{
            printf("[%s:%d] header rdt_id[%d] error[%d]\n", __FUNCTION__, __LINE__, rdt_id, write_len);
            goto PACKET_WRITE_ERROR;
        }
    }

    //Write Data
    remain_data_size = length;
	write_pos = buf;
	while(remain_data_size > 0){
		write_len = RDT_Write(rdt_id, write_pos, remain_data_size > RDT_PACKET_MAXIMUM_DATA_SIZE ? RDT_PACKET_MAXIMUM_DATA_SIZE : remain_data_size);
        if(write_len > 0){
            remain_data_size -= write_len;
            write_pos += write_len;
        }
        else if(write_len == RDT_ER_SEND_BUFFER_FULL){
            // Buffer Full Can't send packet, sleep 1 ms
            // 1. wait for data send
            // 2. avoid CPU loading high
            usleep(1000);
        }
        else{
            printf("[%s:%d] packet rdt_id[%d] error[%d]\n", __FUNCTION__, __LINE__, rdt_id, write_len);
            goto PACKET_WRITE_ERROR;
        }
	}
    RDT_Flush(rdt_id);

    write_len = length + sizeof(RDTPacketHeader);
    return write_len;

PACKET_WRITE_ERROR:
    switch (write_len){
        case RDT_ER_REMOTE_ABORT:
        case RDT_ER_LOCAL_ABORT:
        case RDT_ER_REMOTE_EXIT:
        case RDT_ER_RDT_DESTROYED:
        case IOTC_ER_SESSION_CLOSE_BY_REMOTE:
        case IOTC_ER_REMOTE_TIMEOUT_DISCONNECT:
        default:
            printf("[%s:%d] rdt_id[%d] error[%d] length[%d], need RDT Destroy\n", __FUNCTION__, __LINE__, rdt_id, write_len, length);
            //RDT_Destroy(rdt_id);
            write_len = PACKET_HELPER_RDTDESTROY;
            break;
    }

    return write_len;
}

