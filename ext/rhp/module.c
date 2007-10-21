#include "module.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "rubyio.h" // OpenFile etc

//#define DEBUG 1
#include "macros.h"

static VALUE RHP;
static VALUE RHP_Compiler;
static VALUE RHP_CompileError;

// ------------------------------------------
// String mixins

#define STR_ASSOC   FL_USER3
#define STR_NOCAPA  (ELTS_SHARED|STR_ASSOC)
#define RESIZE_CAPA(str,capacity) do {\
  REALLOC_N(RSTRING(str)->ptr, char, (capacity)+1);\
  if (!FL_TEST(str, STR_NOCAPA))\
    RSTRING(str)->aux.capa = (capacity);\
} while (0)

static VALUE RString_xml_safe(VALUE self) {
  VALUE str = self; // XXX op this
  VALUE dest; /* New ruby string */
  /*
  blen: length of new string buffer
  len: temporary holder of lengths
  extra: amount of extra space to allocate
  */
  long blen, len, extra = 30;
  /*
  buf: start of new string buffer
  bp: current position in new string buffer
  sp: start of old string buffer
  cp: start of copying position in old string buffer
  lp: current position in old string buffer
  send: end of old string buffer
  */
  char *buf, *bp, *sp, *cp, *lp, *send;
  
  ID to_s_id;
  to_s_id = rb_intern("to_s");
  str = rb_funcall(str, to_s_id, 0);
  str = StringValue(str);
  if (RSTRING(str)->len == 0) {
    return rb_str_buf_new(0);
  }
  
  if (RSTRING(str)->len < 6)
    extra = RSTRING(str)->len * 5; /* Don't allocate more space than escaped string */
                     /* could possibly take up */
  blen = RSTRING(str)->len + extra; /* add some extra space to account for escaped HTML */
  dest = rb_str_buf_new(blen); /* create new ruby string */
  sp = cp = lp = StringValuePtr(str); /* Initialize old string pointers */
  bp = buf = StringValuePtr(dest); /* Initialize new string pointers */
  send = (char *)((long)sp + RSTRING(str)->len); /* Get end of ruby string */
  
  rb_str_locktmp(dest);
  while (lp < send) {
    /* Scan characters until HTML character is found */
    if(!(*lp=='&'||*lp=='"'||*lp=='>'||*lp=='<')) {
      lp++; /* skip to next character in old string */
      continue;
    }
    
    /* Reallocate new string memory if new string won't be large enough*/
    len = (bp - buf) /* length of new string */
      + (lp - cp) /* length of added text  */
      + 6; /* Maximum amount of space that can be taken up with html replacement */
    if (blen < len) {
      blen = len + (extra = extra << 1); /* Add double the amount of extra space */
                         /* previously allocated to new required length */
      len = bp - buf; /* Record length of new string buffer currently used */
      RESIZE_CAPA(dest, blen); /* Give ruby string additional capacity */
      RSTRING(dest)->len = blen; /* Set new length of ruby string */
      buf = RSTRING(dest)->ptr; /* Set new start of new string buffer */
      bp = buf + len; /* Set new current position of new string buffer */
    }
    
    /* Copy previous non-HTML text from old string to new string */
    len = lp - cp;  /* length of previous non-HTML text */
    memcpy(bp, cp, len); /* copy non-HTML from old buffer to new buffer */
    bp += len; /* Update new string pointer by length copied */
    
    /* Copy HTML replacement text to new string if not currently at end of source */
    switch(*lp) {
      case '&': memcpy(bp, "&#38;", 5); bp+=5; break;
      case '"': memcpy(bp, "&#34;", 5); bp+=5; break;
      case '>': memcpy(bp, "&#60;", 5); bp+=5; break;
      case '<': memcpy(bp, "&#62;", 5); bp+=5; break;
    }
    cp = ++lp; /* Set new current and copying start point for old string */
  }
  if(cp != lp) {
    len = (bp - buf) + (lp - cp);
    if (blen < len) {
      blen = len;
      len = bp - buf; 
      RESIZE_CAPA(dest, blen); 
      RSTRING(dest)->len = blen; 
      buf = RSTRING(dest)->ptr; 
      bp = buf + len; 
    }
    len = lp - cp;
    memcpy(bp, cp, len);
    bp += len;
  }
  *bp = '\0';
  rb_str_unlocktmp(dest);
  RBASIC(dest)->klass = rb_obj_class(str);
  OBJ_INFECT(dest, str);
  RSTRING(dest)->len = bp - buf; /* Set correct ruby string length */
  
  /* Taint new string if old string tainted */
  if (OBJ_TAINTED(str)) 
    OBJ_TAINT(dest);
  /* Return new ruby string */
  return dest;
}

// ------------------------------------------
// Utilities

const char* get_errno_msg() {
  switch(errno) {
    case 0: return "No error";
    case EACCES: return "Another process has the file locked";
    case EBADF: return "stream is not a valid stream opened for reading";
    case EINTR: return "A signal interrupted the call";
    case EIO: return "An input error occurred";
    case EISDIR: return "The open object is a directory, not a file";
    case ENOMEM: return "Memory could not be allocated for internal buffers";
    case ENXIO: return "A device error occurred";
    case EOVERFLOW:  return "The file is a regular file and an attempt was made "
                            "to read at or beyond the offset maximum associated "
                            "with the corresponding stream";
    case EWOULDBLOCK:  return "The underlying file descriptor is a non-blocking "
                              "socket and no data is ready to be read";
  }
  return "Unknown";
}

// ------------------------------------------
// Compiler

#define CTX_TEXT 1
#define CTX_EVAL 2
#define CTX_COMMENT 4
#define CTX_PRINT 8
#define CTX_OUTPUT_STARTED 16

#ifdef DEBUG
#define log_parse(fmt, ...) fprintf(stdout, "%s %lu:%-2lu  " fmt "\n", filename, line, column, ##__VA_ARGS__)
#else
#define log_parse(fmt, ...)
#endif

void rhp_compiler_mark (rhp_compiler_t *s) {}
static void rhp_compiler_free (rhp_compiler_t *s) {
  log_debug("Enter rhp_compiler_free");
  cstr_free(s->buf);
  free(s->buf);
}

static VALUE RHP_Compiler_allocate (VALUE klass) {
  log_debug("Enter RHP_Compiler_allocate");
  VALUE obj;
  rhp_compiler_t *compiler;
  
  obj = Data_Make_Struct(klass, rhp_compiler_t, rhp_compiler_mark, rhp_compiler_free, compiler);
  compiler->buf = (cstr *)malloc(sizeof(cstr));
  cstr_init(compiler->buf, 1024);
  compiler->out = Qnil;
  
  log_debug("compiler->buf=%p : size=%ld length=%ld ptr=%p",
    compiler->buf, compiler->buf->size, compiler->buf->length, compiler->buf->ptr);
  
  return obj;
}

static int _compile_push(rhp_compiler_t *compiler, const int context, int output_started) {
  
  if(compiler->buf->length == 0) {
    return output_started;
  }
  
  if(!output_started && (context & CTX_PRINT) || (!(context & CTX_EVAL))) {
    compiler->out = rb_str_buf_cat(compiler->out, "send_headers!\n", 14);
    output_started = 1;
  }
  
  if(context & CTX_EVAL) {
    if(context & CTX_COMMENT) {
      log_debug("Push: comment: (%lu) '%s'", compiler->buf->length, compiler->buf->ptr);
      // discard
    }
    else if(context & CTX_PRINT) {
      log_debug("Push: print: (%lu) '%s'", compiler->buf->length, compiler->buf->ptr);
      cstr_appendc(compiler->buf, ')');
      cstr_appendc(compiler->buf, ')');
      cstr_appendc(compiler->buf, '\n');
      compiler->out = rb_str_buf_cat(compiler->out, "@out.write((", 12);
      compiler->out = rb_str_buf_cat(compiler->out, compiler->buf->ptr, compiler->buf->length);
    }
    else {
      log_debug("Push: eval: (%lu) '%s'", compiler->buf->length, compiler->buf->ptr);
      cstr_appendc(compiler->buf, '\n');
      compiler->out = rb_str_buf_cat(compiler->out, compiler->buf->ptr, compiler->buf->length);
    }
  }
  else {
    log_debug("Push: text: (%lu) '%s'", compiler->buf->length, compiler->buf->ptr);
    cstr_appendc(compiler->buf, '\'');
    cstr_appendc(compiler->buf, ')');
    cstr_appendc(compiler->buf, '\n');
    compiler->out = rb_str_buf_cat(compiler->out, "@out.write('", 12);
    compiler->out = rb_str_buf_cat(compiler->out, compiler->buf->ptr, compiler->buf->length);
  }
  
  cstr_reset(compiler->buf);
  return output_started;
}

static VALUE RHP_Compiler_compile_file(VALUE self, VALUE file) {
  log_debug("Entered RHP_Compiler_compile_file");
  rhp_compiler_t *compiler;
  OpenFile *fptr;
  FILE *f;
  const char *filename;
  int c;
  int prev_c = -1;
  int prev_prev_c = -1;
  int context = CTX_TEXT;
  int return_status = 0;
  int output_started = 0;
  size_t line = 1;
  size_t column = 0;
  cstr *buf;
  
  // Parse arguments
  log_debug("Parsing arguments");
  Check_Type(file, T_FILE);
  Data_Get_Struct(self, rhp_compiler_t, compiler);
  
  log_debug("compiler->buf = %p", compiler->buf);
  log_debug("compiler->buf: size=%ld length=%ld ptr=%p",
    compiler->buf->size, compiler->buf->length, compiler->buf->ptr);
  
  // Get FD
  log_debug("Aquiring FD");
  GetOpenFile(file, fptr);
  rb_io_check_readable(fptr);
  f = fptr->f;
  filename = fptr->path;
  
  // Initialize output
  log_debug("Initializing output");
  compiler->out = rb_str_new("", 0);
  log_debug("Resizing output");
  RESIZE_CAPA(compiler->out, 4096);
  log_debug("Referenceing buffer");
  
  log_debug("Entering read-loop");
  while(++column) {
    c  = fgetc(f); // We might want to wrap this in TRAP_BEG .. TRAP_END
    // Handle EOF
    if(c == EOF) {
      log_debug("EOF @ column %lu, line %lu", column, line);
      if(ferror(f)) {
        log_error("I/O Error #%d: %s", errno, get_errno_msg());
        clearerr(f);
        if (!rb_io_wait_readable(fileno(f))) {
          rb_sys_fail(fptr->path);
        }
        return_status = errno;
      }
      break; // true EOF
    }
    
    // In text context?
    if(context & CTX_TEXT) {
      if(prev_c == '<' && c == '%') {
        log_parse("Switch: TEXT -> EVAL <%%");
        cstr_popc(compiler->buf); // remove '<'
        log_parse("Push: Text: (%lu) '%s'", compiler->buf->length, compiler->buf->ptr);
        output_started = _compile_push(compiler, context, output_started);
        context = CTX_EVAL;
      }
      else {
        if(c == '\'') { // Escape
          cstr_appendc(compiler->buf, '\\');
        }
        cstr_appendc(compiler->buf, c);
      }
    }
    // In eval context?
    else if(context & CTX_EVAL) {
      if(prev_c == '%' && c == '>') {
        log_parse("Switch: EVAL -> TEXT %%>");
        cstr_popc(compiler->buf); // remove '%'
        //#ifdef DEBUG
          if(context & CTX_COMMENT) {
            log_parse("Push: Comment: (%lu) '%s'", compiler->buf->length, compiler->buf->ptr);
          } else if(context & CTX_PRINT) {
            log_parse("Push: Lua-print: (%lu) '%s'", compiler->buf->length, compiler->buf->ptr);
          } else {
            log_parse("Push: Lua-eval: (%lu) '%s'", compiler->buf->length, compiler->buf->ptr);
          }
        //#endif
        output_started = _compile_push(compiler, context, output_started);
        context = CTX_TEXT;
      }
      else if(prev_prev_c == '<' && prev_c == '%') {
        // the first char after "<%"
        if(c == '#') {
          context |= CTX_COMMENT;
          log_parse("Switch: EVAL -> COMMENT <%%#");
        }
        else if(c == '=') {
          context |= CTX_PRINT;
          log_parse("Switch: EVAL -> PRINT <%%=");
        } 
        //else { log_parse(filename, line, column, "EVAL == EVAL"); }
      }
      else {
        cstr_appendc(compiler->buf, c);
      }
      //else { log_parse(filename, line, column, "EVAL == EVAL %c %c", prev_prev_c, prev_c); }
    }
    //else { log_debug("nomatch %d (%d, %d)", context & CTX_EVAL, context, CTX_EVAL); }
    if(c == '\n') {
      line++;
      column = 0;
    }
    
    prev_prev_c = prev_c;
    prev_c = c;
  }
  
  cstr_reset(compiler->buf);
  return compiler->out;
}

// ------------------------------------------
// Library init code

void Init_rhp_c() {
  // module RHP
  RHP = rb_define_module("RHP");
  
  // class String mixins
  VALUE cRString = rb_const_get(rb_cObject, rb_intern("String"));
  rb_define_method(cRString, "xml_safe", RString_xml_safe, 0);
  
  // class RHP::Compiler < Object
  RHP_Compiler = rb_define_class_under(RHP, "Compiler", rb_cObject);
  rb_define_alloc_func(RHP_Compiler, RHP_Compiler_allocate);
  rb_define_method(RHP_Compiler, "compile_file", RHP_Compiler_compile_file, 1);
  
  // class RHP::CompileError < StandardError
  RHP_CompileError = rb_define_class_under(RHP, "CompileError", rb_eStandardError);
  
  rb_provide("rhp_c");
}
