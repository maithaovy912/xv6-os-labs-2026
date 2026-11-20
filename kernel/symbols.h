#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "types.h"

struct symbol_entry {
  uint64 addr;
  const char *func;
  const char *file_line;
};

extern struct symbol_entry ksymbols[];
extern int nksymbols;

void print_symbol(uint64 addr);

#endif