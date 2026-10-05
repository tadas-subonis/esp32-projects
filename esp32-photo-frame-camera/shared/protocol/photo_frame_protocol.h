/*
 * Shared protocol constants for CoreS3 ↔ PaperColor photo transfer.
 * Copy or symlink into each firmware project; keep in sync manually until
 * a code-generation step exists.
 */
#pragma once

#define PHOTO_FRAME_PROTOCOL_VERSION 1

#define PHOTO_FRAME_API_PATH "/api/v1/photo"

/** Suggested max JPEG payload (bytes) for v1 */
#define PHOTO_FRAME_MAX_JPEG_BYTES (128 * 1024)

/** Default JPEG quality for frame2jpg() on CoreS3 */
#define PHOTO_FRAME_JPEG_QUALITY 40

/** Spectra-6 panel size (landscape) */
#define PHOTO_FRAME_EPD_WIDTH 600
#define PHOTO_FRAME_EPD_HEIGHT 400

/** Typical full-refresh duration for UX messaging */
#define PHOTO_FRAME_REFRESH_ETA_SEC 18
