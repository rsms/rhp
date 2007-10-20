#ifndef MACROS_H
#define MACROS_H

// Helpers
#define log_error(fmt, ...) fprintf(stderr, "E " __FILE__ ":%d: " fmt "\n", __LINE__, ##__VA_ARGS__)
#ifdef DEBUG
#define log_debug(fmt, ...) fprintf(stderr, "D " __FILE__ ":%d: " fmt "\n", __LINE__, ##__VA_ARGS__)
#else
#define log_debug(fmt, ...)
#endif

#endif
