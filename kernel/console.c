//
// Console input and output, to the uart.
// Reads are line at a time.
// Implements special input characters:
//   newline -- end of line
//   control-h -- backspace
//   control-u -- kill line
//   control-d -- end of file
//   control-p -- print process list
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

#define BACKSPACE 0x100       // erase the last output character
#define C(x)      ((x) - '@') // Control-x

//
// send one character to the uart, but don't use
// interrupts or sleep(). safe to be called from
// interrupts, e.g. by printk and to echo input
// characters.
//
void
consputc(int c)
{
  if (c == BACKSPACE) {
    // if the user typed backspace, overwrite with a space.
    uartputc_sync('\b');
    uartputc_sync(' ');
    uartputc_sync('\b');
  } else {
    uartputc_sync(c);
  }
}

// input circular buffer
#define INPUT_BUF_SIZE 128

struct {
  struct spinlock lock;
  char buf[INPUT_BUF_SIZE];
  uint r; // Read index
  uint w; // Write index
  uint e; // Edit index
  int tabmode; // current line already had a Tab (earlier part is committed to the shell)
  int esc;     // escape sequence state: 0 none, 1 after ESC, 2 after ESC [
} cons;

// Hand a delete key to the shell (call while holding cons.lock).
static void
passthru(int c)
{
  if (cons.e - cons.r < INPUT_BUF_SIZE) {
    cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;
    cons.w = cons.e;
    wakeup(&cons.r);
  }
}

//
// user write() system calls to the console go here.
// uses sleep() and UART interrupts.
//
int
consolewrite(int user_src, uint64 src, int n)
{
  char buf[32]; // move batches from user space to uart.
  int i = 0;

  while (i < n) {
    int nn = sizeof(buf);
    if (nn > n - i)
      nn = n - i;
    if (either_copyin(buf, user_src, src + i, nn) == -1)
      break;
    uartwrite(buf, nn);
    i += nn;
  }

  return i;
}

//
// user read()s from the console go here.
// copy (up to) a whole input line to dst.
// user_dst indicates whether dst is a user
// or kernel address.
//
int
consoleread(int user_dst, uint64 dst, int n)
{
  uint target;
  int c;
  char cbuf;

  target = n;
  acquire(&cons.lock);
  while (n > 0) {
    // wait until interrupt handler has put some
    // input into cons.buffer.
    while (cons.r == cons.w) {
      if (killed(myproc())) {
        release(&cons.lock);
        return -1;
      }
      sleep_prepare(&cons.r);
      release(&cons.lock);
      sleep();
      acquire(&cons.lock);
    }

    c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

    if (c == C('D')) { // end-of-file
      if (n < target) {
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        cons.r--;
      }
      break;
    }

    // copy the input byte to the user-space buffer.
    cbuf = c;
    if (either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;

    dst++;
    --n;

    if (c == '\n' || c == '\t') {
      // a whole line has arrived (or a tab for completion),
      // return to the user-level read().
      break;
    }
  }
  release(&cons.lock);

  return target - n;
}

//
// the console input interrupt handler.
// uartintr() calls this for each input character.
// do erase/kill processing, append to cons.buf,
// wake up consoleread() if a whole line has arrived.
//
void
consoleintr(int c)
{
  acquire(&cons.lock);

  switch (c) {
  case C('P'): // Print process list.
    procdump();
    break;
  case C('U'): // Kill line.
    while (cons.e != cons.w &&
           cons.buf[(cons.e - 1) % INPUT_BUF_SIZE] != '\n') {
      cons.e--;
      consputc(BACKSPACE);
    }
    if (cons.tabmode)
      passthru(c);
    break;
  case C('H'): // Backspace
  case '\x7f': // Delete key
    if (cons.e != cons.w) {
      cons.e--;
      consputc(BACKSPACE);
    } else if (cons.tabmode) {
      passthru(c); // no echo, the shell erases it itself
    }
    break;
  default:
    if (c != 0 && cons.e - cons.r < INPUT_BUF_SIZE) {
      c = (c == '\r') ? '\n' : c;

      int isesc = 0, escdone = 0;
      if (cons.esc == 0 && c == 0x1b) {
        cons.esc = 1;
        isesc = 1;
      } else if (cons.esc == 1) {
        if (c == '[') {
          cons.esc = 2;
          isesc = 1;
        } else {
          cons.esc = 0;   // ESC + other key: treat as a normal char
        }
      } else if (cons.esc == 2) {
        isesc = 1;
        if (!((c >= '0' && c <= '9') || c == ';')) {  // final byte (A/B/C/D/~...)
          cons.esc = 0;
          escdone = 1;
        }
      }

      // echo back to the user (don't echo tab or escape sequences)
      if (c != '\t' && !isesc)
        consputc(c);

      // store for consumption by consoleread().
      cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

      if (c == '\t' || escdone)   // escdone: final byte of an escape sequence
        cons.tabmode = 1;
      else if (c == '\n' || c == C('D'))
        cons.tabmode = 0;

      if (c == '\n' || c == '\t' || escdone || c == C('D') ||
          cons.e - cons.r == INPUT_BUF_SIZE) {
        // wake up consoleread() if a whole line, end-of-file, tab
        // or arrow key has arrived.
        cons.w = cons.e;
        wakeup(&cons.r);
      }
    }
    break;
  }

  release(&cons.lock);
}

void
consoleinit(void)
{
  initlock(&cons.lock, "cons");

  uartinit();

  // connect read and write system calls
  // to consoleread and consolewrite.
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
}