#include "sha256.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <openssl/sha.h>
#include <openssl/evp.h>
#endif

#ifdef _WIN32
bool sha256_file(const char *filepath, char *output) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    NTSTATUS status;
    DWORD cbData = 0, cbHash = 0, cbHashObject = 0;
    PBYTE pbHashObject = NULL;
    PBYTE pbHash = NULL;
    FILE *f = fopen(filepath, "rb");
    if (!f) return false;

    // Open algorithm provider
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status)) { fclose(f); return false; }

    // Get hash object size
    status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0);
    if (!BCRYPT_SUCCESS(status)) { BCryptCloseAlgorithmProvider(hAlg, 0); fclose(f); return false; }

    // Allocate hash object
    pbHashObject = (PBYTE)malloc(cbHashObject);
    if (!pbHashObject) { BCryptCloseAlgorithmProvider(hAlg, 0); fclose(f); return false; }

    // Create hash
    status = BCryptCreateHash(hAlg, &hHash, pbHashObject, cbHashObject, NULL, 0, 0);
    if (!BCRYPT_SUCCESS(status)) { free(pbHashObject); BCryptCloseAlgorithmProvider(hAlg, 0); fclose(f); return false; }

    // Get hash length
    status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0);
    if (!BCRYPT_SUCCESS(status)) { BCryptDestroyHash(hHash); free(pbHashObject); BCryptCloseAlgorithmProvider(hAlg, 0); fclose(f); return false; }

    pbHash = (PBYTE)malloc(cbHash);
    if (!pbHash) { BCryptDestroyHash(hHash); free(pbHashObject); BCryptCloseAlgorithmProvider(hAlg, 0); fclose(f); return false; }

    // Read file in chunks and hash
    BYTE buffer[8192];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        status = BCryptHashData(hHash, buffer, (ULONG)bytesRead, 0);
        if (!BCRYPT_SUCCESS(status)) break;
    }

    bool ok = false;
    if (BCRYPT_SUCCESS(status)) {
        status = BCryptFinishHash(hHash, pbHash, cbHash, 0);
        if (BCRYPT_SUCCESS(status)) {
            // Convert to hex
            for (DWORD i = 0; i < cbHash; i++) {
                sprintf(output + (i * 2), "%02x", pbHash[i]);
            }
            output[cbHash * 2] = '\0';
            ok = true;
        }
    }

    // Cleanup
    free(pbHash);
    BCryptDestroyHash(hHash);
    free(pbHashObject);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    fclose(f);
    return ok;
}

void sha256_string(const char *input, char *output) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    NTSTATUS status;
    DWORD cbData = 0, cbHash = 0, cbHashObject = 0;
    PBYTE pbHashObject = NULL;
    PBYTE pbHash = NULL;
    size_t len = strlen(input);

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status)) return;

    status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0);
    if (!BCRYPT_SUCCESS(status)) { BCryptCloseAlgorithmProvider(hAlg, 0); return; }

    pbHashObject = (PBYTE)malloc(cbHashObject);
    if (!pbHashObject) { BCryptCloseAlgorithmProvider(hAlg, 0); return; }

    status = BCryptCreateHash(hAlg, &hHash, pbHashObject, cbHashObject, NULL, 0, 0);
    if (!BCRYPT_SUCCESS(status)) { free(pbHashObject); BCryptCloseAlgorithmProvider(hAlg, 0); return; }

    status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0);
    if (!BCRYPT_SUCCESS(status)) { BCryptDestroyHash(hHash); free(pbHashObject); BCryptCloseAlgorithmProvider(hAlg, 0); return; }

    pbHash = (PBYTE)malloc(cbHash);
    if (!pbHash) { BCryptDestroyHash(hHash); free(pbHashObject); BCryptCloseAlgorithmProvider(hAlg, 0); return; }

    status = BCryptHashData(hHash, (PUCHAR)input, (ULONG)len, 0);
    if (BCRYPT_SUCCESS(status)) {
        status = BCryptFinishHash(hHash, pbHash, cbHash, 0);
        if (BCRYPT_SUCCESS(status)) {
            for (DWORD i = 0; i < cbHash; i++) {
                sprintf(output + (i * 2), "%02x", pbHash[i]);
            }
            output[cbHash * 2] = '\0';
        }
    }

    free(pbHash);
    BCryptDestroyHash(hHash);
    free(pbHashObject);
    BCryptCloseAlgorithmProvider(hAlg, 0);
}
#else
bool sha256_file(const char *filepath, char *output) {
    FILE *f = fopen(filepath, "rb");
    if (!f) return false;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { fclose(f); return false; }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        fclose(f);
        return false;
    }

    BYTE buffer[8192];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        if (EVP_DigestUpdate(ctx, buffer, bytesRead) != 1) break;
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    unsigned int hashLen = 0;
    bool ok = false;

    if (EVP_DigestFinal_ex(ctx, hash, &hashLen) == 1 && hashLen == SHA256_DIGEST_LENGTH) {
        for (unsigned int i = 0; i < hashLen; i++) {
            sprintf(output + (i * 2), "%02x", hash[i]);
        }
        output[hashLen * 2] = '\0';
        ok = true;
    }

    EVP_MD_CTX_free(ctx);
    fclose(f);
    return ok;
}

void sha256_string(const char *input, char *output) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return;

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return;
    }

    size_t len = strlen(input);
    if (EVP_DigestUpdate(ctx, input, len) != 1) {
        EVP_MD_CTX_free(ctx);
        return;
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    unsigned int hashLen = 0;

    if (EVP_DigestFinal_ex(ctx, hash, &hashLen) == 1 && hashLen == SHA256_DIGEST_LENGTH) {
        for (unsigned int i = 0; i < hashLen; i++) {
            sprintf(output + (i * 2), "%02x", hash[i]);
        }
        output[hashLen * 2] = '\0';
    }

    EVP_MD_CTX_free(ctx);
}
#endif