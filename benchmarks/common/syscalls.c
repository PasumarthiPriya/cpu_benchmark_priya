// See LICENSE for license details.

#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <sys/signal.h>
#include "util.h"

#define SYS_write 64

#undef strcmp

#define NUM_COUNTERS 2
static uintptr_t counters[NUM_COUNTERS];
static char* counter_names[NUM_COUNTERS];

extern volatile uint64_t tohost;
extern volatile uint64_t fromhost;
extern int uart_write(int a0);

static uintptr_t syscall(uintptr_t which, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
  volatile uint64_t magic_mem[8] __attribute__((aligned(64)));
  magic_mem[0] = which;
  magic_mem[1] = arg0;
  magic_mem[2] = arg1;
  magic_mem[3] = arg2;
  __sync_synchronize();

  tohost = (uintptr_t)magic_mem;
  while (fromhost == 0)
    ;
  fromhost = 0;

  __sync_synchronize();
  return magic_mem[0];
}

void __attribute__((noreturn)) tohost_exit(uintptr_t code)
{
  tohost = (code << 1) | 1;
  while (1);
}

void exit()
{
#ifdef CUSTOM
   asm volatile (
        "uart_end: li t1, 0x11300" "\n\t"	//The base address of UART config registers
        "lh a0, 12(t1)" "\n\t"
        "andi a0, a0, 0x1" "\n\t"
        "beqz a0, uart_end" "\n\t"
        "csrr a0, mhpmcounter3" "\n\t"
        "csrr a0, mhpmcounter4" "\n\t"
        "li a0,  0x20000" "\n\t"
        "sw a0,  12(a0)" "\n\t"
				:
				:
				:"a0","t1","cc","memory");
#endif

#ifdef SPIKE
  tohost_exit(0);
#else
 while(1);
#endif
}


void setStats(int enable)
{
  int i = 0;
#define READ_CTR(name) do { \
    while (i >= NUM_COUNTERS) ; \
    uintptr_t csr = read_csr(name); \
    if (!enable) { csr -= counters[i]; counter_names[i] = #name; } \
    counters[i++] = csr; \
  } while (0)

  READ_CTR(mcycle);
  READ_CTR(minstret);

#undef READ_CTR
}

uintptr_t __attribute__((weak)) handle_trap(uintptr_t cause, uintptr_t epc, uintptr_t regs[32])
{
  printf("Trap Cause: %d encountered on PC:%x.\n Exiting ...", cause, epc);
  exit();
}

void abort()
{
  exit();
}

void printstr(const char* s)
{
  syscall(SYS_write, 1, (uintptr_t)s, strlen(s));
}

void __attribute__((weak)) thread_entry(int cid, int nc)
{
  // multi-threaded programs override this function.
  // for the case of single-threaded programs, only let core 0 proceed.
  while (cid != 0);
}

int __attribute__((weak)) main(int argc, char** argv)
{
  // single-threaded programs override this function.
  printstr("Implement main(), foo!\n");
  return -1;
}

#undef putchar
#ifdef CUSTOM
int putchar(int ch)
{
  register char a0 asm("a0") = ch;



    asm volatile (
        "li t1, 0x11300\n\t"
        " uart_status_simple: lh a1, 12(t1)\n\t"
        "andi a1,a1,0x2\n\t"
        "bnez a1, uart_status_simple\n\t"
        "sb a0, 4(t1)\n\t"
        :
        :
        : "a0", "t1", "a1", "cc", "memory"
    );
    return 0;


}
#endif

#ifdef SPIKE
int putchar(int ch)
{
  static __thread char buf[64] __attribute__((aligned(64)));
  static __thread int buflen = 0;

  buf[buflen++] = ch;

  if (ch == '\n' || buflen == sizeof(buf))
  {
    syscall(SYS_write, 1, (uintptr_t)buf, buflen);
    buflen = 0;
  }

  return 0;
}
#endif

void _init(int cid, int nc)
{
  // only single-threaded programs should ever get here.
  int ret = main(0, 0);

  char buf[NUM_COUNTERS * 32] __attribute__((aligned(64)));
  char* pbuf = buf;
  for (int i = 0; i < NUM_COUNTERS; i++)
    if (counters[i])
			printf("%s = %d\n",counter_names[i],counters[i]);

  exit(ret);
}


void printhex(uint64_t x)
{
  char str[17];
  int i;
  for (i = 0; i < 16; i++)
  {
    str[15-i] = (x & 0xF) + ((x & 0xF) < 10 ? '0' : 'a'-10);
    x >>= 4;
  }
  str[16] = 0;

  printstr(str);
}

static inline void printnum(void (*putch)(int, void**), void **putdat,
                    unsigned long long num, unsigned base, int width, int padc)
{
  unsigned digs[sizeof(num)*CHAR_BIT];
  int pos = 0;

  while (1)
  {
    digs[pos++] = num % base;
    if (num < base)
      break;
    num /= base;
  }

  while (width-- > pos)
    putch(padc, putdat);

  while (pos-- > 0)
    putch(digs[pos] + (digs[pos] >= 10 ? 'a' - 10 : '0'), putdat);
}

static unsigned long long getuint(va_list *ap, int lflag)
{
  if (lflag >= 2)
    return va_arg(*ap, unsigned long long);
  else if (lflag)
    return va_arg(*ap, unsigned long);
  else
    return va_arg(*ap, unsigned int);
}

static long long getint(va_list *ap, int lflag)
{
  if (lflag >= 2)
    return va_arg(*ap, long long);
  else if (lflag)
    return va_arg(*ap, long);
  else
    return va_arg(*ap, int);
}

/** @fn float pow_10(unsigned int y)
 * @brief generate different powers of 10
 * @param unsigned int y
 * @return return result in float
 */
float pow_10(unsigned int y)
{
        unsigned int x=1;

        for (unsigned int i=0; i <y; i++)
        {
                x *= 10;
        }

        return ((float) x);
}

/** @fn void reverse(char *str, int length)
 * @brief reverse a string and store in the same string
 * @param char *str
 * @param int length
 */
void reverse(char *str, int length)
{
        int i = 0;
        int j = length - 1;
        char tmp;

        while (i<j)
        {
                tmp = str[i];
                str[i] = str[j];
                str[j] = tmp;

                i++;
                j--;
        }
}

/** @fn int int_to_string(int number, char str[], int afterpoint)
 * @brief convert decimal numbers to string
 * @details Takes num as input and converts it to string.
 *          The converted string is stored in str. The
 *          position of last character in the str is returned.
 *          This function is tailored to support ftoa.
 * @param int number
 * @param char str[]
 * @param int afterpoint
 * @return int
 */
int int_to_string(int number, char str[], unsigned int afterpoint)
{
        uint32_t i = 0;

        /*extract each digit and put into str[i]*/

        while (number != 0)
        {
                str[i] = ((number%10) + '0');
                i++;
                number = number/10;
        }

        /*insert 0 after the numbers, if count of digits less than afterpoint*/

        while (i < afterpoint)
        {
                str[i] = '0';
                i++;
        }

        /*
           zeroth digit is in oth position in array,
           To read digits properly, reverse array
         */
        reverse(str, i);
        str[i] = '\0';

        return i;
}
/** @fn void ftoa(float n, char *res, int afterpoint)
 * @brief converts float to string
 * @details Split floating number into fpart and ipart
 *          Finally merge it into one float number.
 *          Return a string, which has the float value.
 * @param float (floating point number - n)
 * @param char* (float in string - res)
 * @param int (precision - afterpoint)
 */
void ftoa(float n, char *res, unsigned int afterpoint)
{
        int i=0;
        char temp[30]={'\0'};
        n += 0.0000001;
        // Extract integer part
        int ipart = (int)n;

        // Extract floating part
        float fpart = (float) (n - (float)ipart);
        int j=0;

        if(n < (0/1))
        {
                res[j]='-';
                j=1;
        }

        if (ipart == 0)
        {
                res[j]='0';
                j=j+1;
        }
        else{
                if (ipart <0)
                {
                        ipart =(-1)*ipart;
                }

                i = int_to_string(ipart, temp, 0);

                strcpy(res+j,temp);
        }

        i = i+j;

        // check for display option after point
        if (afterpoint != 0)
        {
                res[i] = '.';// add dot

                if (fpart < 0/1)
                {

                        fpart = (-1)*fpart;

                }
                else if (fpart == 0/1)
                {
                        fpart = fpart;
                }

                fpart = fpart * pow_10( afterpoint);

                int_to_string((int)fpart, res + i + 1, afterpoint);
        }
}

static void vprintfmt(void (*putch)(int, void**), void **putdat, const char *fmt, va_list ap)
{
  register const char* p;
  const char* last_fmt;
  register int ch, err;
  unsigned long long num;
  int base, lflag, width, precision, altflag;
  float float_num = 0;
  char float_arr[30] = {'\0'};
  char padc;

  while (1) {
    while ((ch = *(unsigned char *) fmt) != '%') {
      if (ch == '\0')
        return;
      fmt++;
      putch(ch, putdat);
    }
    fmt++;

    // Process a %-escape sequence
    last_fmt = fmt;
    padc = ' ';
    width = -1;
    precision = -1;
    lflag = 0;
    altflag = 0;
  reswitch:
    switch (ch = *(unsigned char *) fmt++) {

    // flag to pad on the right
    case '-':
      padc = '-';
      goto reswitch;

    // flag to pad with 0's instead of spaces
    case '0':
      padc = '0';
      goto reswitch;

    // width field
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      for (precision = 0; ; ++fmt) {
        precision = precision * 10 + ch - '0';
        ch = *fmt;
        if (ch < '0' || ch > '9')
          break;
      }
      goto process_precision;

    case '*':
      precision = va_arg(ap, int);
      goto process_precision;

    case '.':
      if (width < 0)
        width = 0;
      goto reswitch;

    case '#':
      altflag = 1;
      goto reswitch;

    process_precision:
      if (width < 0)
        width = precision, precision = -1;
      goto reswitch;

    // long flag (doubled for long long)
    case 'l':
      lflag++;
      goto reswitch;

    // character
    case 'c':
      putch(va_arg(ap, int), putdat);
      break;

    // string
    case 's':
      if ((p = va_arg(ap, char *)) == NULL)
        p = "(null)";
      if (width > 0 && padc != '-')
        for (width -= strnlen(p, precision); width > 0; width--)
          putch(padc, putdat);
      for (; (ch = *p) != '\0' && (precision < 0 || --precision >= 0); width--) {
        putch(ch, putdat);
        p++;
      }
      for (; width > 0; width--)
        putch(' ', putdat);
      break;

    // (signed) decimal
    case 'd':
      num = getint(&ap, lflag);
      if ((long long) num < 0) {
        putch('-', putdat);
        num = -(long long) num;
      }
      base = 10;
      goto signed_number;

    // unsigned decimal
    case 'u':
      base = 10;
      goto unsigned_number;

    // (unsigned) octal
    case 'o':
      // should do something with padding so it's always 3 octits
      base = 8;
      goto unsigned_number;

    // pointer
    case 'p':
      static_assert(sizeof(long) == sizeof(void*));
      lflag = 1;
      putch('0', putdat);
      putch('x', putdat);
      /* fall through to 'x' */

    // (unsigned) hexadecimal
    case 'x':
      base = 16;
    unsigned_number:
      num = getuint(&ap, lflag);
    signed_number:
      printnum(putch, putdat, num, base, width, padc);
      break;

    case 'f':
      float_num =  va_arg(ap, double);
      ftoa(float_num, float_arr, 6);
      for( int i = 0; float_arr[i] != '\0'; i++) {
          putch(float_arr[i],putdat);
          if(i > 29)
            break;
      }
      break;
    // escaped '%' character
    case '%':
      putch(ch, putdat);
      break;

    // unrecognized escape sequence - just print it literally
    default:
      putch('%', putdat);
      fmt = last_fmt;
      break;
    }
  }
}

int printf(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  vprintfmt((void*)putchar, 0, fmt, ap);

  va_end(ap);
  return 0; // incorrect return value, but who cares, anyway?
}

int sprintf(char* str, const char* fmt, ...)
{
  va_list ap;
  char* str0 = str;
  va_start(ap, fmt);

  void sprintf_putch(int ch, void** data)
  {
    char** pstr = (char**)data;
    **pstr = ch;
    (*pstr)++;
  }

  vprintfmt(sprintf_putch, (void**)&str, fmt, ap);
  *str = 0;

  va_end(ap);
  return str - str0;
}

void* memcpy(void* dest, const void* src, size_t len)
{
  if ((((uintptr_t)dest | (uintptr_t)src | len) & (sizeof(uintptr_t)-1)) == 0) {
    const uintptr_t* s = src;
    uintptr_t *d = dest;
    while (d < (uintptr_t*)(dest + len))
      *d++ = *s++;
  } else {
    const char* s = src;
    char *d = dest;
    while (d < (char*)(dest + len))
      *d++ = *s++;
  }
  return dest;
}

void* memset(void* dest, int byte, size_t len)
{
  if ((((uintptr_t)dest | len) & (sizeof(uintptr_t)-1)) == 0) {
    uintptr_t word = byte & 0xFF;
    word |= word << 8;
    word |= word << 16;
    word |= word << 16 << 16;

    uintptr_t *d = dest;
    while (d < (uintptr_t*)(dest + len))
      *d++ = word;
  } else {
    char *d = dest;
    while (d < (char*)(dest + len))
      *d++ = byte;
  }
  return dest;
}

size_t strlen(const char *s)
{
  const char *p = s;
  while (*p)
    p++;
  return p - s;
}

size_t strnlen(const char *s, size_t n)
{
  const char *p = s;
  while (n-- && *p)
    p++;
  return p - s;
}

int strcmp(const char* s1, const char* s2)
{
  unsigned char c1, c2;

  do {
    c1 = *s1++;
    c2 = *s2++;
  } while (c1 == c2 && c1 != 0);

  return c1 - c2;
}

char* strcpy(char* dest, const char* src)
{
  char* d = dest;
  while ((*d++ = *src++))
    ;
  return dest;
}

long atol(const char* str)
{
  long res = 0;
  int sign = 0;

  while (*str == ' ')
    str++;

  if (*str == '-' || *str == '+') {
    sign = *str == '-';
    str++;
  }

  while (*str) {
    res *= 10;
    res += *str++ - '0';
  }

  return sign ? -res : res;
}
