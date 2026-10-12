#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void memdump(char *fmt, char *data, int len);

int
main(int argc, char *argv[])
{
  if (argc == 1) {
    printf("Example 1:\n");
    int a[2] = {61810, 2026};
    memdump("ii", (char *)a, sizeof(a));

    printf("Example 2:\n");
    memdump("S", "a string", sizeof("a string"));

    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *)&s, sizeof(s));

    struct sss {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;

    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");

    printf("Example 4:\n");
    memdump("pihcS", (char *)&example, sizeof(example));

    printf("Example 5:\n");
    memdump("sccccc", (char *)&example, sizeof(example));
  } else if (argc == 2) {
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while (n < sizeof(data)) {
      int nn = read(0, data + n, sizeof(data) - n);
      if (nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data, n);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}

// Copy 'size' bytes at data[*pos] into dst and advance *pos.
// Print an error and return 0 if fewer than 'size' bytes remain.
static int
read_bytes(char f, char *data, int len, int *pos, void *dst, int size)
{
  if (len - *pos < size) {
    printf("memdump: not enough data for '%c'\n", f);
    return 0;
  }

  memmove(dst, data + *pos, size);
  *pos += size;
  return 1;
}


void
memdump(char *fmt, char *data, int len)
{
  int pos = 0;

  for (char *f = fmt; *f; f++) {
    switch(*f) {
      case 'i': {
        int value;
        if (!read_bytes('i', data, len, &pos, &value, sizeof(value)))
          return;
        printf("%d\n", value);
        break;
      }
      case 'p': {
        uint64 value;
        if (!read_bytes('p', data, len, &pos, &value, sizeof(value)))
          return;
        printf("%lx\n", value);
        break;
      }
      case 'h': {
        short value;
        if (!read_bytes('h', data, len, &pos, &value, sizeof(value)))
          return;
        printf("%d\n", value);
        break;
      }
      case 'c': {
        char value;
        if (!read_bytes('c', data, len, &pos, &value, sizeof(value)))
          return;
        printf("%c\n", value);
        break;
      }
      case 's': {
        char *str;
        if (!read_bytes('s', data, len, &pos, &str, sizeof(str)))
          return;
        printf("%s\n", str);
        break;
      }
      case 'S': {
        char *start = data + pos;
        int remain = len - pos;
        int k = 0;
        while (k < remain && start[k] != 0)
          k++;
        write(1, start, k);
        write(1, "\n", 1);
        pos = len;
        break;
      }
      default:
        printf("memdump: unknown format '%c'\n", *f);
        return;
    }
  }
}
