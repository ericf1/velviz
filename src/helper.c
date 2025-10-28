
#include "helper.h"

FrameSlot frameQueue[MAX_QUEUE];
CRITICAL_SECTION queueLock;
CONDITION_VARIABLE queueNotEmpty;
CONDITION_VARIABLE queueNotFull;
int head = 0;
int tail = 0;
int count = 0;
bool queueRunning = false;

void initFrameQueue() {
    InitializeCriticalSection(&queueLock);
    InitializeConditionVariable(&queueNotEmpty);
    InitializeConditionVariable(&queueNotFull);

    head = 0;
    tail = 0;
    count = 0;
    queueRunning = true;

    for (int i = 0; i < MAX_QUEUE; ++i) {
        frameQueue[i].valid = false;
    }
}

void stopFrameQueue() {
    EnterCriticalSection(&queueLock);
    queueRunning = false;
    WakeConditionVariable(&queueNotEmpty);
    WakeConditionVariable(&queueNotFull);
    LeaveCriticalSection(&queueLock);
}

void destroyFrameQueue() {
    // free any frames still in the queue
    for (int i = 0; i < MAX_QUEUE; ++i) {
        if (frameQueue[i].valid) {
            UnloadImage(frameQueue[i].frame);
            frameQueue[i].valid = false;
        }
    }
    DeleteCriticalSection(&queueLock);
}

void enqueueFrame(const Image *frameIn) {
    EnterCriticalSection(&queueLock);

    if (!queueRunning) {
        LeaveCriticalSection(&queueLock);
        return; // shutting down, ignore
    }

    // If the queue is full, drop the oldest frame to make room.
    // This prevents the render thread from blocking and tanking FPS.
    if (count == MAX_QUEUE) {
        // Drop head
        if (frameQueue[head].valid) {
            UnloadImage(frameQueue[head].frame);
            frameQueue[head].valid = false;
        }
        head = (head + 1) % MAX_QUEUE;
        count--;
    }

    // Move (take ownership of) the Image instead of copying pixel data.
    frameQueue[tail].frame = *frameIn;   // shallow struct copy (pointer + metadata)
    frameQueue[tail].valid = true;

    tail = (tail + 1) % MAX_QUEUE;
    count++;

    WakeConditionVariable(&queueNotEmpty);
    LeaveCriticalSection(&queueLock);
}

bool dequeueFrame(Image* outFrame) {
    EnterCriticalSection(&queueLock);

    while (count == 0 && queueRunning) {
        SleepConditionVariableCS(&queueNotEmpty, &queueLock, INFINITE);
    }

    if (count == 0 && !queueRunning) {
        LeaveCriticalSection(&queueLock);
        return false; // no more work, time to exit thread
    }

    *outFrame = frameQueue[head].frame; // transfer ownership
    frameQueue[head].valid = false;

    head = (head + 1) % MAX_QUEUE;
    count--;

    WakeConditionVariable(&queueNotFull);
    LeaveCriticalSection(&queueLock);

    return true;
}
