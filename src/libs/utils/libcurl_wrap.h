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
 * @file libcurl_wrap.h
 * @brief Wrapper of libcurl to facilitate most common requests.
 */

#ifndef UTILS_LIBCURL_WRAP_H_
#define UTILS_LIBCURL_WRAP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

/* **** Definitions **** */

/* Forward declarations */
typedef struct utils_logs_ctx_s utils_logs_ctx_t;

/**
 * Supported methods enumerator.
 */
typedef enum method_enum {
    LIBCURL_WRAP_METHOD_GET= 0,
    LIBCURL_WRAP_METHOD_POST,
    LIBCURL_WRAP_METHOD_PUT,
    LIBCURL_WRAP_METHOD_MAX
} libcurl_wrap_method_t;

/**
 * HTTP-request context structure.
 */
typedef struct libcurl_wrap_req_ctx_s {
    /**
     * Method to request. Mandatory.
     */
    libcurl_wrap_method_t method;
    /**
     * Headers to apply.
     * Passed variable must be **NULL-terminated** array of character-strings.
     * This field is optional (can be set to NULL).
     */
    const char **headers;
    /**
     * Host to request. Mandatory (cannot be NULL).
     */
    const char *host;
    /**
     * Port to request. Mandatory (cannot be NULL).
     */
    const char *port;
    /**
     * Location (URI) of request if applicable.
     * This field is optional (can be set to NULL).
     */
    const char *location;
    /**
     * Query string of request if applicable.
     * This field is optional (can be set to NULL).
     */
    const char *qstring;
    /**
     * Request content body if applicable.
     * This field is optional (can be set to NULL).
     */
    const char *body;

    /* **** Optional features **** */
    /**
     * Request time-out [seconds].
     */
    long tout;
    /**
     * Set this flag to non-zero to enable internal libcurl verbose
     * (disabled by default).
     */
    volatile int flag_libcurl_verbose;
    // Reserved for future use: add new features here
} libcurl_wrap_req_ctx_t;

/**
 * Request statistics context structure.
 *
 * Time statistics details:
 * @code
 * perform-request
 *  |
 *  |--NAMELOOKUP
 *  |--|--time_connect_usecs == CURLINFO_CONNECT_TIME_T
 *  |--|--|--APPCONNECT(SSL case)
 *  |--|--|--|--PRETRANSFER
 *  |--|--|--|--|--time_first_byte_usecs == CURLINFO_STARTTRANSFER_TIME_T
 *  |--|--|--|--|--|--time_total_usecs == CURLINFO_TOTAL_TIME_T
 *  @endcode
 */
typedef struct libcurl_wrap_stats_ctx_s {
    /**
     * Total time in microseconds from the request start until the connection to the remote host (or proxy) was
     * completed. This includes name resolving and TCP connect.
     * @see curl's option CURLINFO_CONNECT_TIME_T.
     */
    uint64_t time_connect_usecs;
    /**
     * Time it took from the request start until the first byte is received
     * @see curl's option CURLINFO_STARTTRANSFER_TIME_T.
     */
    uint64_t time_first_byte_usecs;
    /**
     * Total time in microseconds for the previous transfer, including name resolving, TCP connect, etc.
     * @see curl's option CURLINFO_TOTAL_TIME_T.
     */
    uint64_t time_total_usecs;
    /**
     * This is the value read from the Content-Length: field. Stores -1 if the size isn't known.
     */
    int64_t download_size_bytes;
} libcurl_wrap_stats_ctx_t;

/* **** Prototypes **** */

/**
 * Globally initialize internal libcurl.
 * This is intended to be called once at the beginning of the application.
 * @return Return 0 on success, negative value if fails.
 */
int libcurl_wrap_init_global();

/**
 * Globally cleanup internal libcurl.
 * This is intended to be called once at the end of the application.
 */
void libcurl_wrap_deinit_global();

/**
 * Perform an HTTP request.
 * @param libcurl_wrap_req_ctx Request context structure.
 * See type 'libcurl_wrap_req_ctx_t' description for details.
 * @param utils_logs_ctx Externally defined logger. This parameter is not
 * mandatory, thus it can be left to NULL. In this case, no logs will be
 * traced.
 * @param ref_response_str Reference to the pointer to a character string in
 * which the response will be returned, if applicable.
 * @param ref_http_ret_code Pointer to a long value in which the HTTP status
 * code will be returned.
 * @param ref_headers_out_str Reference to the pointer to a character string
 * in which the output headers will be returned, if applicable.
 * @param stats_ctx Pointer to an statistic context structure, provided by the caller, to be updated and returned by
 * argument. This parameter is not mandatory, it can be left to NULL if no statistics are required.
 * @return Return 0 on success, non-zero error code otherwise. The libcurl error codes are used as defined at
 * 'CURLcode' enumerator (see header file "curl.h").
 */
int libcurl_wrap_cli_request(const libcurl_wrap_req_ctx_t *libcurl_wrap_req_ctx,
        utils_logs_ctx_t *const utils_logs_ctx, char **ref_response_str, long *ref_http_ret_code,
        char **ref_headers_out_str, libcurl_wrap_stats_ctx_t *const stats_ctx);

/**
 * Supported methods code to readable format lookup table
 */
extern const char* libcurl_wrap_method_lut[];

#ifdef __cplusplus
}
#endif

#endif /* UTILS_LIBCURL_WRAP_H_ */
