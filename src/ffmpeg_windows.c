// from this guy goated guy: https://github.com/tsoding/rendering-video-in-c-with-ffmpeg/blob/master/ffmpeg_windows.c
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ffmpeg.h"

struct FFMPEG {
    HANDLE hProcess;
    HANDLE hPipeWrite;
};

static LPSTR GetLastErrorAsString(void)
{
    // https://stackoverflow.com/questions/1387064/how-to-get-the-error-message-from-the-error-code-returned-by-getlasterror

    DWORD errorMessageId = GetLastError();
    assert(errorMessageId != 0);

    LPSTR messageBuffer = NULL;

    DWORD size =
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, // DWORD   dwFlags,
            NULL, // LPCVOID lpSource,
            errorMessageId, // DWORD   dwMessageId,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // DWORD   dwLanguageId,
            (LPSTR) &messageBuffer, // LPTSTR  lpBuffer,
            0, // DWORD   nSize,
            NULL // va_list *Arguments
        );

    return messageBuffer;
}

FFMPEG *ffmpeg_start_rendering(size_t width, size_t height, size_t fps, const char *path)
{
    HANDLE pipe_read;
    HANDLE pipe_write;

    SECURITY_ATTRIBUTES saAttr = {0};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;

    if (!CreatePipe(&pipe_read, &pipe_write, &saAttr, 1024 * 1024 * 32)) {
        fprintf(stderr, "ERROR: Could not create pipe: %s\n", GetLastErrorAsString());
        return NULL;
    }

    if (!SetHandleInformation(pipe_write, HANDLE_FLAG_INHERIT, 0)) {
        fprintf(stderr, "ERROR: Could not SetHandleInformation: %s\n", GetLastErrorAsString());
        return NULL;
    }

    // https://docs.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output

    STARTUPINFO siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(siStartInfo));
    siStartInfo.cb = sizeof(STARTUPINFO);
    // NOTE: theoretically setting NULL to std handles should not be a problem
    // https://docs.microsoft.com/en-us/windows/console/getstdhandle?redirectedfrom=MSDN#attachdetach-behavior
    siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    // TODO(#32): check for errors in GetStdHandle
    siStartInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    siStartInfo.hStdInput = pipe_read;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    char cmd_buffer[1024*2];
    snprintf(
        cmd_buffer,
        sizeof(cmd_buffer),
        "ffmpeg.exe "
        "-hide_banner -loglevel error "
        "-y "
        "-f rawvideo -pix_fmt rgba "
        "-s %zux%zu -r %zu "
        "-i - "
        "-c:v h264_amf -quality balanced -usage transcoding "
        "-rc vbr_peak -qmin 20 -qmax 28 -b:v 12M -maxrate 15M " // VBR for better quality/speed
        "-bf 3 -refs 2 " // More B-frames and reference frames
        "-pix_fmt yuv420p -an %soutput.mp4",
        width, height, fps, path
    );

    BOOL bSuccess =
        CreateProcess(
            NULL,
            cmd_buffer,
            NULL,
            NULL,
            TRUE,
            0,
            NULL,
            NULL,
            &siStartInfo,
            &piProcInfo
        );

    if (!bSuccess) {
        fprintf(stderr, "ERROR: Could not create child process: %s\n", GetLastErrorAsString());
        return NULL;
    }
    
    SetPriorityClass(piProcInfo.hProcess, ABOVE_NORMAL_PRIORITY_CLASS);

    CloseHandle(pipe_read);
    CloseHandle(piProcInfo.hThread);

    // TODO: use Windows specific allocation stuff?
    FFMPEG *ffmpeg = malloc(sizeof(FFMPEG));
    assert(ffmpeg != NULL && "Buy MORE RAM lol!!");
    ffmpeg->hProcess = piProcInfo.hProcess;
    // ffmpeg->hPipeRead = pipe_read;
    ffmpeg->hPipeWrite = pipe_write;
    return ffmpeg;
}

void ffmpeg_send_frame(FFMPEG *ffmpeg, void *data, size_t width, size_t height)
{
    size_t frame_bytes = width * height * sizeof(uint32_t);

    DWORD bytesWritten;
    WriteFile(ffmpeg->hPipeWrite, data, (DWORD)frame_bytes, &bytesWritten, NULL);
}

void ffmpeg_send_frame_flipped(FFMPEG *ffmpeg, void *data, size_t width, size_t height)
{
    size_t bytes_per_row = width * sizeof(uint32_t);        // 4 bytes per pixel
    size_t frame_bytes   = bytes_per_row * height;

    // allocate a temp buffer for the flipped frame
    static uint8_t *flipbuf = NULL;
    static size_t flipbuf_size = 0;

    if (flipbuf_size < frame_bytes) {
        // grow once if needed
        free(flipbuf);
        flipbuf = (uint8_t *)malloc(frame_bytes);
        flipbuf_size = frame_bytes;
    }

    // source pixels as 32-bit RGBA
    uint8_t *src = (uint8_t *)data;

    // flip into flipbuf: last row -> first row, etc.
    for (size_t y = 0; y < height; y++) {
        size_t src_row = (height - 1 - y);            // original row index
        memcpy(
            flipbuf + y * bytes_per_row,              // dest row y
            src + src_row * bytes_per_row,            // src row from bottom
            bytes_per_row
        );
    }

    // single write of the whole frame
    DWORD bytesWritten;
    WriteFile(ffmpeg->hPipeWrite, flipbuf, (DWORD)frame_bytes, &bytesWritten, NULL);
}

void ffmpeg_end_rendering(FFMPEG *ffmpeg)
{
    FlushFileBuffers(ffmpeg->hPipeWrite);
    // FlushFileBuffers(ffmpeg->hPipeRead);

    CloseHandle(ffmpeg->hPipeWrite);
    // CloseHandle(ffmpeg->hPipeRead);

    DWORD result = WaitForSingleObject(
                       ffmpeg->hProcess,     // HANDLE hHandle,
                       INFINITE // DWORD  dwMilliseconds
                   );

    if (result == WAIT_FAILED) {
        fprintf(stderr, "ERROR: could not wait on child process: %s\n", GetLastErrorAsString());
        return;
    }

    DWORD exit_status;
    if (GetExitCodeProcess(ffmpeg->hProcess, &exit_status) == 0) {
        fprintf(stderr, "ERROR: could not get process exit code: %lu\n", GetLastError());
        return;
    }

    if (exit_status != 0) {
        fprintf(stderr, "ERROR: command exited with exit code %lu\n", exit_status);
        return;
    }

    CloseHandle(ffmpeg->hProcess);
}