from flask import Flask, request, Response, jsonify
import os, io, wave, json
from piper.voice import PiperVoice

app = Flask(__name__)
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
voice = PiperVoice.load(os.path.join(BASE_DIR, "en_US-lessac-medium.onnx"))

# Vowel phonemes from espeak that should open the jaw
VOWEL_PHONEMES = {"a", "e", "i", "o", "u", "A", "E", "I", "O", "U",
                  "æ", "ɑ", "ɒ", "ə", "ɛ", "ɪ", "ʊ", "ʌ", "aɪ", "aʊ", "eɪ", "oʊ"}

PHONEME_JAW = {
    # Wide open vowels
    "a": 0.7, "A": 0.7, "ɑ": 0.7, "ɒ": 0.7, "æ": 0.65,
    # Mid vowels
    "e": 0.5, "E": 0.5, "ɛ": 0.5, "eɪ": 0.5,
    "o": 0.45, "O": 0.45, "oʊ": 0.45,
    # Close vowels (small opening)
    "i": 0.3, "I": 0.3, "ɪ": 0.3,
    "u": 0.25, "U": 0.25, "ʊ": 0.25,
    # Schwa
    "ə": 0.3, "ʌ": 0.4,
    # Diphthongs
    "aɪ": 0.65, "aʊ": 0.65,
    # Consonants — slight opening
    "m": 0.0, "b": 0.0, "p": 0.0,  # bilabials — closed
    "default": 0.15                  # everything else
}

MAX_TEXT_LENGTH = 400

@app.route("/tts", methods=["POST"])
def generate_tts():
    data = request.json
    if not data:
        return "No JSON body", 400

    text = data.get("text", "").strip()[:MAX_TEXT_LENGTH]
    if not text:
        return "No text provided", 400

    try:
        # Get phonemes from piper
        phonemes = voice.phonemize(text)  # returns list of lists of phoneme strings

        # Synthesize audio
        buf = io.BytesIO()
        with wave.open(buf, "wb") as wf:
            voice.synthesize_wav(text, wf)

        buf.seek(44)
        pcm_bytes = buf.read()

        # Estimate timing — piper doesn't give us exact timestamps
        # so we distribute phonemes evenly across audio duration
        sample_rate = 22050
        total_seconds = len(pcm_bytes) / (2 * sample_rate)
        flat_phonemes = [p for sublist in phonemes for p in sublist]
        n = len(flat_phonemes)

        visemes = []
        for idx, phoneme in enumerate(flat_phonemes):
            t = (idx / max(n, 1)) * total_seconds
            jaw = PHONEME_JAW.get(phoneme, PHONEME_JAW["default"])
            visemes.append({"t": round(t, 3), "jaw": jaw})

        # Return JSON with base64 PCM + visemes
        import base64
        return jsonify({
            "pcm": base64.b64encode(pcm_bytes).decode("utf-8"),
            "visemes": visemes,
            "sample_rate": sample_rate
        })

    except Exception as e:
        print("TTS ERROR:", e)
        return str(e), 500

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)