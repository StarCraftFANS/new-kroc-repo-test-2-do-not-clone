%module playerconfig
%{
#include "playerconfig.h"
%}

/* The following macros are replaced by configure with the right values
 * for the build system.  That way the installed version of this header
 * will be usable by anyone.  They MUST not be changed here, or else
 * configure will not replace them with the correct values. */
/*****************************************************************************/
/* DO NOT TOUCH THE MACROS BELOW */
/*****************************************************************************/
#define HAVE_STDINT_H 1
#define HAVE_STRINGS_H 1
/* #undef WORDS_BIGENDIAN */
#define PLAYER_BIG_MESSAGES 1
/*****************************************************************************/
/* DO NOT TOUCH THE MACROS ABOVE */
/*****************************************************************************/

/* the largest possible message that the server will currently send
 * or receive */
#ifdef PLAYER_BIG_MESSAGES
#define PLAYER_MAX_MESSAGE_SIZE 2097152 /*2MB*/
#else
#define PLAYER_MAX_MESSAGE_SIZE 8192 /*8KB*/
#endif

/* make sure we get the various types like 'uint8_t
 *
 * int8_t  : signed 1 byte  (char)
 * int16_t : signed 2 bytes (short)
 * int32_t : signed 4 bytes (int)
 * int64_t : signed 8 bytes (long)
 *
 * uint8_t  : unsigned 1 byte  (unsigned char)
 * uint16_t : unsigned 2 bytes (unsigned short)
 * uint32_t : unsigned 4 bytes (unsigned int)
 * uint64_t : unsigned 8 bytes (unsigned long)
 */

#include <sys/types.h>
#if HAVE_STDINT_H
  #include <stdint.h>
#endif

/*
 * 64-bit conversion macros
 */
#if WORDS_BIGENDIAN
  #define htonll(n) (n)
#else
  #define htonll(n) ((((unsigned long long) htonl(n)) << 32) + htonl((n) >> 32))
#endif

#if WORDS_BIGENDIAN
  #define ntohll(n) (n)
#else
  #define ntohll(n) ((((unsigned long long)ntohl(n)) << 32) + ntohl((n) >> 32))
#endif
