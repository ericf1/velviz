#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "raylib.h"
#include "csvreader.h"
#include "ffmpeg.h"
#include "string.h"
#include "xaxis.h"

#define WIDTH 1920
#define HEIGHT 1080
#define PLOT_WIDTH 1440
#define PLOT_HEIGHT 950
#define Y_OFFSET 20
#define X_OFFSET 20

#define MAX_PLACEMENT 10
#define MAX_TRACKS 1024

#define FPS 60

#define PLOT_TITLE "Eric in 2023"

typedef struct Track {
    Vector2 position;
    Vector2 size;
    Color baseColor;
    Color barColor;
    float currentAlpha;
    char name[128];
    char artist[128];
    int playcount;
    float playcountDiffDaily;
    bool active;
    Texture2D image;
} Track;

Color parse_color(const char *str) {
    Color color = {0};
    float r, g, b, a;
    
    // Parse the string "(r, g, b, a)"
    if (sscanf(str, "(%f, %f, %f, %f)", &r, &g, &b, &a) == 4) {
        color.r = (int)(r * 255.0f);
        color.g = (int)(g * 255.0f);
        color.b = (int)(b * 255.0f);
        color.a = (int)(a * 255.0f);
    }
    
    return color;
}

int main(void) {

    bool drawing = true;
    bool music = false;

    if (!drawing) SetTraceLogLevel(LOG_NONE); else SetTraceLogLevel(LOG_WARNING);
    FFMPEG *ffmpeg = ffmpeg_start_rendering(WIDTH, HEIGHT, FPS);
    if (!ffmpeg) {
        return 1;
    }
    if (!drawing) SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_UNFOCUSED);
    if (!drawing) SetTargetFPS(0); else SetTargetFPS(FPS);

    InitWindow(WIDTH, HEIGHT, "Velocity Visualization Renderer");
    Music bgMusic;
    if (music) {
        InitAudioDevice();
        bgMusic = LoadMusicStream("C:\\Users\\human\\git\\song-timeline-visualization\\cache\\temp_velviz_audio.mp3");
        PlayMusicStream(bgMusic);
        SetMusicVolume(bgMusic, 0.5f);
    }
    RenderTexture2D screen = LoadRenderTexture(WIDTH, HEIGHT);

    // init xaxis
    Axis xaxis;
    initAxis(&xaxis);

    CSVReader reader;
    char fields[CSV_MAX_FIELDS][CSV_MAX_FIELD_LEN];
    int field_count;

    // instantiate every track
    csv_open(&reader, "data/unique_songs_df.csv");

    Track* tracks = (Track*)RL_CALLOC(MAX_TRACKS, sizeof(Track)); 

    int counter = 0;
    char path_buffer[512];

    enum { ALL_TEXT_SIZE = 16384 };
    char all_text[ALL_TEXT_SIZE] = {0};
    char *ptr = all_text;
    int remaining = ALL_TEXT_SIZE;
    int chars_written = 0;
    chars_written = snprintf(ptr, remaining, "↑^%s0123456789\n", PLOT_TITLE);
    if (chars_written > 0) {
        ptr += chars_written;
        remaining -= chars_written;
    }
    
    while(csv_read_row(&reader, fields, &field_count)) {
        unsigned char r = (unsigned char)strtol(fields[3], NULL, 10);
        unsigned char g = (unsigned char)strtol(fields[4], NULL, 10);
        unsigned char b = (unsigned char)strtol(fields[5], NULL, 10);
        unsigned char a = (unsigned char)strtol(fields[6], NULL, 10);

        tracks[counter].position = (Vector2){ X_OFFSET, 0.0f };
        tracks[counter].size = (Vector2){ PLOT_WIDTH * ((counter % (MAX_PLACEMENT + 1)) / (float)(MAX_PLACEMENT + 1)), 
            (float)PLOT_HEIGHT / MAX_PLACEMENT };
        tracks[counter].baseColor = (Color){ r, g, b, a };
        tracks[counter].barColor = (Color){ r, g, b, a };
        tracks[counter].active = 0;
        tracks[counter].currentAlpha = 1.0f;

        if (remaining > 0) {
            chars_written = snprintf(ptr, remaining, "%s %s\n", fields[7], fields[8]);
            if (chars_written > 0) {
                ptr += chars_written;
                remaining -= chars_written;
            }
        }

        snprintf(tracks[counter].name, sizeof(tracks[counter].name), "%s", fields[7]);
        snprintf(tracks[counter].artist, sizeof(tracks[counter].artist), "%s", fields[8]);

        snprintf(path_buffer, sizeof(path_buffer), "C:\\Users\\human\\git\\song-timeline-visualization\\cache\\export_images\\%d.png", counter);

        tracks[counter].image = LoadTexture(path_buffer);

        counter++;
    }
    csv_close(&reader);

    int codepointCount = 0;
    int *codepoints = LoadCodepoints(all_text, &codepointCount);
    Font font = LoadFontEx("resources/NotoSansJP-SemiBold.ttf", 22, codepoints, codepointCount);    
    
    UnloadCodepoints(codepoints);

    // load like normal things with just 256 cause the characters should be simple
    Font bigFont = LoadFontEx("resources/NotoSansJP-SemiBold.ttf", 48, NULL, 0);
    Font smallFont = LoadFontEx("resources/NotoSansJP-SemiBold.ttf", 12, NULL, 0);

    // now on to the frames
    csv_open(&reader, "data/entire_df.csv");
     
    static char buffered_fields[CSV_MAX_FIELDS][CSV_MAX_FIELD_LEN];
    static int have_buffered = 0;
    static size_t buffered_field_count = 0;

    float max_playcount = 0.0f;
    Color bgColor = RAYWHITE;

    bool running = true;
    int frames = 0; // for counting

    const float FIXED_DELTA_TIME = 1.0f / 60.0f; 
    float currentAnimationTime = 0.0f;

    while (running && !WindowShouldClose()) {

        // disable all tracks
        for (int i = 0; i < counter; i++) {
            tracks[i].active = 0;
        }

        char prevTime[CSV_MAX_FIELD_LEN];
        int havePrev = 0;
        float averagePlaycount = 0.0f;
        float cumulativePlaycount = 0.0f;
        while(1) {
            size_t current_field_count;
            char (*current_fields)[CSV_MAX_FIELD_LEN];
            
            // Use buffered row if available, otherwise read new row
            if (have_buffered) {
                current_fields = buffered_fields;
                current_field_count = buffered_field_count;
                have_buffered = 0;
            } else {
                if (!csv_read_row(&reader, fields, &field_count)) {
                    running = false;
                    break;
                }
                current_fields = fields;
                current_field_count = field_count;
            }
            
            // Check for time change BEFORE processing
            if (havePrev && strcmp(current_fields[0], prevTime) != 0) {
                // Save this row for next frame
                for (size_t i = 0; i < current_field_count; i++) {
                    strncpy(buffered_fields[i], current_fields[i], 
                            CSV_MAX_FIELD_LEN - 1);
                    buffered_fields[i][CSV_MAX_FIELD_LEN - 1] = '\0';
                }
                buffered_field_count = current_field_count;
                have_buffered = 1;
                break;
            }
            
            // Process the row
            int index = (int)strtol(current_fields[11], NULL, 10);
            float placement = (float)strtod(current_fields[1], NULL);
            tracks[index].active = 1;

            tracks[index].position.y = placement / MAX_PLACEMENT * PLOT_HEIGHT - Y_OFFSET;

            float playcount = (float)strtod(current_fields[5], NULL);
            tracks[index].size.x = playcount / max_playcount * PLOT_WIDTH;
            tracks[index].playcount = (int)playcount;

            // 0.6h per frame, 40 frames per day
            float playcountDiffDaily = (float)strtod(current_fields[6], NULL) * 40.0f;
            tracks[index].playcountDiffDaily = playcountDiffDaily;

            cumulativePlaycount = (float)strtod(current_fields[9], NULL);
            averagePlaycount = (float)strtod(current_fields[10], NULL);
            max_playcount = (float)strtod(current_fields[7], NULL);
            char* backgroundColorText = current_fields[8];
            bgColor = parse_color(backgroundColorText);

            strncpy(prevTime, current_fields[0], sizeof(prevTime) - 1);
            prevTime[sizeof(prevTime) - 1] = '\0';
            havePrev = 1;
        }

        // statically have an array of length max_placement*3 memory addresses
        Track* activeTracks[MAX_PLACEMENT * 3];
        int activeTrackCount = 0;
        for (int i = 0; i < counter; i++) {
            if (tracks[i].active) {
                activeTracks[activeTrackCount++] = &tracks[i];
            }
        }

        for (int i = 0; i < activeTrackCount - 1; i++) {
            for (int j = i + 1; j < activeTrackCount; j++) {
                if (activeTracks[i]->playcount > activeTracks[j]->playcount) {
                    Track* temp = activeTracks[i];
                    activeTracks[i] = activeTracks[j];
                    activeTracks[j] = temp;
                }
            }
        }

        if (music) UpdateMusicStream(bgMusic);
        if (drawing) BeginDrawing();
        BeginTextureMode(screen);

            // write the title bro in the top left corner
            DrawTextEx(bigFont, PLOT_TITLE, (Vector2){ X_OFFSET, Y_OFFSET }, 48, 1, BLACK);
            // DrawText("Built by Eric Fang", WIDTH - 150, HEIGHT - 30, 12, DARKGRAY);
            DrawTextEx(smallFont, "Built by Eric Fang", (Vector2){ WIDTH - 100, HEIGHT - 30 }, 12, 1, DARKGRAY);

            // draw a box around the plot
            Rectangle scissorArea = { X_OFFSET, 1.0f / MAX_PLACEMENT * PLOT_HEIGHT - Y_OFFSET, \
                                            WIDTH - 2*X_OFFSET, PLOT_HEIGHT };
            DrawRectangleLines(scissorArea.x - 1.0f, scissorArea.y - 1.0f, scissorArea.width + 2.0f, scissorArea.height + 2.0f, BLACK);

            // draw line for every rectangle
            for (int i = 2; i <= MAX_PLACEMENT; i++) {
                float yPos = (i * PLOT_HEIGHT) / MAX_PLACEMENT - Y_OFFSET;
                DrawLine(X_OFFSET, yPos, WIDTH - X_OFFSET, yPos, LIGHTGRAY);
            }

            // PLOT LEVEL CHANGES
            ClearBackground(bgColor);

            DrawText(TextFormat("%.10s", prevTime), 
                                PLOT_WIDTH, PLOT_HEIGHT - 20.0f - 30, 30, BLACK);
            DrawText(TextFormat("%.2f/day · %d streams", averagePlaycount, (int)cumulativePlaycount), 
                                PLOT_WIDTH, PLOT_HEIGHT - 20.0f, 30, DARKGRAY);

            DrawLine(X_OFFSET, 
                (MAX_PLACEMENT + 1.0f) / MAX_PLACEMENT * PLOT_HEIGHT - Y_OFFSET + 1.0f, 
                WIDTH - X_OFFSET, 
                (MAX_PLACEMENT + 1.0f) / MAX_PLACEMENT * PLOT_HEIGHT - Y_OFFSET + 1.0f, 
                BLACK);

            updateAxis(&xaxis, max_playcount);
            for (int i = 0; i < xaxis.tickCount; i++) {
                float xPos = (xaxis.tickValues[i] * PLOT_WIDTH) / xaxis.max + X_OFFSET;
                DrawLine(xPos, (MAX_PLACEMENT + 1.0f) / MAX_PLACEMENT * PLOT_HEIGHT - 5 - Y_OFFSET, xPos,
                            (MAX_PLACEMENT + 1.0f) / MAX_PLACEMENT * PLOT_HEIGHT + 5 - Y_OFFSET, BLACK);
                
                const char *textToDraw = TextFormat("%d", xaxis.tickValues[i]);
                int centeredXPos = xPos - (MeasureText(textToDraw, 18) / 2);
                DrawText(
                    textToDraw,
                    centeredXPos,
                    (MAX_PLACEMENT + 1.0f) / MAX_PLACEMENT * PLOT_HEIGHT + 6 - Y_OFFSET,
                    18,
                    BLACK
                );
            }

            // overlapping logic
            for (int i = 0; i < activeTrackCount; i++) {
                activeTracks[i]->currentAlpha = 1.0f;
            }

            for (int i = 0; i < activeTrackCount; i++) {
                Rectangle currentRect = { activeTracks[i]->position.x, 
                        activeTracks[i]->position.y + 0.1f / MAX_PLACEMENT * PLOT_HEIGHT, 
                        activeTracks[i]->size.x + activeTracks[i]->size.y - 50.0f, 
                        activeTracks[i]->size.y - 0.1f / MAX_PLACEMENT * PLOT_HEIGHT  };
                for (int j = 0; j < activeTrackCount; j++) {
                    if (i == j || !activeTracks[j]->active) continue;
                    Rectangle otherRect = { activeTracks[j]->position.x, 
                        activeTracks[j]->position.y + 0.1f / MAX_PLACEMENT * PLOT_HEIGHT, 
                        activeTracks[j]->size.x + activeTracks[j]->size.y - 50.0f, 
                        activeTracks[j]->size.y - 0.2f / MAX_PLACEMENT * PLOT_HEIGHT  };

                    // draw rectangles for debug
                    // DrawRectangleLinesEx(currentRect, 2.0f, RED);
                    // DrawRectangleLinesEx(otherRect, 2.0f, BLACK);

                    if (CheckCollisionRecs(currentRect, otherRect)) {
                        // apply alpha to the rectangle with higher playcount
                        if (activeTracks[i]->playcount > activeTracks[j]->playcount)
                            activeTracks[i]->currentAlpha = 0.95f;
                        else
                            activeTracks[j]->currentAlpha = 0.95f;
                    }
                }
            }
            // begin scissor mode
            BeginScissorMode(scissorArea.x, scissorArea.y, scissorArea.width, scissorArea.height);
            for (int i = 0; i < activeTrackCount; i++) {
                // SONG LEVEL CHANGES
                if (activeTracks[i]->playcountDiffDaily >= 8.0f) {
                    Vector3 baseHSV = ColorToHSV(activeTracks[i]->baseColor);
                    float pulse = 0.9f + 0.1f * sinf(currentAnimationTime * 10.0f);
                    float vPulsed = baseHSV.z * pulse;
                    activeTracks[i]->barColor = ColorFromHSV(baseHSV.x, baseHSV.y, vPulsed);

                    DrawTextEx(font, TextFormat("%d↑↑", activeTracks[i]->playcount), (Vector2){ activeTracks[i]->size.x + 105.0f, activeTracks[i]->position.y + activeTracks[i]->size.y/13.0f + 44 }, 22, 1, BLUE);
                }
                else if (abs(activeTracks[i]->barColor.r - activeTracks[i]->baseColor.r) > 10 ||
                        abs(activeTracks[i]->barColor.g - activeTracks[i]->baseColor.g) > 10 ||
                        abs(activeTracks[i]->barColor.b - activeTracks[i]->baseColor.b) > 10) {

                    Vector3 baseHSV = ColorToHSV(activeTracks[i]->baseColor);
                    float pulse = 0.9f + 0.1f * sinf(currentAnimationTime * 10.0f);
                    float vPulsed = baseHSV.z * pulse;
                    activeTracks[i]->barColor = ColorFromHSV(baseHSV.x, baseHSV.y, vPulsed);

                    DrawTextEx(font, TextFormat("%d↑", activeTracks[i]->playcount), (Vector2){ activeTracks[i]->size.x + 105.0f, activeTracks[i]->position.y + activeTracks[i]->size.y/13.0f + 44 }, 22, 1, BLUE);
                } else {
                    activeTracks[i]->barColor = activeTracks[i]->baseColor;
                }

                Color finalColor = Fade(activeTracks[i]->barColor, activeTracks[i]->currentAlpha);
                Rectangle barRect = { activeTracks[i]->position.x, activeTracks[i]->position.y, activeTracks[i]->size.x, activeTracks[i]->size.y };

                DrawRectangleRec(barRect, finalColor);
                DrawTextEx(font, TextFormat("%d", activeTracks[i]->playcount), (Vector2){ activeTracks[i]->size.x + 105.0f, activeTracks[i]->position.y + activeTracks[i]->size.y/13.0f + 44 }, 22, 1, BLUE);

                Rectangle imageRect = { activeTracks[i]->size.x, activeTracks[i]->position.y, activeTracks[i]->size.y, activeTracks[i]->size.y };
                DrawTexturePro(activeTracks[i]->image, 
                    (Rectangle){ 0, 0, activeTracks[i]->image.width, activeTracks[i]->image.height },
                    imageRect,
                    (Vector2){ 0, 0 },  
                    0.0f,
                    WHITE
                );

                // Text annotations
                DrawTextEx(font, activeTracks[i]->name, (Vector2){ activeTracks[i]->size.x + 105.0f, activeTracks[i]->position.y + activeTracks[i]->size.y/13.0f }, 22, 1, BLACK);
                DrawTextEx(font, activeTracks[i]->artist, (Vector2){ activeTracks[i]->size.x + 105.0f, activeTracks[i]->position.y + activeTracks[i]->size.y/13.0f + 22 }, 22, 1, DARKGRAY);
            }
            EndScissorMode();
            
        EndTextureMode();
        ClearBackground(bgColor);
        DrawTextureRec(
            screen.texture,
            (Rectangle){0, 0, (float)screen.texture.width, -(float)screen.texture.height},
            (Vector2){0, 0},
            WHITE
        );
        if (drawing) DrawFPS(10, 10);
        if (drawing) EndDrawing();
        Image image = LoadImageFromTexture(screen.texture);
        ffmpeg_send_frame_flipped(ffmpeg, image.data, WIDTH, HEIGHT);
        UnloadImage(image);

        frames++;
        fprintf(stderr, "Rendered frame %d\r", frames);
        currentAnimationTime += FIXED_DELTA_TIME;
    }

    // unload resources
    for (int i = 0; i < counter; i++) {
        UnloadTexture(tracks[i].image);
    }
    RL_FREE(tracks);

    if (music) {
        CloseAudioDevice();
        UnloadMusicStream(bgMusic);
    }

    csv_close(&reader);
    UnloadRenderTexture(screen);
    CloseWindow();
    ffmpeg_end_rendering(ffmpeg);
    return 0;
}
