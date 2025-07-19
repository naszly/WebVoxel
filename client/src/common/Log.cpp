#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "spdlog/fmt/bundled/format.h"

template<typename Mutex>
class emscripten_sink : public spdlog::sinks::base_sink<Mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        switch (msg.level) {
            case spdlog::level::warn:
                EM_ASM_({
                    console.warning(UTF8ToString($0));
                }, fmt::to_string(formatted).c_str());
                break;
            case spdlog::level::err:
            case spdlog::level::critical:
                EM_ASM_({
                    console.error(UTF8ToString($0));
                }, fmt::to_string(formatted).c_str());
                break;
            default:
                EM_ASM_({
                    console.log(UTF8ToString($0));
                }, fmt::to_string(formatted).c_str());
        }
    }

    void flush_() override {
        // No-op for this sink
    }
};

using emscripten_sink_mt = emscripten_sink<std::mutex>;
using emscripten_sink_st = emscripten_sink<spdlog::details::null_mutex>;

#endif


static std::shared_ptr<spdlog::logger> createLogger(const char* name) {
#ifdef __EMSCRIPTEN__
    auto sink = std::make_shared<emscripten_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>(name, sink);
#else
    auto logger = spdlog::stdout_color_mt(name);
#endif
    logger->set_pattern("[%H:%M:%S] [t %t] [%^%n %L%$]: %v");
    logger->set_level(LOG_LEVEL);
    assert(logger);
    return logger;
}

namespace Impl {
    static std::shared_ptr<spdlog::logger> loggers[] = {
        createLogger("CORE"),
        createLogger("WEBGPU"),
        createLogger("APP")
    };

    std::shared_ptr<spdlog::logger> getLogger(size_t id) {
        assert(id < sizeof(loggers) / sizeof(loggers[0]));
        assert(loggers[id]);
        return loggers[id];
    }
}
