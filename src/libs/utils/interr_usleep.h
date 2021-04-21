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

/**
 * @file interr_usleep.h
 * @brief Interruptible usleep module.
 * This module ("interruptible usleep") implements a wrapper to the
 * 'usleep()' function to provide the possibility to interrupt the sleep state
 * by 'unlocking' the module instance handler.
 * @author Rafael Antoniello
 */

#ifndef UTILS_INTERR_USLEEP_H_
#define UTILS_INTERR_USLEEP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <inttypes.h>

typedef struct interr_usleep_ctx_s interr_usleep_ctx_t;
typedef struct utils_logs_ctx_s utils_logs_ctx_t;

/**
 * Initializes (open) interruptible module instance.
 * @param utils_logs_ctx Externally defined logger. This is an optional field
 * (can be set to NULL).
 * @return Pointer to the interruptible module instance context structure
 * ("handler") on success, NULL if fails.
 */
interr_usleep_ctx_t* interr_usleep_open(utils_logs_ctx_t *const utils_logs_ctx);

/**
 * Unlock 'usleep' operation -if applicable- and release interruptible module
 * instance context structure.
 * @param ref_interr_usleep_ctx Reference to the pointer to the interruptible
 * module instance context structure to be released, that was obtained in a
 * previous call to the 'interr_usleep_open()' function. Pointer is set to NULL
 * on return.
 */
void interr_usleep_close(interr_usleep_ctx_t **ref_interr_usleep_ctx);

/**
 * Unlock 'usleep' operation.
 * @param Pointer to the interruptible module instance context structure,
 * that was obtained in a previous call to the 'interr_usleep_open()' function.
 */
void interr_usleep_unblock(interr_usleep_ctx_t *interr_usleep_ctx);

/**
 * Perform the interruptible 'usleep' operation; that is:
 * suspends execution of the calling thread for (at least) usec microseconds.
 * The sleep may be lengthened slightly by any system activity or by the time
 * spent processing the call or by the granularity of system timers.
 * @param Pointer to the interruptible module instance context structure,
 * that was obtained in a previous call to the 'interr_usleep_open()' function.
 * @param usec Unsigned 32-bit integer specifying the amount of microseconds
 * to sleep.
 * @return Status code: <br>
 * - '0' in case of success;
 * - EINTR in the case of interruption caused by a parallel call to
 * 'interr_usleep_unblock()' function;
 * - '-1' or other non-zero status code in the case of an internal error.
 */
int interr_usleep(interr_usleep_ctx_t *interr_usleep_ctx, uint32_t usec);

/**
 * Deleter function for the interruptible sleep module. This function is used
 * essentially in C++ applications using this module for releasing smart
 * pointers.
 * @param p Pointer to the interruptible sleep context structure to be released
 */
void interr_usleep_close_uptr(interr_usleep_ctx_t *p);

#ifdef __cplusplus
} //extern "C"
#endif

#endif /* UTILS_INTERR_USLEEP_H_ */
