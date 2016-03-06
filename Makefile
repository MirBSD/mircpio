# $MirOS: src/bin/pax/Makefile,v 1.18 2016/03/06 14:59:08 tg Exp $
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

.if (${MACHINE_OS} == "GNU") || (${MACHINE_OS} == "Linux")
CPPFLAGS+= -DHAVE_LINKAT # probably
.endif

.if (${MACHINE_OS} == "BSD")
CPPFLAGS+= -DHAVE_STRLCPY
CPPFLAGS+= -DHAVE_STRMODE
CPPFLAGS+= -DHAVE_VIS
.endif

.include <bsd.prog.mk>

.ifmake cats
V_GROFF!=	pkg_info -e 'groff-*'
V_GHOSTSCRIPT!=	pkg_info -e 'ghostscript-*'
.  if empty(V_GROFF) || empty(V_GHOSTSCRIPT)
.    error empty V_GROFF=${V_GROFF} or V_GHOSTSCRIPT=${V_GHOSTSCRIPT}
.  endif
.endif

CLEANFILES+=	${MANALL:S/.cat/.ps/} ${MAN:S/$/.pdf/} ${MANALL:S/$/.gz/}
CLEANFILES+=	${MAN:S/$/.htm/} ${MAN:S/$/.htm.gz/}
CLEANFILES+=	${MAN:S/$/.txt/} ${MAN:S/$/.txt.gz/}
CATS_KW=	cpio, pax, tar
CATS_TITLE_cpio_1=paxcpio - copy file archives in and out
CATS_TITLE_pax_1=pax - read and write file archives and copy directory hierarchies
CATS_TITLE_tar_1=paxtar - Unix tape archiver
cats: ${MANALL} ${MANALL:S/.cat/.ps/}
.if "${MANALL:Ncpio.cat1:Npax.cat1:Ntar.cat1}" != ""
.  error Adjust here.
.endif
.for _m _n in cpio 1 pax 1 tar 1
	x=$$(ident ${.CURDIR:Q}/${_m}.${_n} | \
	    awk '/MirOS:/ { print $$4$$5; }' | \
	    tr -dc 0-9); (( $${#x} == 14 )) || exit 1; exec \
	    ${MKSH} ${BSDSRCDIR:Q}/contrib/hosted/tg/ps2pdfmir -p pa4 -c \
	    -o ${_m}.${_n}.pdf '[' /Author '(The MirOS Project)' \
	    /Title '('${CATS_TITLE_${_m}_${_n}:Q}')' \
	    /Subject '(BSD Reference Manual)' /ModDate "(D:$$x)" \
	    /Creator '(GNU groff version ${V_GROFF:S/groff-//} \(MirPorts\))' \
	    /Producer '(Artifex Ghostscript ${V_GHOSTSCRIPT:S/ghostscript-//:S/-artifex//} \(MirPorts\))' \
	    /Keywords '('${CATS_KW:Q}')' /DOCINFO pdfmark \
	    -f ${_m}.ps${_n}
.endfor
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

# NetBSD®
NOMANDOC=	Yes
# OpenBSD
.if defined(MANLINT) && !empty(MANLINT)
all: use_nroff_instead
use_nroff_instead:
	@echo 'Install GNU groff or AT&T nroff to format *roff manpages!'
	@exit 1
.endif
