/*	$OpenBSD: options.c,v 1.67 2007/02/24 09:50:55 jmc Exp $	*/
/*	$NetBSD: options.c,v 1.6 1996/03/26 23:54:18 mrg Exp $	*/

/*-
 * Copyright (c) 2005, 2006, 2007 Thorsten Glaser <tg@mirbsd.org>
 * Copyright (c) 1992 Keith Muller.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <paths.h>
#include "pax.h"
#include "options.h"
#include "cpio.h"
#include "tar.h"
#include "ar.h"
#include "extern.h"

#if HAS_TAPE
#include <sys/mtio.h>
#endif

__SCCSID("@(#)options.c	8.2 (Berkeley) 4/18/94");
__RCSID("$MirOS: src/bin/pax/options.c,v 1.35 2011/08/16 21:32:47 tg Exp $");

#ifdef __GLIBC__
char *fgetln(FILE *, size_t *);
#endif

/*
 * Routines which handle command line options
 */

static char flgch[] = FLGCH;	/* list of all possible flags */
static OPLIST *ophead = NULL;	/* head for format specific options -x */
static OPLIST *optail = NULL;	/* option tail */

static int no_op(void);
static int no_op_i(int);
static void printflg(unsigned int);
static int c_frmt(const void *, const void *);
static off_t str_offt(char *);
static char *get_line(FILE *fp);
static void pax_options(int, char **);
static void pax_usage(void) __attribute__((__noreturn__));
static void tar_set_action(int);
static void tar_options(int, char **);
static void tar_usage(void) __attribute__((__noreturn__));
static void cpio_set_action(int);
static void cpio_options(int, char **);
static void cpio_usage(void) __attribute__((__noreturn__));
int mkpath(char *);

static void process_M(const char *, void (*)(void));

/* errors from get_line */
#define GET_LINE_FILE_CORRUPT 1
#define GET_LINE_OUT_OF_MEM 2
static int get_line_error;


#define GZIP_CMD	"gzip"		/* command to run as gzip */
#define COMPRESS_CMD	"compress"	/* command to run as compress */

/*
 *	Format specific routine table - MUST BE IN SORTED ORDER BY NAME
 *	(see pax.h for description of each function)
 *
 * 	name, blksz, hdsz, udev, hlk, blkagn, inhead, id, st_read,
 *	read, end_read, st_write, write, end_write, trail,
 *	rd_data, wr_data, options, is_uar
 */

FSUB fsub[] = {
/* 0: UNIX ARCHIVER */
	{"ar", 512, sizeof(HD_AR), 0, 0, 0, 0, uar_id, no_op,
	uar_rd, uar_endrd, uar_stwr, uar_wr, no_op, uar_trail,
	rd_wrfile, uar_wr_data, bad_opt, 1},

/* 1: OLD BINARY CPIO */
	{"bcpio", 5120, sizeof(HD_BCPIO), 1, 0, 0, 1, bcpio_id, cpio_strd,
	bcpio_rd, bcpio_endrd, cpio_stwr, bcpio_wr, cpio_endwr, cpio_trail,
	rd_wrfile, wr_rdfile, bad_opt, 0},

/* 2: OLD OCTAL CHARACTER CPIO */
	{"cpio", 5120, sizeof(HD_CPIO), 1, 0, 0, 1, cpio_id, cpio_strd,
	cpio_rd, cpio_endrd, cpio_stwr, cpio_wr, cpio_endwr, cpio_trail,
	rd_wrfile, wr_rdfile, bad_opt, 0},

/* 3: OLD OCTAL CHARACTER CPIO, UID/GID CLEARED (ANONYMISED) */
	{"dist", 512, sizeof(HD_CPIO), 1, 0, 0, 1, cpio_id, cpio_strd,
	cpio_rd, cpio_endrd, dist_stwr, cpio_wr, cpio_endwr, cpio_trail,
	rd_wrfile, wr_rdfile, bad_opt, 0},

/* 4: SVR4 HEX CPIO */
	{"sv4cpio", 5120, sizeof(HD_VCPIO), 1, 0, 0, 1, vcpio_id, cpio_strd,
	vcpio_rd, vcpio_endrd, cpio_stwr, vcpio_wr, cpio_endwr, cpio_trail,
	rd_wrfile, wr_rdfile, bad_opt, 0},

/* 5: SVR4 HEX CPIO WITH CRC */
	{"sv4crc", 5120, sizeof(HD_VCPIO), 1, 0, 0, 1, crc_id, crc_strd,
	vcpio_rd, vcpio_endrd, crc_stwr, vcpio_wr, cpio_endwr, cpio_trail,
	rd_wrfile, wr_rdfile, bad_opt, 0},

/* 6: OLD TAR */
	{"tar", 10240, BLKMULT, 0, 1, BLKMULT, 0, tar_id, no_op,
	tar_rd, tar_endrd, no_op_i, tar_wr, tar_endwr, tar_trail,
	rd_wrfile, wr_rdfile, tar_opt, 0},

/* 7: POSIX USTAR */
	{"ustar", 10240, BLKMULT, 0, 1, BLKMULT, 0, ustar_id, ustar_strd,
	ustar_rd, tar_endrd, ustar_stwr, ustar_wr, tar_endwr, tar_trail,
	rd_wrfile, wr_rdfile, bad_opt, 0},

/* 8: SVR4 HEX CPIO WITH CRC, UID/GID/MTIME CLEARED (NORMALISED) */
	{"v4norm", 512, sizeof(HD_VCPIO), 1, 0, 0, 1, crc_id, crc_strd,
	vcpio_rd, vcpio_endrd, v4norm_stwr, vcpio_wr, cpio_endwr, cpio_trail,
	rd_wrfile, wr_rdfile, bad_opt, 0},

/* 9: SVR4 HEX CPIO WITH CRC, UID/GID CLEARED (ANONYMISED) */
	{"v4root", 512, sizeof(HD_VCPIO), 1, 0, 0, 1, crc_id, crc_strd,
	vcpio_rd, vcpio_endrd, v4root_stwr, vcpio_wr, cpio_endwr, cpio_trail,
	rd_wrfile, wr_rdfile, bad_opt, 0},
};
#define	F_OCPIO	1	/* format when called as cpio -6 */
#define	F_ACPIO	2	/* format when called as cpio -c */
#define	F_NCPIO	4	/* format when called as tar -R */
#define	F_CPIO	5	/* format when called as cpio or tar -S */
#define F_OTAR	6	/* format when called as tar -o */
#define F_TAR	7	/* format when called as tar */
int F_UAR = 0;
#define DEFLT	7	/* default write format from list above */

/*
 * ford is the archive search order used by get_arc() to determine what kind
 * of archive we are dealing with. This helps to properly id archive formats
 * some formats may be subsets of others....
 */
int ford[] = { 7, 6, 5, 4, 2, 1, -1 };

/* normalise archives */
int anonarch = 0;

/* extract to standard output */
int to_stdout = 0;

/*
 * Do we have -C anywhere?
 */
int havechd = 0;

/*
 * options()
 *	figure out if we are pax, tar or cpio. Call the appropriate options
 *	parser
 */

void
options(int argc, char **argv)
{
	size_t n;

	/*
	 * Are we acting like pax, tar or cpio (based on argv[0])
	 */
	if ((n = strlen(argv[0])) >= 3 && !strcmp(argv[0] + n - 3, NM_TAR)) {
		argv0 = NM_TAR;
		tar_options(argc, argv);
	} else if (n >= 4 && !strcmp(argv[0] + n - 4, NM_CPIO)) {
		argv0 = NM_CPIO;
		cpio_options(argc, argv);
	} else {
		argv0 = NM_PAX;
		pax_options(argc, argv);
	}
}

/*
 * pax_options()
 *	look at the user specified flags. set globals as required and check if
 *	the user specified a legal set of flags. If not, complain and exit
 */

static void
pax_options(int argc, char **argv)
{
	int c;
	size_t i;
	unsigned int flg = 0;
	unsigned int bflg = 0;
	char *pt;
	FSUB tmp;

	/*
	 * process option flags
	 */
	while ((c=getopt(argc,argv,"ab:cdf:iklno:p:rs:tuvwx:zB:DE:G:HLM:OPT:U:XYZ0"))
	    != -1) {
		switch (c) {
		case 'a':
			/*
			 * append
			 */
			flg |= AF;
			break;
		case 'b':
			/*
			 * specify blocksize
			 */
			flg |= BF;
			if ((wrblksz = (int)str_offt(optarg)) <= 0) {
				paxwarn(1, "Invalid block size %s", optarg);
				pax_usage();
			}
			break;
		case 'c':
			/*
			 * inverse match on patterns
			 */
			cflag = 1;
			flg |= CF;
			break;
		case 'd':
			/*
			 * match only dir on extract, not the subtree at dir
			 */
			dflag = 1;
			flg |= DF;
			break;
		case 'f':
			/*
			 * filename where the archive is stored
			 */
			arcname = optarg;
			flg |= FF;
			break;
		case 'i':
			/*
			 * interactive file rename
			 */
			iflag = 1;
			flg |= IF;
			break;
		case 'k':
			/*
			 * do not clobber files that exist
			 */
			kflag = 1;
			flg |= KF;
			break;
		case 'l':
			/*
			 * try to link src to dest with copy (-rw)
			 */
			lflag = 1;
			flg |= LF;
			break;
		case 'n':
			/*
			 * select first match for a pattern only
			 */
			nflag = 1;
			flg |= NF;
			break;
		case 'o':
			/*
			 * pass format specific options
			 */
			flg |= OF;
			if (opt_add(optarg) < 0)
				pax_usage();
			break;
		case 'p':
			/*
			 * specify file characteristic options
			 */
			for (pt = optarg; *pt != '\0'; ++pt) {
				switch (*pt) {
				case 'a':
					/*
					 * do not preserve access time
					 */
					patime = 0;
					break;
				case 'e':
					/*
					 * preserve user id, group id, file
					 * mode, access/modification times
					 */
					pids = 1;
					pmode = 1;
					patime = 1;
					pmtime = 1;
					break;
				case 'm':
					/*
					 * do not preserve modification time
					 */
					pmtime = 0;
					break;
				case 'o':
					/*
					 * preserve uid/gid
					 */
					pids = 1;
					break;
				case 'p':
					/*
					 * preserve file mode bits
					 */
					pmode = 1;
					break;
				default:
					paxwarn(1, "Invalid -p string: %c", *pt);
					pax_usage();
					break;
				}
			}
			flg |= PF;
			break;
		case 'r':
			/*
			 * read the archive
			 */
			flg |= RF;
			break;
		case 's':
			/*
			 * file name substitution name pattern
			 */
			if (rep_add(optarg) < 0) {
				pax_usage();
				break;
			}
			flg |= SF;
			break;
		case 't':
			/*
			 * preserve access time on filesystem nodes we read
			 */
			tflag = 1;
			flg |= TF;
			break;
		case 'u':
			/*
			 * ignore those older files
			 */
			uflag = 1;
			flg |= UF;
			break;
		case 'v':
			/*
			 * verbose operation mode
			 */
			vflag++;
			flg |= VF;
			break;
		case 'w':
			/*
			 * write an archive
			 */
			flg |= WF;
			break;
		case 'x':
			/*
			 * specify an archive format on write
			 */
			tmp.name = optarg;
			if ((frmt = (FSUB *)bsearch((void *)&tmp, (void *)fsub,
			    sizeof(fsub)/sizeof(FSUB), sizeof(FSUB), c_frmt)) != NULL) {
				flg |= XF;
				break;
			}
			paxwarn(1, "Unknown -x format: %s", optarg);
			(void)fputs("pax: Known -x formats are:", stderr);
			for (i = 0; i < (sizeof(fsub)/sizeof(FSUB)); ++i)
				(void)fprintf(stderr, " %s", fsub[i].name);
			(void)fputs("\n\n", stderr);
			pax_usage();
			break;
		case 'z':
			/*
			 * use gzip.  Non standard option.
			 */
			gzip_program = GZIP_CMD;
			break;
		case 'B':
			/*
			 * non-standard option on number of bytes written on a
			 * single archive volume.
			 */
			if ((wrlimit = str_offt(optarg)) <= 0) {
				paxwarn(1, "Invalid write limit %s", optarg);
				pax_usage();
			}
			if (wrlimit % BLKMULT) {
				paxwarn(1, "Write limit is not a %d byte multiple",
				    BLKMULT);
				pax_usage();
			}
			flg |= CBF;
			break;
		case 'D':
			/*
			 * On extraction check file inode change time before the
			 * modification of the file name. Non standard option.
			 */
			Dflag = 1;
			flg |= CDF;
			break;
		case 'E':
			/*
			 * non-standard limit on read faults
			 * 0 indicates stop after first error, values
			 * indicate a limit, "NONE" try forever
			 */
			flg |= CEF;
			if (strcmp(NONE, optarg) == 0)
				maxflt = -1;
			else if ((maxflt = atoi(optarg)) < 0) {
				paxwarn(1, "Error count value must be positive");
				pax_usage();
			}
			break;
		case 'G':
			/*
			 * non-standard option for selecting files within an
			 * archive by group (gid or name)
			 */
			if (grp_add(optarg) < 0) {
				pax_usage();
				break;
			}
			flg |= CGF;
			break;
		case 'H':
			/*
			 * follow command line symlinks only
			 */
			Hflag = 1;
			flg |= CHF;
			break;
		case 'L':
			/*
			 * follow symlinks
			 */
			Lflag = 1;
			flg |= CLF;
			break;
		case 'M':
			/*
			 * MirOS extension: archive normaliser
			 */
			process_M(optarg, pax_usage);
			break;
		case 'O':
			/*
			 * Force one volume.  Non standard option.
			 */
			force_one_volume = 1;
			break;
		case 'P':
			/*
			 * do NOT follow symlinks (default)
			 */
			Lflag = 0;
			flg |= CPF;
			break;
		case 'T':
			/*
			 * non-standard option for selecting files within an
			 * archive by modification time range (lower,upper)
			 */
			if (trng_add(optarg) < 0) {
				pax_usage();
				break;
			}
			flg |= CTF;
			break;
		case 'U':
			/*
			 * non-standard option for selecting files within an
			 * archive by user (uid or name)
			 */
			if (usr_add(optarg) < 0) {
				pax_usage();
				break;
			}
			flg |= CUF;
			break;
		case 'X':
			/*
			 * do not pass over mount points in the file system
			 */
			Xflag = 1;
			flg |= CXF;
			break;
		case 'Y':
			/*
			 * On extraction check file inode change time after the
			 * modification of the file name. Non standard option.
			 */
			Yflag = 1;
			flg |= CYF;
			break;
		case 'Z':
			/*
			 * On extraction check modification time after the
			 * modification of the file name. Non standard option.
			 */
			Zflag = 1;
			flg |= CZF;
			break;
		case '0':
			/*
			 * Use \0 as pathname terminator.
			 * (For use with the -print0 option of find(1).)
			 */
			zeroflag = 1;
			flg |= C0F;
			break;
		default:
			pax_usage();
			break;
		}
	}

	/*
	 * figure out the operation mode of pax read,write,extract,copy,append
	 * or list. check that we have not been given a bogus set of flags
	 * for the operation mode.
	 */
	if (ISLIST(flg)) {
		act = LIST;
		listf = stdout;
		bflg = flg & BDLIST;
	} else if (ISEXTRACT(flg)) {
		act = EXTRACT;
		bflg = flg & BDEXTR;
	} else if (ISARCHIVE(flg)) {
		act = ARCHIVE;
		bflg = flg & BDARCH;
	} else if (ISAPPND(flg)) {
		act = APPND;
		bflg = flg & BDARCH;
	} else if (ISCOPY(flg)) {
		act = COPY;
		bflg = flg & BDCOPY;
	} else
		pax_usage();
	if (bflg) {
		printflg(flg);
		pax_usage();
	}

	/*
	 * if we are writing (ARCHIVE) we use the default format if the user
	 * did not specify a format. when we write during an APPEND, we will
	 * adopt the format of the existing archive if none was supplied.
	 */
	if (!(flg & XF) && (act == ARCHIVE))
		frmt = &(fsub[DEFLT]);

	/*
	 * process the args as they are interpreted by the operation mode
	 */
	switch (act) {
	case LIST:
	case EXTRACT:
		for (; optind < argc; optind++)
			if (pat_add(argv[optind], NULL) < 0)
				pax_usage();
		break;
	case COPY:
		if (optind >= argc) {
			paxwarn(0, "Destination directory was not supplied");
			pax_usage();
		}
		--argc;
		dirptr = argv[argc];
		/* FALL THROUGH */
	case ARCHIVE:
	case APPND:
		for (; optind < argc; optind++)
			if (ftree_add(argv[optind], 0) < 0)
				pax_usage();
		/*
		 * no read errors allowed on updates/append operation!
		 */
		maxflt = 0;
		break;
	}
}


/*
 * tar_options()
 *	look at the user specified flags. set globals as required and check if
 *	the user specified a legal set of flags. If not, complain and exit
 */

static void
tar_set_action(int op)
{
	if (act != ERROR && act != op)
		tar_usage();
	act = op;
}

static void
tar_options(int argc, char **argv)
{
	int c;
	int fstdin = 0;
	int Oflag = 0;
	int nincfiles = 0;
	int incfiles_max = 0;
	struct incfile {
		char *file;
		char *dir;
	};
	struct incfile *incfiles = NULL;

	/*
	 * Set default values.
	 */
	rmleadslash = 1;

	/*
	 * process option flags
	 */
	while ((c = getoldopt(argc, argv,
	    "Ab:cef:hmopqruts:vwxzBC:HI:LM:OPRSXZ014578")) != -1) {
		switch (c) {
		case 'A':
			Oflag = 5;
			break;
		case 'b':
			/*
			 * specify blocksize in 512-byte blocks
			 */
			if ((wrblksz = (int)str_offt(optarg)) <= 0) {
				paxwarn(1, "Invalid block size %s", optarg);
				tar_usage();
			}
			wrblksz *= 512;		/* XXX - check for int oflow */
			break;
		case 'c':
			/*
			 * create an archive
			 */
			tar_set_action(ARCHIVE);
			break;
		case 'e':
			/*
			 * stop after first error
			 */
			maxflt = 0;
			break;
		case 'f':
			/*
			 * filename where the archive is stored
			 */
			if ((optarg[0] == '-') && (optarg[1]== '\0')) {
				/*
				 * treat a - as stdin
				 */
				fstdin = 1;
				arcname = NULL;
				break;
			}
			fstdin = 0;
			arcname = optarg;
			break;
		case 'h':
			/*
			 * follow symlinks
			 */
			Lflag = 1;
			break;
		case 'm':
			/*
			 * do not preserve modification time
			 */
			pmtime = 0;
			break;
		case 'O':
			Oflag = 1;
			to_stdout = 2;
			break;
		case 'o':
			Oflag = 2;
			break;
		case 'p':
			/*
			 * preserve uid/gid and file mode, regardless of umask
			 */
			pmode = 1;
			pids = 1;
			break;
		case 'q':
			/*
			 * select first match for a pattern only
			 */
			nflag = 1;
			break;
		case 'r':
		case 'u':
			/*
			 * append to the archive
			 */
			tar_set_action(APPND);
			break;
		case 'R':
			Oflag = 3;
			anonarch = ANON_INODES | ANON_HARDLINKS;
			break;
		case 'S':
			Oflag = 4;
			anonarch = ANON_INODES | ANON_HARDLINKS;
			break;
		case 's':
			/*
			 * file name substitution name pattern
			 */
			if (rep_add(optarg) < 0) {
				tar_usage();
				break;
			}
			break;
		case 't':
			/*
			 * list contents of the tape
			 */
			tar_set_action(LIST);
			break;
		case 'v':
			/*
			 * verbose operation mode
			 */
			vflag++;
			break;
		case 'w':
			/*
			 * interactive file rename
			 */
			iflag = 1;
			break;
		case 'x':
			/*
			 * extract an archive, preserving mode,
			 * and mtime if possible.
			 */
			tar_set_action(EXTRACT);
			pmtime = 1;
			break;
		case 'z':
			/*
			 * use gzip.  Non standard option.
			 */
			gzip_program = GZIP_CMD;
			break;
		case 'B':
			/*
			 * Nothing to do here, this is pax default
			 */
			break;
		case 'C':
			havechd++;
			chdname = optarg;
			break;
		case 'H':
			/*
			 * follow command line symlinks only
			 */
			Hflag = 1;
			break;
		case 'I':
			if (++nincfiles > incfiles_max) {
				incfiles_max = nincfiles + 3;
				incfiles = realloc(incfiles,
				    sizeof(*incfiles) * incfiles_max);
				if (incfiles == NULL) {
					paxwarn(0, "Unable to allocate space "
					    "for option list");
					exit(1);
				}
			}
			incfiles[nincfiles - 1].file = optarg;
			incfiles[nincfiles - 1].dir = chdname;
			break;
		case 'L':
			/*
			 * follow symlinks
			 */
			Lflag = 1;
			break;
		case 'M':
			process_M(optarg, tar_usage);
			break;
		case 'P':
			/*
			 * do not remove leading '/' from pathnames
			 */
			rmleadslash = 0;
			break;
		case 'X':
			/*
			 * do not pass over mount points in the file system
			 */
			Xflag = 1;
			break;
		case 'Z':
			/*
			 * use compress.
			 */
			gzip_program = COMPRESS_CMD;
			break;
		case '0':
			arcname = DEV_0;
			break;
		case '1':
			arcname = DEV_1;
			break;
		case '4':
			arcname = DEV_4;
			break;
		case '5':
			arcname = DEV_5;
			break;
		case '7':
			arcname = DEV_7;
			break;
		case '8':
			arcname = DEV_8;
			break;
		default:
			tar_usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	/* Tar requires an action. */
	if (act == ERROR)
		tar_usage();

	/* Traditional tar behaviour (pax uses stderr unless in list mode) */
	if (fstdin == 1 && act == ARCHIVE)
		listf = stderr;
	else
		listf = stdout;

	/* Traditional tar behaviour (pax wants to read file list from stdin) */
	if ((act == ARCHIVE || act == APPND) && argc == 0 && nincfiles == 0)
		exit(0);

	/*
	 * process the args as they are interpreted by the operation mode
	 */
	switch (act) {
	case EXTRACT:
		if (to_stdout == 2)
			to_stdout = 1;
	case LIST:
	default:
		{
			int sawpat = 0;
			char *file, *dir = NULL;

			while (nincfiles || *argv != NULL) {
				/*
				 * If we queued up any include files,
				 * pull them in now.  Otherwise, check
				 * for -I and -C positional flags.
				 * Anything else must be a file to
				 * extract.
				 */
				if (nincfiles) {
					file = incfiles->file;
					dir = incfiles->dir;
					incfiles++;
					nincfiles--;
				} else if (strcmp(*argv, "-I") == 0) {
					if (*++argv == NULL)
						break;
					file = *argv++;
					dir = chdname;
				} else
					file = NULL;
				if (file != NULL) {
					FILE *fp;
					char *str;

					if (strcmp(file, "-") == 0)
						fp = stdin;
					else if ((fp = fopen(file, "r")) == NULL) {
						paxwarn(1, "Unable to open file '%s' for read", file);
						tar_usage();
					}
					while ((str = get_line(fp)) != NULL) {
						if (pat_add(str, dir) < 0)
							tar_usage();
						sawpat = 1;
					}
					if (strcmp(file, "-") != 0)
						fclose(fp);
					if (get_line_error) {
						paxwarn(1, "Problem with file '%s'", file);
						tar_usage();
					}
				} else if (strcmp(*argv, "-C") == 0) {
					if (*++argv == NULL)
						break;
					chdname = *argv++;
					havechd++;
				} else if (pat_add(*argv++, chdname) < 0)
					tar_usage();
				else
					sawpat = 1;
			}
			/*
			 * if patterns were added, we are doing	chdir()
			 * on a file-by-file basis, else, just one
			 * global chdir (if any) after opening input.
			 */
			if (sawpat > 0)
				chdname = NULL;
		}
		break;
	case ARCHIVE:
	case APPND:
		switch(Oflag) {
		    case 0:
			frmt = &(fsub[F_TAR]);
			break;
		    case 1:
			frmt = &(fsub[F_OTAR]);
			break;
		    case 2:
			frmt = &(fsub[F_OTAR]);
			if (opt_add("write_opt=nodir") < 0)
				tar_usage();
			break;
		    case 3:
			frmt = &(fsub[F_NCPIO]);
			break;
		    case 4:
			frmt = &(fsub[F_CPIO]);
			break;
		    case 5:
			frmt = &(fsub[F_UAR]);
			break;
		    default:
			tar_usage();
			break;
		}

		if (chdname != NULL) {	/* initial chdir() */
			if (ftree_add(chdname, 1) < 0)
				tar_usage();
		}

		while (nincfiles || *argv != NULL) {
			char *file, *dir = NULL;

			/*
			 * If we queued up any include files, pull them in
			 * now.  Otherwise, check for -I and -C positional
			 * flags.  Anything else must be a file to include
			 * in the archive.
			 */
			if (nincfiles) {
				file = incfiles->file;
				dir = incfiles->dir;
				incfiles++;
				nincfiles--;
			} else if (strcmp(*argv, "-I") == 0) {
				if (*++argv == NULL)
					break;
				file = *argv++;
				dir = NULL;
			} else
				file = NULL;
			if (file != NULL) {
				FILE *fp;
				char *str;

				/* Set directory if needed */
				if (dir) {
					if (ftree_add(dir, 1) < 0)
						tar_usage();
				}

				if (strcmp(file, "-") == 0)
					fp = stdin;
				else if ((fp = fopen(file, "r")) == NULL) {
					paxwarn(1, "Unable to open file '%s' for read", file);
					tar_usage();
				}
				while ((str = get_line(fp)) != NULL) {
					if (ftree_add(str, 0) < 0)
						tar_usage();
				}
				if (strcmp(file, "-") != 0)
					fclose(fp);
				if (get_line_error) {
					paxwarn(1, "Problem with file '%s'",
					    file);
					tar_usage();
				}
			} else if (strcmp(*argv, "-C") == 0) {
				if (*++argv == NULL)
					break;
				if (ftree_add(*argv++, 1) < 0)
					tar_usage();
				havechd++;
			} else if (ftree_add(*argv++, 0) < 0)
				tar_usage();
		}
		/*
		 * no read errors allowed on updates/append operation!
		 */
		maxflt = 0;
		break;
	}
	if (to_stdout != 1)
		to_stdout = 0;
	if (!fstdin && ((arcname == NULL) || (*arcname == '\0'))) {
		arcname = getenv("TAPE");
#ifdef _PATH_DEFTAPE
		if ((arcname == NULL) || (*arcname == '\0'))
			arcname = _PATH_DEFTAPE;
#endif
	}
}

int
mkpath(char *path)
{
	struct stat sb;
	char *slash;
	int done = 0;

	slash = path;

	while (!done) {
		slash += strspn(slash, "/");
		slash += strcspn(slash, "/");

		done = (*slash == '\0');
		*slash = '\0';

		if (stat(path, &sb)) {
			if (errno != ENOENT || mkdir(path, 0777)) {
				paxwarn(1, "%s", path);
				return (-1);
			}
		} else if (!S_ISDIR(sb.st_mode)) {
			syswarn(1, ENOTDIR, "%s", path);
			return (-1);
		}

		if (!done)
			*slash = '/';
	}

	return (0);
}

static void
cpio_set_action(int op)
{
	if ((act == APPND && op == ARCHIVE) || (act == ARCHIVE && op == APPND))
		act = APPND;
	else if ((act == LIST && op == EXTRACT) || (act == EXTRACT && op == LIST))
		act = LIST;
	else if (act != ERROR && act != op)
		cpio_usage();
	else
		act = op;
}

/*
 * cpio_options()
 *	look at the user specified flags. set globals as required and check if
 *	the user specified a legal set of flags. If not, complain and exit
 */

static void
cpio_options(int argc, char **argv)
{
	int c;
	size_t i;
	char *str;
	FSUB tmp;
	FILE *fp;

	kflag = 1;
	pids = 1;
	pmode = 1;
	pmtime = 0;
	arcname = NULL;
	dflag = 1;
	nodirs = 1;
	while ((c=getopt(argc,argv,"abcdfiklmoprstuvzABC:E:F:H:I:LM:O:SZ6")) != -1)
		switch (c) {
			case 'a':
				/*
				 * preserve access time on files read
				 */
				tflag = 1;
				break;
			case 'b':
				/*
				 * swap bytes and half-words when reading data
				 */
				break;
			case 'c':
				/*
				 * ASCII cpio header
				 */
				frmt = &(fsub[F_ACPIO]);
				break;
			case 'd':
				/*
				 * create directories as needed
				 */
				nodirs = 0;
				break;
			case 'f':
				/*
				 * invert meaning of pattern list
				 */
				cflag = 1;
				break;
			case 'i':
				/*
				 * restore an archive
				 */
				cpio_set_action(EXTRACT);
				break;
			case 'k':
				break;
			case 'l':
				/*
				 * use links instead of copies when possible
				 */
				lflag = 1;
				break;
			case 'm':
				/*
				 * preserve modification time
				 */
				pmtime = 1;
				break;
			case 'o':
				/*
				 * create an archive
				 */
				cpio_set_action(ARCHIVE);
				frmt = &(fsub[F_CPIO]);
				break;
			case 'p':
				/*
				 * copy-pass mode
				 */
				cpio_set_action(COPY);
				break;
			case 'r':
				/*
				 * interactively rename files
				 */
				iflag = 1;
				break;
			case 's':
				/*
				 * swap bytes after reading data
				 */
				break;
			case 't':
				/*
				 * list contents of archive
				 */
				cpio_set_action(LIST);
				listf = stdout;
				break;
			case 'u':
				/*
				 * replace newer files
				 */
				kflag = 0;
				break;
			case 'v':
				/*
				 * verbose operation mode
				 */
				vflag++;
				break;
			case 'z':
				/*
				 * use gzip.  Non standard option.
				 */
				gzip_program = GZIP_CMD;
				break;
			case 'A':
				/*
				 * append mode
				 */
				cpio_set_action(APPND);
				break;
			case 'B':
				/*
				 * Use 5120 byte block size
				 */
				wrblksz = 5120;
				break;
			case 'C':
				/*
				 * set block size in bytes
				 */
				wrblksz = atoi(optarg);
				break;
			case 'E':
				/*
				 * file with patterns to extract or list
				 */
				if ((fp = fopen(optarg, "r")) == NULL) {
					paxwarn(1, "Unable to open file '%s' for read", optarg);
					cpio_usage();
				}
				while ((str = get_line(fp)) != NULL) {
					pat_add(str, NULL);
				}
				fclose(fp);
				if (get_line_error) {
					paxwarn(1, "Problem with file '%s'", optarg);
					cpio_usage();
				}
				break;
			case 'F':
			case 'I':
			case 'O':
				/*
				 * filename where the archive is stored
				 */
				if ((optarg[0] == '-') && (optarg[1]== '\0')) {
					/*
					 * treat a - as stdin
					 */
					arcname = NULL;
					break;
				}
				arcname = optarg;
				break;
			case 'H':
				/*
				 * specify an archive format on write
				 */
				if (!strcmp(optarg, "bin")) {
					tmp.name = "bcpio";
				} else if (!strcmp(optarg, "crc")) {
					tmp.name = "sv4crc";
				} else if (!strcmp(optarg, "newc")) {
					tmp.name = "sv4cpio";
				} else if (!strcmp(optarg, "odc")) {
					tmp.name = "cpio";
				} else {
					tmp.name = optarg;
				}
				if ((frmt = (FSUB *)bsearch((void *)&tmp, (void *)fsub,
				    sizeof(fsub)/sizeof(FSUB), sizeof(FSUB), c_frmt)) != NULL)
					break;
				paxwarn(1, "Unknown -H format: %s", optarg);
				(void)fputs("cpio: Known -H formats are:", stderr);
				for (i = 0; i < (sizeof(fsub)/sizeof(FSUB)); ++i)
					(void)fprintf(stderr, " %s", fsub[i].name);
				(void)fputs("\n\n", stderr);
				cpio_usage();
				break;
			case 'L':
				/*
				 * follow symbolic links
				 */
				Lflag = 1;
				break;
			case 'M':
				process_M(optarg, cpio_usage);
				break;
			case 'S':
				/*
				 * swap halfwords after reading data
				 */
				break;
			case 'Z':
				/*
				 * use compress.  Non standard option.
				 */
				gzip_program = COMPRESS_CMD;
				break;
			case '6':
				/*
				 * process Version 6 cpio format
				 */
				frmt = &(fsub[F_OCPIO]);
				break;
			case '?':
			default:
				cpio_usage();
				break;
		}
	argc -= optind;
	argv += optind;

	/*
	 * process the args as they are interpreted by the operation mode
	 */
	switch (act) {
		case LIST:
		case EXTRACT:
			while (*argv != NULL)
				if (pat_add(*argv++, NULL) < 0)
					cpio_usage();
			break;
		case COPY:
			if (*argv == NULL) {
				paxwarn(0, "Destination directory was not supplied");
				cpio_usage();
			}
			dirptr = *argv;
			if (mkpath(dirptr) < 0)
				cpio_usage();
			--argc;
			++argv;
			/* FALL THROUGH */
		case ARCHIVE:
		case APPND:
			if (*argv != NULL)
				cpio_usage();
			/*
			 * no read errors allowed on updates/append operation!
			 */
			maxflt = 0;
			while ((str = get_line(stdin)) != NULL) {
				ftree_add(str, 0);
			}
			if (get_line_error) {
				paxwarn(1, "Problem while reading stdin");
				cpio_usage();
			}
			break;
		default:
			cpio_usage();
			break;
	}
}

/*
 * printflg()
 *	print out those invalid flag sets found to the user
 */

static void
printflg(unsigned int flg)
{
	int nxt;
	int pos = 0;

	(void)fprintf(stderr,"%s: Invalid combination of options:", argv0);
	while ((nxt = ffs(flg)) != 0) {
		flg = flg >> nxt;
		pos += nxt;
		(void)fprintf(stderr, " -%c", flgch[pos-1]);
	}
	(void)putc('\n', stderr);
}

/*
 * c_frmt()
 *	comparison routine used by bsearch to find the format specified
 *	by the user
 */

static int
c_frmt(const void *a, const void *b)
{
	return(strcmp(((const FSUB *)a)->name, ((const FSUB *)b)->name));
}

/*
 * opt_next()
 *	called by format specific options routines to get each format specific
 *	flag and value specified with -o
 * Return:
 *	pointer to next OPLIST entry or NULL (end of list).
 */

OPLIST *
opt_next(void)
{
	OPLIST *opt;

	if ((opt = ophead) != NULL)
		ophead = ophead->fow;
	return(opt);
}

/*
 * bad_opt()
 *	generic routine used to complain about a format specific options
 *	when the format does not support options.
 */

int
bad_opt(void)
{
	OPLIST *opt;

	if (ophead == NULL)
		return(0);
	/*
	 * print all we were given
	 */
	paxwarn(1,"These format options are not supported");
	while ((opt = opt_next()) != NULL)
		(void)fprintf(stderr, "\t%s = %s\n", opt->name, opt->value);
	pax_usage();
	return(0);
}

/*
 * opt_add()
 *	breaks the value supplied to -o into a option name and value. options
 *	are given to -o in the form -o name-value,name=value
 *	multiple -o may be specified.
 * Return:
 *	0 if format in name=value format, -1 if -o is passed junk
 */

int
opt_add(const char *str)
{
	OPLIST *opt;
	char *frpt;
	char *pt;
	char *endpt;
	char *dstr;

	if ((str == NULL) || (*str == '\0')) {
		paxwarn(0, "Invalid option name");
		return(-1);
	}
	if ((dstr = strdup(str)) == NULL) {
		paxwarn(0, "Unable to allocate space for option list");
		return(-1);
	}
	frpt = endpt = dstr;

	/*
	 * break into name and values pieces and stuff each one into a
	 * OPLIST structure. When we know the format, the format specific
	 * option function will go through this list
	 */
	while ((frpt != NULL) && (*frpt != '\0')) {
		if ((endpt = strchr(frpt, ',')) != NULL)
			*endpt = '\0';
		if ((pt = strchr(frpt, '=')) == NULL) {
			paxwarn(0, "Invalid options format");
			free(dstr);
			return(-1);
		}
		if ((opt = (OPLIST *)malloc(sizeof(OPLIST))) == NULL) {
			paxwarn(0, "Unable to allocate space for option list");
			free(dstr);
			return(-1);
		}
		*pt++ = '\0';
		opt->name = frpt;
		opt->value = pt;
		opt->fow = NULL;
		if (endpt != NULL)
			frpt = endpt + 1;
		else
			frpt = NULL;
		if (ophead == NULL) {
			optail = ophead = opt;
			continue;
		}
		optail->fow = opt;
		optail = opt;
	}
	return(0);
}

/*
 * str_offt()
 *	Convert an expression of the following forms to an off_t > 0.
 * 	1) A positive decimal number.
 *	2) A positive decimal number followed by a b (mult by 512).
 *	3) A positive decimal number followed by a k (mult by 1024).
 *	4) A positive decimal number followed by a m (mult by 512).
 *	5) A positive decimal number followed by a w (mult by sizeof int)
 *	6) Two or more positive decimal numbers (with/without k,b or w).
 *	   separated by x (also * for backwards compatibility), specifying
 *	   the product of the indicated values.
 * Return:
 *	0 for an error, a positive value o.w.
 */

static off_t
str_offt(char *val)
{
	char *expr;
	off_t num, t;

#	ifdef LONG_OFF_T
	num = strtol(val, &expr, 0);
	if ((num == LONG_MAX) || (num <= 0) || (expr == val))
#	else
	num = strtoq(val, &expr, 0);
	if ((num == QUAD_MAX) || (num <= 0) || (expr == val))
#	endif
		return(0);

	switch (*expr) {
	case 'b':
		t = num;
		num *= 512;
		if (t > num)
			return(0);
		++expr;
		break;
	case 'k':
		t = num;
		num *= 1024;
		if (t > num)
			return(0);
		++expr;
		break;
	case 'm':
		t = num;
		num *= 1048576;
		if (t > num)
			return(0);
		++expr;
		break;
	case 'w':
		t = num;
		num *= sizeof(int);
		if (t > num)
			return(0);
		++expr;
		break;
	}

	switch (*expr) {
		case '\0':
			break;
		case '*':
		case 'x':
			t = num;
			num *= str_offt(expr + 1);
			if (t > num)
				return(0);
			break;
		default:
			return(0);
	}
	return(num);
}

char *
get_line(FILE *f)
{
	char *name, *temp;
	size_t len;

	name = fgetln(f, &len);
	if (!name) {
		get_line_error = ferror(f) ? GET_LINE_FILE_CORRUPT : 0;
		return(0);
	}
	if (name[len-1] != '\n')
		len++;
	temp = malloc(len);
	if (!temp) {
		get_line_error = GET_LINE_OUT_OF_MEM;
		return(0);
	}
	memcpy(temp, name, len-1);
	temp[len-1] = 0;
	return(temp);
}

/*
 * no_op()
 *	for those option functions where the archive format has nothing to do.
 * Return:
 *	0
 */

static int
no_op(void)
{
	return(0);
}

static int
no_op_i(int is_app __attribute__((__unused__)))
{
	return(0);
}

/*
 * pax_usage()
 *	print the usage summary to the user
 */

void
pax_usage(void)
{
	(void)fputs(
	    "usage: pax [-0cdnOvz] [-E limit] [-f archive] [-G group] [-s replstr]\n"
	    "\t  [-T range] [-U user] [pattern ...]\n"
	    "       pax -r [-0cDdiknOuvYZz] [-E limit] [-f archive] [-G group]\n"
	    "\t  [-o options] [-p string] [-s replstr] [-T range]\n"
	    "\t  [-U user] [pattern ...]\n"
	    "       pax -w [-0adHiLOPtuvXz] [-B bytes] [-b blocksize] [-f archive]\n"
	    "\t  [-G group] [-M flag] [-o options] [-s replstr]\n"
	    "\t  [-T range] [-U user] [-x format] [file ...]\n"
	    "       pax -rw [-0DdHikLlnOPtuvXYZ] [-G group] [-p string] [-s replstr]\n"
	    "\t  [-T range] [-U user] [file ...] directory\n",
	    stderr);
	exit(1);
}

/*
 * tar_usage()
 *	print the usage summary to the user
 */

void
tar_usage(void)
{
	(void)fputs(
	    "usage: tar {crtux}[014578befHhLmOoPpqRSsvwXZz]\n"
	    "\t  [blocking-factor | archive | replstr] [-C directory] [-I file]\n"
	    "\t  [file ...]\n"
	    "       tar {-crtux} [-014578eHhLmOoPpqRSvwXZz] [-b blocking-factor] [-M flag]\n"
	    "\t  [-C directory] [-f archive] [-I file] [-s replstr] [file ...]\n",
	    stderr);
	exit(1);
}

/*
 * cpio_usage()
 *	print the usage summary to the user
 */

void
cpio_usage(void)
{
	(void)fputs("usage: cpio -o [-AaBcLvZz] [-C bytes] [-F archive] [-H format]\n", stderr);
	(void)fputs("               [-M flag] [-O archive] <name-list [>archive]\n", stderr);
	(void)fputs("       cpio -i [-6BbcdfmrSstuvZz] [-C bytes] [-E file] [-F archive]\n", stderr);
	(void)fputs("               [-H format] [-I archive] [pattern...] [<archive]\n", stderr);
	(void)fputs("       cpio -p [-adLlmuv] destination-directory <name-list\n", stderr);
	exit(1);
}

void
anonarch_init(void)
{
	if (anonarch & ANON_VERBOSE) {
		anonarch &= ~ANON_VERBOSE;
		paxwarn(0, "debug: -M 0x%08X -x %s", anonarch, frmt->name);
	}
}

static void
process_M(const char *arg, void (*call_usage)(void))
{
	int j, k = 0;

	if ((arg[0] >= '0') && (arg[0] <= '9')) {
#ifdef __OpenBSD__
		const char *s;
		int64_t i = strtonum(arg, 0,
		    ANON_MAXVAL, &s);
		if (s)
			errx(1, "%s M value: %s", s,
			    arg);
#else
		char *ep;
		long long i = strtoll(arg, &ep, 0);
		if ((ep == arg) || (*ep != '\0') ||
		    (i < 0) || (i > ANON_MAXVAL))
			errx(1, "impossible M value:"
			    " %s", arg);
#endif
		anonarch = i;
		return;
	}

	if (!strncmp(arg, "no-", 3)) {
		j = 0;
		arg += 3;
	} else
		j = 1;
	if (!strncmp(arg, "uid", 3) ||
	    !strncmp(arg, "gid", 3)) {
		k = ANON_UIDGID;
	} else if (!strncmp(arg, "ino", 3)) {
		k = ANON_INODES;
	} else if (!strncmp(arg, "mtim", 4)) {
		k = ANON_MTIME;
	} else if (!strncmp(arg, "link", 4)) {
		k = ANON_HARDLINKS;
	} else if (!strncmp(arg, "norm", 4)) {
		k = ANON_UIDGID | ANON_INODES | ANON_NUMID |
		    ANON_MTIME | ANON_HARDLINKS;
	} else if (!strncmp(arg, "root", 4)) {
		k = ANON_UIDGID | ANON_INODES | ANON_NUMID;
	} else if (!strncmp(arg, "dist", 4)) {
		k = ANON_UIDGID | ANON_INODES | ANON_NUMID |
		    ANON_HARDLINKS;
	} else if (!strncmp(arg, "set", 3)) {
		k = ANON_INODES | ANON_HARDLINKS;
	} else if (!strncmp(arg, "v", 1)) {
		k = ANON_VERBOSE;
	} else if (!strncmp(arg, "debug", 5)) {
		k = ANON_DEBUG;
	} else if (!strncmp(arg, "lncp", 4)) {
		k = ANON_LNCP;
	} else if (!strncmp(arg, "numid", 5)) {
		k = ANON_NUMID;
	} else if (!strncmp(arg, "gslash", 6)) {
		k = ANON_DIRSLASH;
	} else
		call_usage();
	if (j)
		anonarch |= k;
	else
		anonarch &= ~k;
}
