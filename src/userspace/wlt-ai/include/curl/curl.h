/* curl/curl.h — shim that redirects to curl_compat.h
 *
 * vision.c and other files include <curl/curl.h> directly. This shim
 * lets them compile without libcurl-dev installed, using the no-op
 * stubs in curl_compat.h. The local LLM path uses wlt_http.c (raw
 * socket) and does not go through any curl call.
 */
#ifndef CURL_CURL_H_SHIM
#define CURL_CURL_H_SHIM
#include "curl_compat.h"
#endif
