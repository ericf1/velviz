#include "csvreader.h"
#include <string.h>
#include <stdlib.h>

int csv_open(CSVReader *reader, const char *filename) {
    reader->file = fopen(filename, "r");
    if (!reader->file) {
        return 0; // Failed
    }
    reader->delimiter = ',';
    reader->current_row = 0;

    // Skip header row
    char dummy[CSV_MAX_LINE];
    fgets(dummy, sizeof(dummy), reader->file);
    
    return 1; // Success
}

void csv_close(CSVReader *reader) {
    if (reader->file) {
        fclose(reader->file);
        reader->file = NULL;
    }
}

int csv_read_row(CSVReader *reader, char fields[][CSV_MAX_FIELD_LEN],
                 int *field_count) {
    if (!reader->file) return 0;

    char line[CSV_MAX_LINE];
    if (!fgets(line, sizeof(line), reader->file)) {
        return 0; // EOF or error
    }

    // Remove newline
    line[strcspn(line, "\n")] = 0;

    *field_count = 0;
    int in_quotes = 0;
    int pos = 0;
    char *out = fields[*field_count];

    for (char *p = line; *p; p++) {
        char c = *p;

        if (c == '"') {
            if (in_quotes && p[1] == '"') {
                // Escaped quote ("")
                if (pos < CSV_MAX_FIELD_LEN - 1)
                    out[pos++] = '"';
                p++; // skip second quote
            } else {
                in_quotes = !in_quotes; // toggle quote mode
            }
        } 
        else if (c == ',' && !in_quotes) {
            // End of field
            out[pos] = '\0';
            (*field_count)++;
            if (*field_count >= CSV_MAX_FIELDS)
                break;
            out = fields[*field_count];
            pos = 0;
        } 
        else if (c != '\r' && c != '\n') {
            // Regular character
            if (pos < CSV_MAX_FIELD_LEN - 1)
                out[pos++] = c;
        }
    }

    // Final field
    out[pos] = '\0';
    (*field_count)++;

    reader->current_row++;
    return 1; // Success
}