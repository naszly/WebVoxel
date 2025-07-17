#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#ifndef NDEBUG
#define LOG_LEVEL spdlog::level::trace
#else
#define LOG_LEVEL spdlog::level::warn
#endif

namespace impl {

    std::shared_ptr<spdlog::logger> getLogger(size_t id);

    template<size_t ID>
    class _Log {
    public:
        _Log() = delete;

        template<typename... Args>
        static void trace(spdlog::format_string_t<Args...> fmt, Args &&...args) {
            getLogger(ID)->log(spdlog::level::trace, fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void debug(spdlog::format_string_t<Args...> fmt, Args &&...args) {
            getLogger(ID)->log(spdlog::level::debug, fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void info(spdlog::format_string_t<Args...> fmt, Args &&...args) {
            getLogger(ID)->log(spdlog::level::info, fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void warning(spdlog::format_string_t<Args...> fmt, Args &&...args) {
            getLogger(ID)->log(spdlog::level::warn, fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void error(spdlog::format_string_t<Args...> fmt, Args &&...args) {
            getLogger(ID)->log(spdlog::level::err, fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void critical(spdlog::format_string_t<Args...> fmt, Args &&...args) {
            getLogger(ID)->log(spdlog::level::critical, fmt, std::forward<Args>(args)...);
        }
    };
}

using LogCore = impl::_Log<0>;
using LogWebGPU = impl::_Log<1>;
using LogApp = impl::_Log<2>;
