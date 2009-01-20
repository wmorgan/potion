//
// table.h
// the central table type, based on khash
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_TABLE_H
#define POTION_TABLE_H

#include "khash.h"

#if __WORDSIZE != 64
KHASH_MAP_INIT_INT(PN, PN);
#else
KHASH_MAP_INIT_INT64(PN, PN);
#endif

KHASH_MAP_INIT_STR(str, PN);
KHASH_MAP_INIT_INT(id, PN);

struct PNTable {
  PN_OBJECT_HEADER
  kh_PN_t *kh;
};

PN potion_table_new();
PN potion_table_at(Potion *P, PN cl, PN self, PN key);
PN potion_table_put(Potion *P, PN cl, PN self, PN key, PN value);

#endif
