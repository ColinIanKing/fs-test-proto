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

#ifndef __FS_TEST_H__
#define __FS_TEST_H__

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/time.h>

#define OPT_BLOCK_SIZE		(0x00000001)
#define OPT_FILE_SIZE		(0x00000002)
#define OPT_BLOCKS		(0x00000004)
#define OPT_STATS		(0x00000008)
#define OPT_HUMAN_READABLE	(0x00000010)
#define OPT_THREAD_STATS	(0x00000020)
#define OPT_CONT		(0x00000040)
#define OPT_CSV			(0x00000080)
#define OPT_YAML		(0x00000100)
#define OPT_JSON		(0x00000200)

typedef enum {
	STAT_DURATION = 0,
	STAT_RATE,
	STAT_OP_RATE,

	STAT_MEM_TOTAL,
	STAT_MEM_FREE,
	STAT_MEM_AVAILABLE,
	STAT_MEM_BUFFERS,
	STAT_MEM_CACHED,
	STAT_MEM_DIRTY,
	STAT_MEM_WRITEBACK,

	STAT_RESPONSE_TIME,
	STAT_READS_COMPLETED,
	STAT_READS_MERGED,
	STAT_SECTORS_READ,
	STAT_READ_TIME_MS,
	STAT_WRITES_COMPLETED,
	STAT_WRITES_MERGED,
	STAT_SECTORS_WRITTEN,
	STAT_WRITE_TIME_MS,
	STAT_IO_IN_PROGRESS,
	STAT_IO_TIME_SPENT_MS,
	STAT_IO_TIME_SPENT_WEIGHTED_MS,

	STAT_PID_IO_RCHAR,
	STAT_PID_IO_WCHAR,
	STAT_PID_IO_SYSCR,
	STAT_PID_IO_SYSCW,
	STAT_PID_IO_READ,
	STAT_PID_IO_WRITE,
	STAT_PID_IO_CANCEL_WRITE,

	STAT_PID_UTIME,
	STAT_PID_STIME,
	STAT_PID_TTIME,

	STAT_SLAB_BLKDEV_QUEUE,
	STAT_SLAB_BLKDEV_REQUESTS,
	STAT_SLAB_BDEV_CACHE,
	STAT_SLAB_BUFFER_HEAD,
	STAT_SLAB_INODE_CACHE,
	STAT_SLAB_DENTRY_CACHE,

	STAT_MAX_VAL,
	STAT_NULL
} stat_val_t;

typedef enum {
	STAT_MIN = 0,
	STAT_MAX,
	STAT_AVERAGE,
	STAT_STDDEV,
	STAT_RESULT_MAX
} stat_result_t;

typedef struct {
	double val[STAT_MAX_VAL];
} stat_t;

typedef struct test_context_t test_context_t;

typedef struct {
	const char *op_name;			/* Test op name */
	void *(*test) (void *);
	int (*test_init) (test_context_t *);
	int (*test_deinit) (test_context_t *);
	const char *tag;		
	const char *name;
} test_info_t;

typedef struct test_context_t {
	/* Values passed into the test */
	uint64_t	block_size;
	uint64_t	file_size;
	uint64_t	per_thread_file_size;
	uint64_t	blocks;
	uint64_t	per_thread_blocks;
	double		d_per_thread_blocks;

	uint32_t	instance;
	char		*filename;
	char		*pathname;
	int 		open_flags;
	pthread_t	thread;
	test_info_t	*test_info;
	int		ret;

	/* Returned value from test */
	uint64_t	ops;
	double		duration_s;
	double		rate;
	double		op_rate;
	double		response_time_ms;
} test_context_t;

typedef struct {
	stat_val_t stat;
	const char *label;
	const char *units;
	const double scale;
	const bool delta;
	const bool ignore;
} stat_table_t;

/*
 *  timeval_to_double()
 *      convert timeval to seconds as a double
 */
static inline double timeval_to_double(void)
{
        struct timeval tv;

        gettimeofday(&tv, NULL);
        return (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
}

extern const stat_table_t stat_table[];
extern unsigned int opt_flags;

extern uint32_t mwc(uint32_t *z, uint32_t *w);

#endif
