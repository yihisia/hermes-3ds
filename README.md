# Hermes 3DS Client

Connect your Nintendo 3DS to Hermes AI via WiFi.

## Features
- Send text messages via virtual touch keyboard
- Voice input via 3DS microphone (transcribed server-side)
- Receive and display AI responses on upper screen
- Scrollable response display
- Conversation history

## Architecture

```
3DS (client) ←WiFi→ Mac Server (bridge) ←→ Hermes
```

## Setup

### 1. Mac Server

```bash
cd server
pip3 install -r requirements.txt
python3 main.py
```

Note your Mac's local IP (printed on startup).

### 2. 3DS Client

Edit `source/main.c` and change `SERVER_HOST` to your Mac's IP:
```c
#define SERVER_HOST "192.168.1.100"  // <-- change this
```

Build (requires devkitPro):
```bash
# Install devkitPro (macOS)
# Download from: https://github.com/devkitPro/installer/releases
# Install 3ds-dev metapackage
dkp-pacman -S 3ds-dev

# Build
cd 3ds-client
make
```

This produces `hermes3ds.3dsx`.

### 3. Deploy to 3DS

Copy `hermes3ds.3dsx` to your 3DS SD card:
```
SD:/3ds/hermes3ds/hermes3ds.3dsx
```

Launch via Homebrew Launcher.

## Controls

| Button | Action |
|--------|--------|
| Touch screen | Type on virtual keyboard |
| A | Send message |
| B | Backspace / Stop recording |
| Y | Start voice recording |
| D-Pad Up/Down | Scroll response |
| START | Exit |

## Voice Input

Press Y to start recording. Speak into the 3DS microphone.
Press B to stop. Audio is sent to your Mac for transcription
via Whisper, then forwarded to Hermes.

## Troubleshooting

- "Server offline" → Check Mac server is running and both devices are on same WiFi
- "Network error" → Firewall may be blocking port 8333
- Voice not working → Install whisper: `pip3 install openai-whisper`

## Notes

- 3DS WiFi is 802.11b/g only (slow), responses may take a few seconds
- Voice transcription requires whisper on Mac (or you can swap in any STT)
- Server keeps last 20 messages as conversation context
