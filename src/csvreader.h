#ifndef CSV_READER_H
#define CSV_READER_H

#include <stdio.h>

#define CSV_MAX_FIELDS 128
#define CSV_MAX_FIELD_LEN 256
#define CSV_MAX_LINE 524288

typedef struct {
    FILE *file;
    char delimiter;
    int current_row;
} CSVReader;

// Open/close
int csv_open(CSVReader *reader, const char *filename);
void csv_close(CSVReader *reader);

// Read one row
int csv_read_row(CSVReader *reader, char fields[][CSV_MAX_FIELD_LEN],
                 int *field_count);

#endif // CSV_READER_H