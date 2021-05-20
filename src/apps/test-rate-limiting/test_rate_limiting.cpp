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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <ftw.h>
#include <termios.h>
#include <thread>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>
#include <json-c/json.h>
#include <utils/utils_logs.h>
#include <utils/interr_usleep.h>
#include <utils/libcurl_wrap.h>
#include <utils/utils_time.h>
#include <utils/utils_files.h>

/// Path where all temporary files created by this example will be stored
/// This path is completely removed when tests end
#define TEST_DIR PREFIX "/tmp/test_rate_limiting"

/// Path where persistent output is stored (not to be deleted)
#define OUTPUT_DIR PREFIX "/tmp"

/// MIME types file
#define MIME_TYPES_FILE PROJECT_DIR "/assets/nginx_mime.types"

///@{
/// Nginx reverse-proxy related definitions.
#define NGINX_HOST "127.0.0.1"
#define NGINX_PORT "8885"
#define NGINX_LOGLEVEL "error"
#define NGINX_CACHE_FOLDER TEST_DIR "/storage"
#define NGINX_BIN PREFIX "/sbin/nginx"
#define NGINX_CONFFILE TEST_DIR "/nginx.conf"
#define NGINX_PIDFILE TEST_DIR "/nginx.pid"
#define NGINX_STATSLOG TEST_DIR "/proxy_stats.log"
///@}

///@{
/// Origin server related definitions.
#define ORIGIN_HOST "127.0.0.1"
#define ORIGIN_PORT "8886"
#define ORIGIN_HDR1_MAXAGE "3"
#define ORIGIN_LOGLEVEL "error"
#define ORIGIN_CONFFILE TEST_DIR "/origin.conf"
#define ORIGIN_PIDFILE TEST_DIR "/origin.pid"
#define ORIGIN_STATSLOG TEST_DIR "/origin_stats.log"
///@}

///@{
/// Statistics related definitions.
#define STATS_PORT "8887"
#define CLIENT_STATSLOG TEST_DIR "/client_stats.log"
#define TIME_NORMFACTOR_MSECS 1000
///@}

///@{
/// Final-client related definitions.
#define CLIENT_HDRHOST1 "origin1.example.inet"
///@}

typedef struct nginx_wrapper_ctx_s nginx_wrapper_ctx_t;
typedef struct setting_ctx_s setting_ctx_t;

typedef void(*setting_fxn)(const setting_ctx_t*, utils_logs_ctx_t *const);

typedef struct setting_ctx_s {
    setting_fxn fxn;
    std::string title;
    std::string description;
    std::string rpszone_size;
    std::string rps_limit;
    std::string reqburst;
    std::string burstdelay;
} setting_ctx_t;

// **** Prototypes ****

static int select_stdin();
static void http_get_nginx(const char *uri, const char *query_str,
        const char* headers_array[], unsigned int parallel_cnt,
        utils_logs_ctx_t *const utils_logs_ctx);
static void nginx_wrapper_open(char *argv[]);
static void nginx_wrapper_close(const char *fullpath_pidfile,
        utils_logs_ctx_t *utils_logs_ctx);
static void main_proc_quit_signal_handler(int intId);
static void configure_proxy(std::string rpszone_size,
        std::string rps_limit, std::string reqburst, std::string burstdelay,
        utils_logs_ctx_t *const utils_logs_ctx);
static void configure_origin(utils_logs_ctx_t *const utils_logs_ctx);
static void plottingThr(const setting_ctx_t *setting_ctx,
        utils_logs_ctx_t *const utils_logs_ctx);
extern char **environ;

// **** Implementations ****

/// Interruptible u-sleep instantiation
std::unique_ptr<interr_usleep_ctx_t, void(*)(interr_usleep_ctx_t*)>
        interr_usleep_uptr(interr_usleep_open(NULL), interr_usleep_close_uptr);

/// Nice scheme of the system architecture played in this example.
static const char *example_nginx_integration_scheme = "\n\n"
        "  +------------+\n"
        "  |            |\n"
        "  |   Client   |\n"
        "  |            |\n"
        "  +------------+\n"
        "        |\n"
        "        v\n"
        "  +-----------+\n"
        "  |           |\n"
        "  |   Nginx   |\n"
        "  |           |\n"
        "  +-----------+\n"
        " " NGINX_HOST ":" NGINX_PORT "\n"
        "       |\n"
        "       v\n"
        "  +------------+\n"
        "  |            |\n"
        "  |   Origin   |\n"
        "  |            |\n"
        "  +------------+\n"
        " " ORIGIN_HOST ":" ORIGIN_PORT "\n"
        "\n\n";


/// If this flag is set the app. should exit ASAP.
static volatile int flag_exit = 0, flag_exit_plotting_thr = 0;

static std::vector<std::thread> clients_threads;
static std::vector<std::string> clients_stats;
static std::mutex clients_stats_mutex;
static volatile int burst_level = 0;
static volatile uint64_t t0_msecs = 0;

static setting_ctx_s settings[] = {
        {
                .fxn = [](const setting_ctx_t *setting_ctx,
                        utils_logs_ctx_t *const __utils_logs_ctx) -> void {

                    http_get_nginx("/test-path/myfile", "any", nullptr, 40,
                            LOG_CTX_GET());

                },
                .title = "setting-1",
                .description = ""
                        "Sequence: req-burst=40, wait end",
                .rpszone_size = "10m",
                .rps_limit = "10",
                .reqburst = "burst=20",
                .burstdelay = "delay=10"
        },
        {
                .fxn = [](const setting_ctx_t *setting_ctx,
                        utils_logs_ctx_t *const __utils_logs_ctx) -> void {

                    http_get_nginx("/test-path/slow-reply", "any", nullptr, 40,
                            LOG_CTX_GET());

                },
                .title = "setting-1-delayed1sec",
                .description = ""
                        "Sequence: req-burst=40, wait end",
                .rpszone_size = "10m",
                .rps_limit = "10",
                .reqburst = "burst=20",
                .burstdelay = "delay=10"
        },
        {
                .fxn = [](const setting_ctx_t *setting_ctx,
                        utils_logs_ctx_t *const __utils_logs_ctx) -> void {

                    http_get_nginx("/test-path/myfile", "any", nullptr, 40,
                            LOG_CTX_GET());
                    usleep(1200 * 1000);
                    http_get_nginx("/test-path/myfile", "any", nullptr, 20,
                            LOG_CTX_GET());

                },
                .title = "setting-2",
                .description = "Sequence: req-burst=40, wait 1.2, "
                        "req-burst=20, wait end",
                .rpszone_size = "10m",
                .rps_limit = "10",
                .reqburst = "burst=20",
                .burstdelay = "delay=10"
        },
        {
                .fxn = [](const setting_ctx_t *setting_ctx,
                        utils_logs_ctx_t *const __utils_logs_ctx) -> void {

                    http_get_nginx("/test-path/slow-reply", "any", nullptr, 40,
                            LOG_CTX_GET());
                    usleep(1200 * 1000);
                    http_get_nginx("/test-path/slow-reply", "any", nullptr, 20,
                            LOG_CTX_GET());

                },
                .title = "setting-2-delayed1sec",
                .description = "Sequence: req-burst=40, wait 1.2, "
                        "req-burst=20, wait end",
                .rpszone_size = "10m",
                .rps_limit = "10",
                .reqburst = "burst=20",
                .burstdelay = "delay=10"
        },
        {
                .fxn = [](const setting_ctx_t *setting_ctx,
                        utils_logs_ctx_t *const __utils_logs_ctx) -> void {

                    http_get_nginx("/test-path/myfile", "any", nullptr, 40,
                            LOG_CTX_GET());
                    usleep(700 * 1000);
                    http_get_nginx("/test-path/myfile", "any", nullptr, 20,
                            LOG_CTX_GET());

                },
                .title = "setting-3",
                .description = "Sequence: req-burst=40, wait 0.7, "
                        "req-burst=20, wait end",
                .rpszone_size = "10m",
                .rps_limit = "10",
                .reqburst = "burst=20",
                .burstdelay = "delay=10"
        },
        {
                .fxn = [](const setting_ctx_t *setting_ctx,
                        utils_logs_ctx_t *const __utils_logs_ctx) -> void {

                    http_get_nginx("/test-path/slow-reply", "any", nullptr, 40,
                            LOG_CTX_GET());
                    usleep(700 * 1000);
                    http_get_nginx("/test-path/slow-reply", "any", nullptr, 20,
                            LOG_CTX_GET());

                },
                .title = "setting-3-delayed1sec",
                .description = "Sequence: req-burst=40, wait 0.7, "
                        "req-burst=20, wait end",
                .rpszone_size = "10m",
                .rps_limit = "10",
                .reqburst = "burst=20",
                .burstdelay = "delay=10"
        },
        {
                .fxn = [](const setting_ctx_t *setting_ctx,
                        utils_logs_ctx_t *const __utils_logs_ctx) -> void {

                    for (int i = 0; i < 40 ; i++) {
                        http_get_nginx("/test-path/myfile", "any", nullptr, 1,
                                LOG_CTX_GET());
                        usleep(25 * 1000);
                    }

                },
                .title = "setting-4",
                .description = "Sequence: loop 40 iterations ( "
                        "req-burst=1, wait 0.025 ), wait end",
                .rpszone_size = "10m",
                .rps_limit = "10",
                .reqburst = "burst=20",
                .burstdelay = "delay=10"
        },
        {
                .fxn = [](const setting_ctx_t *setting_ctx,
                        utils_logs_ctx_t *const __utils_logs_ctx) -> void {

                    for (int i = 0; i < 40 ; i++) {
                        http_get_nginx("/test-path/myfile", "any", nullptr, 4,
                                LOG_CTX_GET());
                        usleep(100 * 1000);
                    }

                },
                .title = "setting-5",
                .description = "Sequence: loop 40 iterations ( "
                        "req-burst=4, wait 0.1 ), wait end",
                .rpszone_size = "10m",
                .rps_limit = "10",
                .reqburst = "burst=20",
                .burstdelay = "delay=10"
        },
        {
                .fxn = [](const setting_ctx_t *setting_ctx,
                        utils_logs_ctx_t *const __utils_logs_ctx) -> void {

                    http_get_nginx("/test-path/media.mp4", "t0=0&res=720x480",
                            nullptr, 50, LOG_CTX_GET());
                    usleep(1600 * 1000);
                    http_get_nginx("/test-path/media.mp4", "t0=0&res=720x480",
                            nullptr, 18, LOG_CTX_GET());
                    usleep(1000 * 1000);
                    http_get_nginx("/test-path/media.mp4", "t0=0&res=720x480",
                            nullptr, 5, LOG_CTX_GET());

                },
                .title = "setting-6",
                .description = "Sequence: req-burst=50, wait 1.6, "
                        "req-burst=18, wait 1.0, req-burst=5, wait end",
                .rpszone_size = "10m",
                .rps_limit = "10",
                .reqburst = "burst=20",
                .burstdelay = "delay=10"
        },
        {
                .fxn = [](const setting_ctx_t *setting_ctx,
                        utils_logs_ctx_t *const __utils_logs_ctx) -> void {

                    for (int i = 0; i < 5 ; i++) {
                        http_get_nginx("/test-path/media.mp4",
                                "t0=0&res=720x480", nullptr, 8, LOG_CTX_GET());
                        usleep(1000 * 1000);
                    }

                },
                .title = "setting-7",
                .description = "Sequence: loop 5 iterations ( "
                        "req-burst=8, wait 1.0 ), wait end",
                .rpszone_size = "10m",
                .rps_limit = "5",
                .reqburst = "burst=12",
                .burstdelay = "delay=8"
        },
        {
                .fxn = [](const setting_ctx_t *setting_ctx,
                        utils_logs_ctx_t *const __utils_logs_ctx) -> void {

                    for (int i = 0; i < 40 ; i++) {
                        http_get_nginx("/test-path/media.mp4",
                                "t0=0&res=720x480", nullptr, 1, LOG_CTX_GET());
                        usleep(125 * 1000);
                    }

                },
                .title = "setting-8",
                .description = "Sequence: loop 40 iterations ( "
                        "req-burst=1, wait 0.125 ), wait end",
                .rpszone_size = "10m",
                .rps_limit = "5",
                .reqburst = "burst=12",
                .burstdelay = "delay=8"
        },
        {
                .fxn = [](const setting_ctx_t *setting_ctx,
                        utils_logs_ctx_t *const __utils_logs_ctx) -> void {

                    http_get_nginx("/test-path/myfile", "any", nullptr, 100,
                                                LOG_CTX_GET());

                },
                .title = "setting-9",
                .description = "Sequence: req-burst=100, wait end",
                .rpszone_size = "10m",
                .rps_limit = "10",
                .reqburst = "burst=50",
                .burstdelay = "delay=25"
        },
        {
                .fxn = [](const setting_ctx_t *setting_ctx,
                        utils_logs_ctx_t *const __utils_logs_ctx) -> void {

                    http_get_nginx("/test-path/myfile", "any", nullptr, 50,
                                                LOG_CTX_GET());

                },
                .title = "setting-10",
                .description = "Sequence: req-burst=50, wait end",
                .rpszone_size = "10m",
                .rps_limit = "10",
                .reqburst = "burst=50",
                .burstdelay = "delay=25"
        },
        {
                .fxn = [](const setting_ctx_t *setting_ctx,
                        utils_logs_ctx_t *const __utils_logs_ctx) -> void {

                    http_get_nginx("/test-path/myfile", "any", nullptr, 50,
                                                LOG_CTX_GET());
                    usleep(1000 * 1000);
                    http_get_nginx("/test-path/myfile", "any", nullptr, 50,
                                                LOG_CTX_GET());

                },
                .title = "setting-11",
                .description = "Sequence: req-burst=50, wait 1.0, "
                        "req-burst=50, wait end",
                .rpszone_size = "10m",
                .rps_limit = "10",
                .reqburst = "burst=50",
                .burstdelay = "delay=25"
        },
        {.fxn = nullptr}
};

int main(int argc, char* argv[])
{
    sigset_t set;
    struct termios terminal_settings, old_terminal_settings;
    std::thread plottingThread;
    LOG_CTX_INIT(utils_logs_open(NULL, NULL));

    // Change the file-mode mask to be able to write to any files
    umask(0);

    // Set SIGNAL handlers for this example
    sigfillset(&set);
    sigdelset(&set, SIGINT);
    sigdelset(&set, SIGTERM);
    pthread_sigmask(SIG_SETMASK, &set, NULL);
    signal(SIGINT, main_proc_quit_signal_handler);
    signal(SIGTERM, main_proc_quit_signal_handler);

    // Set the terminal to raw mode to avoid pressing 'ENTER' all the time
    tcgetattr(fileno(stdin), &terminal_settings);
    tcgetattr(fileno(stdin), &old_terminal_settings);
    terminal_settings.c_lflag &= ~(ECHO|ICANON);
    terminal_settings.c_cc[VTIME] = 0;
    terminal_settings.c_cc[VMIN] = 0;
    tcsetattr(fileno(stdin), TCSANOW, &terminal_settings);

    printf("\nExample scheme (proxy and origin)\n");
    printf("%s", example_nginx_integration_scheme);
    printf("\nPress 'ENTER' key to continue\n");
    printf("\nPress 'CTRL^c' to exit example\n");
    select_stdin();

    // Set arguments used to fork-exec nginx.
    char *nginx_argv[2][4] = {
        {(char*)NGINX_BIN, (char*)"-c", (char*)ORIGIN_CONFFILE, (char*)NULL},
        {(char*)NGINX_BIN, (char*)"-c", (char*)NGINX_CONFFILE, (char*)NULL}
    };

    // Remove and restore Nginx's cache and log files
    utils_rmpath(TEST_DIR, LOG_CTX_GET());
    mkdir(TEST_DIR, 0777);
    mkdir(NGINX_CACHE_FOLDER, 0777);

    // Launch origin server
    printf("\nLaunching origin...");
    configure_origin(LOG_CTX_GET());
    nginx_wrapper_open(nginx_argv[0]);

    // Just wait an instant to make sure server thread is up...
    if(interr_usleep(interr_usleep_uptr.get(), 1 * 1000 * 1000) == EINTR)
        goto end;

    // Apply the different test-settings
    for (int i = 0; settings[i].fxn != nullptr; i++) {
        setting_ctx_t *setting_ctx = &settings[i];

        // Launch Nginx proxy
        configure_proxy(setting_ctx->rpszone_size, setting_ctx->rps_limit,
                setting_ctx->reqburst, setting_ctx->burstdelay, LOG_CTX_GET());
        nginx_wrapper_open(nginx_argv[1]);

        // Wait an instant to make sure server thread is up...
        if (interr_usleep(interr_usleep_uptr.get(), 1* 1000* 1000) == EINTR)
            goto end;

        // Launch plotting/sampling thread and apply setting function
        flag_exit_plotting_thr = 0;
        burst_level = 0;
        t0_msecs = utils_gettime_msecs(LOG_CTX_GET()); // test initial time
        plottingThread = std::thread(plottingThr, setting_ctx, LOG_CTX_GET());
        setting_ctx->fxn(setting_ctx, LOG_CTX_GET());

        // Join all client threads
        for (unsigned int cli = 0; cli < clients_threads.size(); cli++)
            clients_threads[cli].join();
        clients_threads.clear();

        // Wait for delayed requests to finalize (to be able to plot them)
        while (!flag_exit && burst_level > 0) {
            if (interr_usleep(interr_usleep_uptr.get(), 100 * 1000) == EINTR)
                goto end;
        }

        // Join plotter thread
        flag_exit_plotting_thr = 1;
        plottingThread.join();
        if (interr_usleep(interr_usleep_uptr.get(), 2 * 1000 * 1000) == EINTR)
            goto end;

        // Kill nginx-proxy
        nginx_wrapper_close(NGINX_PIDFILE, LOG_CTX_GET());

        // Remove some log files
        unlink(CLIENT_STATSLOG);
    }

    // Exit app
end:
    printf("\n\n=================== End of example ======================\n");
    flag_exit = 1;

    // Restore terminal
    tcsetattr(fileno(stdin), TCSANOW, &old_terminal_settings);

    // Kill nginx-origin
    nginx_wrapper_close(ORIGIN_PIDFILE, LOG_CTX_GET());

    // Remove example target directory
    utils_rmpath(TEST_DIR, LOG_CTX_GET());

    printf("Example finished.\n");
    utils_logs_close(&LOG_CTX_GET());
    return 0;
}

static int select_stdin()
{
   int ret_char;
   fd_set fdset;
   FD_ZERO(&fdset);
   FD_SET(fileno(stdin),&fdset);
   select(1, &fdset, NULL, NULL, NULL);
   ret_char = !flag_exit ? fgetc(stdin) : -1;
   return ret_char;
}

static void curl_req(const char *uri, const char *query_str,
        const char* headers_array[], utils_logs_ctx_t *const __utils_logs_ctx)
{
    long http_ret_code;
    char *response_str = nullptr;
    const libcurl_wrap_req_ctx_t libcurl_wrap_req_ctx = {
            .method = LIBCURL_WRAP_METHOD_GET, .headers = headers_array,
            .host = NGINX_HOST, .port = NGINX_PORT,
            .location = uri, .qstring = query_str,
            .body = nullptr, .tout = 5, .flag_libcurl_verbose = 0
    };
    libcurl_wrap_stats_ctx_t stats_ctx = {};

    if (libcurl_wrap_cli_request(&libcurl_wrap_req_ctx, nullptr,
            &response_str, &http_ret_code, nullptr, &stats_ctx) != 0)
        LOGE("Error while requesting GET to address %s:%s%s\n",
                libcurl_wrap_req_ctx.host, libcurl_wrap_req_ctx.port,
                libcurl_wrap_req_ctx.location);
    else {
        char line[256];

        uint64_t tcurr = utils_gettime_msecs(LOG_CTX_GET()) - t0_msecs;
        float t = (float)tcurr / TIME_NORMFACTOR_MSECS;
        uint64_t responseTimem_sec = stats_ctx.time_total_usecs / 1000;
        sprintf(line, "%.1f %lu\n", t, responseTimem_sec);

        std::lock_guard<std::mutex> lck(clients_stats_mutex);
        clients_stats.push_back(line);
    }

    if (response_str != nullptr)
        free(response_str);
}

static void http_get_nginx(const char *uri, const char *query_str,
        const char* headers_array[], unsigned int parallel_cnt,
        utils_logs_ctx_t *const utils_logs_ctx)
{
    LOG_CTX_INIT(utils_logs_ctx);

    LOGD("\nPerforming x%u GET request: '%s:%s%s?%s'\n", parallel_cnt,
            NGINX_HOST, NGINX_PORT, uri, query_str);

    for (unsigned int i = 0; i < parallel_cnt; i++)
        clients_threads.push_back(std::thread(curl_req, uri, query_str,
                headers_array, LOG_CTX_GET()));
}

static void main_proc_quit_signal_handler(int intId)
{
    flag_exit = 1;
    putc('e', stdin); fflush(stdin);
    printf("Signaling application to finalize...\n");
    interr_usleep_unblock(interr_usleep_uptr.get());
}

static void nginx_wrapper_open(char *argv[])
{
    printf("\nNginx process starting PID is %d.\nCommand: '%s %s %s'\n",
            (int)getpid(), argv[0], argv[1], argv[2]);

    pid_t cpid = fork();
    if(cpid < 0)
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    else if(cpid > 0)
    {
        // Parent code
        return;
    }

    // **** Child code (cpid== 0) ****

    if(setsid() < 0) {
        printf("\nCould not create a new SID for the process\n");
        exit(EXIT_FAILURE);
    }
    execvpe(argv[0], argv, environ);

    // The 'execvpe()' function return only if an error has occurred
    exit(EXIT_FAILURE);
}

static void nginx_wrapper_close(const char *fullpath_pidfile,
        utils_logs_ctx_t *utils_logs_ctx)
{
    pid_t cpid;
    int status, filedesc;
    char strpid[32] = {0};
    LOG_CTX_INIT(utils_logs_ctx);

    // Check arguments
    CHECK_DO(fullpath_pidfile != NULL, return);

    // Get process PID
    filedesc = open(fullpath_pidfile, O_RDONLY);
    CHECK_DO(filedesc >= 0, return);
    CHECK_DO(read(filedesc, &strpid, sizeof(strpid)) > 0, return);
    close(filedesc);
    cpid = strtol(strpid, NULL, 10);

    // Signal nginx to quit gracefully; Nginx can be signaled as follows:
    // - quit – Shut down gracefully;
    // - reload – Reload the configuration file;
    // - reopen – Reopen log files;
    // - stop – Shut down immediately (fast shutdown).
    printf("\nSignaling nginx to exit (sent to pid= %d)\n", cpid);
    CHECK(kill(cpid, SIGQUIT) == 0);

    // Wait nginx to finalize
    int tries = 0;
    const int maxTries = 10;
    do {
        if((waitpid(cpid, &status, WUNTRACED| WCONTINUED))== -1)
            perror("\nFailure while executing 'waitpid()'\n");

        if(WIFEXITED(status))
        {
            printf("\nnginx exited with status= %d\n", WEXITSTATUS(status));
        }
        else if(WIFSIGNALED(status))
        {
            printf("\nnginx killed by signal %d\n", WTERMSIG(status));
        }
        else
        {
            usleep(1 * 1000 * 1000);
        }
        tries++;

    } while(!WIFEXITED(status) && !WIFSIGNALED(status) && tries < maxTries);

    return;
}

static void configure_proxy(std::string rpszone_size,
        std::string rps_limit, std::string reqburst, std::string burstdelay,
        utils_logs_ctx_t *const __utils_logs_ctx)
{
    std::string nginx_conf = R"(
daemon off;
user nginx nginx;
worker_processes auto;
error_log /dev/stderr )" NGINX_LOGLEVEL R"(;
thread_pool tcdn_webcache_thread_pool threads=8;
events {
    worker_connections 1024;
}
worker_rlimit_nofile 30000;
pid )" NGINX_PIDFILE R"(;
http {
    include )" MIME_TYPES_FILE R"(;
    default_type application/octet-stream;

    upstream backend {
        server )" ORIGIN_HOST ":" ORIGIN_PORT R"(;
    }

    log_format stats-log '$msec, $status';
    access_log )" NGINX_STATSLOG R"( stats-log;

    vhost_traffic_status_zone;

    limit_req_zone $binary_remote_addr zone=mylimit:)" + rpszone_size +
        R"( rate=)" + rps_limit + R"(r/s;

    server {
        listen )" NGINX_HOST ":" NGINX_PORT R"(;
        server_name nginx-proxy;
        location /test-path {
            proxy_pass http://backend;
            limit_req zone=mylimit )" + reqburst + " " + burstdelay + R"(;
        }
    }
    server {
        listen )" NGINX_HOST ":" STATS_PORT R"(;
        server_name nginx-status;
        location /status {
            vhost_traffic_status_display;
        }
        location = /basic_status {
            stub_status;
        }
    }
}
    )";

    utils_files_dump2file(nginx_conf.c_str(), NGINX_CONFFILE, 1, 0,
            0, LOG_CTX_GET());
}

static void configure_origin(utils_logs_ctx_t *const __utils_logs_ctx)
{
    // Notes on configuration:
    // - Set to 'daemon off' to succeed doing 'waitpid()' when forking nginx
    std::string origin_conf = R"(
    daemon off;
    user nginx nginx;
    worker_processes 1;
    error_log /dev/stderr )" ORIGIN_LOGLEVEL R"(;
    events {
        worker_connections 512;
    }
    pid )" ORIGIN_PIDFILE R"(;
    http {
        include )" MIME_TYPES_FILE R"(;

        log_format stats-log '$msec, $status';
        access_log )" ORIGIN_STATSLOG R"( stats-log;

        # 'Origin-1' server
        server {
            listen )" ORIGIN_HOST ":" ORIGIN_PORT R"(;
            server_name  Origin_1;
            location ~ /(.*) {
                return 200 "Server 'Origin-1' received HTTP request";
            }
            location = /test-path/slow-reply {
                echo_sleep 1.0;
                echo "Server 'Origin-1' received HTTP request; response delayed";
            }
        }
    })";

    utils_files_dump2file(origin_conf.c_str(), ORIGIN_CONFFILE, 1, 0,
            0, LOG_CTX_GET());
}

typedef struct trace_stats_ctx_s {
    FILE *const statsfile;
    int tot_accepted_prev;
    int tot_req_prev;
    int tot_2xx_prev;
    int tot_5xx_prev;
} trace_stats_ctx_t;

static void trace_stats_irequests(const struct json_object * jobj,
        trace_stats_ctx_t *ctx, utils_logs_ctx_t *const __utils_logs_ctx)
{
    struct json_object *conn, *accepted;

    CHECK_DO(json_object_object_get_ex(jobj, "connections", &conn) != 0,
            return);
    CHECK_DO(json_object_object_get_ex(conn, "accepted", &accepted) != 0,
            return);
    int tot_accepted = json_object_get_int(accepted);

    fprintf(ctx->statsfile, " %d", tot_accepted - ctx->tot_accepted_prev - 1);

    ctx->tot_accepted_prev = tot_accepted;
}

static void trace_stats_responses(const struct json_object * jobj,
        trace_stats_ctx_t *ctx, utils_logs_ctx_t *const __utils_logs_ctx)
{
    struct json_object *servZone, *serv;
    struct json_object *jobj_level; //TRICK
    int tot_req = 0, tot_2xx = 0, tot_5xx = 0;
    int level = 0; //TRICK

    CHECK_DO(json_object_object_get_ex(jobj, "serverZones", &servZone) != 0,
            return);

    if (json_object_object_get_ex(servZone, "nginx-proxy", &serv) != 0) {
        struct json_object *requestCounter, *resp, *resp_2xx, *resp_5xx;

        CHECK_DO(json_object_object_get_ex(serv, "requestCounter",
                &requestCounter) != 0, return);
        tot_req = json_object_get_int(requestCounter);

        CHECK_DO(json_object_object_get_ex(serv, "responses", &resp) != 0,
                return);
        CHECK_DO(json_object_object_get_ex(resp, "2xx", &resp_2xx) != 0,
                return);
        tot_2xx = json_object_get_int(resp_2xx);
        CHECK_DO(json_object_object_get_ex(resp, "5xx", &resp_5xx) != 0,
                return);
        tot_5xx = json_object_get_int(resp_5xx);
    }

    CHECK_DO(json_object_object_get_ex(jobj, "nowMsec", &jobj_level) != 0,
            return);
    burst_level = level = json_object_get_int(jobj_level);

    fprintf(ctx->statsfile, " %d %d %d %d", tot_req - ctx->tot_req_prev,
            tot_2xx - ctx->tot_2xx_prev, tot_5xx- ctx->tot_5xx_prev, level);

    ctx->tot_req_prev = tot_req;
    ctx->tot_2xx_prev = tot_2xx;
    ctx->tot_5xx_prev = tot_5xx;
}

static void trace_stats(const char *stats, trace_stats_ctx_t *ctx,
        utils_logs_ctx_t *const __utils_logs_ctx)
{
    struct json_object *jobj;

    CHECK_DO((jobj = json_tokener_parse(stats)) != nullptr, return);

    trace_stats_irequests(jobj, ctx, LOG_CTX_GET());
    trace_stats_responses(jobj, ctx, LOG_CTX_GET());

    int loop_guard = 100, flag_obj_freed = 0;
    while (loop_guard > 0 && flag_obj_freed == 0) {
        flag_obj_freed = json_object_put(jobj);
        loop_guard--;
    }
    CHECK(flag_obj_freed == 1);
}

static void plottingThr(const setting_ctx_t *setting_ctx,
        utils_logs_ctx_t *const __utils_logs_ctx)
{
    const libcurl_wrap_req_ctx_t libcurl_wrap_req_ctx = {
            .method = LIBCURL_WRAP_METHOD_GET, .headers = nullptr,
            .host = NGINX_HOST, .port = STATS_PORT,
            .location = "/status/format/json", .qstring = nullptr,
            .body = nullptr, .tout = 5, .flag_libcurl_verbose = 0
    };

    FILE *statsfile = fopen(TEST_DIR "/stats.dat", "wb");
    CHECK_DO(statsfile != nullptr, return);

    trace_stats_ctx_s trace_stats_ctx = {
            .statsfile = statsfile,
            .tot_accepted_prev = 0,
            .tot_req_prev = 0,
            .tot_2xx_prev = 0,
            .tot_5xx_prev = 0
    };

    uint64_t tstart = utils_gettime_msecs(LOG_CTX_GET());

    while (!flag_exit && !flag_exit_plotting_thr) {
        long http_ret_code;
        char *response = nullptr;

        int ret_code = libcurl_wrap_cli_request(&libcurl_wrap_req_ctx,
                nullptr, &response, &http_ret_code, nullptr, nullptr);
        CHECK(ret_code == 0 && response != nullptr);

        // Trace time
        uint64_t tcurr = utils_gettime_msecs(LOG_CTX_GET());
        fprintf(statsfile, "%.1f", (float)(tcurr - tstart) / 1000);

        // Trace rest of stats
        trace_stats(response, &trace_stats_ctx, LOG_CTX_GET());
        fprintf(statsfile, "\n");
        fflush(statsfile);

        if (response != nullptr)
            free(response);

        usleep(100 * 1000); // 100 milliseconds sample period
    }

    // Print clients statistics log file
    FILE *clistatsfile = fopen(CLIENT_STATSLOG, "wb");
    CHECK(clistatsfile != nullptr);
    for (unsigned int i = 0; i < clients_stats.size(); i++)
        fprintf(clistatsfile, "%s", clients_stats[i].c_str());
    clients_stats.clear();
    fclose(clistatsfile);

    // Plot
    FILE *gnuplot = popen("gnuplot", "w");
    CHECK_DO(gnuplot != nullptr, return);

    //fprintf(gnuplot, "set terminal\n");
    fprintf(gnuplot, "set term svg enhanced background rgb 'white' "
            "size 3440,1440\n");
    std::string plotpath = std::string(OUTPUT_DIR) + "/" + setting_ctx->title +
            "_plot.svg";
    fprintf(gnuplot, "set output '%s'\n", plotpath.c_str());
    std::string plottitle = (std::string)"Plot tag: " + setting_ctx->title + "\\n";//"\"this is a\\n two line title\"";
    plottitle += "Parameters: " + setting_ctx->rps_limit + "r/s; " +
            setting_ctx->reqburst + "; " + setting_ctx->burstdelay + "\\n";
    plottitle += setting_ctx->description + "\\n";
    fprintf(gnuplot, "set multiplot layout 2,1 title \"%s\" enhanced font 'Arial,18'\n",
            plottitle.c_str());
    fprintf(gnuplot, "set tmargin 1\n");
    fprintf(gnuplot, "set bmargin 3\n");
    fprintf(gnuplot, "set lmargin 10\n");
    //fprintf(gnuplot, "set format x \"%%2.1f\"\n");
    //fprintf(gnuplot, "set xrange [*:5.3]\n");

    // First plot
    fprintf(gnuplot, "set style data histograms\n");
    fprintf(gnuplot, "set style fill solid\n");
    fprintf(gnuplot, "set ytics font \",8\"\n");
    fprintf(gnuplot, "set grid y\n");
    fprintf(gnuplot, "set xlabel 'seconds'\n");
    fprintf(gnuplot, "set auto y\n");
    fprintf(gnuplot, "set ylabel 'count'\n");
    fprintf(gnuplot, "plot "
            "'" TEST_DIR "/stats.dat' using 2:xtic(1) "
                    "title 'proxy total accepted req.', "
            "'" TEST_DIR "/stats.dat' using 3:xtic(1) "
                    "title 'proxy total responded req.' linecolor rgb 'blue', "
            "'" TEST_DIR "/stats.dat' using 4:xtic(1) "
                    "title 'proxy-200 count' linecolor rgb 'green', "
            "'" TEST_DIR "/stats.dat' using 5:xtic(1) "
                    "title 'proxy-50X count' linecolor rgb 'red', "
            "'" TEST_DIR "/stats.dat' using 6:xtic(1) "
                    "title 'burst queue level' linecolor rgb 'black', "
            "'' u ($0-0.3):($2+0.3):(stringcolumn(2)) w labels notitle, "
            "'' u ($0-0.1):($3+0.3):(stringcolumn(3)) w labels notitle, "
            "'' u ($0+0.05):($4+0.3):(stringcolumn(4)) w labels notitle, "
            "'' u ($0+0.2):($5+0.3):(stringcolumn(5)) w labels notitle, "
            "'' u ($0+0.3):($6+0.3):(stringcolumn(6)) w labels notitle"
            "\n");
    // Get stats to be able to set the same x-axis maximum for stacked plot
    fprintf(gnuplot, "stats '" TEST_DIR "/stats.dat' using 1\n");
    //fprintf(gnuplot, "show variables all\n");
    fprintf(gnuplot, "set xtics 0.1\n");
    fprintf(gnuplot, "set xrange [STATS_min-0.1:STATS_max+0.1]\n");

    // Second plot
    fprintf(gnuplot, "set style data points\n");
    fprintf(gnuplot, "set pointsize 2\n");
    fprintf(gnuplot, "set ylabel 'milliseconds'\n");
    fprintf(gnuplot, "plot "
            "'" CLIENT_STATSLOG "' using 1:2 "
                    "title 'client total response time' linecolor rgb 'magenta'"
            "\n");
    //fprintf(gnuplot, "replot\n");
    fprintf(gnuplot, "unset multiplot\n");
    //fprintf(gnuplot, "unset output\n");

    fflush(gnuplot);
    fclose(gnuplot);
    fclose(statsfile);
}
