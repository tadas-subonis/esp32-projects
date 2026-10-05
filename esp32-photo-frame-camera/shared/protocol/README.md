# Photo frame wire protocol (v1 draft)

HTTP between **CoreS3** (client) and **PaperColor** (server) on the LAN.

## Endpoint

```
POST /api/v1/photo HTTP/1.1
Host: <papercolor-ip>
Content-Type: multipart/form-data; boundary=----pfboundary
```

### Multipart fields

| Part name | Content-Type | Description |
|-----------|--------------|-------------|
| `meta` | `application/json` | Metadata (see below) |
| `image` | `image/jpeg` | JPEG bytes |

### `meta` JSON schema

```json
{
  "version": 1,
  "id": "2026-06-22T18:30:00Z-abc123",
  "caption": "A coffee mug on a wooden desk.",
  "width": 320,
  "height": 240,
  "jpeg_quality": 40,
  "captured_ms": 1719082200123
}
```

| Field | Required | Notes |
|-------|----------|-------|
| `version` | yes | Protocol version `1` |
| `id` | yes | Unique frame id for dedup / logging |
| `caption` | no | Empty string if LLM disabled |
| `width`, `height` | yes | Source dimensions before PaperColor resize |
| `jpeg_quality` | no | Encoder setting on CoreS3 |
| `captured_ms` | no | `millis()` or Unix ms |

## Response

```json
HTTP/1.1 202 Accepted
Content-Type: application/json

{"status":"queued","refresh_eta_s":18}
```

| Code | Meaning |
|------|---------|
| 202 | Accepted; e-ink refresh started |
| 413 | JPEG too large (suggest max 128 KB v1) |
| 415 | Not JPEG |
| 503 | Busy (refresh in progress) |

## Discovery (optional v1.1)

- mDNS: `_photo-frame._tcp` on PaperColor
- Or static IP in `cores3/secrets.h` → `PAPERCOLOR_HOST`

## C constants

See [`photo_frame_protocol.h`](photo_frame_protocol.h) for shared `#define`s (path copied into each firmware tree as needed).
