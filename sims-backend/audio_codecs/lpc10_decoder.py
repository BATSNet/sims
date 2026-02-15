"""
Raw PCM to WAV Wrapper

Wraps raw 16-bit PCM audio data in a WAV header for Whisper transcription.
No decoding needed - the device sends raw PCM samples directly.

Note: Module still named lpc10_decoder for import compatibility.
"""

import io
import wave


def decode_to_wav_bytes(raw_pcm: bytes, sample_rate: int = 16000) -> bytes:
    """
    Wrap raw 16-bit mono PCM data in a WAV header.

    Args:
        raw_pcm: Raw 16-bit little-endian mono PCM samples
        sample_rate: Sample rate of the audio (default 16000 Hz)

    Returns:
        Complete WAV file as bytes
    """
    wav_buffer = io.BytesIO()
    with wave.open(wav_buffer, 'wb') as wav_file:
        wav_file.setnchannels(1)       # mono
        wav_file.setsampwidth(2)       # 16-bit
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(raw_pcm)

    return wav_buffer.getvalue()
