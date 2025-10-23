#include <stdlib.h>
#include <stdio.h>

#include "raylib.h"
#include "csvreader.h"
#include "ffmpeg.h"
#include "string.h"

#define WIDTH 1920
#define HEIGHT 1080
#define PLOT_WIDTH 1440
#define PLOT_HEIGHT 864
#define Y_OFFSET 80
#define X_OFFSET 20
#define PLOT_POSITION_X WIDTH * 0.9f
#define PLOT_POSITION_Y HEIGHT * 0.95f

#define MAX_PLACEMENT 10
#define MAX_TRACKS 2048

#define FPS 45

typedef struct Track {
    Vector2 position;             
    Vector2 size;
    Color color;            
    char name[128];
    char artist[128];
    int playcount;
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
    SetTraceLogLevel(LOG_WARNING);
    FFMPEG *ffmpeg = ffmpeg_start_rendering(WIDTH, HEIGHT, FPS);
    if (!ffmpeg) {
        return 1;
    }
    InitWindow(WIDTH, HEIGHT, "Raylib Example");
    RenderTexture2D screen = LoadRenderTexture(WIDTH, HEIGHT);
    // SetTextureFilter(screen.texture, TEXTURE_FILTER_POINT);

    Font font = LoadFontEx("resources/NotoSans-SemiBold.ttf", 18, NULL, 256);
    // SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);

    CSVReader reader;
    char fields[CSV_MAX_FIELDS][CSV_MAX_FIELD_LEN];
    int field_count;

    // instantiate every track
    csv_open(&reader, "data/temp_combined_videos_debug.csv");

    Track* tracks = (Track*)RL_CALLOC(MAX_TRACKS, sizeof(Track)); 

    int counter = 0;
    char path_buffer[512];
    while(csv_read_row(&reader, fields, &field_count)) {
        unsigned char r = (unsigned char)strtol(fields[3], NULL, 10);
        unsigned char g = (unsigned char)strtol(fields[4], NULL, 10);
        unsigned char b = (unsigned char)strtol(fields[5], NULL, 10);
        unsigned char a = (unsigned char)strtol(fields[6], NULL, 10);

        tracks[counter].position = (Vector2){ X_OFFSET, 0.0f };
        tracks[counter].size = (Vector2){ 0.0f, (float)PLOT_HEIGHT / MAX_PLACEMENT };
        tracks[counter].color = (Color){ r, g, b, a };
        tracks[counter].active = 0;
        snprintf(tracks[counter].name, sizeof(tracks[counter].name), "%s", fields[7]);
        snprintf(tracks[counter].artist, sizeof(tracks[counter].artist), "%s", fields[8]);


        
        snprintf(path_buffer, sizeof(path_buffer), "C:\\Users\\human\\git\\song-timeline-visualization\\cache\\export_images\\%d.png", counter);

        tracks[counter].image = LoadTexture(path_buffer);
        // SetTextureFilter(tracks[counter].image, TEXTURE_FILTER_POINT);

        counter++;
    }
    csv_close(&reader);

    // now on to the frames
    csv_open(&reader, "data/entire_df.csv");
     
    static char buffered_fields[CSV_MAX_FIELDS][CSV_MAX_FIELD_LEN];
    static int have_buffered = 0;
    static size_t buffered_field_count = 0;

    float max_playcount = 0.0f;
    Color bgColor = RAYWHITE;

    SetTargetFPS(FPS);
    while (!WindowShouldClose()) {

        // disable all tracks
        for (int i = 0; i < counter; i++) {
            tracks[i].active = 0;
        }

        char prevTime[CSV_MAX_FIELD_LEN];
        int havePrev = 0;
        
        while(1) {
            size_t current_field_count;
            char (*current_fields)[CSV_MAX_FIELD_LEN];
            
            // Use buffered row if available, otherwise read new row
            if (have_buffered) {
                current_fields = buffered_fields;
                current_field_count = buffered_field_count;
                have_buffered = 0;
            } else {
                if (!csv_read_row(&reader, fields, &field_count)) break;
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
            max_playcount = (float)strtod(current_fields[7], NULL);
            int index = (int)strtol(current_fields[11], NULL, 10);
            tracks[index].active = 1;
            
            float placement = (float)strtod(current_fields[1], NULL);
            tracks[index].position.y = placement / MAX_PLACEMENT * PLOT_HEIGHT - tracks[index].size.y * 0.5f;
            
            float playcount = (float)strtod(current_fields[5], NULL);
            tracks[index].size.x = playcount / max_playcount * PLOT_WIDTH;
            tracks[index].playcount = (int)playcount;

            char* backgroundColorText = current_fields[8];
            bgColor = parse_color(backgroundColorText);

            strncpy(prevTime, current_fields[0], sizeof(prevTime) - 1);
            prevTime[sizeof(prevTime) - 1] = '\0';
            havePrev = 1;
        }

        BeginDrawing();
        BeginTextureMode(screen);

            // plot level changes
            ClearBackground(bgColor);

            DrawText(prevTime, WIDTH - 200, HEIGHT - 25, 20, BLACK);

            // draw x axis it is from 0 to max_playcount
            DrawLine(X_OFFSET, HEIGHT - Y_OFFSET, WIDTH - X_OFFSET, HEIGHT - Y_OFFSET, BLACK);

            for (int x = 0; x <= 10; x++) {
                float xPos = (x / 8.0f * PLOT_WIDTH);
                DrawLine(xPos, HEIGHT - Y_OFFSET - 5, xPos,
                            HEIGHT - Y_OFFSET + 5, BLACK);
                DrawText(
                    TextFormat("%d", (int)(x  / 8.0f * max_playcount)),
                    (int)(xPos - 5),
                    HEIGHT - Y_OFFSET + 10,
                    10,
                    DARKGRAY
                );
            }

            // // add one more tick
            // DrawLine(PLOT_WIDTH * 1.1f, HEIGHT - Y_OFFSET - 5, PLOT_WIDTH * 1.1f,
            //             HEIGHT - Y_OFFSET + 5, BLACK);
            // DrawText(
            //     TextFormat("%d", (int)(max_playcount * 1.1f)),
            //     (int)(PLOT_WIDTH * 1.1f - 5),
            //     HEIGHT - Y_OFFSET + 10,
            //     10,
            //     DARKGRAY
            // );

            for (int i = 0; i < counter; i++) {
                // song level changes
                if (!tracks[i].active) continue;
                // Bar
                DrawRectangleV(tracks[i].position, tracks[i].size, tracks[i].color);

                DrawTexturePro(tracks[i].image, 
                    (Rectangle){ 0, 0, tracks[i].image.width, tracks[i].image.height },
                    (Rectangle){ tracks[i].size.x, tracks[i].position.y, tracks[i].size.y, tracks[i].size.y },
                    (Vector2){ 0, 0 },  
                    0.0f,
                    WHITE   
                );

                // Text annotations
                DrawTextEx(font, tracks[i].name, (Vector2){ tracks[i].size.x + 110.0f, tracks[i].position.y + tracks[i].size.y/16.0f }, 18, 1, BLACK);
                DrawTextEx(font, tracks[i].artist, (Vector2){ tracks[i].size.x + 110.0f, tracks[i].position.y + tracks[i].size.y/16.0f + 18 }, 18, 1, DARKGRAY);
                DrawTextEx(font, 
                    TextFormat("%d", tracks[i].playcount), 
                    (Vector2){ tracks[i].size.x + 110.0f, tracks[i].position.y + tracks[i].size.y/16.0f + 36 }, 
                    18, 
                    1,
                    BLUE);
            }
            
        EndTextureMode();
        ClearBackground(bgColor);
        DrawTextureRec(
            screen.texture,
            (Rectangle){0, 0, (float)screen.texture.width, -(float)screen.texture.height},
            (Vector2){0, 0},
            WHITE
        );
        EndDrawing();
        Image image = LoadImageFromTexture(screen.texture);
        ffmpeg_send_frame_flipped(ffmpeg, image.data, WIDTH, HEIGHT);
        UnloadImage(image);
    }

    // unload resources
    for (int i = 0; i < counter; i++) {
        UnloadTexture(tracks[i].image);
    }
    RL_FREE(tracks);

    csv_close(&reader);
    UnloadRenderTexture(screen);
    CloseWindow();
    ffmpeg_end_rendering(ffmpeg);
    return 0;
}
