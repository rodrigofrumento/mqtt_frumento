#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <uuid/uuid.h>
#include "util.h"
#include "config.h"


static FILE *fh = NULL;

void sol_log_init(const char *file) {
    assert(file);
    fh = fopen(file, "a+");
    if (!fh)
        printf("%lu * WARNING: Unable to open file %s\n",
               (unsigned long) time(NULL), file);
}

void sol_log_close(void) {
    if (fh) {
        fflush(fh);
        fclose(fh);
    }
}

void sol_log(int level, const char *fmt, ...) {
    assert(fmt);
    va_list ap;
    char msg[MAX_LOG_SIZE + 4];
    if (level < conf->loglevel)
        return;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    /* Truncate message too long and copy 3 bytes to make space for 3 dots */
    memcpy(msg + MAX_LOG_SIZE, "...", 3);
    msg[MAX_LOG_SIZE + 3] = '\0';

    // Distinguish message level prefix
    const char *mark = "#i*!";

    // Open two handler, one for standard output and a second for the
    // persistent log file
    FILE *fp = stdout;
    if (!fp)
        return;
    fprintf(fp, "%lu %c %s\n", (unsigned long) time(NULL), mark[level], msg);
    if (fh)
        fprintf(fh, "%lu %c %s\n", (unsigned long) time(NULL), mark[level], msg);
    fflush(fp);
    if (fh)
        fflush(fh);
}

/*
 * Return the 'length' of a positive number, as the number of chars it would
 * take in a string
 */
int number_len(size_t number) {
    int len = 1;
    while (number) {
        len++;
        number /= 10;
    }
    return len;
}

/* Parse the integer part of a string, by effectively iterate through it and
   converting the numbers found */
int parse_int(const char *string) {
    int n = 0;
    while (*string && isdigit(*string)) {
        n = (n * 10) + (*string - '0');
        string++;
    }
    return n;
}

char *remove_occur(char *str, char c) {
    char *p = str;
    char *pp = str;
    while (*p) {
        *pp = *p++;
        pp += (*pp != c);
    }
    *pp = '\0';
    return str;
}

/*
 * Append a string to another, the destination string must be NUL-terminated
 * and long enough to contain the resulting string, for the chunk part that
 * will be appended the function require the length, the resulting string will
 * be heap alloced and nul-terminated.
 */
char *append_string(char *src, char *chunk, size_t chunklen) {
    size_t srclen = strlen(src);
    char *ret = malloc(srclen + chunklen + 1);
    memcpy(ret, src, srclen);
    memcpy(ret + srclen, chunk, chunklen);
    ret[srclen + chunklen] = '\0';
    return ret;
}

int generate_uuid(char *uuid_placeholder) {

    /* Generate random uuid */
    uuid_t binuuid;
    uuid_generate_random(binuuid);
    uuid_unparse(binuuid, uuid_placeholder);

    return 0;
}