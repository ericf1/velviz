#include "video_writer.h"

static DWORD WINAPI EncoderThread(LPVOID param) {
    VideoWriter *vw = (VideoWriter *)param;

    Image frame;
    while (dequeueFrame(&frame)) {
        ffmpeg_send_frame_flipped(
            vw->ffmpeg,
            frame.data,
            (size_t)frame.width,
            (size_t)frame.height
        );
        UnloadImage(frame);
    }

    return 0;
}

void videoWriterInit(VideoWriter *vw, int width, int height, int fps) {
    vw->width  = width;
    vw->height = height;
    vw->fps    = fps;
    vw->running = 1;

    initFrameQueue();
    vw->ffmpeg = ffmpeg_start_rendering(width, height, fps);
    vw->threadHandle = CreateThread(
        NULL,           // default security
        0,              // default stack size
        EncoderThread,  // thread entry function
        vw,             // parameter to thread = this VideoWriter instance
        0,              // run immediately
        NULL            // don't need thread ID
    );
}

void videoWriterPushFrame(VideoWriter *vw, Image *img) {
    (void)vw;
    enqueueFrame(img);
}

void videoWriterStop(VideoWriter *vw) {
    if (!vw->running) {
        return; // already stopped
    }
    vw->running = 0;
    stopFrameQueue();
    WaitForSingleObject(vw->threadHandle, INFINITE);
    CloseHandle(vw->threadHandle);
    ffmpeg_end_rendering(vw->ffmpeg);
    destroyFrameQueue();
}
