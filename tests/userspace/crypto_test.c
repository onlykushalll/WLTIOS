#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/userspace/wlt-crypto/include/crypto.h"
#include "../../src/userspace/wlt-crypto/include/keyvault.h"
int main(void) {
    int pass=0, fail=0;
    printf("=== WLT-Crypto Test ===\n\n");
    { const char *s = wlt_crypto_error_str(WLT_CRYPTO_OK); if (s && strlen(s)>0) { printf("[PASS] Error str OK: %s\n", s); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { const char *s = wlt_crypto_error_str(WLT_CRYPTO_ERR_KEYVAULT); if (s && strlen(s)>0) { printf("[PASS] Error str KEYVAULT: %s\n", s); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { wlt_crypto_error_t err = wlt_crypto_init(); if (err == WLT_CRYPTO_OK) { printf("[PASS] Crypto init OK\n"); pass++; } else { printf("[FAIL] returned %d\n", err); fail++; } }
    { wlt_crypto_error_t err = wlt_keyvault_init(); if (err != WLT_CRYPTO_OK) { printf("[PASS] Keyvault init -> error (stub correct, code %d)\n", err); pass++; } else { printf("[FAIL] returned OK\n"); fail++; } }
    { wlt_crypto_error_t err = wlt_keyvault_ping(); if (err != WLT_CRYPTO_OK) { printf("[PASS] Keyvault ping -> error (stub correct)\n"); pass++; } else { printf("[FAIL]\n"); fail++; } }
    { wlt_crypto_error_t err = wlt_keyvault_delete_key("test"); if (err != WLT_CRYPTO_OK) { printf("[PASS] Keyvault delete -> error (stub correct)\n"); pass++; } else { printf("[FAIL]\n"); fail++; } }
    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
