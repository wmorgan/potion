//
// string.c
// internals of utf-8 and byte strings
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "potion.h"
#include "internal.h"
#include "khash.h"

KHASH_MAP_INIT_STR(str, PN);

struct PNStrTable {
  PN_OBJECT_HEADER
  kh_str_t kh[0];
};

void potion_add_str(PN self, const char *str, PN id) {
  int ret;
  struct PNStrTable *t = (struct PNStrTable *)self;
  unsigned k = kh_put(str, t->kh, str, &ret);
  if (!ret) kh_del(str, t->kh, k);
  kh_value(t->kh, k) = id;
}

PN potion_lookup_str(PN self, const char *str) {
  struct PNStrTable *t = (struct PNStrTable *)self;
  unsigned k = kh_get(str, t->kh, str);
  if (k != kh_end(t->kh)) return kh_value(t->kh, k);
  return PN_NIL;
}

PN potion_str(Potion *P, const char *str) {
  PN id = potion_lookup_str(P->strings, str);
  if (!id) {
    size_t len = strlen(str);
    struct PNString *s = PN_BOOT_OBJ_ALLOC(struct PNString, PN_TSTRING, len + 1);
    s->len = (unsigned int)len;
    PN_MEMCPY_N(s->chars, str, char, len);
    s->chars[len] = '\0';
    id = (PN)s;

    potion_add_str(P->strings, s->chars, id);
  }
  return id;
}

PN potion_str2(Potion *P, char *str, size_t len) {
  PN s;
  char c = str[len];
  str[len] = '\0';
  s = potion_str(P, str);
  str[len] = c;
  return s;
}

static PN potion_str_length(Potion *P, PN closure, PN self) {
  return PN_NUM(potion_cp_strlen_utf8(PN_STR_PTR(self)));
}

static PN potion_str_eval(Potion *P, PN closure, PN self) {
  return potion_eval(P, PN_STR_PTR(self));
}

static PN potion_str_number(Potion *P, PN closure, PN self) {
  char *str = PN_STR_PTR(self);
  int i = 0, dec = 0, sign = 0, len = PN_STR_LEN(self);
  if (len < 1) return PN_ZERO;

  sign = (str[0] == '-' ? -1 : 1);
  if (str[0] == '-' || str[0] == '+') {
    dec++; str++; len--;
  }
  for (i = 0; i < len; i++)
    if (str[i] < '0' || str[i] > '9')
      break;
  if (i < 10 && i == len) {
    return PN_NUM(sign * PN_ATOI(str, i));
  }

  return potion_decimal(P, PN_STR_LEN(self), dec + i, PN_STR_PTR(self));
}

static PN potion_str_string(Potion *P, PN closure, PN self) {
  return self;
}

static size_t potion_utf8char_width(const char *s, size_t index) {
  if((s[index] & 0x80) == 0) return 1;
  switch(s[index] & 0xF0) {
    case 0xE0: return 3;
    case 0xF0: return 4;
    default: return 2; // assume valid. what could possibly go wrong?
  }
}

static PN potion_str_escape(Potion *P, PN closure, PN self) {
  PN ret;
  char* str = PN_STR_PTR(self);
  /* at worst we're escaping every character into octal form (4 bytes) */
  char buf[1 + (PN_STR_LEN(self) * 4)];

  int str_offset = 0;
  int buf_offset = 0;

  while(str_offset < PN_STR_LEN(self)) {
    size_t width = potion_utf8char_width(str, str_offset);

    if(width > 1) { // multibyte; no escaping required
      PN_MEMCPY_N(&buf[buf_offset], &str[str_offset], char, width);
      buf_offset += width;
    }
    else {
      char c = str[str_offset];
      if(c == '\\') { buf[buf_offset++] = '\\'; buf[buf_offset++] = '\\'; }
      else if(isprint(c)) { buf[buf_offset++] = str[str_offset]; }
      else switch(c) {
        case  7: buf[buf_offset++] = '\\'; buf[buf_offset++] = 'a'; break;
        case  8: buf[buf_offset++] = '\\'; buf[buf_offset++] = 'b'; break;
        case  9: buf[buf_offset++] = '\\'; buf[buf_offset++] = 't'; break;
        case 10: buf[buf_offset++] = '\\'; buf[buf_offset++] = 'n'; break;
        case 11: buf[buf_offset++] = '\\'; buf[buf_offset++] = 'v'; break;
        case 12: buf[buf_offset++] = '\\'; buf[buf_offset++] = 'f'; break;
        case 13: buf[buf_offset++] = '\\'; buf[buf_offset++] = 'r'; break;
        case 27: buf[buf_offset++] = '\\'; buf[buf_offset++] = 'e'; break;
        default: buf_offset += sprintf(buf, "\\%o", (unsigned int)str[str_offset]); break;
      }
    }

    str_offset += width;
  }

  ret = potion_str2(P, buf, buf_offset);
  return ret;
}

static PN potion_str_print(Potion *P, PN closure, PN self) {
  printf("%s", PN_STR_PTR(self));
  return PN_NIL;
}

static PN potion_str__link(Potion *P, PN closure, PN self, PN link) {
  return link;
}

static size_t potion_utf8char_offset(const char *s, size_t index) {
  int i;
  for (i = 0; s[i]; i++) {
    if ((s[i] & 0xC0) != 0x80) {
      if (index-- == 0) {
        return i;
      }
    }
  }
  return i;
}

inline static PN potion_str_slice_index(PN index, size_t len, int nilvalue) {
  int i = PN_INT(index);
  int corrected;
  if (PN_IS_NIL(index)) {
    corrected = nilvalue;
  } else if (i < 0) {
    corrected = i + len;
    if (corrected < 0) {
      corrected = 0;
    }
  } else if (i > len) {
    corrected = len;
  } else {
    corrected = i;
  }
  return PN_NUM(corrected);
}

static PN potion_str_slice(Potion *P, PN closure, PN self, PN start, PN end) {
  char *str = PN_STR_PTR(self);
  size_t len = potion_cp_strlen_utf8(str);
  size_t startoffset = potion_utf8char_offset(str, PN_INT(potion_str_slice_index(start, len, 0)));
  size_t endoffset = potion_utf8char_offset(str, PN_INT(potion_str_slice_index(end, len, len)));
  return potion_str2(P, str + startoffset, endoffset - startoffset);
}

PN potion_byte_str(Potion *P, const char *str) {
  size_t len = strlen(str);
  struct PNBytes *s = (struct PNBytes *)potion_bytes(P, len + 1);
  PN_MEMCPY_N(s->chars, str, char, len);
  s->len = len;
  s->chars[len] = '\0';
  return (PN)s;
}

PN potion_bytes(Potion *P, size_t len) {
  struct PNBytes *s = PN_OBJ_ALLOC(struct PNBytes, PN_TBYTES, 0);
  s->len = (PN_SIZE)len;
  s->chars = PN_ALLOC_N(char, len);
  return (PN)s;
}

PN_SIZE pn_printf(Potion *P, PN bytes, const char *format, ...) {
  PN_SIZE len;
  va_list args;
  struct PNBytes *s = (struct PNBytes *)bytes;

  va_start(args, format);
  len = (PN_SIZE)vsnprintf(NULL, 0, format, args);
  va_end(args);
  PN_REALLOC_N(s->chars, char, s->len + len + 1);

  va_start(args, format);
  vsnprintf(s->chars + s->len, len + 1, format, args);
  va_end(args);

  s->len += len;
  return len;
}

void potion_bytes_obj_string(Potion *P, PN bytes, PN obj) {
  potion_bytes_append(P, 0, bytes, potion_send(obj, PN_string));
}

PN potion_bytes_append(Potion *P, PN closure, PN self, PN str) {
  struct PNBytes *s = (struct PNBytes *)self;
  PN_SIZE len = PN_STR_LEN(str);
  PN_REALLOC_N(s->chars, char, s->len + len + 1);
  PN_MEMCPY_N(s->chars + s->len, PN_STR_PTR(str), char, len);
  s->len += len;
  s->chars[s->len] = '\0';
  return self;
}

static PN potion_bytes_length(Potion *P, PN closure, PN self) {
  return PN_NUM(PN_STR_LEN(self));
}

static PN potion_bytes__link(Potion *P, PN closure, PN self, PN link) {
  struct PNBytes *s = (struct PNBytes *)self;
  PN_CLINK(s->chars);
  return link;
}

void potion_str_hash_init(Potion *P) {
  struct PNStrTable *t = PN_CALLOC(struct PNStrTable, sizeof(kh_str_t));
  PN_GB(t);
  t->vt = PN_TTABLE;
  P->strings = (PN)t;
}

void potion_str_init(Potion *P) {
  PN str_vt = PN_VTABLE(PN_TSTRING);
  PN byt_vt = PN_VTABLE(PN_TBYTES);
  potion_method(str_vt, "eval", potion_str_eval, 0);
  potion_method(str_vt, "length", potion_str_length, 0);
  potion_method(str_vt, "number", potion_str_number, 0);
  potion_method(str_vt, "print", potion_str_print, 0);
  potion_method(str_vt, "string", potion_str_string, 0);
  potion_method(str_vt, "~link", potion_str__link, 0);
  potion_method(str_vt, "slice", potion_str_slice, "start=N,end=N");
  potion_method(byt_vt, "append", potion_bytes_append, 0);
  potion_method(byt_vt, "length", potion_bytes_length, 0);
  potion_method(byt_vt, "print", potion_str_print, 0);
  potion_method(byt_vt, "string", potion_str_string, 0);
  potion_method(byt_vt, "~link", potion_bytes__link, 0);
}
