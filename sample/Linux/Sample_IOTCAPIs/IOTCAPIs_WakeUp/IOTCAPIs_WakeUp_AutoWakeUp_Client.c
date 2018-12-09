#include <stdio.h>
#include "IOTCAPIs.h"
#include "IOTCWakeUp.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("No UID input!!!\n");
        printf("IOTCAPIs_Client [UID]\n");
        return 0;
    }

    char *UID = (char *)argv[1];
    int SID, ret;

    IOTC_Initialize2(0);

    // set IOTC_Connect_ByUID & IOTC_Connect_ByUID_Parallel auto wakeup sleeping device.
    // default value is false. And this setup be reset in IOTC_DeInitialize().
    IOTC_WakeUp_Setup_Auto_WakeUp(1);

    SID = IOTC_Get_SessionID();
    if (SID < 0)
    {
        printf("IOTC_Get_SessionID error [%d]\n", SID);
        return 0;
    }

    ret = IOTC_Connect_ByUID_Parallel(UID, SID);
    printf("call IOTC_Connect_ByUID_Parallel(%s)...[%d]\n", UID, ret);



    IOTC_Session_Close(SID);
    IOTC_DeInitialize();
    return 0;
}
