#include <ruby.h>

static VALUE mRHP;


// String additions
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

// ---------------------



// ---------------------

void Init_rhp() {
  mRHP = rb_define_module("RHP");
  
  ID cRString_id = rb_intern("String"); 
  VALUE cRString = rb_const_get(rb_cObject, cRString_id);
  rb_define_method(cRString, "xml_safe", RString_xml_safe, 0);
}