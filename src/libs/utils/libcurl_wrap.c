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

#include "libcurl_wrap.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <curl/curl.h>

#include "utils_logs.h"

/* **** Definitions **** */

/**
 * We set a maximum URL or location size for the sake of security.
 * This maximum should never be achieved; it is just a security extreme bound
 * and we can set it to be "incoherently" high.
 */
#define URL_MAX_SIZE (1024* 16)

/**
 * We set a maximum number of headers for the sake of security.
 * This maximum should never be achieved; it is just a security extreme bound
 * and we can set it to be "incoherently" high.
 */
#define HDRS_MAX_NUM 40

/**
 * We set a maximum header-string size for the sake of security.
 * This maximum should never be achieved; it is just a security extreme bound
 * and we can set it to be "incoherently" high.
 */
#define HDRS_MAX_SIZE 512

/**
 * Curl memory context structure used for reading.
 * This type will be used as the private data passed to the *read* callback
 * function used by our libcurl's handler implementation
 * (refer to function 'curl_read_body_callback()').
 */
typedef struct curl_read_mem_ctx_s {
    const char *p_read;
    size_t size_left;
    utils_logs_ctx_t *utils_logs_ctx;
} curl_read_mem_ctx_t;

/**
 * Curl memory context structure used for writing.
 * This type will be used as the private data passed to the *write* callback
 * function used by our libcurl's handler implementation
 * (refer to function 'curl_write_body_callback()').
 */
typedef struct curl_write_mem_ctx_s {
    char *data;
    size_t size;
    utils_logs_ctx_t *utils_logs_ctx;
} curl_write_mem_ctx_t;

/**
 * Using POST with HTTP 1.1 implies the use of a "Expect: 100-continue" header.
 * We can disable this header with CURLOPT_HTTPHEADER as usual.
 */
#define DISABLE_EXPECT

/* **** Prototypes **** */

static int libcurl_wrap_cli_request_post_options(CURL *curl, const char *body,
        curl_read_mem_ctx_t *const curl_read_mem_ctx,
        const char **headers, struct curl_slist **ref_hdr_list,
        utils_logs_ctx_t *const utils_logs_ctx);

static int libcurl_wrap_cli_request_put_options(CURL *curl, const char *body,
        curl_read_mem_ctx_t *const curl_read_mem_ctx,
        const char **headers, struct curl_slist **ref_hdr_list,
        utils_logs_ctx_t *const utils_logs_ctx);

static void set_headers(CURL *curl, const char **headers,
        struct curl_slist **ref_hdr_list,
        utils_logs_ctx_t *const utils_logs_ctx);
static int find_header(const char **headers, const char *hdr_needle,
        const char *val_needle, utils_logs_ctx_t *const utils_logs_ctx);

static size_t curl_read_body_callback(void *dest, size_t size, size_t nmemb,
        void *userp);
static size_t curl_write_body_callback(void *contents, size_t size,
        size_t nmemb, void *userp);

/* **** Implementations **** */

int libcurl_wrap_init_global()
{
    if(curl_global_init(CURL_GLOBAL_DEFAULT)!= CURLE_OK)
        return -1;
    return 0;
}

void libcurl_wrap_deinit_global()
{
    curl_global_cleanup();
}

int libcurl_wrap_cli_request(
        const libcurl_wrap_req_ctx_t *libcurl_wrap_req_ctx,
        utils_logs_ctx_t *const utils_logs_ctx, char **ref_response_str,
        long *ref_http_ret_code, char **ref_headers_out_str,
        libcurl_wrap_stats_ctx_t *const stats_ctx)
{
    CURLcode curl_code;
    libcurl_wrap_method_t method_code;
    const char *host, *port, *location, *qstring;
    int idx, ret_code, end_code= -1, flag_attach_query= 0;
    CURL *curl= NULL;
    size_t url_size= 0;
    char *url= NULL;
    struct curl_slist *hdr_list= NULL;
    struct curl_read_mem_ctx_s curl_read_mem_ctx= {0};
    struct curl_write_mem_ctx_s curl_write_mem_ctx= {0};
    struct curl_write_mem_ctx_s curl_write_mem_ctx_hdr= {0};
    long http_ret_code= 404; // Initialize to 'Not Found'
    LOG_CTX_INIT(utils_logs_ctx);

    /* Check arguments.
     * Parameter 'utils_logs_ctx' is allowed to be NULL.
     * Parameter 'ref_headers_out_str' is allowed to be NULL.
     * Parameter 'stats_ctx' is allowed to be NULL.
     */
    CHECK_DO(libcurl_wrap_req_ctx!= NULL, return -1);
    CHECK_DO(ref_response_str!= NULL, return -1);
    CHECK_DO(ref_http_ret_code!= NULL, return -1);

    /* Unless we succeed, we set error 404 by default */
    *ref_http_ret_code= http_ret_code;

    /* Get method */
    method_code= libcurl_wrap_req_ctx->method;
    CHECK_DO(method_code< LIBCURL_WRAP_METHOD_MAX, goto end);

    /* Get a curl handle */
    curl= curl_easy_init();
    CHECK_DO(curl!= NULL, goto end);

    /* Check 'host':'port''location'?'qstring' */
    host= libcurl_wrap_req_ctx->host;
    port= libcurl_wrap_req_ctx->port;
    CHECK_DO(host!= NULL && port!= NULL, goto end);
    location= libcurl_wrap_req_ctx->location; // Allowed to be NULL
    qstring= libcurl_wrap_req_ctx->qstring; // Allowed to be NULL

    /* Compute complete request URL size */
    url_size= strlen(host)+ 1/*":"*/+ strlen(port)+ 1/*NULL-char*/;
    if(location!= NULL)
        url_size+= strlen(location);
    flag_attach_query= method_code== LIBCURL_WRAP_METHOD_GET &&
            qstring!= NULL && strlen(qstring)> 0;
    if(flag_attach_query!= 0 && qstring!= NULL)
        url_size+= 1/*"?"*/+ strlen(qstring);
    CHECK_DO(url_size> 0 && url_size< URL_MAX_SIZE, goto end);

    /* Allocate and compose URL */
    url= (char*)calloc(1, url_size);
    CHECK_DO(url!= NULL, goto end);
    snprintf(url, url_size, "%s:%s%s%s%s", host, port,
            location!= NULL? location: "",
            flag_attach_query? "?": "", flag_attach_query? qstring: "");

    /* Set the URL that is about to receive the request */
    CHECK_DO(curl_easy_setopt(curl, CURLOPT_URL, url)== CURLE_OK, goto end);

    /* Treat POST options if applicable */
    if(method_code== LIBCURL_WRAP_METHOD_POST) {
        ret_code= libcurl_wrap_cli_request_post_options(curl,
                libcurl_wrap_req_ctx->body, &curl_read_mem_ctx,
                libcurl_wrap_req_ctx->headers, &hdr_list, LOG_CTX_GET());
        CHECK_DO(ret_code== 0, goto end);
    } else if(method_code== LIBCURL_WRAP_METHOD_PUT) {
        ret_code= libcurl_wrap_cli_request_put_options(curl,
                libcurl_wrap_req_ctx->body, &curl_read_mem_ctx,
                libcurl_wrap_req_ctx->headers, &hdr_list, LOG_CTX_GET());
        CHECK_DO(ret_code== 0, goto end);
    }

    /* Set headers if applicable */
    set_headers(curl, libcurl_wrap_req_ctx->headers, &hdr_list, LOG_CTX_GET());

    /* Send all data to this function */
    CHECK_DO(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
            curl_write_body_callback)== CURLE_OK, goto end);

    /* We pass our 'chunk' structure to the callback function
     * (it will grown as needed by reallocation).
     */
    curl_write_mem_ctx.data= (char*)calloc(1, 1);
    CHECK_DO(curl_write_mem_ctx.data!= NULL, goto end);
    curl_write_mem_ctx.size= 0; // no data at this point yet
    curl_write_mem_ctx.utils_logs_ctx= LOG_CTX_GET();
    CHECK_DO(curl_easy_setopt(curl, CURLOPT_WRITEDATA,
            (void*)&curl_write_mem_ctx)== CURLE_OK, goto end);

    /* Send all header-data to this function */
    CHECK_DO(curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
            curl_write_body_callback)== CURLE_OK, goto end);

    /* We pass our 'chunk' structure to the callback function
     * (it will grown as needed by reallocation).
     */
    curl_write_mem_ctx_hdr.data= (char*)calloc(1, 1);
    CHECK_DO(curl_write_mem_ctx_hdr.data!= NULL, goto end);
    curl_write_mem_ctx_hdr.size= 0; // no data at this point yet
    curl_write_mem_ctx_hdr.utils_logs_ctx= LOG_CTX_GET();
    CHECK_DO(curl_easy_setopt(curl, CURLOPT_HEADERDATA,
            (void*)&curl_write_mem_ctx_hdr)== CURLE_OK, goto end);

    /* Get verbose debug output if applicable */
    if(libcurl_wrap_req_ctx->flag_libcurl_verbose!= 0)
        CHECK_DO(curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L)== CURLE_OK,
                goto end);

    /* Some servers don't like requests that are made without a user-agent
     * field, so we provide one.
     */
    idx= find_header(libcurl_wrap_req_ctx->headers, "User-Agent", NULL,
            LOG_CTX_GET());
    if(idx>= 0) { // Header not found -> index== -1
        CHECK_DO(curl_easy_setopt(curl, CURLOPT_USERAGENT,
                libcurl_wrap_req_ctx->headers[idx])== CURLE_OK, goto end);
    }

    /* Set time-out*/
    CHECK_DO(curl_easy_setopt(curl, CURLOPT_TIMEOUT,
            libcurl_wrap_req_ctx->tout)== CURLE_OK, goto end);

    CHECK_DO(curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L)== CURLE_OK, goto end);

    /* Perform the request */
    LOGD("Requesting HTTP-%s to address %s\n", libcurl_wrap_method_lut[method_code], url);
    if((curl_code= curl_easy_perform(curl))!= CURLE_OK) {
        LOGE("curl_easy_perform() failed: %s; while requesting HTTP-%s to address %s\n", curl_easy_strerror(curl_code),
                libcurl_wrap_method_lut[method_code], url);
        end_code = curl_code;
        goto end;
    }

    /* Populate statistics context structure if it was required */
    if(stats_ctx != NULL)
    {
        curl_off_t connect = 0, start = 0, total = 0, download_size = 0;
        CHECK(curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME_T, &connect) == CURLE_OK);
        LOGD("Connection time: %" CURL_FORMAT_CURL_OFF_T ".%06ld\n", connect / 1000000, (long)(connect % 1000000));
        stats_ctx->time_connect_usecs = (uint64_t)connect;

        CHECK(curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME_T, &start) == CURLE_OK);
        LOGD("Time to first-byte: %" CURL_FORMAT_CURL_OFF_T ".%06ld", start / 1000000, (long)(start % 1000000));
        stats_ctx->time_first_byte_usecs = (uint64_t)start;

        CHECK(curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME_T, &total) == CURLE_OK);
        LOGD("Total request time: %" CURL_FORMAT_CURL_OFF_T ".%06ld", total / 1000000, (long)(total % 1000000));
        stats_ctx->time_total_usecs = (uint64_t)total;

        CHECK(curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &download_size) == CURLE_OK);
        LOGD("Download size: %" CURL_FORMAT_CURL_OFF_T "\n", download_size);
        stats_ctx->download_size_bytes = (int64_t)download_size;
    }

    /* Prepare response data if applicable */
    if(curl_write_mem_ctx.size> 0) {
        *ref_response_str= curl_write_mem_ctx.data;
        curl_write_mem_ctx.data= NULL; // Avoid double referencing
        curl_write_mem_ctx.size= 0;
    }

    /* Prepare response output headers if applicable */
    if(ref_headers_out_str != NULL && curl_write_mem_ctx_hdr.size> 0) {
        *ref_headers_out_str= curl_write_mem_ctx_hdr.data;
        curl_write_mem_ctx_hdr.data= NULL; // Avoid double referencing
        curl_write_mem_ctx_hdr.size= 0;
    }

    /* Prepare response code */
    CHECK_DO(curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_ret_code)==
            CURLE_OK, goto end);
    *ref_http_ret_code= http_ret_code;

    end_code= 0; // succeed
end:
    if(curl!= NULL) {
        curl_easy_cleanup(curl);
        curl= NULL;
    }
    if(url!= NULL) {
        free(url);
        url= NULL;
    }
    if(hdr_list!= NULL) {
        curl_slist_free_all(hdr_list);
        hdr_list= NULL;
    }
    if(curl_write_mem_ctx.data!= NULL) {
        free(curl_write_mem_ctx.data);
        curl_write_mem_ctx.data= NULL;
    }
    if(curl_write_mem_ctx_hdr.data!= NULL) {
        free(curl_write_mem_ctx_hdr.data);
        curl_write_mem_ctx_hdr.data= NULL;
    }
    return end_code;
}

static int libcurl_wrap_cli_request_post_options(CURL *curl, const char *body,
        curl_read_mem_ctx_t *const curl_read_mem_ctx,
        const char **headers, struct curl_slist **ref_hdr_list,
        utils_logs_ctx_t *const utils_logs_ctx)
{
    int idx;
    LOG_CTX_INIT(utils_logs_ctx);

    /* Check arguments */
    CHECK_DO(curl!= NULL, return -1);
    // Parameter 'body' is allowed to be NULL.
    CHECK_DO(curl_read_mem_ctx!= NULL, return -1);
    // Parameter 'headers' is allowed to be NULL.
    CHECK_DO(ref_hdr_list!= NULL, return -1); //*ref_hdr_list may be NULL
    // Parameter 'utils_logs_ctx' is allowed to be NULL.

    /* Specify we want to POST data if applicable */
    CHECK_DO(curl_easy_setopt(curl, CURLOPT_POST, 1L)== CURLE_OK, return -1);

    /* Configure body sending related function */
    if(body!= NULL) {
        /* Set our read function -used to SEND data- if applicable */
        CHECK_DO(curl_easy_setopt(curl, CURLOPT_READFUNCTION,
                curl_read_body_callback)== CURLE_OK, return -1);

        /* Context structure pointer to pass to our read function */
        curl_read_mem_ctx->p_read= body;
        curl_read_mem_ctx->size_left= strlen(body);
        curl_read_mem_ctx->utils_logs_ctx= LOG_CTX_GET();
        CHECK_DO(curl_easy_setopt(curl, CURLOPT_READDATA, curl_read_mem_ctx)==
                CURLE_OK, return -1);
    }

    /* If transfer is not chunked, set the expected POST size.
     * NOTE: To POST large amounts of data, consider
     * CURLOPT_POSTFIELDSIZE_LARGE.
     */
    idx= find_header(headers, "Transfer-Encoding", "chunked", LOG_CTX_GET());
    if(idx< 0) { // Header not found -> index== -1
        CHECK_DO(curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                (long)curl_read_mem_ctx->size_left)== CURLE_OK, return -1);
    }

#ifndef DISABLE_EXPECT
    /* A less good option would be to enforce HTTP 1.0, but that might
     * also have other implications.
     */
    *ref_hdr_list= curl_slist_append(*ref_hdr_list, "Expect: 100-continue");
#endif
    return 0;
}

static int libcurl_wrap_cli_request_put_options(CURL *curl, const char *body,
        curl_read_mem_ctx_t *const curl_read_mem_ctx,
        const char **headers, struct curl_slist **ref_hdr_list,
        utils_logs_ctx_t *const utils_logs_ctx)
{
    LOG_CTX_INIT(utils_logs_ctx);

    /* Check arguments */
    CHECK_DO(curl!= NULL, return -1);
    // Parameter 'body' is allowed to be NULL.
    CHECK_DO(curl_read_mem_ctx!= NULL, return -1);
    // Parameter 'headers' is allowed to be NULL.
    CHECK_DO(ref_hdr_list!= NULL, return -1); //*ref_hdr_list may be NULL
    // Parameter 'utils_logs_ctx' is allowed to be NULL.

    /* Specify we want to PUT data if applicable */
    CHECK_DO(curl_easy_setopt(curl, CURLOPT_PUT, 1L)== CURLE_OK, return -1);

    /* Configure body sending related function */
    if(body!= NULL) {
        //printf("body: '%s' (%d)\n", body, (int)strlen(body)); // commment-me

        /* enable uploading */
        CHECK_DO(curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L)== CURLE_OK,
                return -1);

        /* Set our read function -used to SEND data- if applicable */
        CHECK_DO(curl_easy_setopt(curl, CURLOPT_READFUNCTION,
                curl_read_body_callback)== CURLE_OK, return -1);

        /* Context structure pointer to pass to our read function */
        curl_read_mem_ctx->p_read= body;
        curl_read_mem_ctx->size_left= strlen(body);
        curl_read_mem_ctx->utils_logs_ctx= LOG_CTX_GET();
        CHECK_DO(curl_easy_setopt(curl, CURLOPT_READDATA, curl_read_mem_ctx)==
                CURLE_OK, return -1);

        /* Provide the size of the upload, we specicially typecast the value
         * to curl_off_t since we must be sure to use the correct data size.
         */
        CHECK_DO(curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)
                curl_read_mem_ctx->size_left)== CURLE_OK, return -1);
    }

    return 0;
}

static void set_headers(CURL *curl, const char **headers,
        struct curl_slist **ref_hdr_list,
        utils_logs_ctx_t *const utils_logs_ctx)
{
    LOG_CTX_INIT(utils_logs_ctx);

    /* Check arguments */
    CHECK_DO(curl!= NULL, return);
    // Parameter 'headers' is allowed to be NULL
    CHECK_DO(ref_hdr_list!= NULL, return); //*ref_hdr_list may be NULL
    // Parameter 'utils_logs_ctx' is allowed to be NULL.

    /* Set user headers if applicable */
    if(headers!= NULL) {
        int i;
        for(i= 0; i< HDRS_MAX_NUM; i++) {
            const char *hdr= headers[i];
            if(hdr== NULL)
                break; // 'headers' should be a NULL-terminated array
            /* Append header to libcurl's hedear linked list */
            *ref_hdr_list= curl_slist_append(*ref_hdr_list, hdr);
        }
    }

    /* Append headers to libcurl's linked list.
     * Note: As we've already checked that 'headers[0]!= NULL' we are sure
     * we should have at least one header in the list.
     */
    if(*ref_hdr_list!= NULL) {
        CURLcode curl_code= curl_easy_setopt(curl, CURLOPT_HTTPHEADER,
                *ref_hdr_list);
        if(curl_code!= CURLE_OK) {
            LOGE("Could not set header: %s\n", curl_easy_strerror(curl_code));
        }
    }
    return;
}

static int find_header(const char **headers, const char *hdr_needle,
        const char *val_needle, utils_logs_ctx_t *const utils_logs_ctx)
{
    register int i, flag_match_found= 0;
    LOG_CTX_INIT(utils_logs_ctx);

    /* Check arguments */
    // Parameter 'headers' is allowed to be NULL.
    if(headers== NULL || headers[0]== NULL)
        return -1; // Not found as we do not have headers passed
    CHECK_DO(hdr_needle!= NULL, return -1);
    // Parameter 'val_needle' is allowed to be NULL.
    // Parameter 'utils_logs_ctx' is allowed to be NULL.

    for(i= 0; i< HDRS_MAX_NUM; i++) {
        char *p;
        size_t hdr_tag_size= 0;
        const char *hdr= headers[i];
        if(hdr== NULL)
            break; // 'headers' should be a NULL-terminated array

        /* Get header tag size */
        p= (char*)strchr(hdr, ':');
        if(p== NULL || p <= hdr) {
            LOGE("Wrong header format passed: %s\n", hdr);
            continue;
        }
        hdr_tag_size= p- hdr;
        if(hdr_tag_size> HDRS_MAX_SIZE) {
            LOGE("Wrong header format passed, too long: %s\n", hdr);
            continue;
        }

        /* Compare with needle */
        if(strncasecmp(hdr_needle, hdr, hdr_tag_size)== 0) {
            /* Caller may be interested in matching also header value.
             * If not, we can consider we found a match.
             * NOTE: We do not check for correct syntax.
             */
            if(val_needle== NULL || strstr(p, val_needle)!= NULL) {
                // match header and do not need to match value (= NULL) or
                // match header and match value
                flag_match_found= 1;
                LOGD("header: %s (tag size= %d)\n", hdr,
                        hdr_tag_size); //comment-me
                break;
            }
        }
    }
    return flag_match_found!= 0? i: -1;
}

static size_t curl_read_body_callback(void *dest, size_t size, size_t nmemb,
        void *userp)
{
    size_t size_left;
    curl_read_mem_ctx_t *curl_read_mem_ctx= (curl_read_mem_ctx_t*)userp;
    size_t max_read_size= size* nmemb;
    LOG_CTX_INIT(NULL);

    /* Check arguments */
    CHECK_DO(curl_read_mem_ctx!= NULL, return 0);
    CHECK_DO(curl_read_mem_ctx->p_read!= NULL, return 0);
    CHECK_DO(dest!= NULL, return 0);

    // Member 'curl_read_mem_ctx->utils_logs_ctx' is allowed to be NULL.
    LOG_CTX_SET(curl_read_mem_ctx->utils_logs_ctx);

    /* Sanity check: 'size_left' and 'max_read_size' */
    if(max_read_size== 0 || (size_left= curl_read_mem_ctx->size_left)== 0)
        return 0;

    /* Copy as much as possible from the source to the destination */
    if(size_left> max_read_size)
        size_left= max_read_size;
    memcpy(dest, curl_read_mem_ctx->p_read, size_left);

    /* Update reading context structure members */
    curl_read_mem_ctx->p_read+= size_left;
    curl_read_mem_ctx->size_left-= size_left;

    return size_left;
}

/**
 * Memory write callback used by our lib-curl handler to get request body.
 * @param contents Pointer to a (partial) chunk of the body content
 * @param size [bytes] of the units in which the chunk is received
 * @param nmemb number of size-units received composing the chunk
 * @param userp private user data pointer
 * @return total number of bytes received and processed.
 */
static size_t curl_write_body_callback(void *contents, size_t size,
        size_t nmemb, void *userp)
{
    char *p_data;
    size_t realsize= size* nmemb;
    curl_write_mem_ctx_t *curl_write_mem_ctx= (curl_write_mem_ctx_t*)userp;
    LOG_CTX_INIT(NULL);

    /* Check arguments */
    CHECK_DO(curl_write_mem_ctx!= NULL, return 0);
    CHECK_DO(curl_write_mem_ctx->data!= NULL, return 0);
    CHECK_DO(contents!= NULL, return 0);

    // Member 'curl_write_mem_ctx->utils_logs_ctx' is allowed to be NULL.
    LOG_CTX_SET(curl_write_mem_ctx->utils_logs_ctx);

    /* Sanity check: 'size' and 'nmemb' */
    if(size== 0 || nmemb== 0)
        return 0;

    p_data= (char*)realloc(curl_write_mem_ctx->data, curl_write_mem_ctx->size+
            realsize+ 1);
    CHECK_DO(p_data!= NULL, return 0);
    curl_write_mem_ctx->data= p_data;

    memcpy(&curl_write_mem_ctx->data[curl_write_mem_ctx->size], contents,
            realsize);
    curl_write_mem_ctx->size+= realsize;
    curl_write_mem_ctx->data[curl_write_mem_ctx->size]= 0; //'NULL' character

    return realsize;
}

const char* libcurl_wrap_method_lut[] = {"GET", "POST", "PUT", "n/a"};
