#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

#define NPAGES 32
#define MARKER "secret"
#define MARKER_LEN 6   // excluding the '\0' byte

static int
is_printable(char c)
{
  return c >= 32 && c < 127;
}

// Check if an independent "secret\0" token exists at position i
// (the byte right before it must be 0 to ensure it is a standalone string,
//  not a suffix of another string)
static int
match_marker(char *p, int i, int len)
{
  if (i + MARKER_LEN >= len)
    return 0;
  for (int k = 0; k < MARKER_LEN; k++) {
    if (p[i + k] != MARKER[k])
      return 0;
  }
  if (p[i + MARKER_LEN] != 0)
    return 0;
  if (i > 0 && p[i - 1] != 0)
    return 0;
  return 1;
}

int
main(int argc, char *argv[])
{
  char *p = sbrk(NPAGES * PGSIZE);
  if (p == (char *) -1) {
    exit(1);
  }
  int len = NPAGES * PGSIZE;

  for (int i = 0; i < len; i++) {
    if (!match_marker(p, i, len))
      continue;

    // Secret is located RIGHT BEFORE the "secret" marker (may be separated
    // by a few padding bytes due to 16-byte stack alignment during exec()).
    // Move backward, skip zero-padding bytes, then measure the printable string length.
    int j = i - 1;
    while (j >= 0 && p[j] == 0)
      j--;
    if (j < 0 || !is_printable(p[j]))
      continue;

    int end = j;
    while (j >= 0 && is_printable(p[j]))
      j--;
    int start = j + 1;

    if (end >= start) {
      printf("%s\n", p + start);
      exit(0);
    }
  }

  exit(1);
}