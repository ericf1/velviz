#ifndef VIDEO_WRITER_H
#define VIDEO_WRITER_H

#include "ffmpeg.h"
#include "frame_queue.h"

// Represents one active recording session.
typedef struct VideoWriter {
    FFMPEG *ffmpeg;        // handle to the ffmpeg process and its pipe
    HANDLE threadHandle;   // background encoder thread
    int width;
    int height;
    int fps;
    int running;           // internal flag to sanity check state
} VideoWriter;


void videoWriterInit(VideoWriter *vw, int width, int height, int fps, const char *output_path);
void videoWriterPushFrame(VideoWriter *vw, Image *img);
void videoWriterStop(VideoWriter *vw);

#endif // VIDEO_WRITER_H
