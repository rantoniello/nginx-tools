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

///@file utils_logs.h
///@brief Logging wrapper/unifier utility (C language interface).

extern "C" {
#include "utils_logs.h"
}

#include <cstdlib>
#include <string>

/// Color codes for terminal
#define CNRM "\x1B[0m"
#define CRED "\033[1;31m" // Bold-red
#define CYEL "\x1B[33m"

/// Module instance context structure
typedef struct utils_logs_ctx_s
{
    void *opaque_logger_ctx_ptr;
    utils_logs_ext_trace_fxn_t utils_logs_ext_trace_fxn;
} utils_logs_ctx_t;


utils_logs_ctx_t* utils_logs_open(void *const opaque_logger_ctx_ptr,
        utils_logs_ext_trace_fxn_t utils_logs_ext_trace_fxn)
{
    utils_logs_ctx_t *utils_logs_ctx = NULL;

    // Check arguments
    // Parameter 'opaque_logger_ctx_ptr' is allowed to be NULL (use STDOUT).
    // Parameter 'utils_logs_ext_trace_fxn' is allowed to be NULL.
    //if()
    //    return NULL; // Reserved for future use... -nothing to check-

    // Allocate module instance context structure
    utils_logs_ctx = (utils_logs_ctx_t*)calloc(1, sizeof(utils_logs_ctx_t));
    if(utils_logs_ctx == NULL)
        return NULL;

    //Initialize context structure
    utils_logs_ctx->opaque_logger_ctx_ptr = opaque_logger_ctx_ptr;
    utils_logs_ctx->utils_logs_ext_trace_fxn = utils_logs_ext_trace_fxn;

    return utils_logs_ctx;
}


void utils_logs_close(utils_logs_ctx_t **ref_utils_logs_ctx)
{
    utils_logs_ctx_t *utils_logs_ctx;

    if(ref_utils_logs_ctx == NULL || (utils_logs_ctx = *ref_utils_logs_ctx) == NULL)
        return;

    free(utils_logs_ctx);
    *ref_utils_logs_ctx = NULL;
}


void utils_logs_trace(utils_logs_ctx_t *utils_logs_ctx, utils_logs_level_t utils_logs_level, const char *fileName,
        int lineNo, const char *funcName, const char *format, ...)
{
    void *opaque_logger_ctx_ptr;
    utils_logs_ext_trace_fxn_t utils_logs_ext_trace_fxn;
    va_list args;

    // Check arguments.
    // Parameter 'lineNo' may take any value.
    if(utils_logs_ctx == NULL || utils_logs_level >= UTILS_LOGS_LEVEL_ENUM_MAX || fileName == NULL ||
            funcName == NULL || format == NULL)
        return;

    va_start(args, format);

    opaque_logger_ctx_ptr = utils_logs_ctx->opaque_logger_ctx_ptr;
    utils_logs_ext_trace_fxn = utils_logs_ctx->utils_logs_ext_trace_fxn;

    if(opaque_logger_ctx_ptr == NULL && utils_logs_ext_trace_fxn == NULL)
    {
        std::string colcode;

        if(utils_logs_level == UTILS_LOGS_ERR)
            colcode = CRED;
        else if(utils_logs_level == UTILS_LOGS_WAR)
            colcode = CYEL;
        else
            colcode = CNRM;
        printf("%s%s-%d: ", colcode.c_str(), fileName, lineNo);
        vprintf(format, args);
        printf("%s", CNRM);
        fflush(stdout);
    }
    else if(opaque_logger_ctx_ptr != NULL && utils_logs_ext_trace_fxn != NULL)
    {
        utils_logs_ext_trace_fxn(opaque_logger_ctx_ptr, utils_logs_level, fileName, lineNo, funcName, format, args);
    }

    va_end(args);
    return;
}


void utils_logs_close_uptr(utils_logs_ctx_t *p)
{
    utils_logs_close(&p);
}


void* utils_logs_show_opaque_logger(utils_logs_ctx_t *utils_logs_ctx)
{
    return utils_logs_ctx->opaque_logger_ctx_ptr;
}
