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

#ifndef UTILS_FILES_H_
#define UTILS_FILES_H_

#ifdef __cplusplus
extern "C" {
#endif

// **** Definitions ****

// Forward declarations
typedef struct utils_logs_ctx_s utils_logs_ctx_t;

// **** Prototypes ****

/// Store given character string into file.
/// @param str2dump Pointer to the character string to store/dump.
/// @param filePath Name of the file, including the complete path, where to store the string.
/// @param doTruncateFile Set to non-zero to truncate the file before storing the string.
/// @param doCreatePath Set to non-zero to create the folders path if applicable.
/// @param mkdirMode If 'doCreatePath' is set to non-zero, folders will be created using this mode.
/// This argument specifies the mode for the new directories.
/// @param utils_logs_ctx Pointer to generic logger context structure.
void utils_files_dump2file(const char *str2dump, const char *filePath, int doTruncateFile, int doCreatePath,
        int mkdirMode, utils_logs_ctx_t *const utils_logs_ctx);

/// Create folders of given path recursively. Folders are created if applicable, using give mode 'mkdirMode'.
/// @param path System path to create recursively.
/// @param mkdirMode Specifies the mode for each new directory created. If set to negative value, default mode 0777
/// will be used. Otherwise, it is modified by the process's umask in the usual way: in the absence of a default ACL,
/// the mode of the created directory is (mkdirMode & ~umask & 0777).
/// @param utils_logs_ctx Pointer to generic logger context structure.
/// @return 0 if succeed; on failure, -1 is returned and errno is set accordingly.
int utils_files_mkpath(const char *path, int mkdirMode, utils_logs_ctx_t *const utils_logs_ctx);

void utils_rmpath(const char *path, utils_logs_ctx_t *const utils_logs_ctx);

/// Reads content from given file path and stores it in a heap-allocated character buffer.
/// The returned buffer should be freed by the calling application.
/// @param completeFilePath Name of the file, including the complete path, from which the content is read.
/// @param utils_logs_ctx Pointer to generic logger context structure.
/// @return Pointer to the heap-allocated character buffer where the content is stored. Returns NULL if any error
/// occurred.
char* utils_files_file2buf(const char *completeFilePath, utils_logs_ctx_t *const utils_logs_ctx);

#ifdef __cplusplus
} //extern "C"
#endif

#endif /* UTILS_FILES_H_ */
