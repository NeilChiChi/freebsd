/*-
 * Copyright (c) 2014 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "image.h"
#include "mkimg.h"
#include "scheme.h"

#ifndef FS_NANDFS
#define	FS_NANDFS	30
#endif

static struct mkimg_alias bsd_aliases[] = {
    {	ALIAS_FREEBSD_NANDFS, ALIAS_INT2TYPE(FS_NANDFS) },
    {	ALIAS_FREEBSD_SWAP, ALIAS_INT2TYPE(FS_SWAP) },
    {	ALIAS_FREEBSD_UFS, ALIAS_INT2TYPE(FS_BSDFFS) },
    {	ALIAS_FREEBSD_VINUM, ALIAS_INT2TYPE(FS_VINUM) },
    {	ALIAS_FREEBSD_ZFS, ALIAS_INT2TYPE(FS_ZFS) },
    {	ALIAS_NONE, 0 }
};

static u_int
bsd_metadata(u_int where)
{
	u_int secs;

	secs = BBSIZE / secsz;
	return ((where == SCHEME_META_IMG_START) ? secs : 0);
}

static int
bsd_write(lba_t imgsz, void *bootcode)
{
	u_char *buf, *p;
	struct disklabel *d;
	struct partition *dp;
	struct part *part;
	int error, n;
	uint16_t checksum;

	buf = malloc(BBSIZE);
	if (buf == NULL)
		return (ENOMEM);
	if (bootcode != NULL) {
		memcpy(buf, bootcode, BBSIZE);
		memset(buf + secsz, 0, secsz);
	} else
		memset(buf, 0, BBSIZE);

	imgsz = (lba_t)ncyls * nheads * nsecs;
	error = image_set_size(imgsz);
	if (error) {
		free(buf);
		return (error);
	}

	d = (void *)(buf + secsz);
	le32enc(&d->d_magic, DISKMAGIC);
	le32enc(&d->d_secsize, secsz);
	le32enc(&d->d_nsectors, nsecs);
	le32enc(&d->d_ntracks, nheads);
	le32enc(&d->d_ncylinders, ncyls);
	le32enc(&d->d_secpercyl, nsecs * nheads);
	le32enc(&d->d_secperunit, imgsz);
	le16enc(&d->d_rpm, 3600);
	le32enc(&d->d_magic2, DISKMAGIC);
	le16enc(&d->d_npartitions, (8 > nparts + 1) ? 8 : nparts + 1);
	le32enc(&d->d_bbsize, BBSIZE);

	dp = &d->d_partitions[RAW_PART];
	le32enc(&dp->p_size, imgsz);
	STAILQ_FOREACH(part, &partlist, link) {
		n = part->index + ((part->index >= RAW_PART) ? 1 : 0);
		dp = &d->d_partitions[n];
		le32enc(&dp->p_size, part->size);
		le32enc(&dp->p_offset, part->block);
		dp->p_fstype = ALIAS_TYPE2INT(part->type);
	}

	dp = &d->d_partitions[nparts + 1];
	checksum = 0;
	for (p = buf; p < (u_char *)dp; p += 2)
		checksum ^= le16dec(p);
	le16enc(&d->d_checksum, checksum);

	error = image_write(0, buf, BBSIZE / secsz);
	free(buf);
	return (error);
}

static struct mkimg_scheme bsd_scheme = {
	.name = "bsd",
	.description = "BSD disk label",
	.aliases = bsd_aliases,
	.metadata = bsd_metadata,
	.write = bsd_write,
	.nparts = 19,
	.bootcode = BBSIZE,
	.maxsecsz = 512
};

SCHEME_DEFINE(bsd_scheme);