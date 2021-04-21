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

#include "utils_files.h"

#include <iostream>
#include <fstream>
#include <cerrno>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <ftw.h>
#include <libgen.h>

#include "utils_logs.h"


void utils_files_dump2file(const char *str2dump, const char *filePath, int doTruncateFile, int doCreatePath,
        int mkdirMode, utils_logs_ctx_t *const utils_logs_ctx)
{
    size_t filePathLen;
    LOG_CTX_INIT(utils_logs_ctx); // parameter 'utils_logs_ctx' allowed to be NULL

    // Check arguments
    CHECK_DO(str2dump != NULL && filePath != NULL && (filePathLen = strlen(filePath)) > 0, return);

    // Recursively create path if applicable
    if(doCreatePath)
    {
        char path[filePathLen + 1] = { 0 };
        memcpy(path, filePath, filePathLen);
        utils_files_mkpath(dirname((char*)&path), mkdirMode, utils_logs_ctx);
    }

    LOGD("Storing string into file '%s' (trace truncated if exceeds %d bytes): \n'%.*s'\n\n",
            filePath, UTILS_LOGS_TRACE_MAX_SIZE, UTILS_LOGS_TRACE_MAX_SIZE, str2dump);

    int fd = doTruncateFile ? open(filePath, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) :
            open(filePath, O_RDWR|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(fd < 0) {
        LOGE("Could not open file '%s' (errno: %d)\n", filePath, errno);
        return;
    }
    size_t str2dump_len = strlen(str2dump);
    if(write(fd, str2dump, str2dump_len) != (ssize_t)str2dump_len)
    {
        LOGE("Could not write to file '%s' (errno: %d)\n", filePath, errno);
    }
    CHECK(close(fd) == 0);
}


int utils_files_mkpath(const char *path, int mkdirMode, utils_logs_ctx_t *const utils_logs_ctx)
{
    LOG_CTX_INIT(utils_logs_ctx); // parameter 'utils_logs_ctx' allowed to be NULL

    // Check arguments
    CHECK_DO(path != NULL, return -1);

    size_t pathFolderIdx = 0;
    std::string pathStr = path;
    do
    {
        pathFolderIdx = pathStr.find_first_of("\\/", pathFolderIdx + 1);
        std::string pathFolder = pathStr.substr(0, pathFolderIdx);
        LOGD("Trying folder (create if it does not exist) '%s'\n", pathFolder.c_str());
        if(mkdir(pathFolder.c_str(), mkdirMode >= 0 ? (mode_t)mkdirMode : 0777) != 0)
        {
            if(errno != EEXIST)
            {
                LOGE("Could not create folder '%s' (errno=%d)\n", pathFolder.c_str(), errno);
                return -1;
            }
        }
    } while (pathFolderIdx != std::string::npos);

    return 0;
}


static int unlink_files(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf)
{
    if(sb->st_mode & S_IFREG)
    {
        unlink(fpath);
    }
    return 0; //tell nftw() to continue
}


static int rm_dirs(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf)
{
    if(sb->st_mode & S_IFDIR)
    {
        rmdir(fpath);
    }
    return 0; //tell nftw() to continue
}


void utils_rmpath(const char *path, utils_logs_ctx_t *const utils_logs_ctx)
{
    nftw(path, unlink_files, 4, 0);
    nftw(path, rm_dirs, 4, FTW_DEPTH);
}


char* utils_files_file2buf(const char *filePath, utils_logs_ctx_t *const utils_logs_ctx)
{
    LOG_CTX_INIT(utils_logs_ctx); // parameter 'utils_logs_ctx' allowed to be NULL

    // Check arguments
    CHECK_DO(filePath != NULL, return nullptr);

    char *buf = nullptr;
    std::ifstream fileIfStream(filePath, std::ios::in | std::ios::binary | std::ios::ate);
    if(fileIfStream.is_open())
    {
        size_t file_size = fileIfStream.tellg();
        buf = (char*)calloc(1, file_size + 1);
        if(buf != nullptr)
        {
            fileIfStream.seekg(0, std::ios::beg); // rewind, opened at pos. end
            fileIfStream.read(buf, file_size);
        }
        else
            LOGE("Could not allocate reading buffer of size %zu.\n", file_size);
    }
    else
        LOGW("Error while opening file '%s'.\n", filePath);

    return buf;
}

