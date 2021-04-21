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

#include "utils_time.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <inttypes.h>

#include "utils_logs.h"

/* **** Definitions **** */

#define GETTIME(CLOCKID, TS) \
    CHECK_DO(utils_clock_gettime(CLOCKID, &TS) == 0, \
            TS.tv_sec = 0; TS.tv_nsec = 0);

#define TIMESPEC2MSEC(RET_MSEC, TS) \
    RET_MSEC  = (uint64_t)TS.tv_sec * 1000;\
    RET_MSEC += (uint64_t)TS.tv_nsec / 1000000;

#define UTILS_GETTIME_GENERIC(CLOCKID, TRANSFORM_MACRO, LOGCTX) \
    LOG_CTX_INIT(LOGCTX);\
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 0};\
    uint64_t retval;\
    GETTIME(CLOCKID, ts)\
    TRANSFORM_MACRO(retval, ts)\
    return retval;

/* **** Implementations **** */

utils_clock_gettime_fxn utils_clock_gettime = clock_gettime;

uint64_t utils_gettime_monotcoarse_msecs(utils_logs_ctx_t *utils_logs_ctx)
{
    UTILS_GETTIME_GENERIC(CLOCK_MONOTONIC_COARSE, TIMESPEC2MSEC,
            utils_logs_ctx)
}

uint64_t utils_gettime_monot_msecs(utils_logs_ctx_t *utils_logs_ctx)
{
    UTILS_GETTIME_GENERIC(CLOCK_MONOTONIC, TIMESPEC2MSEC, utils_logs_ctx)
}

uint64_t utils_gettime_msecs(utils_logs_ctx_t *utils_logs_ctx)
{
    UTILS_GETTIME_GENERIC(CLOCK_REALTIME, TIMESPEC2MSEC, utils_logs_ctx)
}
