/* Stubs for WLT-Net */
#include "../../src/userspace/wlt-net/include/net.h"
const char *wlt_net_error_str(wlt_net_error_t e) {
    switch(e) { case WLT_NET_OK: return "OK"; case WLT_NET_ERR_INVALID_ARG: return "Invalid argument";
    case WLT_NET_ERR_IO: return "IO error"; default: return "Error"; } }
wlt_net_error_t wlt_net_config_defaults(wlt_net_state_t *s) { return s ? WLT_NET_OK : WLT_NET_ERR_INVALID_ARG; }
wlt_net_error_t wlt_net_shutdown(wlt_net_state_t *s) { return s ? WLT_NET_OK : WLT_NET_ERR_INVALID_ARG; }
wlt_net_error_t wlt_net_kill_switch_trigger(wlt_net_state_t *s) { return s ? WLT_NET_OK : WLT_NET_ERR_INVALID_ARG; }
wlt_net_error_t wlt_net_kill_switch_release(wlt_net_state_t *s) { return s ? WLT_NET_OK : WLT_NET_ERR_INVALID_ARG; }
wlt_net_error_t wlt_net_tor_check_status(wlt_net_state_t *s) { return s ? WLT_NET_OK : WLT_NET_ERR_INVALID_ARG; }
wlt_net_error_t wlt_net_tor_new_identity(wlt_net_state_t *s) { return s ? WLT_NET_OK : WLT_NET_ERR_INVALID_ARG; }
wlt_net_error_t wlt_net_run_loop(wlt_net_state_t *s) { return s ? WLT_NET_OK : WLT_NET_ERR_INVALID_ARG; }
