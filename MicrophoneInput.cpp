
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "MicrophoneInput.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>

struct MicrophoneInput::Impl
{
    ma_device device{};

    // 오디오 스레드와 렌더링 스레드 사이에서 공유
    std::atomic<float> latestRms{ 0.0f };

    bool running = false;

    // miniaudio가 오디오 데이터를 전달하는 콜백
    static void dataCallback(
        ma_device* device,
        void* output,
        const void* input,
        ma_uint32 frameCount)
    {
        (void)output;

        auto* self = static_cast<Impl*>(
            device->pUserData
            );

        if (!self || !input || frameCount == 0)
            return;

        // ma_format_f32, 모노 입력으로 설정함
        const float* samples =
            static_cast<const float*>(input);

        double sumSquares = 0.0;

        for (ma_uint32 i = 0; i < frameCount; ++i)
        {
            const double sample = samples[i];
            sumSquares += sample * sample;
        }

        // RMS = Root Mean Square
        const float rms = static_cast<float>(
            std::sqrt(sumSquares / frameCount)
            );

        // 락 없이 가장 최근 음량만 전달
        self->latestRms.store(
            rms,
            std::memory_order_relaxed
        );
    }
};

MicrophoneInput::MicrophoneInput()
    : impl_(std::make_unique<Impl>())
{
}

MicrophoneInput::~MicrophoneInput()
{
    stop();
}

bool MicrophoneInput::start()
{
    if (impl_->running)
        return true;

    ma_device_config config =
        ma_device_config_init(
            ma_device_type_capture
        );

    // 32비트 부동소수점 모노 입력
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate = 48000;

    config.dataCallback = Impl::dataCallback;
    config.pUserData = impl_.get();

    impl_->latestRms.store(
        0.0f,
        std::memory_order_relaxed
    );

    // nullptr = 운영체제 기본 오디오 장치
    if (ma_device_init(
        nullptr,
        &config,
        &impl_->device
    ) != MA_SUCCESS)
    {
        return false;
    }

    if (ma_device_start(
        &impl_->device
    ) != MA_SUCCESS)
    {
        ma_device_uninit(&impl_->device);
        return false;
    }

    impl_->running = true;
    return true;
}

void MicrophoneInput::stop()
{
    if (!impl_->running)
        return;

    // 오디오 콜백이 종료된 뒤 장치 해제
    ma_device_uninit(&impl_->device);

    impl_->running = false;

    impl_->latestRms.store(
        0.0f,
        std::memory_order_relaxed
    );
}

float MicrophoneInput::getRms() const
{
    if (!impl_->running)
        return 0.0f;

    return impl_->latestRms.load(
        std::memory_order_relaxed
    );
}

float MicrophoneInput::getMouthWeight() const
{
    const float rms = getRms();

    // 이 값보다 작은 소리는 무시
    constexpr float NoiseGate = 0.008f;

    // 이 정도 음량이면 입을 완전히 벌림
    constexpr float FullOpenRms = 0.065f;

    float weight =
        (rms - NoiseGate) /
        (FullOpenRms - NoiseGate);

    weight = std::clamp(
        weight,
        0.0f,
        1.0f
    );

    // 작은 음량에도 적당히 반응하도록 보정
    return std::sqrt(weight);
}
