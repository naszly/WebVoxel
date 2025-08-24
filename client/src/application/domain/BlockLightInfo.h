#pragma once

#include <cassert>

struct BlockLightInfo {

    BlockLightInfo() : m_intensity(0) {}

    explicit BlockLightInfo(const float intensity) : m_intensity(static_cast<uint8_t>(intensity * 31)) {
        assert(m_intensity < 32);
    }

    [[nodiscard]] uint8_t getIntensity() const {
        return m_intensity;
    }

    void setIntensity(const uint8_t intensity) {
        m_intensity = intensity;
    }

private:
    uint8_t m_intensity;
};

