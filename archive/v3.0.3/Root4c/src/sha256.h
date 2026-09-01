#ifndef SHA256_H
#define SHA256_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Compute SHA256 hash of a file
 * @param filepath Path to file
 * @param output Buffer to store 64-char hex string (must be at least 65 bytes)
 * @return true on success, false on failure
 */
bool sha256_file(const char *filepath, char *output);

/**
 * Compute SHA256 hash of a string
 * @param input Input string
 * @param output Buffer to store 64-char hex string (must be at least 65 bytes)
 */
void sha256_string(const char *input, char *output);

#ifdef __cplusplus
}
#endif

#endif