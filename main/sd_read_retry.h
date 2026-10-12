#ifndef SD_READ_RETRY_H
#define SD_READ_RETRY_H

#include <stddef.h>
#include <stdio.h>

/**
 * @file sd_read_retry.h
 * @brief fread() that tries a failed read of the storage once more (build option `fixes`).
 *
 * The SD card driver reads into a buffer that is not DMA-capable (a PSRAM buffer, an unaligned one)
 * through a 512-byte DMA-capable bounce buffer from the internal heap. When the internal heap has
 * no such block for a moment - WiFi, a TLS handshake and several web requests at once - the driver
 * answers `allocate_dma_buf: not enough mem, err=0x101`, FATFS turns that into a failed fread(),
 * and a thumbnail, an image or a file of the web UI is missing for that one request. The card
 * itself is fine and the same read works a moment later.
 *
 * fork_fread() is fread() for such a failure: it notes where the read started, and when it fails
 * with a stream error it logs the internal heap (free, the lowest it has been, the largest
 * DMA-capable block - the number the failure log lacks), waits a moment, seeks back and reads
 * again, once. Everything else - a short read at the end of the file, a second failure - is what
 * fread() returned.
 *
 * With the option on (`#if FORK_FIXES`), the files that read images (web server, image processor,
 * display manager) include this header, which turns their fread() into fork_fread().
 */

typedef struct {
    size_t (*read)(void *buf, size_t size, size_t count, void *ctx);
    int (*error)(void *ctx);           // non-zero: the stream is in an error state (ferror)
    void (*clear)(void *ctx);          // clears that state (clearerr)
    long (*tell)(void *ctx);           // the position, -1 if unknown (ftell)
    int (*seek)(void *ctx, long pos);  // 0 on success (fseek(SEEK_SET))
    void (*on_failure)(void *ctx, size_t wanted, size_t got);  // before the pause: log the state
    void (*pause)(void *ctx);                                  // a short wait before the second try
    void (*on_retry_result)(void *ctx, int recovered);         // after the second try
} sd_read_ops_t;

/**
 * Reads like fread(); a failed read (error state, fewer items than asked) is repeated once from
 * the position it started at. Returns what the last attempt returned. `*retried` (may be NULL) is
 * set to 1 when a second attempt was made.
 */
size_t sd_read_with_retry(const sd_read_ops_t *ops, void *ctx, void *buf, size_t size, size_t count,
                          int *retried);

// What the files that include this header see (they do so only with the option on):
size_t fork_fread(void *buf, size_t size, size_t count, FILE *fp);

#ifndef SD_READ_RETRY_NO_MACRO
#define fread(buf, size, count, fp) fork_fread((buf), (size), (count), (fp))
#endif

#endif
