/* Stubs for WLT-Crypto + KeyVault */
#include "../../src/userspace/wlt-crypto/include/crypto.h"
#include "../../src/userspace/wlt-crypto/include/keyvault.h"
const char *wlt_crypto_error_str(wlt_crypto_error_t e) {
    switch(e) { case WLT_CRYPTO_OK: return "OK"; case WLT_CRYPTO_ERR_KEYVAULT: return "Key Vault PD unreachable";
    case WLT_CRYPTO_ERR_INVALID_ARG: return "Invalid argument"; default: return "Error"; } }
wlt_crypto_error_t wlt_crypto_init(void) { return WLT_CRYPTO_OK; }
wlt_crypto_error_t wlt_crypto_luks_close(const char *n) { return n ? WLT_CRYPTO_OK : WLT_CRYPTO_ERR_INVALID_ARG; }
wlt_crypto_error_t wlt_crypto_shred(const char *p, int n) { return p ? WLT_CRYPTO_OK : WLT_CRYPTO_ERR_INVALID_ARG; }
wlt_crypto_error_t wlt_keyvault_init(void) { return WLT_CRYPTO_ERR_KEYVAULT; /* stub: PD unreachable */ }
wlt_crypto_error_t wlt_keyvault_ping(void) { return WLT_CRYPTO_ERR_KEYVAULT; }
wlt_crypto_error_t wlt_keyvault_delete_key(const char *id) { return id ? WLT_CRYPTO_ERR_KEYVAULT : WLT_CRYPTO_ERR_INVALID_ARG; }
