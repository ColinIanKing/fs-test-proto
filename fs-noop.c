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
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "fs-test.h"

void *noop(void *ctxt)
{
	struct timeval tv;
	double time_start, time_end;
	test_context_t *test = (test_context_t *)ctxt;
	uint64_t secs = test->per_thread_file_size;

	time_start = timeval_to_double();
	while ((opt_flags & OPT_CONT) && secs) {
		tv.tv_sec = secs / 1000000;
		tv.tv_usec = secs % 1000000;
		if (tv.tv_sec > 1)
			tv.tv_sec = 1;

		secs -= ((1000000 * tv.tv_sec) + tv.tv_usec);
		select(0, NULL, NULL, NULL, &tv);
	}
	time_end = timeval_to_double();

	test->duration_s = time_end - time_start;
	test->response_time_ms = 0;
	test->rate = 0.0;
	test->ops = 0;
	test->op_rate = 0;

	return NULL;
}
