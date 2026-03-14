# MetaHuman AI NPC 
A real-time AI NPC system built in Unreal Engine 5 featuring an Indian MetaHuman avatar with phoneme-driven lip sync, text-to-speech, and Gemini AI responses.

---

## System Requirements

- Unreal Engine 5.3
- Python 3.10
- Windows 10/11

---

## Project Structure
```
Project/
├── GeminiNPC/          ← Unreal Engine project
│   └── Source/
│       └── GeminiNPC/
│           ├── AudioHelperLibrary.h
│           └── AudioHelperLibrary.cpp
└── TTS/                ← Python TTS server
    ├── tts_server.py
    ├── requirements.txt
    ├── setup.bat
    ├── en_US-lessac-medium.onnx
    └── en_US-lessac-medium.onnx.json
```

---

## Setup

### 1. TTS Server

Navigate to the `TTS/` folder and run setup:
```
setup.bat
```

This installs all Python dependencies. The `.onnx` model files are already included — no download needed.

Once setup is done, start the server:
```
python tts_server.py
```

You should see:
```
* Running on http://0.0.0.0:5000
```

Keep this terminal open while using the Unreal project.

### 2. Unreal Project

1. Open `GeminiNPC.uproject` in Unreal Engine 5.3
2. Click **Compile** if prompted
3. Press **Play**

---

## How to Use

1. Start the Python TTS server first (see above)
2. Launch the Unreal project and press Play
3. **Walk your character toward the MetaHuman NPC**
4. When you get close enough, a **text input popup** will appear on screen
5. Type your question into the text box
6. Press **Ask**
7. The MetaHuman will:
   - Send your question to Gemini AI
   - Convert the answer to speech via the local TTS server
   - Speak the answer with real-time lip sync

> **Tip:** If the popup does not appear, make sure you are walking directly toward the NPC and are within interaction range.

---

## How It Works
```
[User types question]
        ↓
  Gemini AI API
  (generates answer)
        ↓
  Python TTS Server
  (Piper TTS — fast local inference)
        ↓
  PCM audio + phoneme viseme timings
        ↓
  Unreal Engine
  (plays audio + drives MetaHuman jaw via JawOpenAlpha and TeethShowAlpha)
```

### Lip Sync
The system extracts phoneme data from the TTS engine and maps each phoneme to a jaw open value — wide for vowels like `a/o`, closed for bilabials like `m/b/p`. These are sent as timestamped keyframes to Unreal, which interpolates between them at 60fps and drives the MetaHuman face rig directly.

---

## Troubleshooting

**TTS server not connecting**
- Make sure `tts_server.py` is running before launching Unreal
- Check that port `5000` is not blocked by firewall

**No audio playing**
- Confirm the Python server prints no errors when a request comes in
- Check Unreal's output log for HTTP errors

**Lip sync looks off**
- The jaw multiplier can be tuned in `AudioHelperLibrary.cpp` in the `StartJawDrive` function
- Raise the `0.4f` cap value for more jaw movement, lower it for subtler movement

**Teeth showing too much or too little**
- Open `AudioHelperLibrary.cpp` and find the `TargetTeeth` line in `StartJawDrive`
- The cap value `0.35f` controls maximum teeth visibility — raise it for more, lower it for less
- The threshold `0.25f` controls how open the jaw needs to be before teeth appear at all
- Values between `0.2f` and `0.5f` give the most natural results

> **Note on Gemini API Daily Limit:** This project uses the free tier of the Google Gemini API which has a limited number of requests per day. Please use it wisely during evaluation — avoid sending questions rapidly in succession. If the NPC stops responding, the daily quota may have been reached and will reset the following day.

---

## Dependencies

| Component | Technology |
|---|---|
| AI Responses | Google Gemini API |
| Text to Speech | Piper TTS (en_US-lessac-medium) |
| Avatar | MetaHuman Creator |
| Engine | Unreal Engine 5.3 |
| Backend | Python 3.10 + Flask |

---

## Acknowledgements

Special thanks to **Mohammed Faraz Ulla Shariff** for their invaluable help in building the Gemini API integration.
