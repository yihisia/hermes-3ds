"""
Hermes Bridge Server - Mac side
Receives messages from 3DS, forwards to Hermes CLI, returns results.
Runs on the same Mac that has Hermes running.
"""

from fastapi import FastAPI, UploadFile, File, Form
from fastapi.responses import JSONResponse
import subprocess
import tempfile
import os
import json
import time
from typing import Optional

app = FastAPI(title="Hermes 3DS Bridge")

# Conversation history (keep last N exchanges)
MAX_HISTORY = 20
conversation_history = []


def call_hermes(message: str, timeout: int = 120) -> str:
    """Call Hermes CLI and get response."""
    try:
        # Use hermes send --wait to send a message and wait for reply
        result = subprocess.run(
            ["hermes", "send", "--wait", message],
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=os.path.expanduser("~")
        )
        if result.returncode == 0:
            return result.stdout.strip()
        else:
            return f"Error: {result.stderr.strip()}"
    except subprocess.TimeoutExpired:
        return "请求超时，请稍后重试"
    except FileNotFoundError:
        return call_hermes_http(message)
    except Exception as e:
        return f"Error: {str(e)}"


def call_hermes_http(message: str) -> str:
    """Fallback: call Hermes via local HTTP API (gateway)."""
    try:
        import urllib.request
        import urllib.error

        url = "http://localhost:3578/api/chat"
        data = json.dumps({"message": message}).encode("utf-8")
        req = urllib.request.Request(
            url,
            data=data,
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, timeout=120) as resp:
            result = json.loads(resp.read().decode("utf-8"))
            return result.get("response", "No response")
    except Exception as e:
        return f"Hermes unavailable: {str(e)}"


def transcribe_audio(audio_data: bytes, sample_rate: int = 16000) -> str:
    """Transcribe audio using whisper (local) or macOS say fallback."""
    try:
        import whisper

        # Save raw PCM to wav
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
            # Write WAV header
            import struct
            num_samples = len(audio_data) // 2
            data_size = num_samples * 2
            header = struct.pack('<4sI4s4sIHHIIHH4sI',
                b'RIFF', 36 + data_size, b'WAVE',
                b'fmt ', 16, 1, 1, sample_rate, sample_rate * 2, 2, 16,
                b'data', data_size)
            f.write(header)
            f.write(audio_data)
            tmp_path = f.name

        model = whisper.load_model("base")
        result = model.transcribe(tmp_path, language="zh")
        os.unlink(tmp_path)
        return result["text"].strip()

    except ImportError:
        return "[语音识别未安装: pip3 install openai-whisper]"
    except Exception as e:
        return f"[语音识别失败: {str(e)}]"


def format_for_3ds(text: str, max_chars: int = 2000) -> str:
    """Format text for 3DS upper screen display.
    Upper screen is 400x240, about 25 lines x 38 chars per line.
    """
    lines = text.split("\n")
    # Remove excessive blank lines
    cleaned = []
    for line in lines:
        cleaned.append(line.rstrip())

    text = "\n".join(cleaned)

    # Truncate if too long
    if len(text) > max_chars:
        text = text[:max_chars - 20] + "\n...[截断]"

    # Wrap long lines
    wrapped = []
    for line in text.split("\n"):
        while len(line) > 38:
            wrapped.append(line[:38])
            line = line[38:]
        wrapped.append(line)

    # Limit to 24 lines
    if len(wrapped) > 24:
        wrapped = wrapped[:23] + ["...[更多已省略]"]

    return "\n".join(wrapped)


@app.get("/")
async def root():
    return {"status": "online", "service": "Hermes 3DS Bridge", "version": "1.0"}


@app.get("/health")
async def health():
    return {"status": "ok", "timestamp": int(time.time())}


@app.post("/chat")
async def chat(
    message: str = Form(default=""),
    history: bool = Form(default=True)
):
    """Send a text message to Hermes and get response."""
    global conversation_history

    if not message.strip():
        return JSONResponse({"error": "Empty message"}, status_code=400)

    # Build context with recent history
    full_message = message
    if history and conversation_history:
        recent = conversation_history[-6:]  # last 3 exchanges
        context = "\n".join([f"{h['role']}: {h['content'][:100]}" for h in recent])
        full_message = f"[之前的对话:\n{context}]\n\n新消息: {message}"

    # Call Hermes
    response = call_hermes(full_message)

    # Store in history
    conversation_history.append({"role": "user", "content": message})
    conversation_history.append({"role": "assistant", "content": response[:200]})

    # Trim history
    if len(conversation_history) > MAX_HISTORY * 2:
        conversation_history = conversation_history[-(MAX_HISTORY * 2):]

    # Format for 3DS screen
    formatted = format_for_3ds(response)

    return {
        "response": formatted,
        "raw": response,
        "timestamp": int(time.time())
    }


@app.post("/voice")
async def voice(audio: UploadFile = File(...)):
    """Receive audio from 3DS mic, transcribe, send to Hermes."""
    audio_data = await audio.read()

    # Transcribe (3DS mic is 16kHz mono 16-bit PCM)
    text = transcribe_audio(audio_data, sample_rate=16000)

    if text.startswith("["):
        return {"error": text, "transcription": text, "response": ""}

    # Forward transcribed text to Hermes
    response = call_hermes(text)
    formatted = format_for_3ds(response)

    # Store in history
    conversation_history.append({"role": "user", "content": f"[语音] {text}"})
    conversation_history.append({"role": "assistant", "content": response[:200]})

    return {
        "transcription": text,
        "response": formatted,
        "raw": response,
        "timestamp": int(time.time())
    }


@app.get("/history")
async def get_history():
    """Get conversation history for 3DS display."""
    return {
        "history": conversation_history[-10:],
        "count": len(conversation_history)
    }


@app.delete("/history")
async def clear_history():
    """Clear conversation history."""
    global conversation_history
    conversation_history = []
    return {"status": "cleared"}


def get_local_ip():
    """Get local network IP for 3DS to connect to."""
    import socket
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


if __name__ == "__main__":
    import uvicorn
    print("Starting Hermes 3DS Bridge on port 8333...")
    print("Make sure your 3DS is on the same WiFi network.")
    print(f"Server IP: {get_local_ip()}")
    uvicorn.run(app, host="0.0.0.0", port=8333)
