char _far *strcpy(char _far *s, const char _far *t )
{
    char _far *dst;

    dst = s;
    while( *dst++ = *t++ )
        ;
    return( s );
}

void _far *memcpy(void _far *in_dst, void _far *in_src, int len)
{
    char _far *dst = in_dst;
    const char _far *src = in_src;

    for( ; len; --len ) {
        *dst++ = *src++;
    }
    return( in_dst );
}

void _far *memset(void _far *dst, char c, int len)
{
    char _far *p;

    for( p = dst; len; --len ) {
        *p++ = c;
    }
    return( dst );
}

int memcmp(void _far *in_s1, void _far *in_s2, int len)
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

int strlen(char _far *s)
{
    const char _far *p;

    p = s;
    while( *p != '\0' )
        ++p;
    return( p - s );
}

char _far *strlwr(char _far *s)
{
    char _far *t = s;

    while( *t )
    {
        if (*t >= 'A' && *t <= 'Z')
        {
            *t -= 0x20;
        }
        t++;
    }

    return s;
}

#define memeq( p1, p2, len )    ( memcmp((p1),(p2),(len)) == 0 )

void _far *memchr( const void _far *s, char c, int n )
{
    const char _far *cs = s;

    while( n ) {
        if( *cs == c ) {
            return( (void _far *)cs );
        }
        ++cs;
        --n;
    }
    return( 0 );
}

char far *strchr(const char _far *s, int c)
{
    char cc = (char)c;
    do {
        if( *s == cc )
            return( (char far *)s );
    } while( *s++ != '\0' );
    return( 0 );
}

char _far *strstr(const char _far *s1, const char _far *s2)
{
    char _far *end_of_s1;
    int     s1len, s2len;

    if( s2[0] == '\0' ) {
        return( (char _far *)s1 );
    } else if( s2[1] == '\0' ) {
        return( strchr( s1, s2[0] ) );
    }
    end_of_s1 = memchr( s1, '\0', ~0 );
    s2len = strlen( (char far *)s2 );
    for( ;; ) {
        s1len = end_of_s1 - s1;
        if( s1len < s2len )
            break;
        s1 = memchr( s1, *s2, s1len );  /* find start of possible match */

        if( s1 == 0 )
            break;
        if( memeq( (void far *)s1, (void far *)s2, s2len ) )
            return( (char far *)s1 );
        ++s1;
    }
    return( 0 );
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
