#include "helper.h"

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

char *read_file_to_char_array(const char *filename, size_t *out_size) {
    FILE *file = fopen(filename, "rb");  // "rb" = read binary
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // Allocate buffer (+1 for null terminator)
    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        perror("Failed to allocate buffer");
        return NULL;
    }

    // Read contents
    size_t read_size = fread(buffer, 1, file_size, file);
    buffer[read_size] = '\0';  // null terminate (useful if it's text)

    fclose(file);

    if (out_size)
        *out_size = read_size;

    return buffer;
}

void formatNumber(double num, char *buffer, size_t size) {
    if (num >= 1e12) {
        snprintf(buffer, size, "%.2fT", num / 1e12);
    } else if (num >= 1e9) {
        snprintf(buffer, size, "%.2fB", num / 1e9);
    } else if (num >= 1e6) {
        snprintf(buffer, size, "%.2fM", num / 1e6);
    } else if (num >= 1e3) {
        snprintf(buffer, size, "%.2fK", num / 1e3);
    } else {
        snprintf(buffer, size, "%d", (int)num);
    }

    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == '.') {
            char *end = buffer + i;
            while (*end != '\0') end++;
            while (end > buffer + i && (*(end - 1) == '0' || *(end - 1) == '.'))
                *(--end) = '\0';
            break;
        }
    }
}