#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <strings.h>

#define UUID_LEN     37  // 36 + nul char
#define MAX_LOG_SIZE 119

enum log_level { DEBUG, INFORMATION, WARNING, ERROR };

int number_len(size_t);
int parse_int(const char *);
int generate_uuid(char *);
char *remove_occur(char *, char) ;
char *append_string(char *, char *, size_t);

/* Logging */
void sol_log_init(const char *);
void sol_log_close(void);
void sol_log(int, const char *, ...);

#define log(...) sol_log( __VA_ARGS__ )
#define sol_debug(...) log(DEBUG, __VA_ARGS__)
#define sol_warning(...) log(WARNING, __VA_ARGS__)
#define sol_error(...) log(ERROR, __VA_ARGS__)
#define sol_info(...) log(INFORMATION, __VA_ARGS__)

#define STREQ(s1, s2, len) strncasecmp(s1, s2, len) == 0 ? true : false

#endif