
#pragma once

#include <memory>

class MicrophoneInput
{
public:
    MicrophoneInput();
    ~MicrophoneInput();

    MicrophoneInput(const MicrophoneInput&) = delete;
    MicrophoneInput& operator=(const MicrophoneInput&) = delete;

    // 기본 마이크 캡처 시작
    bool start();

    // 마이크 캡처 완전히 종료
    void stop();

    // 마이크 음량을 0~1의 입 벌림 값으로 변환
    float getMouthWeight() const;

    // 디버깅용 실제 RMS 음량
    float getRms() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
