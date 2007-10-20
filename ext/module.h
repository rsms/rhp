#include <ruby.h>
#include "cstr.h"
#define DEBUG 1
#include "macros.h"

typedef struct rhp_compiler {
    cstr *buf;
    VALUE out; // RString
} rhp_compiler_t;

// Utilities
const char* get_errno_msg ();

// Library init
void Init_rhp ();

