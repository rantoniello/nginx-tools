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
 * @file interr_usleep.c
 * @author Rafael Antoniello
 */

#include "interr_usleep.h"

#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "utils_logs.h"

/* **** Definitions **** */

/**
 * Interruptible usleep context structure.
 */
typedef struct interr_usleep_ctx_s {
    utils_logs_ctx_t *utils_logs_ctx;
    volatile int flag_must_release_logs_ctx;
    volatile int flag_exit;
    pthread_mutex_t interr_mutex;
    pthread_cond_t interr_signal;
} interr_usleep_ctx_t;

/**
 * MACRO used to unlock internal MUTEX:
 * sets exit flag and send signal to eventually unlock MUTEX
 */
#define UNLOCK() \
        interr_usleep_ctx->flag_exit= 1; \
        pthread_mutex_lock(&interr_usleep_ctx->interr_mutex); \
        pthread_cond_broadcast(&interr_usleep_ctx->interr_signal); \
        pthread_mutex_unlock(&interr_usleep_ctx->interr_mutex);

/* **** Prototypes **** */

/* **** Implementations **** */

interr_usleep_ctx_t* interr_usleep_open(utils_logs_ctx_t *const utils_logs_ctx)
{
    pthread_condattr_t condattr;
    int ret_code, end_code= -1;
    interr_usleep_ctx_t *interr_usleep_ctx= NULL;
    LOG_CTX_INIT(NULL);

    /* Check arguments */
    // Parameter 'utils_logs_ctx' is allowed to be NULL.

    /* Allocate context structure */
    interr_usleep_ctx= (interr_usleep_ctx_t*)calloc(1, sizeof(
            interr_usleep_ctx_t));
    CHECK_DO(interr_usleep_ctx!= NULL, goto end);

    /* **** Initialize context structure **** */

    if(utils_logs_ctx!= NULL) {
        interr_usleep_ctx->flag_must_release_logs_ctx= 0;
        interr_usleep_ctx->utils_logs_ctx= utils_logs_ctx;
    } else {
        interr_usleep_ctx->flag_must_release_logs_ctx= 1;
        interr_usleep_ctx->utils_logs_ctx= utils_logs_open(NULL, NULL);
    }
    CHECK_DO(interr_usleep_ctx->utils_logs_ctx!= NULL, goto end);
    LOG_CTX_SET(interr_usleep_ctx->utils_logs_ctx);

    interr_usleep_ctx->flag_exit= 0;

    ret_code= pthread_mutex_init(&interr_usleep_ctx->interr_mutex, NULL);
    CHECK_DO(ret_code== 0, goto end);

    pthread_condattr_init(&condattr);
    pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
    ret_code= pthread_cond_init(&interr_usleep_ctx->interr_signal,
            &condattr);
    CHECK_DO(ret_code== 0, goto end);

    end_code= 0;
end:
    if(end_code!= 0)
        interr_usleep_close(&interr_usleep_ctx);
    return interr_usleep_ctx;
}

void interr_usleep_close(interr_usleep_ctx_t **ref_interr_usleep_ctx)
{
    interr_usleep_ctx_t *interr_usleep_ctx;
    LOG_CTX_INIT(NULL);

    if(ref_interr_usleep_ctx== NULL ||
            (interr_usleep_ctx= *ref_interr_usleep_ctx)== NULL)
        return;

    LOG_CTX_SET(interr_usleep_ctx->utils_logs_ctx);

    /* Set exit flag and send signal to eventually unlock MUTEX */
    UNLOCK();

    /* Release MUTEX and conditional */
    CHECK(pthread_mutex_destroy(&interr_usleep_ctx->interr_mutex)== 0);
    CHECK(pthread_cond_destroy(&interr_usleep_ctx->interr_signal)== 0);

    /* Release logger if it was internally allocated */
    if(interr_usleep_ctx->flag_must_release_logs_ctx!= 0) {
        interr_usleep_ctx->flag_must_release_logs_ctx= 0;
        utils_logs_close(&interr_usleep_ctx->utils_logs_ctx);
    }

    free(interr_usleep_ctx);
    *ref_interr_usleep_ctx= NULL;
}

void interr_usleep_unblock(interr_usleep_ctx_t *interr_usleep_ctx)
{
    //LOG_CTX_INIT(NULL);

    /* Check arguments */
    if(interr_usleep_ctx== NULL)
        return;

    //LOG_CTX_SET(interr_usleep_ctx->utils_logs_ctx);

    /* Set exit flag and send signal to eventually unlock MUTEX */
    UNLOCK();
}

int interr_usleep(interr_usleep_ctx_t *interr_usleep_ctx, uint32_t usec)
{
    uint64_t curr_nsec;
    struct timespec monotime_curr, monotime_tout;
    int ret_code;
    LOG_CTX_INIT(NULL);

    /* Check arguments */
    CHECK_DO(interr_usleep_ctx!= NULL, return -1);

    LOG_CTX_SET(interr_usleep_ctx->utils_logs_ctx);

    /* Get current time */
    CHECK_DO(clock_gettime(CLOCK_MONOTONIC, &monotime_curr)== 0,
            return -1);
    curr_nsec= (uint64_t)monotime_curr.tv_sec*1000000000+
            (uint64_t)monotime_curr.tv_nsec;

    /* Compute time-out */
    curr_nsec+= ((uint64_t)usec)* 1000;
    monotime_tout.tv_sec= curr_nsec/ 1000000000;
    monotime_tout.tv_nsec= curr_nsec% 1000000000;

    /* While exit is not signaled, block for the given time */
    pthread_mutex_lock(&interr_usleep_ctx->interr_mutex);
    ret_code= 0;
    while(interr_usleep_ctx->flag_exit== 0 && ret_code!= ETIMEDOUT) {
        ret_code= pthread_cond_timedwait(
                &interr_usleep_ctx->interr_signal,
                &interr_usleep_ctx->interr_mutex, &monotime_tout);
    }
    pthread_mutex_unlock(&interr_usleep_ctx->interr_mutex);
    return (ret_code== ETIMEDOUT)? 0: EINTR;
}

void interr_usleep_close_uptr(interr_usleep_ctx_t *p)
{
    interr_usleep_close(&p);
}
