#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>

char *read_file_to_char_array(const char *filename, size_t *out_size);
void formatNumber(double num, char *buffer, size_t size);
Color parse_color(const char *str);