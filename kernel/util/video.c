#include <heap.h>
#include <assert.h>
#include <string.h>
#include <irql.h>
#include <video.h>
#include <virtual.h>
#include <errno.h>
#include <log.h>

static struct video_driver video_driver = {
    .putchar = NULL,
    .puts = NULL
};

void DeferPuts(void* v) {
    video_driver.puts((char*) v);
}

void DeferPutchar(void* v) {
    video_driver.putchar((char) (size_t) v);
}

void DbgScreenPutchar(char c) {
    if (video_driver.putchar != NULL) {
        DeferUntilIrql(IRQL_STANDARD, DeferPutchar, (void*) (size_t) c);
    }    
}

void DbgScreenPuts(char* s) {
    if (video_driver.puts != NULL) {
        DeferUntilIrql(IRQL_STANDARD, DeferPuts, s);
    }
}

void InitVideoConsole(struct video_driver driver) {
    video_driver = driver;
}
