#include "image_utils.hpp"

#include <cstdio>
#include <cstring>

static bool get_jpg_size_mem(const uint8_t* data, size_t len, int* width, int* height)
{
    if (len < 2 || data[0] != 0xFF || data[1] != 0xD8) {
        return false;
    }

    size_t pos = 2;
    while (pos < len) {
        while (pos < len && data[pos] != 0xFF) {
            pos++;
        }
        if (pos >= len) {
            break;
        }
        pos++;
        while (pos < len && data[pos] == 0xFF) {
            pos++;
        }
        if (pos >= len) {
            break;
        }

        const uint8_t marker = data[pos++];
        if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
            if (pos + 7 > len) {
                break;
            }
            *height = (data[pos + 3] << 8) | data[pos + 4];
            *width  = (data[pos + 5] << 8) | data[pos + 6];
            return *width > 0 && *height > 0;
        }
        if (pos + 2 > len) {
            break;
        }
        const uint16_t seg_len = (uint16_t)((data[pos] << 8) | data[pos + 1]);
        if (seg_len < 2) {
            break;
        }
        pos += seg_len;
    }
    return false;
}

bool get_image_size_from_memory(const uint8_t* data, size_t len, int* width, int* height, const char* ext)
{
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) {
        return get_jpg_size_mem(data, len, width, height);
    }
    return false;
}

bool get_image_size_from_file(const char* path, int* width, int* height)
{
    const char* dot = strrchr(path, '.');
    if (!dot) {
        return false;
    }

    FILE* f = fopen(path, "rb");
    if (!f) {
        return false;
    }

    uint8_t buf[65536];
    const size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (n == 0) {
        return false;
    }

    return get_image_size_from_memory(buf, n, width, height, dot);
}
