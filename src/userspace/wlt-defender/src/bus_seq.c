/* bus_seq.c — thread-safe event-sequence allocator for the guardian bus.
 *
 * Extracted verbatim from guardian.c (was guardian.c:256-261) so that
 * mcp_server / kairos_cli (separate mains) can link g_bus_next_seq()
 * without pulling in the daemon's main(). This TU keeps its OWN mutex
 * (g_seq_lock) and counter (g_seq_counter); guardian.c's g_bus_lock
 * continues to guard only the subscriber list.
 *
 * Build note: the daemon picks this file up automatically via
 * $(wildcard $(SRC)/*.c); the mcp target links $(BUILD)/bus_seq.o
 * explicitly. See Makefile target "mcp".
 */
#include "guardian.h"
#include <stdint.h>
#include <pthread.h>

static uint64_t        g_seq_counter = 0;
static pthread_mutex_t g_seq_lock    = PTHREAD_MUTEX_INITIALIZER;

uint64_t g_bus_next_seq(void) {
    pthread_mutex_lock(&g_seq_lock);
    uint64_t s = ++g_seq_counter;
    pthread_mutex_unlock(&g_seq_lock);
    return s;
}
