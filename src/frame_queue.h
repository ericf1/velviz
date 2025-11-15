#ifndef HELPER_H
#define HELPER_H

#include "raylib.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#define NODRAWTEXT
#define NOUSER
#include <windows.h>
#include <stdbool.h>

#define MAX_QUEUE 4

typedef struct {
    Image frame;
    bool valid;
} FrameSlot;

extern FrameSlot frameQueue[MAX_QUEUE];
extern CRITICAL_SECTION queueLock;
extern CONDITION_VARIABLE queueNotEmpty;
extern CONDITION_VARIABLE queueNotFull;
extern int head;
extern int tail;
extern int count;
extern bool queueRunning;

// Queue lifecycle
void initFrameQueue(void);
void stopFrameQueue(void);
void destroyFrameQueue(void);

// Producer (main/render thread)
void enqueueFrame(const Image *frame);

// Consumer (encoder thread)
bool dequeueFrame(Image *outFrame);

#endif // HELPER_H