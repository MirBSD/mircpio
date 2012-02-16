# $MirOS: src/bin/pax/Makefile,v 1.9 2012/02/16 17:48:06 tg Exp $
# $OpenBSD: Makefile,v 1.10 2001/05/26 00:32:20 millert Exp $
#-
# It may be necessary to define some options on pre-4.4BSD or
# other operating systems:
#
# -DLONG_OFF_T	The base type of off_t is a long, not a long long.
#		This is often defined in: /usr/include/sys/types.h

PROG=   pax
SRCS=	ar.c ar_io.c ar_subs.c buf_subs.c cache.c cpio.c file_subs.c ftree.c \
	gen_subs.c getoldopt.c options.c pat_rep.c pax.c sel_subs.c tables.c \
	tar.c tty_subs.c
MAN=	pax.1 tar.1 cpio.1
LINKS=	${BINDIR}/pax ${BINDIR}/tar \
	${BINDIR}/pax ${BINDIR}/cpio

.if (${MACHINE_OS} == "Interix") || (${MACHINE_OS} == "Linux") || \
    ((${MACHINE_OS} == "GNU") && (${OSNAME} != "GNU/kFreeBSD"))
CPPFLAGS+= -DLONG_OFF_T
.endif

.include <bsd.prog.mk>
