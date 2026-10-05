#pragma once

#include "esp_err.h"

#define PHOTO_STORAGE_BASE_PATH "/data"

esp_err_t photo_storage_init();
bool photo_storage_is_mounted();
