#ifndef CSTR_H
#define CSTR_H

#define CSTR_VERSION "$Id: cstr.h 7 2007-10-18 04:08:08Z rasmus $"

#include <stdlib.h>

typedef struct {
	char* ptr;
  size_t initial_size;
	size_t size;
	size_t length;
} cstr;

cstr cstr_new (size_t capacity);
void cstr_init (cstr *s, size_t capacity);
void cstr_free (cstr *s);
void cstr_reset (cstr *s);
int cstr_resize (cstr *s, const size_t increment);
int cstr_append (cstr *s, const char *src, const size_t srclen);
int cstr_appendc (cstr *s, const char ch);
char cstr_popc (cstr *s);

#endif
