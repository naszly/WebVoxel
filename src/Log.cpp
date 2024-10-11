#include "Log.h"

namespace impl {
    static std::shared_ptr<spdlog::logger> createLogger(const char* name) {
        auto logger = spdlog::stdout_color_mt(name);
        logger->set_pattern("[%H:%M:%S] [t %t] [%^%n %L%$]: %v");
        logger->set_level(LOG_LEVEL);
        assert(logger);
        return logger;
    }

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
