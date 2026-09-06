/**
 * @file server.h
 * @brief Declares the internal Tapas language-server process entry point.
 * @details Runs one Language Server Protocol connection over caller-provided
 * streams for the executable and its in-repository tests.
 * @note This is executable integration code, not a supported public C API.
 */
#ifndef TAPAS_LSP_SERVER_H
#define TAPAS_LSP_SERVER_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Run a Language Server Protocol connection until an exit notification or
 * end-of-file. The streams are borrowed and must be opened in binary mode on
 * platforms that distinguish text from binary I/O. */
int tlsp_run(FILE *input, FILE *output);

#ifdef __cplusplus
}
#endif

#endif
