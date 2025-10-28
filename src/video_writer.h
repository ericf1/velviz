#ifndef VIDEO_WRITER_H
#define VIDEO_WRITER_H

#include "ffmpeg.h"
#include "helper.h"

// Represents one active recording session.
typedef struct VideoWriter {
    FFMPEG *ffmpeg;        // handle to the ffmpeg process and its pipe
    HANDLE threadHandle;   // background encoder thread
    int width;
    int height;
    int fps;
    int running;           // internal flag to sanity check state
} VideoWriter;


void videoWriterInit(VideoWriter *vw, int width, int height, int fps);
void videoWriterPushFrame(VideoWriter *vw, Image *img);
void videoWriterStop(VideoWriter *vw);

#endif // VIDEO_WRITER_H
