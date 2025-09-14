#pragma once

#include <webgpu/webgpu.h>
#include <cstdint>
#include <memory>
#include <type_traits>

class UniformsBuffer {
public:
    UniformsBuffer(const WGPUDevice& device, uint64_t byteSize);
    ~UniformsBuffer();

    UniformsBuffer(const UniformsBuffer&) = delete;
    UniformsBuffer& operator=(const UniformsBuffer&) = delete;

    template<typename T>
    static std::unique_ptr<UniformsBuffer> make(const WGPUDevice& device) {
        static_assert(std::is_trivially_copyable_v<T>, "Uniform type must be trivially copyable");
        static_assert(sizeof(T) % 16 == 0, "Uniform buffer size must be a multiple of 16 bytes");
        return std::make_unique<UniformsBuffer>(device, sizeof(T));
    }

    [[nodiscard]] WGPUBuffer get() const { return m_buffer; }
    [[nodiscard]] uint64_t size() const { return m_size; }

    void write(const WGPUQueue& queue, const void* data, uint64_t byteSize) const;

    template<typename T>
    void write(const WGPUQueue& queue, const T& data) const {
        static_assert(std::is_trivially_copyable_v<T>, "Uniform type must be trivially copyable");
        this->write(queue, &data, sizeof(T));
    }

private:
    WGPUBuffer m_buffer{};
    uint64_t m_size{};
};
