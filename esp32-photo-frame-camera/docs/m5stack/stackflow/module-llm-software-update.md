# Module LLM — Software Update (apt)

**Source:** https://docs.m5stack.com/en/stackflow/module_llm/software  
**Retrieved:** 2026-06-22

---

Application and model updates use **`apt`** on the Module LLM Linux system. For full OS reflash, see [image firmware update](./module-llm-image-firmware-update.md).

## 1. Preparation

Access terminal via [ADB / UART / SSH](./module-llm-adb-uart-ssh.md). Configure network if needed.

## 2. Add StackFlow apt repository

```bash
wget -qO /etc/apt/keyrings/StackFlow.gpg https://repo.llm.m5stack.com/m5stack-apt-repo/key/StackFlow.gpg
echo 'deb [arch=arm64 signed-by=/etc/apt/keyrings/StackFlow.gpg] https://repo.llm.m5stack.com/m5stack-apt-repo jammy ax630c' > /etc/apt/sources.list.d/StackFlow.list
apt update
```

## 3. List and install packages

```bash
apt list | grep llm
```

Naming:
- `llm-<name>` — functional unit packages
- `llm-model-<name>` — model packages (large; install selectively)

Example:

```bash
apt install llm-whisper
```

Package metadata: [StackFlow GitHub package list](https://github.com/m5stack/StackFlow)

## 4. Dependency packages

```bash
apt install lib-llm    # runtime environment
apt install llm-sys    # StackFlow base
```

## 5. Feature modules

| Package | Function |
|---------|----------|
| `llm-audio` | Unified sound card management |
| `llm-camera` | Camera management |
| `llm-kws` | Keyword spotting |
| `llm-vad` | Voice activity detection |
| `llm-asr` | Automatic speech recognition |
| `llm-whisper` | Speech-to-text (Whisper) |
| `llm-llm` | Text generation |
| `llm-vlm` | **Vision-language / multimodal** |
| `llm-tts` | Text-to-speech |
| `llm-melotts` | MeloTTS |

## Packages for photo-frame project (VLM)

Required for [VLM CoreS3 example](./vlm-cores3-example.md):

```bash
apt install llm-vlm
apt install llm-model-internvl2.5-1b-364-ax630c
```

## Storage warning

Models consume significant eMMC space. Install only what you need; remove unused models if space is tight.
