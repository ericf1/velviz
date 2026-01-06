#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "raylib.h"
#include "csvreader.h"
#include "ffmpeg.h"
#include "string.h"
#include "xaxis.h"
#include "video_writer.h"
#include "helper.h"

#define WIDTH 1920
#define HEIGHT 1080
#define PLOT_WIDTH 1440
#define PLOT_HEIGHT 950
#define Y_OFFSET 20
#define X_OFFSET 20

#define MAX_PLACEMENT 10
#define MAX_TRACKS 1024
#define MAX_ALBUM_VERSIONS 256

// #define PLOT_TITLE "How The Top Companies By Market Cap Got There Since 2010 (in USD)"
// #define PLOT_TITLE "US Spotify in 2024"
#define PLOT_TITLE "Eric in 2024"
#define DRAWING 1
#define MUSIC 1 // theres a bug with music jk
#define MASTER_TIME_DELTA "0.6h" // "0.6h" or "2h" or "4h"
#define AWARDS_ON 1
#define MAX_AWARDS 3
#define ARTIST_MODE 1
#define ALBUM_MODE 0
#define DAILY 0
#define BIG_NUMBER_MODE 0
#define MONEY_MODE 0
#define FPS 60

#define DATA_PATH "C:\\Users\\human\\git\\velviz\\data\\"
#define OUTPUT_PATH "C:\\Users\\human\\git\\velviz\\output\\"

typedef struct AlbumVersion {
    char albumName[128];
    int frameStart;
    Texture2D image;
    Color baseColor;
} AlbumVersion;

typedef struct Track {
    Vector2 position;
    Vector2 size;
    float placement;
    Color baseColor;
    Color barColor;
    float currentAlpha;
    char name[128];
    char artist[128];
    float playcount;
    float playcountDiffDaily;
    bool active;
    Texture2D image;
    int previousIndex;
    // version pointer
    int versionCount;
    AlbumVersion* versions[MAX_ALBUM_VERSIONS];
} Track;

typedef struct Award {
    char category[128];
    char text[256];
    char stat[64];
    Color bgColor;
    Texture2D image;
} Award;

typedef enum {
    STATE_CSV,
    STATE_AWARDS,
    STATE_EXIT,
} AnimationState;

int main(void) {
    // FFMPEG *ffmpeg = ffmpeg_start_rendering(WIDTH, HEIGHT, FPS);
    // if (!ffmpeg) return 1;

    VideoWriter vw;
    videoWriterInit(&vw, WIDTH, HEIGHT, FPS, OUTPUT_PATH);

    if (DRAWING) {
        SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_UNDECORATED);
        SetTraceLogLevel(LOG_WARNING);
        SetTargetFPS(FPS);
    } else {
        SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_UNFOCUSED);
        SetTraceLogLevel(LOG_NONE);
        SetTargetFPS(0);
    }

    InitWindow(WIDTH, HEIGHT, "Velocity Visualization Renderer");
    Music bgMusic;
    if (MUSIC) {
        InitAudioDevice();
        bgMusic = LoadMusicStream(DATA_PATH "temp_velviz_audio.wav");
        PlayMusicStream(bgMusic);
        SetMusicVolume(bgMusic, 0.5f);
    }
    RenderTexture2D screen = LoadRenderTexture(WIDTH, HEIGHT);

    // init xaxis
    Axis xaxis;
    initAxis(&xaxis);

    // init tracks
    Track* tracks = (Track*)RL_CALLOC(MAX_TRACKS, sizeof(Track));
    AlbumVersion* albumVersions = (AlbumVersion*)RL_CALLOC(MAX_TRACKS, sizeof(AlbumVersion));

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

    CSVReader reader;
    char fields[CSV_MAX_FIELDS][CSV_MAX_FIELD_LEN];
    int field_count;

    csv_open(&reader, DATA_PATH "unique_songs_df.csv");
    while(csv_read_row(&reader, fields, &field_count)) {
        unsigned char r = (unsigned char)strtol(fields[2], NULL, 10);
        unsigned char g = (unsigned char)strtol(fields[3], NULL, 10);
        unsigned char b = (unsigned char)strtol(fields[4], NULL, 10);
        unsigned char a = (unsigned char)strtol(fields[5], NULL, 10);

        tracks[counter].position = (Vector2){ X_OFFSET, HEIGHT };
        tracks[counter].size = (Vector2){ PLOT_WIDTH * ((counter % (MAX_PLACEMENT + 1)) / (float)(MAX_PLACEMENT + 1)), 
            (float)PLOT_HEIGHT / MAX_PLACEMENT };
        tracks[counter].baseColor = (Color){ r, g, b, a };
        tracks[counter].barColor = (Color){ r, g, b, a };
        tracks[counter].active = 0;
        tracks[counter].currentAlpha = 1.0f;
        tracks[counter].previousIndex = -1;
        tracks[counter].versionCount = 0;

        if (remaining > 0) {
            chars_written = snprintf(ptr, remaining, "%s %s\n", fields[6], fields[7]);
            if (chars_written > 0) {
                ptr += chars_written;
                remaining -= chars_written;
            }
        }

        snprintf(tracks[counter].name, sizeof(tracks[counter].name), "%s", fields[6]);
        snprintf(tracks[counter].artist, sizeof(tracks[counter].artist), "%s", fields[7]);

        // printf("Loaded track %d: %s by %s\n", counter, tracks[counter].name, tracks[counter].artist);

        snprintf(path_buffer, sizeof(path_buffer), DATA_PATH "export_images\\%d.jpg", counter);

        tracks[counter].image = LoadTexture(path_buffer);

        // printf("Loaded track %d: %s by %s\n", counter, tracks[counter].name, tracks[counter].artist);

        counter++;
    }
    csv_close(&reader);

    if (ALBUM_MODE) {
        // load versions
        csv_open(&reader, DATA_PATH "album_versioning.csv");
        int version_counter = 0;

        while(csv_read_row(&reader, fields, &field_count)) {
            if (version_counter >= MAX_ALBUM_VERSIONS) {
                // or MAX_ALBUM_VERSIONS_TOTAL if you define one
                TraceLog(LOG_WARNING, "Too many album versions in CSV, skipping");
                break;
            }
            // load in the album version data
            char* albumName = fields[1];
            int trackIndex = (int)strtol(fields[6], NULL, 10);
            int frameStart = (int)strtol(fields[7], NULL, 10);
            char* baseColorText = fields[8];
            Color baseColor = parse_color(baseColorText);
            
            albumVersions[version_counter].frameStart = frameStart;
            snprintf(albumVersions[version_counter].albumName, sizeof(albumVersions[version_counter].albumName), "%s", albumName);
            
            char fullImagePath[256];
            snprintf(fullImagePath, sizeof(fullImagePath), DATA_PATH "export_version_images\\%d.jpg", version_counter);
            albumVersions[version_counter].image = LoadTexture(fullImagePath);

            albumVersions[version_counter].baseColor = baseColor;

            Track *t = &tracks[trackIndex];
            t->versions[t->versionCount] = &albumVersions[version_counter];
            t->versionCount++;
            version_counter++;
        }
        csv_close(&reader);
    }

    int codepointCount = 0;
    if (ARTIST_MODE || ALBUM_MODE) {
        size_t size;
        char *glyphs = read_file_to_char_array(DATA_PATH "unique_title_chars.txt", NULL);

        if (glyphs) {
            // merge it into all_text and add to codepoints
            strncat(all_text, glyphs, ALL_TEXT_SIZE - strlen(all_text) - 1);
            free(glyphs);
        }
    }

    // in alltexts also add the "-"
    strncat(all_text, "-\n", ALL_TEXT_SIZE - strlen(all_text) - 1);
   
    int *codepoints = LoadCodepoints(all_text, &codepointCount);
    int normalFontSize = 24;
    Font normalFont = LoadFontEx("resources/NotoSansJP-SemiBold.ttf", normalFontSize, codepoints, codepointCount);

    UnloadCodepoints(codepoints);

    // load like normal things with just 256 cause the characters should be simple
    Font bigFont = LoadFontEx("resources/NotoSansJP-SemiBold.ttf", 48, NULL, 0);
    Font tinyFont = LoadFontEx("resources/NotoSansJP-SemiBold.ttf", 16, NULL, 0);
    Font smallFont = LoadFontEx("resources/NotoSansJP-SemiBold.ttf", 20, NULL, 0);
    
    int medCodepoints[96 + 1];  // 95 ASCII chars + 1 for '·'
    for (int i = 0; i < 95; i++) medCodepoints[i] = 32 + i;  // ASCII range
    medCodepoints[95] = 0x00B7;  // add middle dot
    int mediumFontSize = 34;
    Font mediumFont = LoadFontEx("resources/NotoSansJP-SemiBold.ttf", mediumFontSize, medCodepoints, 96);


    // init awards
    Award awards[MAX_AWARDS];
    // get the data from awards_export.csv
    csv_open(&reader, DATA_PATH "awards_export.csv");
    int award_counter = 0;
    while(csv_read_row(&reader, fields, &field_count)) {
        if (award_counter >= MAX_AWARDS) {
            TraceLog(LOG_WARNING, "Too many awards in CSV, skipping");
            break;
        }
        snprintf(awards[award_counter].category, sizeof(awards[award_counter].category), "%s", fields[0]);
        snprintf(awards[award_counter].text, sizeof(awards[award_counter].text), "%s", fields[1]);
        snprintf(awards[award_counter].stat, sizeof(awards[award_counter].stat), "%s", fields[2]);

        char* bgColorText = fields[3];
        awards[award_counter].bgColor = parse_color(bgColorText);

        char awardImagePath[256];
        int imageIndex = (int)strtol(fields[4], NULL, 10);
        snprintf(awardImagePath, sizeof(awardImagePath), DATA_PATH "export_images\\%d.jpg", imageIndex);
        awards[award_counter].image = LoadTexture(awardImagePath);

        award_counter++;
    }
    csv_close(&reader);

    // now on to the frames
    csv_open(&reader, DATA_PATH "entire_df.csv");
     
    static char buffered_fields[CSV_MAX_FIELDS][CSV_MAX_FIELD_LEN];
    static int have_buffered = 0;
    static size_t buffered_field_count = 0;

    float max_playcount = 0.0f;
    Color bgColor = RAYWHITE;

    bool running = true;

    int frames = 0;
    int frames_by_csv = 0;
    const float FIXED_DELTA_TIME = 1.0f / FPS; 
    float currentAnimationTime = 0.0f;
    static double start;
    int currentAwardIndex = 0;

    AnimationState state = STATE_CSV;
    while (state != STATE_EXIT && !WindowShouldClose()) {
        if (frames == 144) start = GetTime();
        // disable all tracks
        for (int i = 0; i < counter; i++) {
            tracks[i].active = 0;
        }

        char prevTime[CSV_MAX_FIELD_LEN];
        char* fromTime;
        int havePrev = 0;
        float averagePlaycount = 0.0f;
        float cumulativePlaycount = 0.0f;
        while(state == STATE_CSV) {
            size_t current_field_count;
            char (*current_fields)[CSV_MAX_FIELD_LEN];
            
            // Use buffered row if available, otherwise read new row
            if (have_buffered) {
                current_fields = buffered_fields;
                current_field_count = buffered_field_count;
                have_buffered = 0;
            } else {
                if (!csv_read_row(&reader, fields, &field_count)) {
                    if (AWARDS_ON) {
                        state = STATE_AWARDS;
                        currentAwardIndex = 0;
                    } else {
                        state = STATE_EXIT;
                    }
                    frames_by_csv = frames;
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
            // tracks[index].position.y = placement / MAX_PLACEMENT * PLOT_HEIGHT - Y_OFFSET;
            tracks[index].active = 1;
            tracks[index].placement = placement;

            // update the name of the track from current_fields[2]
            if (ARTIST_MODE) snprintf(tracks[index].name, sizeof(tracks[index].name), "%s", current_fields[2]);

            // if album mode, update the track based on the album version data
            if (ALBUM_MODE) {
                int versionCount = tracks[index].versionCount;
                bool picked = false;
                for (int v = versionCount - 1; v >= 0; v--) {
                    AlbumVersion *version = tracks[index].versions[v];
                    if (version->frameStart <= frames) {
                        tracks[index].image = version->image;
                        strncpy(tracks[index].name, version->albumName, sizeof(tracks[index].name) - 1);
                        tracks[index].name[sizeof(tracks[index].name) - 1] = '\0';
                        tracks[index].baseColor = version->baseColor;
                        // tracks[index].barColor = version->baseColor;
                        picked = true;
                        break;
                    }
                }
                if (versionCount > 0 && !picked) {
                    // if we have versions but none of them are active yet, use the first one
                    AlbumVersion *version = tracks[index].versions[0];
                    tracks[index].image = version->image;
                    strncpy(tracks[index].name, version->albumName, sizeof(tracks[index].name) - 1);
                    tracks[index].name[sizeof(tracks[index].name) - 1] = '\0';
                    tracks[index].baseColor = version->baseColor;
                    // tracks[index].barColor = version->baseColor;
                }
            }

            float playcount = (float)strtod(current_fields[5], NULL);
            tracks[index].size.x = playcount / max_playcount * PLOT_WIDTH;
            tracks[index].playcount = playcount;

            // 0.6h per frame, 40 frames per day
            // 2h per frame, 12 frames per day
            float framesPerDay;
            if (MASTER_TIME_DELTA == "4h") {
                framesPerDay = 6.0f;
            } else if (MASTER_TIME_DELTA == "2h") {
                framesPerDay = 12.0f;
            } else { // default to 0.6h
                framesPerDay = 40.0f;
            }
            float playcountDiffDaily = (float)strtod(current_fields[6], NULL) * framesPerDay;
            tracks[index].playcountDiffDaily = playcountDiffDaily;

            // this is the plot level stuff
            cumulativePlaycount = (float)strtod(current_fields[9], NULL);
            averagePlaycount = (float)strtod(current_fields[10], NULL);
            max_playcount = (float)strtod(current_fields[7], NULL);
            char* backgroundColorText = current_fields[8];
            bgColor = parse_color(backgroundColorText);

            fromTime = current_fields[12];

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

        for (int i = 0; i < activeTrackCount; i++) {
            for (int j = i + 1; j < activeTrackCount; j++) {
                if (activeTracks[j]->placement > activeTracks[i]->placement) {
                    Track* temp = activeTracks[i];
                    activeTracks[i] = activeTracks[j];
                    activeTracks[j] = temp;
                }
            }
        }

        // for (int i = 0; i < activeTrackCount; i++) {
        //     for (int j = i + 1; j < activeTrackCount; j++) {
        //         if (activeTracks[j]->playcount < activeTracks[i]->playcount) {
        //             Track* temp = activeTracks[i];
        //             activeTracks[i] = activeTracks[j];
        //             activeTracks[j] = temp;
        //         }
        //     }
        // }

        // // place the last track as the track with the lowest (best) placement
        // for (int i = 0; i < activeTrackCount; i++) {
        //     if (activeTracks[i]->placement < activeTracks[activeTrackCount - 1]->placement) {
        //         Track* temp = activeTracks[activeTrackCount - 1];
        //         activeTracks[activeTrackCount - 1] = activeTracks[i];
        //         activeTracks[i] = temp;
        //     }
        // }
        // // sort the rest of the tracks by playcount ascending
        // for (int i = 0; i < activeTrackCount - 1; i++) {
        //     for (int j = i + 1; j < activeTrackCount - 1; j++) {
        //         if (activeTracks[j]->playcount < activeTracks[i]->playcount) {
        //             Track* temp = activeTracks[i];
        //             activeTracks[i] = activeTracks[j];
        //             activeTracks[j] = temp;
        //         }
        //     }
        // }

        // stablize every track except for the last one cause that one is stablized above
        for (int i = 0; i < activeTrackCount - 1; i++) {
            if (activeTracks[i]->previousIndex == -1) continue;
            if (activeTracks[i]->previousIndex == i) continue;
            int prevIdx = activeTracks[i]->previousIndex;  // Save this first!
            Track *prevTrack = activeTracks[prevIdx];
            if (prevTrack->previousIndex == -1) continue;
            // if the track at our previous position has playcount close to ours,
            // swap back to previous positions to stay stable in the array
            if (i == prevTrack->previousIndex && 
                fabsf(prevTrack->playcount - activeTracks[i]->playcount) == 0.0f) {
                Track* temp = activeTracks[i];
                activeTracks[i] = activeTracks[prevIdx];
                activeTracks[prevIdx] = temp;
            }
        }

        // assign positions based on sorted order
        for (int i = 0; i < activeTrackCount; i++) {
            float goalRank = (activeTrackCount) - i;
            float goalPosition = goalRank / MAX_PLACEMENT * PLOT_HEIGHT - Y_OFFSET;

            if (abs(activeTracks[i]->position.y - goalPosition) < 0.5f) {
                activeTracks[i]->position.y = goalPosition;
                continue;
            }

            activeTracks[i]->position.y += (goalPosition - activeTracks[i]->position.y) * 0.25f;
            
            activeTracks[i]->previousIndex = i;
        }


        if (MUSIC) UpdateMusicStream(bgMusic);
        if (DRAWING) BeginDrawing();
        BeginTextureMode(screen);
            // write the title bro in the top left corner
            DrawTextEx(bigFont, PLOT_TITLE, (Vector2){ X_OFFSET, Y_OFFSET }, 48, 1, BLACK);
            DrawTextEx(tinyFont, "Built by Eric Fang", (Vector2){ WIDTH - 125, HEIGHT - 30 }, 16, 1, DARKGRAY);

            // draw a box around the plot
            Rectangle scissorArea = { X_OFFSET, 1.0f / MAX_PLACEMENT * PLOT_HEIGHT - Y_OFFSET,
                                            WIDTH - 2*X_OFFSET, PLOT_HEIGHT };                         
            DrawRectangle(scissorArea.x - 1.0f, scissorArea.y - 1.0f, scissorArea.width + 2.0f, scissorArea.height + 2.0f, bgColor);
            DrawRectangleLines(scissorArea.x - 1.0f, scissorArea.y - 1.0f, scissorArea.width + 2.0f, scissorArea.height + 2.0f, BLACK);

            // draw line for every rectangle
            for (int i = 2; i <= MAX_PLACEMENT; i++) {
                float yPos = (i * PLOT_HEIGHT) / MAX_PLACEMENT - Y_OFFSET;
                DrawLine(X_OFFSET, yPos, WIDTH - X_OFFSET, yPos, LIGHTGRAY);
            }

            // PLOT LEVEL CHANGES
            ClearBackground(RAYWHITE);
            
            const char* timeFmt = DAILY
                ? "%.10s"
                : "%.10s to %.10s";
            char timeText[256];
            snprintf(timeText, sizeof(timeText), timeFmt, fromTime, prevTime);
            // DrawText(timeText, PLOT_WIDTH - 60.0f, PLOT_HEIGHT - 50.0f, 30, BLACK);

            char averageStreamsText[64];
            char cumulativePlaycountText[64];
            char totalStreamsText[128];
            formatNumber(averagePlaycount, averageStreamsText, sizeof(averageStreamsText));
            formatNumber(cumulativePlaycount, cumulativePlaycountText, sizeof(cumulativePlaycountText));

            // Choose output style depending on mode
            if (MONEY_MODE) {
                snprintf(totalStreamsText, sizeof(totalStreamsText),
                    BIG_NUMBER_MODE
                    ? "$%s"
                    : "$%.2f",
                    averageStreamsText,
                    averagePlaycount);
            }  else if (BIG_NUMBER_MODE) {
                snprintf(totalStreamsText, sizeof(totalStreamsText),
                    "%s/day \u00B7 %s streams", 
                        averageStreamsText, cumulativePlaycountText);
            } else {
                snprintf(totalStreamsText, sizeof(totalStreamsText),
                        "%.2f/day \u00B7 %s streams",
                        averagePlaycount, cumulativePlaycountText);
            }
            // DrawText(totalStreamsText, PLOT_WIDTH - 60.0f, PLOT_HEIGHT - 20.0f, 30, DARKGRAY);

            char combinedText[256];
            if (state == STATE_CSV) {
                snprintf(combinedText, sizeof(combinedText), "%s  |  %s", timeText, totalStreamsText);
            }
            float xOffset = MeasureTextEx(mediumFont, combinedText, mediumFontSize, 1).x;

            DrawTextEx(mediumFont, combinedText, (Vector2){ WIDTH - xOffset - X_OFFSET, Y_OFFSET + 12.0f }, mediumFontSize, 1, BLACK);
            DrawLine(X_OFFSET, 
                (MAX_PLACEMENT + 1.0f) / MAX_PLACEMENT * PLOT_HEIGHT - Y_OFFSET + 1.0f, 
                WIDTH - X_OFFSET, 
                (MAX_PLACEMENT + 1.0f) / MAX_PLACEMENT * PLOT_HEIGHT - Y_OFFSET + 1.0f, 
                BLACK);

            updateAxis(&xaxis, max_playcount);
            for (int i = 0; i < xaxis.tickCount; i++) {
                float xPos = (xaxis.tickValues[i] * (float)PLOT_WIDTH) / xaxis.max + X_OFFSET;
                DrawLine(xPos, (MAX_PLACEMENT + 1.0f) / MAX_PLACEMENT * PLOT_HEIGHT - 5 - Y_OFFSET, xPos,
                            (MAX_PLACEMENT + 1.0f) / MAX_PLACEMENT * PLOT_HEIGHT + 5 - Y_OFFSET, BLACK);

                char tickLabel[32];
                formatNumber(xaxis.tickValues[i], tickLabel, sizeof(tickLabel));
                int centeredXPos = xPos - (MeasureText(tickLabel, 18) / 2);
                DrawTextEx(
                    smallFont,
                    tickLabel,
                    (Vector2){ centeredXPos, (MAX_PLACEMENT + 1.0f) / MAX_PLACEMENT * PLOT_HEIGHT + 6 - Y_OFFSET },
                    20,
                    1,
                    BLACK
                );
            }

            // overlapping logic
            if (state == STATE_CSV) {
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
            }
            // begin scissor mode
            BeginScissorMode(scissorArea.x, scissorArea.y, scissorArea.width, scissorArea.height);

            if (state == STATE_CSV) {
                for (int i = 0; i < activeTrackCount; i++) {
                    // SONG LEVEL CHANGES
                    char playcountText[32];
                    playcountText[0] = '\0';

                    float songsThresholdDaily;

                    // if we gain 20% of our playcount in a day, we pulse harder
                    if (MONEY_MODE) {
                        songsThresholdDaily = activeTracks[i]->playcount * 0.25f;
                    } else {
                        songsThresholdDaily = BIG_NUMBER_MODE ? 500000.0f : 8.0f;
                    }

                    if (activeTracks[i]->playcountDiffDaily >= songsThresholdDaily) {
                        Vector3 baseHSV = ColorToHSV(activeTracks[i]->baseColor);
                        float pulse = 0.9f + 0.1f * sinf(currentAnimationTime * 10.0f);
                        float vPulsed = baseHSV.z * pulse;
                        activeTracks[i]->barColor = ColorFromHSV(baseHSV.x, baseHSV.y, vPulsed);

                        formatNumber(activeTracks[i]->playcount, playcountText, sizeof(playcountText));
                        strcat(playcountText, "↑↑");
                    }
                    else if (abs(activeTracks[i]->barColor.r - activeTracks[i]->baseColor.r) > 10 ||
                            abs(activeTracks[i]->barColor.g - activeTracks[i]->baseColor.g) > 10 ||
                            abs(activeTracks[i]->barColor.b - activeTracks[i]->baseColor.b) > 10) {

                        Vector3 baseHSV = ColorToHSV(activeTracks[i]->baseColor);
                        float pulse = 0.9f + 0.1f * sinf(currentAnimationTime * 10.0f);
                        float vPulsed = baseHSV.z * pulse;
                        activeTracks[i]->barColor = ColorFromHSV(baseHSV.x, baseHSV.y, vPulsed);

                        formatNumber(activeTracks[i]->playcount, playcountText, sizeof(playcountText));
                        strcat(playcountText, "↑");
                    } else {
                        activeTracks[i]->barColor = activeTracks[i]->baseColor;
                    }
                    if (playcountText[0] == '\0') formatNumber(activeTracks[i]->playcount, playcountText, sizeof(playcountText));

                    Color finalColor = Fade(activeTracks[i]->barColor, activeTracks[i]->currentAlpha);
                    Rectangle barRect = { activeTracks[i]->position.x, activeTracks[i]->position.y, activeTracks[i]->size.x, activeTracks[i]->size.y };
                    DrawRectangleRec(barRect, finalColor);

                    Texture2D img = activeTracks[i]->image;
                    int cropSize = (img.width < img.height) ? img.width : img.height;
                    int offsetX = (img.width  - cropSize) / 2;
                    int offsetY = (img.height - cropSize) / 2;

                    Rectangle src = {
                        offsetX,
                        offsetY,
                        (float)cropSize,
                        (float)cropSize
                    };

                    Rectangle dst = {
                        activeTracks[i]->size.x,
                        activeTracks[i]->position.y,
                        activeTracks[i]->size.y,
                        activeTracks[i]->size.y  // destination is square
                    };

                    DrawTexturePro(
                        img,
                        src,         // cropped source region
                        dst,         // draw region (resized)
                        (Vector2){0, 0},
                        0.0f,
                        WHITE
                    );

                    // Text annotations
                    float annotationXOffset = activeTracks[i]->size.y * 1.105f;
                    DrawTextEx(normalFont, 
                        activeTracks[i]->name, 
                        (Vector2){ activeTracks[i]->size.x + annotationXOffset, activeTracks[i]->position.y + activeTracks[i]->size.y/12.0f }, 
                        normalFontSize, 1, BLACK);
                    DrawTextEx(normalFont, 
                        activeTracks[i]->artist, 
                        (Vector2){ activeTracks[i]->size.x + annotationXOffset, activeTracks[i]->position.y + activeTracks[i]->size.y/12.0f + normalFontSize }, 
                        normalFontSize, 1, DARKGRAY);

                    // if (MONEY_MODE) {
                    //     // add $ in front of playcountText
                    //     char moneyPlaycountText[64];
                    //     snprintf(moneyPlaycountText, sizeof(moneyPlaycountText), "$%s", playcountText);
                    //     DrawTextEx(normalFont, 
                    //         moneyPlaycountText, 
                    //         (Vector2){ activeTracks[i]->size.x + annotationXOffset, activeTracks[i]->position.y + activeTracks[i]->size.y/12.0f + normalFontSize*2 }, 
                    //         normalFontSize, 1, BLUE);
                    // } else {
                    DrawTextEx(normalFont, 
                        playcountText, 
                        (Vector2){ activeTracks[i]->size.x + annotationXOffset, activeTracks[i]->position.y + activeTracks[i]->size.y/12.0f + normalFontSize*2 }, 
                        normalFontSize, 1, BLUE);
                    // }
                    
                }
            }

            if (state == STATE_AWARDS) {
                // draw awards screen
                if (currentAwardIndex < MAX_AWARDS) {
                    Award* award = &awards[currentAwardIndex];

                    // draw background
                    DrawRectangle(0, 0, WIDTH, HEIGHT, award->bgColor);

                    // draw award image in center
                    float imgWidth = award->image.width;
                    float imgHeight = award->image.height;
                    float scaleFactor = 600.0f / imgHeight;
                    float drawWidth = imgWidth * scaleFactor;
                    float drawHeight = imgHeight * scaleFactor;

                    // fade image in
                    float fadeDuration = 1.0f; // seconds
                    float timeSinceAwardStart = (frames - frames_by_csv) / (float)FPS;
                    float alpha = (timeSinceAwardStart < fadeDuration) 
                        ? (timeSinceAwardStart / fadeDuration) 
                        : 1.0f;
                    DrawTexturePro(
                        award->image,
                        (Rectangle){0, 0, imgWidth, imgHeight},
                        (Rectangle){(WIDTH - drawWidth) / 2.0f, (HEIGHT - drawHeight) / 2.0f - 50.0f, drawWidth, drawHeight},
                        (Vector2){0, 0},
                        0.0f,
                        Fade(WHITE, alpha)
                    );

                    // draw text
                    Vector2 textSize = MeasureTextEx(normalFont, award->text, 24, 1);
                    float x = (WIDTH - textSize.x) / 2.0f;
                    DrawTextEx(normalFont, 
                        award->text, 
                        (Vector2){ x, HEIGHT - 275.0f }, 
                        24, 1, BLACK
                    );

                    Vector2 statSize = MeasureTextEx(normalFont, award->stat, 24, 1);
                    float statX = (WIDTH - statSize.x) / 2.0f;
                    DrawTextEx(normalFont,
                        award->stat, 
                        (Vector2){ statX, HEIGHT - 250.0f }, 
                        24, 1, BLACK
                    );

                    // draw the category title above the image
                    Vector2 categorySize = MeasureTextEx(bigFont, award->category, 48, 1);
                    float categoryX = (WIDTH - categorySize.x) / 2.0f;
                    DrawTextEx(bigFont, 
                        award->category, 
                        (Vector2){ categoryX, 125.0f }, 
                        48, 1, BLACK
                    );

                    // persist the images for a few seconds
                    if (frames - frames_by_csv > FPS * 9) {
                        currentAwardIndex++;
                        frames_by_csv = frames;
                    }
                } else {
                    // end the animation after all awards shown
                    state = STATE_EXIT;
                }

                // if (frames - frames_by_csv > FPS * 10) {
                //     state = STATE_EXIT;
                // }
            }

            EndScissorMode();
            
            
        EndTextureMode();
        if (DRAWING)  {
            DrawTextureRec(
                screen.texture,
                (Rectangle){0, 0, screen.texture.width, -screen.texture.height},
                (Vector2){0, 0},
                WHITE
            );
            ClearBackground(RAYWHITE);
            DrawFPS(10, 10);
            EndDrawing();
        }
        Image image = LoadImageFromTexture(screen.texture);
        videoWriterPushFrame(&vw, &image);

        if (frames % 144 == 0 && frames > 144) {
            double elapsed = GetTime() - start;
            fprintf(stderr, "Frame %d | elapsed %.3fs | avg FPS %.1f\r",
                    frames, elapsed, frames / elapsed);
            fflush(stderr);
        }
        frames++;
        currentAnimationTime += FIXED_DELTA_TIME;
    }

    // print out the final time and fps and frame
    double totalElapsed = GetTime() - start;
    fprintf(stderr, "\nTotal frames %d | Total time %.3fs | Total avg FPS %.1f\n",
            frames, totalElapsed, frames / totalElapsed);

    // unload resources
    for (int i = 0; i < counter; i++) {
        UnloadTexture(tracks[i].image);
    }
    RL_FREE(tracks);

    if (MUSIC) {
        CloseAudioDevice();
        UnloadMusicStream(bgMusic);
    }

    csv_close(&reader);
    UnloadRenderTexture(screen);
    CloseWindow();
    // ffmpeg_end_rendering(ffmpeg);
    videoWriterStop(&vw);
    return 0;
}
