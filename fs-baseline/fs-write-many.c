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
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

#include "fs-test.h"

static void mk_filename(uint32_t *z, uint32_t *w, char *path, char *filename, size_t len)
{
	size_t i;
	char *ptr;

	ptr = filename + snprintf(filename, len, "%s/temp-%d-", path, getpid());

	for (i = 0; i < len - 1; i++)
		*ptr++ = 'a' + mwc(z, w) % 26;

	*ptr = '\0';
}

void *write_many(void *ctxt)
{
	void *buffer;
	int fd;
	double time_start, time_end;
	uint64_t fs, ops = 0;
	test_context_t *test = (test_context_t *)ctxt;
	uint32_t z, w;
	char filename[PATH_MAX];
	int i, count = 0;

	test->ret = 0;
	if (posix_memalign(&buffer, 4096, (size_t)test->block_size) < 0) {
		fprintf(stderr, "Cannot allocate block buffer: %d %s\n",
			errno, strerror(errno));
		test->ret = -ENOMEM;
		return NULL;
	}
	memset(buffer, test->instance & 0xff, test->block_size);
	fs = test->per_thread_file_size;

	time_start = timeval_to_double();

	z = 362436069;
	w = 521288629 + test->instance;
	while ((opt_flags & OPT_CONT) && (fs != 0)) {
		uint64_t bytes = test->block_size * (1 + (count & 31));
		size_t len = 16 + (count & 63);
		mk_filename(&z, &w, test->pathname, filename, len);

		fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | test->open_flags, S_IRUSR | S_IWUSR);
		if (fd < 0) {
			fprintf(stderr, "Cannot open for writing: %s: %d %s\n",
				test->filename, errno, strerror(errno));
			test->ret = -errno;
			free(buffer);
			return NULL;
		}

		if (bytes > fs)
			bytes = fs;

		fs -= bytes;

		while (bytes != 0) {
			size_t sz = bytes > test->block_size ? test->block_size : bytes;
			ssize_t n = write(fd, buffer, sz);
			if (n < 0) {
				fprintf(stderr, "Write failed: %d %s\n",
					errno, strerror(errno));
				test->ret = -errno;
				close(fd);
				goto out;
			}
			bytes -= n;
			ops++;
		}
		fsync(fd);
		close(fd);
		count++;
	}

	time_end = timeval_to_double();
	test->duration_s = time_end - time_start;
	test->response_time_ms = 1000 * test->duration_s / (double)test->blocks;
	test->rate = (double)test->per_thread_file_size / test->duration_s;
	test->ops = ops;
	test->op_rate = (double)ops / test->duration_s;

	z = 362436069;
	w = 521288629 + test->instance;
	for (i = 0; i < count; i++) {
		size_t len = 16 + (i & 63);
		mk_filename(&z, &w, test->pathname, filename, len);
		unlink(filename);
	}

out:
	free(buffer);

	return NULL;
}
