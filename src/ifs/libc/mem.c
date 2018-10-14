#include <stdlib.h> // for size_t

#define  INCL_BASE
#include <os2.h>

#include "portable.h"

extern const char __Alphabet[];

unsigned char _HShift = 3;

void _far *memset(void _far *dst, char c, size_t len)
{
    char _far *p;

    for( p = dst; len; --len ) {
        *p++ = c;
    }
    return( dst );
}

void _far *memmove (void _far *_to, const void _far *_from, size_t _len)
{

    char _far *from = (char _far *)_from;
    char _far *to   = (char _far *)_to;

    if ( from == to )
    {
        return( to );
    }

    if ( from < to  &&  from + _len > to )
    {
        to += _len;
        from += _len;
        while( _len != 0 )
        {
            *--to = *--from;
            _len--;
        }
    }
    else
    {
        while( _len != 0 )
        {
            *to++ = *from++;
            _len--;
        }
    }

    return( to );
}

void _far *memcpy(void _far *in_dst, void _far *in_src, size_t len)
{
    char _far *dst = in_dst;
    const char _far *src = in_src;

    for( ; len; --len ) {
        *dst++ = *src++;
    }
    return( in_dst );
}


int memcmp(void _far *in_s1, void _far *in_s2, size_t len)
{
    const char _far *s1 = in_s1;
    const char _far *s2 = in_s2;

    for( ; len; --len )  {
        if( *s1 != *s2 ) {
            return( *s1 - *s2 );
        }
        ++s1; 
        ++s2;
    }
    return( 0 );    /* both operands are equal */
}


int memicmp( const void _far *in_s1, const void _far *in_s2, size_t len )
{
    const unsigned char _far *s1 = (const unsigned char _far *)in_s1;
    const unsigned char _far *s2 = (const unsigned char _far *)in_s2;
    unsigned char           c1;
    unsigned char           c2;

    for( ; len; --len )  {
        c1 = *s1;
        c2 = *s2;
        if( c1 >= 'A'  &&  c1 <= 'Z' )  c1 += 'a' - 'A';
        if( c2 >= 'A'  &&  c2 <= 'Z' )  c2 += 'a' - 'A';
        if( c1 != c2 ) return( c1 - c2 );
        ++s1;
        ++s2;
    }
    return( 0 );    /* both operands are equal */
}

char _far *strcpy(char _far *s, const char _far *t )
{
    char _far *dst;

    dst = s;
    while( *dst++ = *t++ )
        ;
    return( s );
}

size_t strlen(char _far *s)
{
    const char _far *p;

    p = s;
    while( *p != '\0' )
        ++p;
    return( p - s );
}

int strnicmp(const char _far *s, const char _far *t, size_t n)
{
    unsigned char c1;
    unsigned char c2;

    for( ;; ) {
        if( n == 0 )
            return( 0 );            /* equal */
        c1 = *s;
        c2 = *t;

        if( c1 >= 'A'  &&  c1 <= 'Z' )
            c1 += 'a' - 'A';
        if( c2 >= 'A'  &&  c2 <= 'Z' )
            c2 += 'a' - 'A';

        if( c1 != c2 )
            return( c1 - c2 );      /* less than or greater than */
        if( c1 == '\0' )
            return( 0 );            /* equal */
        ++s;
        ++t;
        --n;
    }
}

char _far *strncpy(char _far *dst, char _far *src, size_t len)
{
    char _far *ret;

    ret = dst;
    for( ;len; --len ) {
        if( *src == '\0' ) 
            break;
        *dst++ = *src++;
    }
    while( len != 0 ) {
        *dst++ = '\0';      /* pad destination string with null chars */
        --len;
    }
    return( ret );
}

void _far *memchr( const void _far *s, char c, size_t n )
{
    const char _far *cs = s;

    while( n ) {
        if( *cs == c ) {
            return( (void *)cs );
        }
        ++cs;
        --n;
    }
    return( 0 );
}

char _far *strchr( const char _far *s, int c );

#ifdef _MSC_VER // MSC
char *strchr( const char *s, int c )
{
    char cc = (char)c;
    do {
        if( *s == cc )
            return( (char *)s );
    } while( *s++ != '\0' );
    return( 0 );
}
#endif

#define memeq( p1, p2, len )    ( memcmp((p1),(p2),(len)) == 0 )

char _far *strstr(const char _far *s1, const char _far *s2 )
{
    char _far *end_of_s1;
    int     s1len, s2len;

    if( s2[0] == '\0' ) {
        return( (char *)s1 );
    } else if( s2[1] == '\0' ) {
        return( strchr( s1, s2[0] ) );
    }
    end_of_s1 = memchr( s1, '\0', ~0 );
    s2len = strlen( (char *)s2 );
    for( ;; ) {
        s1len = end_of_s1 - s1;
        if( s1len < s2len )
            break;
        s1 = memchr( s1, *s2, s1len );  /* find start of possible match */

        if( s1 == 0 )
            break;
        if( memeq( (void *)s1, (void *)s2, s2len ) )
            return( (char *)s1 );
        ++s1;
    }
    return( 0 );
}

char _far *strcat( char _far *dst, const char _far *t )
{
    char _far *s;

    s = dst;
    while( *s != '\0' )
        ++s;
    while( *s++ = *t++ )
        ;
    return( dst );
}

char _far *strpbrk( const char _far *str, const char _far *charset )
{
    char            tc;
    //unsigned char  vector[ CHARVECTOR_SIZE ];

    //__setbits( vector, charset );
    for( ; tc = *str; ++str ) {
        /* quit when we find any char in charset */
        //if( GETCHARBIT( vector, tc ) != 0 )
        if (strchr(charset, tc))
             return( (char _far *)str );
    }
    return( 0 );
}

int strcmp( const char _far *s, const char _far *t )
{
    for( ; *s == *t; s++, t++ )
        if( *s == '\0' )
            return( 0 );
    return( *s - *t );
}

#ifdef __WATCOM
char _far *utoa( unsigned value, char _far *buffer, int radix )
{
    char     _far *p = buffer;
    char     _far *q;
    unsigned    rem;
    unsigned    quot;
    char        buf[34];    // only holds ASCII so 'char' is OK

    buf[0] = '\0';
    q = &buf[1];
    do {
        rem = value % radix;
        quot = value / radix;

        *q = __Alphabet[rem];
        ++q;
        value = quot;
    } while( value != 0 );
    while( (*p++ = (char)*--q) )
        ;
    return( buffer );
}

char _far *itoa( int value, char _far *buffer, int radix )
{
    char _far *p = buffer;

    if( radix == 10 ) {
        if( value < 0 ) {
            *p++ = '-';
            value = - value;
        }
    }
    utoa( value, p, radix );
    return( buffer );
}

char _far *ultoa( unsigned long value, char _far *buffer, int radix )
{
    char  _far  *p = buffer;
    char        *q;
    unsigned    rem;
    char        buf[34];        // only holds ASCII so 'char' is OK

    buf[0] = '\0';
    q = &buf[1];
    do {
        rem = value % radix;
        value = value / radix;

        *q = __Alphabet[rem];
        ++q;
    } while( value != 0 );
    while( (*p++ = (char)*--q) )
        ;
    return( buffer );
}


char _far *ltoa( long value, char _far *buffer, int radix )
{
    char _far *p = buffer;

    if( radix == 10 ) {
        if( value < 0 ) {
            *p++ = '-';
            value = - value;
        }
    }
    ultoa( value, p, radix );
    return( buffer );
}
#endif

void cdecl _loadds Message(PSZ pszMessage, ...);

char _far *ulltoa( ULONGLONG value, char _far *buffer, int radix )
{
    char  _far  *p = buffer;
    char        *q;
    unsigned    rem;
    char        buf[34];        // only holds ASCII so 'char' is OK

    buf[0] = '\0';
    q = &buf[1];
    do {
#ifdef INCL_LONGLONG
        rem = value % radix;
        value = value / radix;
#else
        rem = (unsigned)ModUL(value, radix).ulLo;
        value = DivUL(value, radix);
#endif

        *q = __Alphabet[rem];
        ++q;
#ifdef INCL_LONGLONG
    } while( value != 0 );
#else
    } while( NeqUL(value, 0) );
#endif
    while( (*p++ = (char)*--q) )
        ;
    return( buffer );
}

char _far *lltoa( LONGLONG value, char _far *buffer, int radix )
{
    char _far *p = buffer;
    ULONGLONG ullValue;
#ifndef INCL_LONGLONG
    LONGLONG llValue;
#endif

    if( radix == 10 ) {
#ifdef INCL_LONGLONG
        if( value < 0 ) {
#else
        if( iLessL(value, 0) ) {
#endif
            *p++ = '-';
#ifdef INCL_LONGLONG
            ullValue = - value;
#else
            iAssignL(&llValue, 0);
            llValue = iSub(llValue, value);
            ullValue = *(PULONGLONG)&llValue;
#endif
        }
        else {
#ifdef INCL_LONGLONG
            ullValue = value;
#else
            ullValue = *(PULONGLONG)&value;
#endif
        }
    }
    ulltoa( ullValue, p, radix );
    return( buffer );
}

int isspace( int c )
{
  switch (c)
    {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
      return 1;
    default:
      break;
    }

  return 0;
}

int isdigit( int c )
{
   return (c <= 0x39 && c >= 0x30) ? 1 : 0;
}

#ifdef __WATCOM
int atoi( const char _far *p )  /* convert ASCII string to integer */
{
    int             value;
    char            sign;

    //__ptr_check( p, 0 );

    while( isspace( *p ) )
        ++p;
    sign = *p;
    if( sign == '+' || sign == '-' )
        ++p;
    value = 0;
    while( isdigit(*p) ) {
        value = value * 10 + *p - '0';
        ++p;
    }
    if( sign == '-' )
        value = - value;
    return( value );
}

long int atol( const char _far *p )
{
    long int        value;
    char            sign;

    //__ptr_check( p, 0 );

    while( isspace( *p ) )
        ++p;
    sign = *p;
    if( sign == '+' || sign == '-' )
        ++p;
    value = 0;
    while( isdigit(*p) ) {
        value = value * 10 + *p - '0';
        ++p;
    }
    if( sign == '-' )
        value = - value;
    return( value );
}

int isalpha(int c)
{
	return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

char isupper (unsigned char c)
{
    if ( c >= 'A' && c <= 'Z' )
        return 1;
    return 0;
}

int tolower (int c)
{
  if (c >= 'A' && c <= 'Z')
    return (c + ('a' - 'A'));

  return c;
}

int toupper (int c)
{
  if (c >= 'a' && c <= 'z')
    return (c + ('A' - 'a'));

  return c;
}
#endif

#ifdef _MSC_VER // MSC
void _far _cdecl _U8DQ();
void _far _cdecl _I8DQ();
void _far _cdecl _U8DR();
void _far _cdecl _I8DR();
void _far _cdecl _U8M();
void _far _cdecl _I8M();

ULONGLONG _cdecl Div(ULONGLONG ullA, ULONGLONG ullB)
{
  ULONGLONG  ullRet;
  PULONGLONG pullRet = &ullRet;
  PULONGLONG pullA = &ullA;
  PULONGLONG pullB = &ullB;

  _asm {
    push es
    push si

    les si, pullA

    mov  dx, word ptr es:[si]
    mov  cx, word ptr es:[si + 2]
    mov  bx, word ptr es:[si + 4]
    mov  ax, word ptr es:[si + 6]

    les si, pullB

    push word ptr es:[si + 6]
    push word ptr es:[si + 4]
    push word ptr es:[si + 2]
    push word ptr es:[si]
    mov  si, sp

    call _U8DQ
    add  sp, 8

    les si, pullRet

    mov  word ptr es:[si], dx
    mov  word ptr es:[si + 2], cx
    mov  word ptr es:[si + 4], bx
    mov  word ptr es:[si + 6], ax

    pop  si
    pop  es
  }

  return ullRet;
}

ULONGLONG _cdecl DivUL(ULONGLONG ullA, ULONG ulB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = ulB;
  ullSecond.ulHi = 0;

  return Div(ullA, ullSecond);
}

ULONGLONG _cdecl DivUS(ULONGLONG ullA, USHORT usB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = usB;
  ullSecond.ulHi = 0;

  return Div(ullA, ullSecond);
}

LONGLONG _cdecl iDiv(LONGLONG llA, LONGLONG llB)
{
  LONGLONG  llRet;
  PLONGLONG pllRet = &llRet;
  PLONGLONG pllA = &llA;
  PLONGLONG pllB = &llB;

  _asm {
    push es
    push si

    les si, pllA

    mov  dx, word ptr es:[si]
    mov  cx, word ptr es:[si + 2]
    mov  bx, word ptr es:[si + 4]
    mov  ax, word ptr es:[si + 6]

    les si, pllB

    push word ptr es:[si + 6]
    push word ptr es:[si + 4]
    push word ptr es:[si + 2]
    push word ptr es:[si]
    mov  si, sp

    call _I8DQ
    add  sp, 8

    les si, pllRet

    mov  word ptr es:[si], dx
    mov  word ptr es:[si + 2], cx
    mov  word ptr es:[si + 4], bx
    mov  word ptr es:[si + 6], ax

    pop  si
    pop  es
  }

  return llRet;
}

LONGLONG _cdecl iDivL(LONGLONG llA, LONG lB)
{
  LONGLONG llSecond;

  if (lB >= 0)
     {
     llSecond.ulLo = lB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -lB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iDiv(llA, llSecond);
}

LONGLONG _cdecl iDivS(LONGLONG llA, SHORT sB)
{
  LONGLONG llSecond;

  if (sB >= 0)
     {
     llSecond.ulLo = sB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -sB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iDiv(llA, llSecond);
}

LONGLONG _cdecl iDivUL(LONGLONG llA, ULONG ulB)
{
  LONGLONG llSecond;

  llSecond.ulLo = ulB;
  llSecond.ulHi = 0;

  return iDiv(llA, llSecond);
}

LONGLONG _cdecl iDivUS(LONGLONG llA, USHORT usB)
{
  LONGLONG llSecond;

  llSecond.ulLo = usB;
  llSecond.ulHi = 0;

  return iDiv(llA, llSecond);
}

ULONGLONG _cdecl Mod(ULONGLONG ullA, ULONGLONG ullB)
{
  ULONGLONG  ullRet;
  PULONGLONG pullRet = &ullRet;
  PULONGLONG pullA = &ullA;
  PULONGLONG pullB = &ullB;

  _asm {
    push es
    push si

    les si, pullA

    mov  dx, word ptr es:[si]
    mov  cx, word ptr es:[si + 2]
    mov  bx, word ptr es:[si + 4]
    mov  ax, word ptr es:[si + 6]

    les si, pullB

    push word ptr es:[si + 6]
    push word ptr es:[si + 4]
    push word ptr es:[si + 2]
    push word ptr es:[si]
    mov  si, sp

    call _U8DR
    add  sp, 8

    les si, pullRet

    mov  word ptr es:[si], dx
    mov  word ptr es:[si + 2], cx
    mov  word ptr es:[si + 4], bx
    mov  word ptr es:[si + 6], ax

    pop  si
    pop  es
  }

  return ullRet;
}

ULONGLONG _cdecl ModUL(ULONGLONG ullA, ULONG ulB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = ulB;
  ullSecond.ulHi = 0;

  return Mod(ullA, ullSecond);
}

ULONGLONG _cdecl ModUS(ULONGLONG ullA, USHORT usB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = usB;
  ullSecond.ulHi = 0;

  return Mod(ullA, ullSecond);
}

LONGLONG _cdecl iMod(LONGLONG llA, LONGLONG llB)
{
  LONGLONG  llRet;
  PLONGLONG pllRet = &llRet;
  PLONGLONG pllA = &llA;
  PLONGLONG pllB = &llB;

  _asm {
    push es
    push si

    les si, pllA

    mov  dx, word ptr es:[si]
    mov  cx, word ptr es:[si + 2]
    mov  bx, word ptr es:[si + 4]
    mov  ax, word ptr es:[si + 6]

    les si, pllB

    push word ptr es:[si + 6]
    push word ptr es:[si + 4]
    push word ptr es:[si + 2]
    push word ptr es:[si]
    mov  si, sp

    call _I8DR
    add  sp, 8

    les si, pllRet

    mov  word ptr es:[si], dx
    mov  word ptr es:[si + 2], cx
    mov  word ptr es:[si + 4], bx
    mov  word ptr es:[si + 6], ax

    pop  si
    pop  es
  }

  return llRet;
}

LONGLONG _cdecl iModL(LONGLONG llA, LONG lB)
{
  LONGLONG llSecond;

  if (lB >= 0)
     {
     llSecond.ulLo = lB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -lB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iMod(llA, llSecond);
}

LONGLONG _cdecl iModS(LONGLONG llA, SHORT sB)
{
  LONGLONG llSecond;

  if (sB >= 0)
     {
     llSecond.ulLo = sB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -sB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iMod(llA, llSecond);
}

LONGLONG _cdecl iModUL(LONGLONG llA, ULONG ulB)
{
  LONGLONG llSecond;

  llSecond.ulLo = ulB;
  llSecond.ulHi = 0;

  return iMod(llA, llSecond);
}

LONGLONG _cdecl iModUS(LONGLONG llA, USHORT usB)
{
  LONGLONG llSecond;

  llSecond.ulLo = usB;
  llSecond.ulHi = 0;

  return iMod(llA, llSecond);
}

ULONGLONG _cdecl Mul(ULONGLONG ullA, ULONGLONG ullB)
{
  ULONGLONG  ullRet;
  PULONGLONG pullRet = &ullRet;
  PULONGLONG pullA = &ullA;
  PULONGLONG pullB = &ullB;

  _asm {
    push es
    push si

    les si, pullA

    mov  dx, word ptr es:[si]
    mov  cx, word ptr es:[si + 2]
    mov  bx, word ptr es:[si + 4]
    mov  ax, word ptr es:[si + 6]

    les si, pullB

    push word ptr es:[si + 6]
    push word ptr es:[si + 4]
    push word ptr es:[si + 2]
    push word ptr es:[si]
    mov  si, sp

    call _U8M
    add  sp, 8

    les si, pullRet

    mov  word ptr es:[si], dx
    mov  word ptr es:[si + 2], cx
    mov  word ptr es:[si + 4], bx
    mov  word ptr es:[si + 6], ax

    pop  si
    pop  es
  }

  return ullRet;
}

ULONGLONG _cdecl MulUL(ULONGLONG ullA, ULONG ulB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = ulB;
  ullSecond.ulHi = 0;

  return Mul(ullA, ullSecond);
}

ULONGLONG _cdecl MulUS(ULONGLONG ullA, USHORT usB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = usB;
  ullSecond.ulHi = 0;

  return Mul(ullA, ullSecond);
}

LONGLONG _cdecl iMul(LONGLONG llA, LONGLONG llB)
{
  LONGLONG  llRet;
  PLONGLONG pllRet = &llRet;
  PLONGLONG pllA = &llA;
  PLONGLONG pllB = &llB;

  _asm {
    push es
    push si

    les si, pllA

    mov  dx, word ptr es:[si]
    mov  cx, word ptr es:[si + 2]
    mov  bx, word ptr es:[si + 4]
    mov  ax, word ptr es:[si + 6]

    les si, pllB

    push word ptr es:[si + 6]
    push word ptr es:[si + 4]
    push word ptr es:[si + 2]
    push word ptr es:[si]
    mov  si, sp

    call _I8M
    add  sp, 8

    les si, pllRet

    mov  word ptr es:[si], dx
    mov  word ptr es:[si + 2], cx
    mov  word ptr es:[si + 4], bx
    mov  word ptr es:[si + 6], ax

    pop  si
    pop  es
  }

  return llRet;
}

LONGLONG _cdecl iMulL(LONGLONG llA, LONG lB)
{
  LONGLONG llSecond;

  if (lB >= 0)
     {
     llSecond.ulLo = lB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -lB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iMul(llA, llSecond);
}

LONGLONG _cdecl iMulS(LONGLONG llA, SHORT sB)
{
  LONGLONG llSecond;

  if (sB >= 0)
     {
     llSecond.ulLo = sB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -sB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iMul(llA, llSecond);
}

LONGLONG _cdecl iMulUL(LONGLONG llA, ULONG ulB)
{
  LONGLONG llSecond;

  llSecond.ulLo = ulB;
  llSecond.ulHi = 0;

  return iMul(llA, llSecond);
}

LONGLONG _cdecl iMulUS(LONGLONG llA, USHORT usB)
{
  LONGLONG llSecond;

  llSecond.ulLo = usB;
  llSecond.ulHi = 0;

  return iMul(llA, llSecond);
}

ULONGLONG _cdecl Add(ULONGLONG ullA, ULONGLONG ullB)
{
  ULONGLONG  ullRet;
  PULONGLONG pullRet = &ullRet;
  PULONGLONG pullA = &ullA;
  PULONGLONG pullB = &ullB;

  _asm {
    push es
    push si

    les si, pullA

    mov  dx, word ptr es:[si]
    mov  cx, word ptr es:[si + 2]
    mov  bx, word ptr es:[si + 4]
    mov  ax, word ptr es:[si + 6]

    les si, pullB

    push word ptr es:[si + 6]
    push word ptr es:[si + 4]
    push word ptr es:[si + 2]
    push word ptr es:[si]
    mov  si, sp

    add  dx, word ptr ss:[si]
    adc  cx, word ptr ss:[si + 2]
    adc  bx, word ptr ss:[si + 4]
    adc  ax, word ptr ss:[si + 6]

    add  sp, 8

    les si, pullRet

    mov  word ptr es:[si], dx
    mov  word ptr es:[si + 2], cx
    mov  word ptr es:[si + 4], bx
    mov  word ptr es:[si + 6], ax

    pop  si
    pop  es
  }

  return ullRet;
}

ULONGLONG _cdecl AddUL(ULONGLONG ullA, ULONG ulB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = ulB;
  ullSecond.ulHi = 0;

  return Add(ullA, ullSecond);
}

ULONGLONG _cdecl AddUS(ULONGLONG ullA, USHORT usB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = usB;
  ullSecond.ulHi = 0;

  return Add(ullA, ullSecond);
}

ULONGLONG _cdecl Sub(ULONGLONG ullA, ULONGLONG ullB)
{
  ULONGLONG  ullRet;
  PULONGLONG pullRet = &ullRet;
  PULONGLONG pullA = &ullA;
  PULONGLONG pullB = &ullB;

  _asm {
    push es
    push si

    les si, pullA

    mov  dx, word ptr es:[si]
    mov  cx, word ptr es:[si + 2]
    mov  bx, word ptr es:[si + 4]
    mov  ax, word ptr es:[si + 6]

    les si, pullB

    push word ptr es:[si + 6]
    push word ptr es:[si + 4]
    push word ptr es:[si + 2]
    push word ptr es:[si]
    mov  si, sp

    sub  dx, word ptr ss:[si]
    sbb  cx, word ptr ss:[si + 2]
    sbb  bx, word ptr ss:[si + 4]
    sbb  ax, word ptr ss:[si + 6]

    add  sp, 8

    les si, pullRet

    mov  word ptr es:[si], dx
    mov  word ptr es:[si + 2], cx
    mov  word ptr es:[si + 4], bx
    mov  word ptr es:[si + 6], ax

    pop  si
    pop  es
  }

  return ullRet;
}

ULONGLONG _cdecl SubUL(ULONGLONG ullA, ULONG ulB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = ulB;
  ullSecond.ulHi = 0;

  return Sub(ullA, ullSecond);
}

ULONGLONG _cdecl SubUS(ULONGLONG ullA, USHORT usB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = usB;
  ullSecond.ulHi = 0;

  return Sub(ullA, ullSecond);
}

LONGLONG _cdecl iAdd(LONGLONG llA, LONGLONG llB)
{
  LONGLONG  llRet;
  PLONGLONG pllRet = &llRet;
  PLONGLONG pllA = &llA;
  PLONGLONG pllB = &llB;

  _asm {
    push es
    push si

    les si, pllA

    mov  dx, word ptr es:[si]
    mov  cx, word ptr es:[si + 2]
    mov  bx, word ptr es:[si + 4]
    mov  ax, word ptr es:[si + 6]

    les si, pllB

    push word ptr es:[si + 6]
    push word ptr es:[si + 4]
    push word ptr es:[si + 2]
    push word ptr es:[si]
    mov  si, sp

    add  dx, word ptr ss:[si]
    adc  cx, word ptr ss:[si + 2]
    adc  bx, word ptr ss:[si + 4]
    adc  ax, word ptr ss:[si + 6]

    add  sp, 8

    les si, pllRet

    mov  word ptr es:[si], dx
    mov  word ptr es:[si + 2], cx
    mov  word ptr es:[si + 4], bx
    mov  word ptr es:[si + 6], ax

    pop  si
    pop  es
  }

  return llRet;
}

LONGLONG _cdecl iAddL(LONGLONG llA, LONG lB)
{
  LONGLONG llSecond;

  if (lB >= 0)
     {
     llSecond.ulLo = lB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -lB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iAdd(llA, llSecond);
}

LONGLONG _cdecl iAddS(LONGLONG llA, SHORT sB)
{
  LONGLONG llSecond;

  if (sB >= 0)
     {
     llSecond.ulLo = sB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -sB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iAdd(llA, llSecond);
}

LONGLONG _cdecl iAddUL(LONGLONG llA, ULONG ulB)
{
  LONGLONG llSecond;

  llSecond.ulLo = ulB;
  llSecond.ulHi = 0;

  return iAdd(llA, llSecond);
}

LONGLONG _cdecl iAddUS(LONGLONG llA, USHORT usB)
{
  LONGLONG llSecond;

  llSecond.ulLo = usB;
  llSecond.ulHi = 0;

  return iAdd(llA, llSecond);
}

LONGLONG _cdecl iSub(LONGLONG llA, LONGLONG llB)
{
  LONGLONG  llRet;
  PLONGLONG pllRet = &llRet;
  PLONGLONG pllA = &llA;
  PLONGLONG pllB = &llB;

  _asm {
    push es
    push si

    les si, pllA

    mov  dx, word ptr es:[si]
    mov  cx, word ptr es:[si + 2]
    mov  bx, word ptr es:[si + 4]
    mov  ax, word ptr es:[si + 6]

    les si, pllB

    push word ptr es:[si + 6]
    push word ptr es:[si + 4]
    push word ptr es:[si + 2]
    push word ptr es:[si]
    mov  si, sp

    sub  dx, word ptr ss:[si]
    sbb  cx, word ptr ss:[si + 2]
    sbb  bx, word ptr ss:[si + 4]
    sbb  ax, word ptr ss:[si + 6]

    add  sp, 8

    les si, pllRet

    mov  word ptr es:[si], dx
    mov  word ptr es:[si + 2], cx
    mov  word ptr es:[si + 4], bx
    mov  word ptr es:[si + 6], ax

    pop  si
    pop  es
  }

  return llRet;
}

LONGLONG _cdecl iSubL(LONGLONG llA, LONG lB)
{
  LONGLONG llSecond;

  if (lB >= 0)
     {
     llSecond.ulLo = lB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -lB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iSub(llA, llSecond);
}

LONGLONG _cdecl iSubS(LONGLONG llA, SHORT sB)
{
  LONGLONG llSecond;

  if (sB >= 0)
     {
     llSecond.ulLo = sB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -sB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iSub(llA, llSecond);
}

LONGLONG _cdecl iSubUL(LONGLONG llA, ULONG ulB)
{
  LONGLONG llSecond;

  llSecond.ulLo = ulB;
  llSecond.ulHi = 0;

  return iSub(llA, llSecond);
}

LONGLONG _cdecl iSubUS(LONGLONG llA, USHORT usB)
{
  LONGLONG llSecond;

  llSecond.ulLo = usB;
  llSecond.ulHi = 0;

  return iSub(llA, llSecond);
}

BOOL _cdecl Less(ULONGLONG ullA, ULONGLONG ullB)
{
  if (Sub(ullA, ullB).ulHi & 0x80000000)
    return TRUE;
  else
    return FALSE;
}

BOOL _cdecl iLess(LONGLONG llA, LONGLONG llB)
{
  if (iSub(llA, llB).ulHi & 0x80000000)
    return TRUE;
  else
    return FALSE;
}

BOOL _cdecl LessE(ULONGLONG ullA, ULONGLONG ullB)
{
  return !Greater(ullA, ullB);
}

BOOL _cdecl iLessE(LONGLONG llA, LONGLONG llB)
{
  return !iGreater(llA, llB);
}

BOOL _cdecl LessUL(ULONGLONG ullA, ULONG ulB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = ulB;
  ullSecond.ulHi = 0;

  return Less(ullA, ullSecond);
}

BOOL _cdecl LessUS(ULONGLONG ullA, USHORT usB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = usB;
  ullSecond.ulHi = 0;

  return Less(ullA, ullSecond);
}

BOOL _cdecl iLessL(LONGLONG llA, LONG lB)
{
  LONGLONG llSecond;

  if (lB >= 0)
     {
     llSecond.ulLo = lB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -lB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iLess(llA, llSecond);
}

BOOL _cdecl iLessS(LONGLONG llA, SHORT sB)
{
  LONGLONG llSecond;

  if (sB >= 0)
     {
     llSecond.ulLo = sB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -sB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iLess(llA, llSecond);
}

BOOL _cdecl iLessUL(LONGLONG llA, ULONG ulB)
{
  LONGLONG llSecond;

  llSecond.ulLo = ulB;
  llSecond.ulHi = 0;

  return iLess(llA, llSecond);
}

BOOL _cdecl iLessUS(LONGLONG llA, USHORT usB)
{
  LONGLONG llSecond;

  llSecond.ulLo = usB;
  llSecond.ulHi = 0;

  return iLess(llA, llSecond);
}

BOOL _cdecl LessEUL(ULONGLONG ullA, ULONG ulB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = ulB;
  ullSecond.ulHi = 0;

  return LessE(ullA, ullSecond);
}

BOOL _cdecl LessEUS(ULONGLONG ullA, USHORT usB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = usB;
  ullSecond.ulHi = 0;

  return LessE(ullA, ullSecond);
}

BOOL _cdecl iLessEL(LONGLONG llA, LONG lB)
{
  LONGLONG llSecond;

  if (lB >= 0)
     {
     llSecond.ulLo = lB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -lB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iLessE(llA, llSecond);
}

BOOL _cdecl iLessES(LONGLONG llA, SHORT sB)
{
  LONGLONG llSecond;

  if (sB >= 0)
     {
     llSecond.ulLo = sB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -sB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iLessE(llA, llSecond);
}

BOOL _cdecl iLessEUL(LONGLONG llA, ULONG ulB)
{
  LONGLONG llSecond;

  llSecond.ulLo = ulB;
  llSecond.ulHi = 0;

  return iLessE(llA, llSecond);
}

BOOL _cdecl iLessEUS(LONGLONG llA, USHORT usB)
{
  LONGLONG llSecond;

  llSecond.ulLo = usB;
  llSecond.ulHi = 0;

  return iLessE(llA, llSecond);
}

BOOL _cdecl Greater(ULONGLONG ullA, ULONGLONG ullB)
{
  ULONGLONG ullC = Sub(ullB, ullA);

  if (ullC.ulHi & 0x80000000)
    return TRUE;
  else
    return FALSE;
}

BOOL _cdecl iGreater(LONGLONG llA, LONGLONG llB)
{
  if (iSub(llB, llA).ulHi & 0x80000000)
    return TRUE;
  else
    return FALSE;
}

BOOL _cdecl GreaterE(ULONGLONG ullA, ULONGLONG ullB)
{
  return !Less(ullA, ullB);
}

BOOL _cdecl iGreaterE(LONGLONG llA, LONGLONG llB)
{
  return !iLess(llA, llB);
}

BOOL _cdecl GreaterUL(ULONGLONG ullA, ULONG ulB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = ulB;
  ullSecond.ulHi = 0;

  return Greater(ullA, ullSecond);
}

BOOL _cdecl GreaterUS(ULONGLONG ullA, USHORT usB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = usB;
  ullSecond.ulHi = 0;

  return Greater(ullA, ullSecond);
}

BOOL _cdecl iGreaterL(LONGLONG llA, LONG lB)
{
  LONGLONG llSecond;

  if (lB >= 0)
     {
     llSecond.ulLo = lB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -lB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iGreater(llA, llSecond);
}

BOOL _cdecl iGreaterS(LONGLONG llA, SHORT sB)
{
  LONGLONG llSecond;

  if (sB >= 0)
     {
     llSecond.ulLo = sB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -sB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iGreater(llA, llSecond);
}

BOOL _cdecl iGreaterUL(LONGLONG llA, ULONG ulB)
{
  LONGLONG llSecond;

  llSecond.ulLo = ulB;
  llSecond.ulHi = 0;

  return iGreater(llA, llSecond);
}

BOOL _cdecl iGreaterUS(LONGLONG llA, USHORT usB)
{
  LONGLONG llSecond;

  llSecond.ulLo = usB;
  llSecond.ulHi = 0;

  return iGreater(llA, llSecond);
}

BOOL _cdecl GreaterEUL(ULONGLONG ullA, ULONG ulB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = ulB;
  ullSecond.ulHi = 0;

  return GreaterE(ullA, ullSecond);
}

BOOL _cdecl GreaterEUS(ULONGLONG ullA, USHORT usB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = usB;
  ullSecond.ulHi = 0;

  return GreaterE(ullA, ullSecond);
}

BOOL _cdecl iGreaterEL(LONGLONG llA, LONG lB)
{
  LONGLONG llSecond;

  if (lB >= 0)
     {
     llSecond.ulLo = lB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -lB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iGreaterE(llA, llSecond);
}

BOOL _cdecl iGreaterES(LONGLONG llA, SHORT sB)
{
  LONGLONG llSecond;

  if (sB >= 0)
     {
     llSecond.ulLo = sB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -sB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iGreaterE(llA, llSecond);
}

BOOL _cdecl iGreaterEUL(LONGLONG llA, ULONG ulB)
{
  LONGLONG llSecond;

  llSecond.ulLo = ulB;
  llSecond.ulHi = 0;

  return iGreaterE(llA, llSecond);
}

BOOL _cdecl iGreaterEUS(LONGLONG llA, USHORT usB)
{
  LONGLONG llSecond;

  llSecond.ulLo = usB;
  llSecond.ulHi = 0;

  return iGreaterE(llA, llSecond);
}

BOOL _cdecl Eq(ULONGLONG ullA, ULONGLONG ullB)
{
  ULONGLONG ullDiff;

  ullDiff = Sub(ullA, ullB);

  if (!ullDiff.ulHi && !ullDiff.ulLo)
    return TRUE;
  else
    return FALSE;
}

BOOL _cdecl EqUL(ULONGLONG ullA, ULONG ulB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = ulB;
  ullSecond.ulHi = 0;

  return Eq(ullA, ullSecond);
}

BOOL _cdecl EqUS(ULONGLONG ullA, USHORT usB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = usB;
  ullSecond.ulHi = 0;

  return Eq(ullA, ullSecond);
}

BOOL _cdecl iEq(LONGLONG llA, LONGLONG llB)
{
  LONGLONG llDiff;

  llDiff = iSub(llA, llB);

  if (!llDiff.ulHi && !llDiff.ulLo)
    return TRUE;
  else
    return FALSE;
}

BOOL _cdecl iEqL(LONGLONG llA, LONG lB)
{
  LONGLONG llSecond;

  if (lB >= 0)
     {
     llSecond.ulLo = lB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -lB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iEq(llA, llSecond);
}

BOOL _cdecl iEqS(LONGLONG llA, SHORT sB)
{
  LONGLONG llSecond;

  if (sB >= 0)
     {
     llSecond.ulLo = sB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -sB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iEq(llA, llSecond);
}

BOOL _cdecl iEqUL(LONGLONG llA, ULONG ulB)
{
  LONGLONG llSecond;

  llSecond.ulLo = ulB;
  llSecond.ulHi = 0;

  return iEq(llA, llSecond);
}

BOOL _cdecl iEqUS(LONGLONG llA, USHORT usB)
{
  LONGLONG llSecond;

  llSecond.ulLo = usB;
  llSecond.ulHi = 0;

  return iEq(llA, llSecond);
}

BOOL _cdecl Neq(ULONGLONG ullA, ULONGLONG ullB)
{
  return !Eq(ullA, ullB);
}

BOOL _cdecl NeqUL(ULONGLONG ullA, ULONG ulB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = ulB;
  ullSecond.ulHi = 0;

  return Neq(ullA, ullSecond);
}

BOOL _cdecl NeqUS(ULONGLONG ullA, USHORT usB)
{
  ULONGLONG ullSecond;

  ullSecond.ulLo = usB;
  ullSecond.ulHi = 0;

  return Neq(ullA, ullSecond);
}

BOOL _cdecl iNeq(LONGLONG llA, LONGLONG llB)
{
  return !iEq(llA, llB);
}

BOOL _cdecl iNeqL(LONGLONG llA, LONG lB)
{
  LONGLONG llSecond;

  if (lB >= 0)
     {
     llSecond.ulLo = lB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -lB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iNeq(llA, llSecond);
}

BOOL _cdecl iNeqS(LONGLONG llA, SHORT sB)
{
  LONGLONG llSecond;

  if (sB >= 0)
     {
     llSecond.ulLo = sB;
     llSecond.ulHi = 0;
     }
  else
     {
     LONGLONG llZero;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -sB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     }

  return iNeq(llA, llSecond);
}

BOOL _cdecl iNeqUL(LONGLONG llA, ULONG ulB)
{
  LONGLONG llSecond;

  llSecond.ulLo = ulB;
  llSecond.ulHi = 0;

  return iNeq(llA, llSecond);
}

BOOL _cdecl iNeqUS(LONGLONG llA, USHORT usB)
{
  LONGLONG llSecond;

  llSecond.ulLo = usB;
  llSecond.ulHi = 0;

  return iNeq(llA, llSecond);
}

void _cdecl Assign(PULONGLONG pullA, ULONGLONG ullB)
{
  pullA->ulLo = ullB.ulLo;
  pullA->ulHi = ullB.ulHi;
}

void _cdecl AssignUL(PULONGLONG pullA, ULONG ulB)
{
  pullA->ulLo = ulB;
  pullA->ulHi = 0;
}

void _cdecl AssignUS(PULONGLONG pullA, USHORT usB)
{
  pullA->ulLo = usB;
  pullA->ulHi = 0;
}

void _cdecl iAssign(PLONGLONG pllA, LONGLONG llB)
{
  pllA->ulLo = llB.ulLo;
  pllA->ulHi = llB.ulHi;
}

void _cdecl iAssignL(PLONGLONG pllA, LONG lB)
{
  if (lB >= 0)
     {
     pllA->ulLo = lB;
     pllA->ulHi = 0;
     }
  else
     {
     LONGLONG llZero, llSecond;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -lB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     pllA->ulLo = llSecond.ulLo;
     pllA->ulHi = llSecond.ulHi;
     }
}

void _cdecl iAssignS(PLONGLONG pllA, SHORT sB)
{
  if (sB >= 0)
     {
     pllA->ulLo = sB;
     pllA->ulHi = 0;
     }
  else
     {
     LONGLONG llZero, llSecond;
     llZero.ulLo = 0;
     llZero.ulHi = 0;
     llSecond.ulLo = -sB;
     llSecond.ulHi = 0;
     llSecond = iSub(llZero, llSecond);
     pllA->ulLo = llSecond.ulLo;
     pllA->ulHi = llSecond.ulHi;
     }
}

void _cdecl iAssignUL(PLONGLONG pllA, ULONG ulB)
{
  pllA->ulLo = ulB;
  pllA->ulHi = 0;
}

void _cdecl iAssignUS(PLONGLONG pllA, USHORT usB)
{
  pllA->ulLo = usB;
  pllA->ulHi = 0;
}

#endif
