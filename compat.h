/*-
 * Copyright © 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
 *	       2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
 *	mirabilos <m@mirbsd.org>
 * Copyright © 2018
 *	mirabilos <t.glaser@tarent.de>
 *
 * Provided that these terms and disclaimer and all copyright notices
 * are retained or reproduced in an accompanying document, permission
 * is granted to deal in this work without restriction, including un‐
 * limited rights to use, publicly perform, distribute, sell, modify,
 * merge, give away, or sublicence.
 *
 * This work is provided “AS IS” and WITHOUT WARRANTY of any kind, to
 * the utmost extent permitted by applicable law, neither express nor
 * implied; without malicious intent or gross negligence. In no event
 * may a licensor, author or contributor be held liable for indirect,
 * direct, other damage, loss, or other issues arising in any way out
 * of dealing in the work, even if advised of the possibility of such
 * damage or existence of a defect, except proven that it results out
 * of said person’s immediate fault when using the work as intended.
 */

#ifndef PAX_COMPAT_H
#define PAX_COMPAT_H

#if HAVE_SYS_MKDEV_H
#include <sys/mkdev.h>
#endif
#if HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif
#if HAVE_STDINT_H
#include <stdint.h>
#endif

#undef __attribute__
#if HAVE_ATTRIBUTE_BOUNDED
#define MKSH_A_BOUNDED(x,y,z)	__attribute__((__bounded__(x, y, z)))
#else
#define MKSH_A_BOUNDED(x,y,z)	/* nothing */
#endif
#if HAVE_ATTRIBUTE_FORMAT
#define MKSH_A_FORMAT(x,y,z)	__attribute__((__format__(x, y, z)))
#else
#define MKSH_A_FORMAT(x,y,z)	/* nothing */
#endif
#if HAVE_ATTRIBUTE_NONNULL
#define MKSH_A_NONNULL(x)	__attribute__((__nonnull__(x)))
#else
#define MKSH_A_NONNULL(x)	/* nothing */
#endif
#if HAVE_ATTRIBUTE_NORETURN
#define MKSH_A_NORETURN		__attribute__((__noreturn__))
#else
#define MKSH_A_NORETURN		/* nothing */
#endif
#if HAVE_ATTRIBUTE_PURE
#define MKSH_A_PURE		__attribute__((__pure__))
#else
#define MKSH_A_PURE		/* nothing */
#endif
#if HAVE_ATTRIBUTE_UNUSED
#define MKSH_A_UNUSED		__attribute__((__unused__))
#else
#define MKSH_A_UNUSED		/* nothing */
#endif
#if HAVE_ATTRIBUTE_USED
#define MKSH_A_USED		__attribute__((__used__))
#else
#define MKSH_A_USED		/* nothing */
#endif

#if defined(MirBSD) && (MirBSD >= 0x09A1) && \
    defined(__ELF__) && defined(__GNUC__) && \
    !defined(__llvm__) && !defined(__NWCC__)
/*
 * We got usable __IDSTRING __COPYRIGHT __RCSID __SCCSID macros
 * which work for all cases; no need to redefine them using the
 * "portable" macros from below when we might have the "better"
 * gcc+ELF specific macros or other system dependent ones.
 */
#else
#undef __IDSTRING
#undef __IDSTRING_CONCAT
#undef __IDSTRING_EXPAND
#undef __COPYRIGHT
#undef __RCSID
#undef __SCCSID
#define __IDSTRING_CONCAT(l,p)		__LINTED__ ## l ## _ ## p
#define __IDSTRING_EXPAND(l,p)		__IDSTRING_CONCAT(l,p)
#ifdef MKSH_DONT_EMIT_IDSTRING
#define __IDSTRING(prefix, string)	/* nothing */
#else
#define __IDSTRING(prefix, string)				\
	static const char __IDSTRING_EXPAND(__LINE__,prefix) []	\
	    MKSH_A_USED = "@(""#)" #prefix ": " string
#endif
#define __COPYRIGHT(x)		__IDSTRING(copyright,x)
#define __RCSID(x)		__IDSTRING(rcsid,x)
#define __SCCSID(x)		__IDSTRING(sccsid,x)
#endif

#ifdef EXTERN
__IDSTRING(rcsid_compat_h, "$MirOS: src/bin/pax/compat.h,v 1.1.2.4 2018/12/12 06:53:12 tg Exp $");
#endif

/* arithmetic types: C implementation */
#if !HAVE_CAN_INTTYPES
#if !HAVE_CAN_UCBINTS
typedef signed int int32_t;
typedef unsigned int uint32_t;
#else
typedef u_int32_t uint32_t;
#endif
#endif
#if !HAVE_CAN_INT16TYPE
#if !HAVE_CAN_UCBINT16
typedef unsigned short int uint16_t;
#else
typedef u_int16_t uint16_t;
#endif
#endif

#ifdef MKSH_TYPEDEF_SSIZE_T
typedef MKSH_TYPEDEF_SSIZE_T ssize_t;
#endif

#if HAVE_ST_MTIM
#define st_timecmp(x,sbpa,sbpb,op) \
	timespeccmp(&(sbpa)->st_ ## x ## tim, &(sbpb)->st_ ## x ## tim, op)
#define st_timecpy(x,sbpd,sbps) do {					\
	(sbpd)->st_ ## x ## tim = (sbps)->st_ ## x ## tim;		\
} while (/* CONSTCOND */ 0)
#elif HAVE_ST_MTIMENSEC
#define st_timecmp(x,sbpa,sbpb,op) ( \
	((sbpa)->st_ ## x ## time == (sbpb)->st_ ## x ## time) ? \
	    ((sbpa)->st_ ## x ## timensec op (sbpb)->st_ ## x ## timensec) : \
	    ((sbpa)->st_ ## x ## time op (sbpb)->st_ ## x ## time) \
	)
#define st_timecpy(x,sbpd,sbps) do {					\
	(sbpd)->st_ ## x ## time = (sbps)->st_ ## x ## time;		\
	(sbpd)->st_ ## x ## timensec = (sbps)->st_ ## x ## timensec;	\
} while (/* CONSTCOND */ 0)
#else
#define st_timecmp(x,sbpa,sbpb,op) \
	((sbpa)->st_ ## x ## time op (sbpb)->st_ ## x ## time)
#define st_timecpy(x,sbpd,sbps) do {					\
	(sbpd)->st_ ## x ## time = (sbps)->st_ ## x ## time;		\
} while (/* CONSTCOND */ 0)
#endif

/* compat.c */

#if !HAVE_DPRINTF
/* replacement only as powerful as needed for this */
void dprintf(int, const char *, ...)
    MKSH_A_NONNULL(2)
    MKSH_A_FORMAT(__printf__, 2, 3);
#endif

#if !HAVE_STRLCPY
size_t strlcat(char *, const char *, size_t)
    MKSH_A_BOUNDED(__string__, 1, 3);
size_t strlcpy(char *, const char *, size_t)
    MKSH_A_BOUNDED(__string__, 1, 3);
#endif

#if !HAVE_STRMODE
void strmode(mode_t, char *);
#endif

#endif
