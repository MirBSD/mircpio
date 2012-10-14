# $MirOS: src/bin/pax/Makefile,v 1.14 2012/10/14 16:24:18 tg Exp $
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
MAN=	cpio.1 pax.1 tar.1
LINKS+=	${BINDIR}/pax ${BINDIR}/cpio
LINKS+=	${BINDIR}/pax ${BINDIR}/tar

.if (${MACHINE_OS} == "Interix") || (${MACHINE_OS} == "Linux") || \
    ((${MACHINE_OS} == "GNU") && (${OSNAME} != "GNU/kFreeBSD"))
CPPFLAGS+= -DLONG_OFF_T
.endif

.if (${MACHINE_OS} == "BSD")
CPPFLAGS+= -DHAVE_STRLCPY
CPPFLAGS+= -DHAVE_STRMODE
CPPFLAGS+= -DHAVE_VIS
.endif

.include <bsd.prog.mk>

CLEANFILES+=	${MANALL:S/.cat/.ps/} ${MAN:S/$/.pdf/} ${MANALL:S/$/.gz/}
CLEANFILES+=	${MAN:S/$/.htm/} ${MAN:S/$/.htm.gz/}
CLEANFILES+=	${MAN:S/$/.txt/} ${MAN:S/$/.txt.gz/}
cats: ${MANALL} ${MANALL:S/.cat/.ps/}
.if "${MANALL:Ncpio.cat1:Npax.cat1:Ntar.cat1}" != ""
.  error Adjust here.
.endif
	x=$$(ident ${.CURDIR:Q}/cpio.1 | \
	    awk '/MirOS:/ { print $$4$$5; }' | \
	    tr -dc 0-9); (( $${#x} == 14 )) || exit 1; exec \
	    ${MKSH} ${BSDSRCDIR:Q}/contrib/hosted/tg/ps2pdfmir -c \
	    -o cpio.1.pdf '[' /Author '(The MirOS Project)' \
	    /Title '(paxcpio - copy file archives in and out)' \
	    /Subject '(BSD Reference Manual)' /ModDate "(D:$$x)" \
	    /Creator '(GNU groff version 1.19.2-3 \(MirPorts\))' \
	    /Producer '(Artifex Ghostscript 8.54-3 \(MirPorts\))' \
	    /Keywords '(cpio, pax, tar)' /DOCINFO pdfmark \
	    -f cpio.ps1
	x=$$(ident ${.CURDIR:Q}/pax.1 | \
	    awk '/MirOS:/ { print $$4$$5; }' | \
	    tr -dc 0-9); (( $${#x} == 14 )) || exit 1; exec \
	    ${MKSH} ${BSDSRCDIR:Q}/contrib/hosted/tg/ps2pdfmir -c \
	    -o pax.1.pdf '[' /Author '(The MirOS Project)' \
	    /Title '(pax - read and write file archives and copy directory hierarchies)' \
	    /Subject '(BSD Reference Manual)' /ModDate "(D:$$x)" \
	    /Creator '(GNU groff version 1.19.2-3 \(MirPorts\))' \
	    /Producer '(Artifex Ghostscript 8.54-3 \(MirPorts\))' \
	    /Keywords '(cpio, pax, tar)' /DOCINFO pdfmark \
	    -f pax.ps1
	x=$$(ident ${.CURDIR:Q}/tar.1 | \
	    awk '/MirOS:/ { print $$4$$5; }' | \
	    tr -dc 0-9); (( $${#x} == 14 )) || exit 1; exec \
	    ${MKSH} ${BSDSRCDIR:Q}/contrib/hosted/tg/ps2pdfmir -c \
	    -o tar.1.pdf '[' /Author '(The MirOS Project)' \
	    /Title '(paxtar - Unix tape archiver)' \
	    /Subject '(BSD Reference Manual)' /ModDate "(D:$$x)" \
	    /Creator '(GNU groff version 1.19.2-3 \(MirPorts\))' \
	    /Producer '(Artifex Ghostscript 8.54-3 \(MirPorts\))' \
	    /Keywords '(cpio, pax, tar)' /DOCINFO pdfmark \
	    -f tar.ps1
	set -e; . ${BSDSRCDIR:Q}/scripts/roff2htm; set_target_absolute; \
	    for m in ${MANALL}; do \
		bn=$${m%.*}; ext=$${m##*.cat}; \
		[[ $$bn != $$m ]]; [[ $$ext != $$m ]]; \
		gzip -n9 <"$$m" >"$$m.gz"; \
		col -bx <"$$m" >"$$bn.$$ext.txt"; \
		rm -f "$$bn.$$ext.txt.gz"; gzip -n9 "$$bn.$$ext.txt"; \
		do_conversion_verbose "$$bn" "$$ext" "$$m" "$$bn.$$ext.htm"; \
		rm -f "$$bn.$$ext.htm.gz"; gzip -n9 "$$bn.$$ext.htm"; \
	done
