#include <stdio.h>
#include <unistd.h>
#include <time.h>
int main(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("=== WLTIOS System Status ===\n");
    printf("Time: %04d-%02d-%02d %02d:%02d:%02d\n", t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    printf("Memory: %ld MB\n", (pages * page_size) / (1024*1024));
    printf("=== End Status ===\n");
    return 0;
}
