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

#include "fs-test.h"

void *read_rnd(void *ctxt)
{
	test_context_t *test = (test_context_t *)ctxt;
	void *buffer;
	int fd;
	double time_start, time_end;
	uint64_t fs, ops = 0;
	uint32_t z = 362436069, w = 521288629 + test->instance;
	off_t  offset = (off_t)(test->instance * test->per_thread_file_size);

	test->ret = 0;

	fd = open(test->filename, O_RDONLY | test->open_flags, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		fprintf(stderr, "Cannot open for reading: %s: %d %s\n",
			test->filename, errno, strerror(errno));
		test->ret = -errno;
		return NULL;
	}
	if (posix_memalign(&buffer, 4096, (size_t)test->block_size) < 0) {
		fprintf(stderr, "Cannot allocate block buffer: %d %s\n",
			errno, strerror(errno));
		test->ret = -ENOMEM;
		close(fd);
		return NULL;
	}

	memset(buffer, test->instance & 0xff, test->block_size);

	time_start = timeval_to_double();
	fs = test->per_thread_file_size;

	while ((opt_flags & OPT_CONT) && (fs != 0)) {
		size_t sz = fs > test->block_size ? test->block_size : fs;
		ssize_t n;
		uint32_t r_mwc = mwc(&z, &w);
		off_t  offset_rnd = offset +
			((r_mwc % test->per_thread_blocks) * test->block_size);

		if (lseek(fd, offset_rnd, SEEK_SET) < 0) {
			fprintf(stderr, "Cannot seek: %s: %d %s\n",
				test->filename, errno, strerror(errno));
			test->ret = -errno;
			close(fd);
			return NULL;
		}

		n = read(fd, buffer, sz);
		if (n < 0) {
			fprintf(stderr, "Read failed: %d %s\n",
				errno, strerror(errno));
			test->ret = -errno;
			goto out;
		}
		fs -= n;
		ops++;
	}

	time_end = timeval_to_double();
	test->duration_s = time_end - time_start;
	test->response_time_ms = 1000 * test->duration_s / (double)test->blocks;
	test->rate = (double)test->per_thread_file_size / test->duration_s;
	test->ops = ops;
	test->op_rate = (double)ops / test->duration_s;

out:
	free(buffer);
	close(fd);

	return NULL;
}
