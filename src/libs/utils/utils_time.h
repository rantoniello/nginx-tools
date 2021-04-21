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

#ifndef UTILS_TIME_H_
#define UTILS_TIME_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <inttypes.h>

/* **** Definitions **** */

/* Forward declarations */
typedef struct utils_logs_ctx_s utils_logs_ctx_t;

typedef int(*utils_clock_gettime_fxn)(clockid_t clockid, struct timespec *tp);

/* **** Prototypes *****/

/**
 * This function internally calls 'clock_gettime' with clock-id
 * CLOCK_MONOTONIC_COARSE but returning a 64-bit unsigned integer representing
 * the time in milliseconds.
 * @param utils_logs_ctx Pointer to the log module context structure.
 * @return A 64-bit unsigned integer representing the time in milliseconds.
 */
uint64_t utils_gettime_monotcoarse_msecs(utils_logs_ctx_t *utils_logs_ctx);

/**
 * This function internally calls 'clock_gettime' with clock-id
 * CLOCK_MONOTONIC but returning a 64-bit unsigned integer representing
 * the time in milliseconds.
 * @param utils_logs_ctx Pointer to the log module context structure.
 * @return A 64-bit unsigned integer representing the time in milliseconds.
 */
uint64_t utils_gettime_monot_msecs(utils_logs_ctx_t *utils_logs_ctx);

uint64_t utils_gettime_msecs(utils_logs_ctx_t *utils_logs_ctx);

extern utils_clock_gettime_fxn utils_clock_gettime;

#ifdef __cplusplus
} //extern "C"
#endif

#endif /* UTILS_TIME_H_ */
