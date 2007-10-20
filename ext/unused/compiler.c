#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "cstr.h"
#include "macros.h"

#define CTX_TEXT 1
#define CTX_EVAL 2
#define CTX_COMMENT 4
#define CTX_PRINT 8


#define log_parse(fmt, ...) fprintf(stdout, "%s %lu:%-2lu  " fmt "\n", filename, line, column, ##__VA_ARGS__)


static int _push(cstr *buf, const int context) {
  
  if(context & CTX_EVAL && context & CTX_COMMENT) {
    /*...*/
  }
  
  //switch(context) {}
  // push line onto lua
  /*error = luaL_loadbuffer(L, buff, strlen(buff), "line") || lua_pcall(L, 0, 0, 0);
  if (error) {
    fprintf(stderr, "%s", lua_tostring(L, -1));
    lua_pop(L, 1);  // pop error message from the stack
  }*/
  cstr_reset(&buf);
  return 0;
}


static int _compile_file(const char *filename, FILE *fp, cstr *buf) {
  
  int ch_prev = -1;
  int ch_prev_prev = -1;
  int ch;
  int context = CTX_TEXT;
  int return_status = 0;
  size_t line = 1;
  size_t column = 0;
  
  while(++column) {
    ch = fgetc(fp);
    if(ch == EOF) {
      log_debug("EOF @ column %lu, line %lu", column, line);
      if(ferror(fp)) {
        log_error("I/O Error #%d: %s", errno, get_errno_msg());
        return_status = errno;
      } // else: EOF
      break;
    }
    
    if(context & CTX_TEXT) {
      if(ch_prev == '<' && ch == '%') {
        log_parse("Switch: TEXT -> EVAL <%%");
        cstr_popc(&buf); // remove '<'
        log_parse("Push: Text: (%lu) '%s'", buf.length, buf.ptr);
        _compile_push(L, context);
        context = CTX_EVAL;
      }
      else {
        cstr_appendc(&buf, ch);
      }
    }
    else if(context & CTX_EVAL) {
      if(ch_prev == '%' && ch == '>') {
        log_parse("Switch: EVAL -> TEXT %%>");
        cstr_popc(&buf); // remove '%'
        //#ifdef DEBUG
          if(context & CTX_COMMENT) {
            log_parse("Push: Comment: (%lu) '%s'", buf.length, buf.ptr);
          } else if(context & CTX_PRINT) {
            log_parse("Push: Lua-print: (%lu) '%s'", buf.length, buf.ptr);
          } else {
            log_parse("Push: Lua-eval: (%lu) '%s'", buf.length, buf.ptr);
          }
        //#endif
        _compile_push(L, context);
        context = CTX_TEXT;
      }
      else if(ch_prev_prev == '<' && ch_prev == '%') {
        // the first char after "<%"
        if(ch == '#') {
          context |= CTX_COMMENT;
          log_parse("Switch: EVAL -> COMMENT <%%#");
        }
        else if(ch == '=') {
          context |= CTX_PRINT;
          log_parse("Switch: EVAL -> PRINT <%%=");
        } 
        //else { log_parse(filename, line, column, "EVAL == EVAL"); }
      }
      else {
        cstr_appendc(&buf, ch);
      }
      //else { log_parse(filename, line, column, "EVAL == EVAL %c %c", ch_prev_prev, ch_prev); }
    }
    //else { log_debug("nomatch %d (%d, %d)", context & CTX_EVAL, context, CTX_EVAL); }
    
    if(ch == '\n') {
      line++;
      column = 0;
    }
    
    ch_prev_prev = ch_prev;
    ch_prev = ch;
  }
  
  cstr_reset(&buf);
  return return_status;
}
