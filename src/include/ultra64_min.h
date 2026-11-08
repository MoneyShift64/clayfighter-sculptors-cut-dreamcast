/*
 * The small part of libultra (the N64 SDK) that the decompiled C uses so far.
 * Layouts match libultra 2.0 so structures line up with the game's data.
 */
#ifndef ULTRA64_MIN_H
#define ULTRA64_MIN_H

#include "common.h"

typedef s32 OSId;
typedef void *OSMesg;
typedef struct OSThread_s OSThread;

typedef struct OSMesgQueue_s {
    OSThread *mtqueue;      /* threads waiting to receive */
    OSThread *fullqueue;    /* threads waiting to send    */
    s32 validCount;
    s32 first;
    s32 msgCount;
    OSMesg *msg;
} OSMesgQueue;              /* 0x18 bytes */

#define OS_MESG_NOBLOCK 0
#define OS_MESG_BLOCK   1

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32  osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);
s32  osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);
OSId osGetThreadId(OSThread *t);

#endif /* ULTRA64_MIN_H */
