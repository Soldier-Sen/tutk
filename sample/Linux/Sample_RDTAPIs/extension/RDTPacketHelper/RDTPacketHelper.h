#ifndef _PACKET_HELPER_H_
#define _PACKET_HELPER_H_

#define MAX_PACKET_SIZE 0xFFFF

typedef enum
{
    PACKET_HELPER_OK = 0,
    PACKET_HELPER_PARAMETER_ERROR   = -1,
    PACKET_HELPER_MALLOC_ERROR      = -2,
    PACKET_HELPER_TIMEOUT           = -3,
    PACKET_HELPER_RDTDESTROY        = -4,
}PACKET_HELPER;

int RDTPacketRead(int rdt_id, char** buf);
void RDTPacketRelease(char** buf);
int RDTPacketWrite(int rdt_id, char* buf, int length);

#endif //_PACKET_HELPER_H_

