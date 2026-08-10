#ifndef GDSHADER_LOGGER_H
#define GDSHADER_LOGGER_H

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <vector>
#include <iostream>
#include <string>
#include <cstdlib>

// Platform-specific debug break for assertions
#if defined(_MSC_VER)
    #define GDSHADER_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
    #define GDSHADER_DEBUG_BREAK() __builtin_trap()
#else
    #define GDSHADER_DEBUG_BREAK() std::abort()
#endif

namespace gdshader_lsp 
{

/**
 * @brief The log level is defined by the compiler command! In release, we log only info and higher while in debug we compile everything down to trace log level.
 */
class Logger 
{

    public:
        
        static void init(const std::string& log_path = "gdshader_lsp_log.txt") 
        {
            try {
                std::vector<spdlog::sink_ptr> sinks;

                // Console sink (stderr)
                auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
                console_sink->set_level(spdlog::level::debug);
                console_sink->set_pattern("[%H:%M:%S.%e] [T:%t] [%^%l%$] [%s:%#] %v");
                sinks.push_back(console_sink);

                // Check if file logging is explicitly disabled
                bool file_logging_disabled = log_path.empty() || log_path == "none" || log_path == "off";

                if (!file_logging_disabled) {
                    // Max size: 50MB, Max files: 3
                    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_path, 1024 * 1024 * 50, 3);
                    file_sink->set_level(spdlog::level::trace);
                    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [T:%t] [%l] [%!] [%s:%#] %v");
                    sinks.push_back(file_sink);
                }

                spdlog::init_thread_pool(8192, 1);
                auto logger = std::make_shared<spdlog::async_logger>(
                    "server_logger", sinks.begin(), sinks.end(), 
                    spdlog::thread_pool(), spdlog::async_overflow_policy::block
                );
                logger->enable_backtrace(32);

                spdlog::set_default_logger(logger);
                spdlog::set_level(spdlog::level::trace);
                
                spdlog::flush_on(spdlog::level::err); 
                SPDLOG_INFO("Logging system initialized. Async: Enabled. File log: {}", 
                            file_logging_disabled ? "Disabled" : log_path);

            } catch (const spdlog::spdlog_ex& ex) {
                std::cerr << "Log initialization failed: " << ex.what() << std::endl;
            }
        }

        static void shutdown()
        {
            spdlog::shutdown();
        };

};

// ============================================================================
// LOGGING & ASSERTION MACROS
// ============================================================================
// Note: We use the `do { ... } while(0)` idiom to make macros safe in single-line 
// if-statements without brackets. We also use `#condition` to stringify the code 
// that failed directly into the log message.

// --- Standard Conditional Logging ---

#define GDSHADER_WARN_IF(condition, format_str, ...) \
    do { \
        if (condition) { \
            SPDLOG_WARN("Triggered [{}]: " format_str, #condition __VA_OPT__(,) __VA_ARGS__); \
        } \
    } while(0)

#define GDSHADER_ERROR_IF(condition, format_str, ...) \
    do { \
        if (condition) { \
            SPDLOG_ERROR("Triggered [{}]: " format_str, #condition __VA_OPT__(,) __VA_ARGS__); \
        } \
    } while(0)

// --- Early Exit Macros (Guard Clauses) ---

#define GDSHADER_RETURN_IF(condition, format_str, ...) \
    do { \
        if (condition) { \
            SPDLOG_ERROR("Early exit [{}]: " format_str, #condition __VA_OPT__(,) __VA_ARGS__); \
            return; \
        } \
    } while(0)

#define GDSHADER_RETURN_VAL_IF(condition, ret_val, format_str, ...) \
    do { \
        if (condition) { \
            SPDLOG_ERROR("Early exit [{}]: " format_str, #condition __VA_OPT__(,) __VA_ARGS__); \
            return ret_val; \
        } \
    } while(0)

// --- Assertions ---

#define GDSHADER_FATAL(format_str, ...) \
    do { \
        SPDLOG_CRITICAL(format_str __VA_OPT__(,) __VA_ARGS__); \
        spdlog::dump_backtrace(); \
        spdlog::default_logger()->flush(); \
        GDSHADER_DEBUG_BREAK(); \
    } while(0)

#ifdef NDEBUG
    #define GDSHADER_ASSERT(condition, format_str, ...) do { (void)sizeof(condition); } while(0)
#else
    #define GDSHADER_ASSERT(condition, format_str, ...) \
        do { \
            if (!(condition)) { \
                SPDLOG_CRITICAL("Assertion Failed [{}] | " format_str, #condition __VA_OPT__(,) __VA_ARGS__); \
                spdlog::dump_backtrace(); \
                spdlog::default_logger()->flush(); \
                GDSHADER_DEBUG_BREAK(); \
            } \
        } while(0)
#endif

#define GDSHADER_ALWAYS_ASSERT(condition, format_str, ...) \
    do { \
        if (!(condition)) { \
            SPDLOG_CRITICAL("Fatal Assertion Failed [{}] | " format_str, #condition __VA_OPT__(,) __VA_ARGS__); \
            spdlog::dump_backtrace(); \
            spdlog::default_logger()->flush(); \
            GDSHADER_DEBUG_BREAK(); \
        } \
    } while(0)
}

#endif