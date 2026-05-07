// AAudio low-latency audio output for BeebEm Android.
// Pulls 8-bit unsigned mono PCM from the ring buffer (android_platform.cpp)
// and converts it to float for AAudio.

#include <aaudio/AAudio.h>
#include <android/log.h>
#include <stdint.h>
#include <string.h>

#include "android_internal.h"

#define LOG_TAG "BeebAudio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static AAudioStream *s_stream = nullptr;

// Pre-allocated conversion buffer — callback never allocates.
static constexpr int kMaxBurst = 8192;
static uint8_t s_tmpBuf[kMaxBurst];

static aaudio_data_callback_result_t AudioDataCallback(
        AAudioStream * /*stream*/,
        void * /*userData*/,
        void *audioData,
        int32_t numFrames)
{
    auto *out = static_cast<float *>(audioData);
    int n = numFrames < kMaxBurst ? numFrames : kMaxBurst;

    // Pop from BeebEm ring buffer; output silence on underrun.
    int got = GetBytesFromSDLSoundBuffer(n, s_tmpBuf);
    // Two separate loops avoid a branch on every sample.
    for (int i = 0; i < got; ++i)
        out[i] = (float)(s_tmpBuf[i] - 128) * (1.0f / 128.0f);
    if (got < n)
        memset(out + got, 0, (size_t)(n - got) * sizeof(float));
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void AudioErrorCallback(
        AAudioStream * /*stream*/,
        void * /*userData*/,
        aaudio_result_t error)
{
    LOGE("AAudio error: %s", AAudio_convertResultToText(error));
}

void AndroidAudioStart(unsigned int sampleRate)
{
    if (s_stream) return; // already running

    AAudioStreamBuilder *builder = nullptr;
    aaudio_result_t r = AAudio_createStreamBuilder(&builder);
    if (r != AAUDIO_OK) {
        LOGE("createStreamBuilder: %s", AAudio_convertResultToText(r));
        return;
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(builder, 1); // mono
    AAudioStreamBuilder_setSampleRate(builder, (int32_t)sampleRate);
    AAudioStreamBuilder_setDataCallback(builder, AudioDataCallback, nullptr);
    AAudioStreamBuilder_setErrorCallback(builder, AudioErrorCallback, nullptr);

    r = AAudioStreamBuilder_openStream(builder, &s_stream);
    AAudioStreamBuilder_delete(builder);

    if (r != AAUDIO_OK) {
        LOGE("openStream: %s", AAudio_convertResultToText(r));
        s_stream = nullptr;
        return;
    }

    int32_t actualRate  = AAudioStream_getSampleRate(s_stream);
    int32_t burstFrames = AAudioStream_getFramesPerBurst(s_stream);
    AAudioStream_setBufferSizeInFrames(s_stream, burstFrames * 3);
    LOGI("AAudio stream: %d Hz, burst=%d frames", actualRate, burstFrames);

    r = AAudioStream_requestStart(s_stream);
    if (r != AAUDIO_OK) {
        LOGE("requestStart: %s", AAudio_convertResultToText(r));
        AAudioStream_close(s_stream);
        s_stream = nullptr;
    }
}

void AndroidAudioStop()
{
    if (!s_stream) return;
    AAudioStream_requestStop(s_stream);
    AAudioStream_close(s_stream);
    s_stream = nullptr;
    LOGI("AAudio stream closed");
}
