#pragma once

#include <exception>
#include <string>
#include <utility>
#include <format>

#include "Log.h"

class Exception final : public std::exception {
public:
    explicit Exception(std::string message) noexcept : m_message(std::move(message)) {
        LogCore::critical(m_message);
    }

    template <typename... Args>
    explicit Exception(std::format_string<Args...> format, Args&&... args) noexcept
        : m_message(std::format(format, std::forward<Args>(args)...)) {
        LogCore::critical(m_message);
    }

    [[nodiscard]] const char* what() const noexcept override {
        return m_message.c_str();
    }

private:
    std::string m_message;
};
