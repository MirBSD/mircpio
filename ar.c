/*-
 * Copyright (c) 2011 Thorsten Glaser.
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
#include <err.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "pax.h"
#include "extern.h"
#include "options.h"
#include "ar.h"

__RCSID("$MirOS: src/bin/pax/ar.c,v 1.1 2011/08/16 21:32:45 tg Exp $");

/*
 * Routines for reading and writing Unix Archiver format libraries
 */

static /*const*/ char magic[8] = {
	0x21, 0x3C, 0x61, 0x72, 0x63, 0x68, 0x3E, 0x0A
};


/*
 * initialisation for ar write
 * returns 0 if ok, -1 otherwise
 */
int
uar_stwr(int is_app)
{
	return (is_app ? 0 : wr_rdbuf(magic, 8));
}

/*
 * check for ar magic
 * returns 0 if ok, -1 otherwise
 */
int
uar_ismagic(char *buf)
{
	return (memcmp(buf, magic, 8) ? -1 : 0);
}

/*
 * used during format identification, but we differ
 */
int
uar_id(char *buf __attribute__((__unused__)),
    int len __attribute__((__unused__)))
{
	errx(1, "internal error: %s should never have been called",
	    "uar_id");
}

/* internal parsing functions */
static uint64_t
uar_atoi64(const char *buf, size_t len)
{
	char c;
	uint64_t res = 0;

 loop:
	if (!len-- || (c = *buf++) < '0' || c > '9')
		return (res);
	res = (res * 10) + (c - '0');
	goto loop;
}

static uint32_t
uar_atoi32(const char *buf, size_t len)
{
	char c;
	uint32_t res = 0;

 loop:
	if (!len-- || (c = *buf++) < '0' || c > '9')
		return (res);
	res = (res * 10) + (c - '0');
	goto loop;
}

static uint32_t
uar_otoi32(const char *buf, size_t len)
{
	char c;
	uint32_t res = 0;

 loop:
	if (!len-- || (c = *buf++) < '0' || c > '7')
		return (res);
	res = (res << 3) | (c & 7);
	goto loop;
}

/*
 * parse header
 */
int
uar_rd(ARCHD *arcn, char *buf)
{
	HD_AR *h = (HD_AR *)buf;
	uint64_t i;

	if (h->ar_magic[0] != 0x60 || h->ar_magic[1] != 0x0A)
		return (-1);

	memset(arcn, 0, sizeof(*arcn));
	arcn->org_name = arcn->name;
	arcn->sb.st_nlink = 1;
	arcn->type = PAX_REG;

	arcn->sb.st_ctime = arcn->sb.st_atime = arcn->sb.st_mtime =
	    uar_atoi64(h->ar_mtime, sizeof(h->ar_mtime));
	arcn->sb.st_uid = uar_atoi32(h->ar_uid, sizeof(h->ar_uid));
	arcn->sb.st_gid = uar_atoi32(h->ar_gid, sizeof(h->ar_gid));
	arcn->sb.st_mode = uar_otoi32(h->ar_mode, sizeof(h->ar_mode)) |
	    S_IFREG;
	i = uar_atoi64(h->ar_size, sizeof(h->ar_size));
	arcn->pad = i & 1;

	if (h->ar_name[0] == 0x23 &&
	    h->ar_name[1] == 0x31 &&
	    h->ar_name[2] == 0x2F) {
		arcn->nlen = uar_atoi32(&(h->ar_name[3]),
		    sizeof(h->ar_name) - 3);
		if (arcn->nlen > PAXPATHLEN)
			/*XXX just skip over this file */
			return (-1);
		if (rd_wrbuf(arcn->name, arcn->nlen) != arcn->nlen)
			return (-1);
		i -= arcn->nlen;
	} else {
		register char c;

		/*arcn->nlen = 0;*/
		while (arcn->nlen < (int)sizeof(h->ar_name)) {
			c = h->ar_name[arcn->nlen];
			if (c == ' ' || c == '/' || c == '\0')
				break;
			arcn->name[arcn->nlen++] = c;
		}
	}
	arcn->name[arcn->nlen] = '\0';
	arcn->sb.st_size = i;
	arcn->skip = i;
	return (0);
}

/* internal emission functions */
static char *
uar_itoa64(char *dst, uint64_t num)
{
	if (num >= 10)
		dst = uar_itoa64(dst, num / 10);
	*dst++ = '0' + (num % 10);
	return (dst);
}

static char *
uar_itoa32(char *dst, uint32_t num)
{
	if (num >= 10)
		dst = uar_itoa32(dst, num / 10);
	*dst++ = '0' + (num % 10);
	return (dst);
}

static char *
uar_itoo32(char *dst, uint32_t num)
{
	if (num & ~7)
		dst = uar_itoo32(dst, num >> 3);
	*dst++ = '0' | (num & 7);
	return (dst);
}

/*
 * write a header
 */
int
uar_wr(ARCHD *arcn)
{
	HD_AR h;
	u_long t_uid, t_gid;
	time_t t_mtime = 0;
	char *extname;
	size_t n;

	anonarch_init();

	switch (arcn->type) {
	case PAX_CTG:
	case PAX_REG:
	case PAX_HRG:
		/* regular files, more or less */
		break;
	case PAX_DIR:
		/* directory, ignore silently */
		return (1);
	default:
		paxwarn(1, "ar can only archive regular files, which %s is not",
		    arcn->org_name);
		return (1);
	}

	/* trim trailing slashes */
	n = strlen(arcn->name) - 1;
	while (n && arcn->name[n] == '/')
		--n;
	arcn->name[++n] = '\0';
	/* find out basename */
	if ((extname = strrchr(arcn->name, '/')) == NULL)
		extname = arcn->name;

	t_uid = (anonarch & ANON_UIDGID) ? 0UL : (u_long)arcn->sb.st_uid;
	t_gid = (anonarch & ANON_UIDGID) ? 0UL : (u_long)arcn->sb.st_gid;
	if (!(anonarch & ANON_MTIME))
		t_mtime = arcn->sb.st_mtime;

	if (sizeof(time_t) > 4 && t_mtime > (time_t)999999999999ULL) {
		paxwarn(1, "%s overflow for %s", "mtime", arcn->org_name);
		t_mtime = (time_t)999999999999UL;
	}
	if (t_uid > 999999UL) {
		paxwarn(1, "%s overflow for %s", "uid", arcn->org_name);
		t_uid = 999999UL;
	}
	if (t_gid > 999999UL) {
		paxwarn(1, "%s overflow for %s", "gid", arcn->org_name);
		t_gid = 999999UL;
	}
	if (arcn->sb.st_mode > 077777777UL) {
		paxwarn(1, "%s overflow for %s", "mode", arcn->org_name);
		arcn->sb.st_mode &= 077777777UL;
	}
	if ((uint64_t)arcn->sb.st_size > ((uint64_t)9999999999ULL)) {
		paxwarn(1, "%s overflow for %s", "size", arcn->org_name);
		return (1);
	}

	if (anonarch & ANON_DEBUG)
		paxwarn(0, "writing mode %8lo user %ld:%ld "
		    "mtime %08lX name '%s'", (u_long)arcn->sb.st_mode,
		    t_uid, t_gid, (u_long)t_mtime, extname);

	memset(&h, ' ', sizeof(HD_AR));

	if ((n = strlen(extname)) <= sizeof(h.ar_name)) {
		while (n)
			if (extname[--n] == ' ')
				break;
		if (n == 0) {
			memcpy(h.ar_name, extname, strlen(extname));
			extname = NULL;
			goto got_name;
		}
	}
	n = strlen(extname);
	/* assert: n <= PAXPATHLEN <= 9999999999999 */
	h.ar_name[0] = 0x23;
	h.ar_name[1] = 0x31;
	h.ar_name[2] = 0x2F;
	uar_itoa32(&(h.ar_name[3]), n);
 got_name:
	uar_itoa64(h.ar_mtime, t_mtime);
	uar_itoa32(h.ar_uid, t_uid);
	uar_itoa32(h.ar_gid, t_gid);
	uar_itoo32(h.ar_mode, arcn->sb.st_mode);
	uar_itoa64(h.ar_size, arcn->sb.st_size +
	    (extname ? strlen(extname) : 0));
	h.ar_magic[0] = 0x60;
	h.ar_magic[1] = 0x0A;
	arcn->pad = arcn->sb.st_size & 1;

	if (wr_rdbuf((void *)&h, sizeof(HD_AR)) < 0)
		return (-1);
	if (extname) {
		if (wr_rdbuf(extname, strlen(extname)) < 0)
			return (-1);
	}
	/* so let the data follow */
	return (0);
}

/*
 * return size of trailer
 */
off_t
uar_endrd(void)
{
	return (0);
}

/*
 * another artefact of paxtar integration
 */
int
uar_trail(ARCHD *ignore __attribute__((__unused__)),
    char *buf __attribute__((__unused__)),
    int in_resync __attribute__((__unused__)),
    int *cnt __attribute__((__unused__)))
{
	errx(1, "internal error: %s should never have been called",
	    "uar_trail");
}
