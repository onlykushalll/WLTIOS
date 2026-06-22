/* Stubs for WLT-Sandbox — only confirmed functions */
#include "../../src/userspace/wlt-sandbox/include/sandbox.h"
#include "../../src/userspace/wlt-sandbox/include/manifest.h"
const char *wlt_sandbox_error_str(wlt_sandbox_error_t e) {
    switch(e) { case WLT_SANDBOX_OK: return "OK"; case WLT_SANDBOX_ERR_INVALID_ARG: return "Invalid argument"; default: return "Error"; } }
void wlt_sandbox_shutdown(wlt_SandboxManager *m) { (void)m; }
