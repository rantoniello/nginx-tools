/*
 * Copyright 2021 Rafael Antoniello
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/// @file utils_logs.h
/// @brief Logging wrapper/unifier utility (C language interface).
///
/// <h3>Purpose:<h3><br>
/// The purpose of this module is to offer a unified interface to adapt to any specific logger.<br>
/// This module implements a generic wrapper to be able to integrate different loggers into any logger-agnostic
/// library. For example, let's suppose we develop a library with some utilities, and we want to call this library
/// from a third party software using its own logger but also we need to use it from our own developed applications
/// with our own logger class/module. In this setting, our utilities library should be somehow logger-agnostic to
/// be integrated properly, and this can be achieved by using this module.<br>
/// <h3>Usage:<h3>
/// For a simple stand-alone example of use, please refer to the source file 'example_utils_logs.cpp'.

#ifndef UTILS_UTILS_LOGS_H_
#define UTILS_UTILS_LOGS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h> //va_list
#include <string.h> //strrchr

// **** Definitions ****

#define UTILS_LOGS_TRACE_MAX_SIZE 4096

// Forward declarations
typedef struct utils_logs_ctx_s utils_logs_ctx_t;

/// Logging level enumeration type
typedef enum utils_logs_level_enum {
    UTILS_LOGS_DBG= 0,
    UTILS_LOGS_WAR,
    UTILS_LOGS_ERR,
    UTILS_LOGS_LEVEL_ENUM_MAX
} utils_logs_level_t;

/// Source code file-name without path
#define __FILENAME__ strrchr("/" __FILE__, '/') + 1

/// Externally defined logging callback type
typedef void (*utils_logs_ext_trace_fxn_t)(void *opaque_logger_ctx_ptr,
        utils_logs_level_t utils_logs_level, const char *fileName,
        int lineNo, const char *funcName, const char *format, va_list args);

// **** Prototypes ****

/// Creates an instance of this module.
/// This function will be called once, typically at the beginning of the application.
/// @param opaque_logger_ctx_ptr Opaque structure/class that will be made available as argument in the callback
/// function 'utils_logs_ext_trace_fxn'.
/// @param utils_logs_ext_trace_fxn Private logging function passed as a callback. Opaque structure/class
/// 'opaque_logger_ctx_ptr' will be available in this function as an argument/parameter
/// (see 'utils_logs_ext_trace_fxn_t')
/// @return Pointer to the new module instance context structure (handler) created.
utils_logs_ctx_t* utils_logs_open(void *const opaque_logger_ctx_ptr,
        utils_logs_ext_trace_fxn_t utils_logs_ext_trace_fxn);

/// Release an instance of this module, obtained by a previously call to 'utils_logs_open()'.
/// @param Reference to the pointer to the module instance context structure (handler) to be released.
void utils_logs_close(utils_logs_ctx_t **ref_utils_logs_ctx);

/// Tracing function.
/// @param utils_logs_ctx
/// @param utils_logs_level
/// @param fileName
/// @param lineNo
/// @param funcName
/// @param format
/// @param ...
void utils_logs_trace(utils_logs_ctx_t *utils_logs_ctx, utils_logs_level_t utils_logs_level, const char *fileName,
        int lineNo, const char *funcName, const char *format, ...);

/// Generic logger deleter for the smart pointers.
/// @param p Pointer to the generic logger context structure to be deleted
void utils_logs_close_uptr(utils_logs_ctx_t *p);

/// Get opaque logger reference used internally by given module instance.
/// @param utils_logs_ctx
void* utils_logs_show_opaque_logger(utils_logs_ctx_t *utils_logs_ctx);

// **** MACROS ****

/// This is an internal MACRO, used by the logger MACROS LOG[D/W/E...]() for tracing.
#define _LOG(LEVEL, FORMAT, ...) \
        utils_logs_trace(__utils_logs_ctx, LEVEL, __FILENAME__, __LINE__, \
                __FUNCTION__, FORMAT, ##__VA_ARGS__)

/// Logger initializer: this MACRO must be called in any function before using the LOG[D/W/E...]() MACROs. As can be
/// seen, It just declare and initializes a "transparent" local pointer to the logger context structure
/// ('__utils_logs_ctx'). This local pointer will be also used transparently by the other logger MACROs.
#define LOG_CTX_INIT(LOG_CTX) utils_logs_ctx_t *__utils_logs_ctx = LOG_CTX

/// Logger setter: this MACRO sets/modifies (but not declare) the value of the transparent pointer '__utils_logs_ctx'.
#define LOG_CTX_SET(LOG_CTX) __utils_logs_ctx = LOG_CTX

/// Logger getter: this MACRO gets the transparent pointer '__utils_logs_ctx'.
#define LOG_CTX_GET() __utils_logs_ctx

/// Logger Debug MACRO
#define LOGD(FORMAT, ...) _LOG(UTILS_LOGS_DBG, FORMAT, ##__VA_ARGS__)

/// Logger Warning MACRO
#define LOGW(FORMAT, ...) _LOG(UTILS_LOGS_WAR, FORMAT, ##__VA_ARGS__)

/// Logger Error MACRO
#define LOGE(FORMAT, ...) _LOG(UTILS_LOGS_ERR, FORMAT, ##__VA_ARGS__)

/// This is an internal MACRO used by the CHECK_DO() and ASSERT() MACROS.
#define UTILS_LOGS_CHECK_DO_(COND, ACTION, LOG) \
        if(!(COND)) {\
            LOG;\
            ACTION;\
        }

/// Simple ASSERT implementation: does not exit the program but just outputs an error trace.
#define CHECK(COND) \
        UTILS_LOGS_CHECK_DO_(COND, , LOGE("Assertion failed.\n"))

/// Simple checker for generically tracking check-points failures. If the checked condition fails, it performs the
/// action specified -if any-.
#define CHECK_DO(COND, ACTION) \
        UTILS_LOGS_CHECK_DO_(COND, ACTION, LOGE("Check point failed.\n"))

#ifdef __cplusplus
} //extern "C"
#endif

#endif // UTILS_UTILS_LOGS_H_
