#include "cstr.h"

#define DEBUG 1
#include <stdio.h>

#include "macros.h"
#include <string.h>

/* $Id: cstr.c 7 2007-10-18 04:08:08Z rasmus $ */

/*
typedef struct {
	char* ptr;
  size_t initial_size;
	size_t size;
	size_t length;
} cstr;
*/

cstr cstr_new(size_t capacity) {
  cstr s;
  cstr_init(&s, capacity);
  return s;
}

void cstr_init (cstr *s, size_t capacity) {
  s->ptr = (char *)malloc(sizeof(char)*(capacity+1));
  s->ptr[0] = 0;
  s->initial_size = capacity;
  s->size = capacity;
  s->length = 0;
  log_debug("cstr_init(): s->length=%ld >= s->size=%ld", s->length, s->size);
}

void cstr_free(cstr *s) {
  if(s->ptr) {
    free(s->ptr);
  }
}

void cstr_reset(cstr *s) {
  s->length = 0;
  s->ptr[s->length] = 0;
}

int cstr_resize(cstr *s, const size_t increment) {
  size_t new_size;
  if(increment < s->initial_size) {
		new_size = s->size + s->initial_size + 1;
	} else {
	  new_size = s->size + increment + 1;
	}
  char *new = (char *)realloc(s->ptr, sizeof(char)*new_size);
  if(new != NULL) {
    s->ptr = new;
    s->size = new_size;
  } else {
    return 1;
  }
  return 0;
}

int cstr_append(cstr *s, const char *src, const size_t srclen) {
  if(s->size - s->length <= srclen) {
    if(!cstr_resize(s, srclen)) {
      return 1;
    }
  }
  memcpy(s->ptr + s->length, src, srclen);
  s->length += srclen;
  s->ptr[s->length] = 0;
  return 0;
}

int cstr_appendc(cstr *s, const char ch) {
  if(s->length >= s->size) {
    log_debug("cstr_appendc(): will resize. s->length=%ld >= s->size=%ld", s->length, s->size);
    if(cstr_resize(s, (size_t)1)) {
      return 1;
    }
  }
  log_debug("cstr_appendc(): will set");
  s->ptr[s->length++] = ch;
  s->ptr[s->length] = 0;
  return 0;
}

char cstr_popc(cstr *s) {
  if(s->length) {
    char ch = s->ptr[s->length--];
    s->ptr[s->length] = 0;
    return ch;
  }
  return (char)0;
}