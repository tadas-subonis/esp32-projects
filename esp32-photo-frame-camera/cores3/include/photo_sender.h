#pragma once

#include <cstdint>
#include <cstddef>

/** Connect to PhotoFrame AP with static STA IP. Blocks until connected or timeout. */
bool wifi_photo_frame_connect();

/** POST JPEG + metadata to PaperColor. Returns HTTP status (0 on transport error). */
int photo_frame_send(const uint8_t* jpeg, size_t jpeg_len, int width, int height);
