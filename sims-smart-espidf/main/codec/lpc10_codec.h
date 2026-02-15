/**
 * Raw PCM Speech Recorder
 *
 * Records 16kHz 16-bit mono PCM directly into PSRAM with software gain.
 * Input: 16kHz PCM from microphone.
 * Output: Raw 16-bit PCM samples at native 16kHz (no downsampling).
 * 5 seconds at 16kHz 16-bit = 160,000 bytes.
 *
 * Note: Files still named lpc10_codec for build compatibility.
 * The flag bit 4 in the binary format now means "raw 16kHz PCM".
 */

#ifndef LPC10_CODEC_H
#define LPC10_CODEC_H

#include <stdint.h>
#include <stddef.h>

// Max raw PCM buffer: 5 seconds at 16kHz 16-bit mono = 160,000 bytes
#define RAW_PCM_MAX_BYTES  160000

class LPC10Encoder {
public:
    LPC10Encoder();
    ~LPC10Encoder();

    // Reset for a new recording
    void reset();

    // Feed 16kHz PCM samples - applies gain and stores at native 16kHz
    void feedSamples(const int16_t* samples, size_t count);

    // Access recorded data
    const uint8_t* getEncodedData() const { return _buf; }
    size_t getEncodedSize() const { return _bufPos; }

    // Check if buffer is full
    bool isFull() const { return _bufPos >= RAW_PCM_MAX_BYTES; }

    // Get recording duration in milliseconds
    uint32_t getDurationMs() const;

private:
    // Sample counter for duration
    uint32_t _sampleCount;

    // Raw PCM output buffer (allocated in PSRAM)
    uint8_t* _buf;
    size_t _bufPos;
};

#endif // LPC10_CODEC_H
