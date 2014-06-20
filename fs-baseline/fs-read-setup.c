/*
 * Copyright (C) 2014 Canonical
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Author Colin Ian King,  colin.king@canonical.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fs-test.h"

int read_init(test_context_t *test)
{
	int fd, rc = 0;
	void *buffer;
	uint64_t fs = test->file_size;

#define BUF_SIZE	(128 * 1024)

	/* Start off with clean already existing file */
	(void)unlink(test->filename);
	fd = open(test->filename, O_WRONLY | O_CREAT,  S_IRUSR | S_IWUSR);
	if (fd < 0) {
		fprintf(stderr, "Cannot open for writing: %s: %d %s\n",
			test->filename, errno, strerror(errno));
		return -errno;
	}
	if (posix_memalign(&buffer, 4096, BUF_SIZE) < 0) {
		fprintf(stderr, "Cannot allocate block buffer: %d %s\n",
			errno, strerror(errno));
		close(fd);
		return -ENOMEM;
	}

	memset(buffer, 0xff, BUF_SIZE);

	while ((opt_flags & OPT_CONT) && (fs != 0)) {
		size_t sz = fs > BUF_SIZE ? BUF_SIZE : fs;
		ssize_t n = write(fd, buffer, sz);
		if (n < 0) {
			fprintf(stderr, "Write failed: %d %s\n",
				errno, strerror(errno));
			rc = -errno;
			break;
		}
		fs -= n;
	}

	(void)close(fd);
	free(buffer);

	return rc;

#undef BUF_SIZE
}

int read_deinit(test_context_t *test)
{
	(void)unlink(test->filename);
	return 0;
}
