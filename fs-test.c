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
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>

#define TEST_NAME		"write-test"

#define MAX_THREADS		99

#define OPT_BLOCK_SIZE		(0x00000001)
#define OPT_FILE_SIZE		(0x00000002)
#define OPT_BLOCKS		(0x00000004)
#define OPT_STATS		(0x00000008)
#define OPT_HUMAN_READABLE	(0x00000010)
#define OPT_THREAD_STATS	(0x00000020)
#define OPT_CONT		(0x00000040)

typedef struct test_context_t test_context_t;

typedef struct {
	const char *op_name;
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
	stat_val_t stat;
	const char *label;
	const char *units;
	const double scale;
	const bool delta;
	const bool ignore;
} stat_table_t;

static const stat_table_t stat_table[] = {
	{ STAT_DURATION,	"Duration",		"secs",		1.0,	false,	false },
	{ STAT_RATE,		"Rate",			"MB/sec", 1048576.0, 	false,	false },
	{ STAT_OP_RATE,		"Op-Rate",		"Ops/sec",	1.0, 	false,	false },
	{ STAT_NULL,		"",			"",		1.0,	false,	false },

	{ STAT_PID_UTIME,	"CPU user %",		NULL,		1.0,	true,	false },
	{ STAT_PID_STIME,	"CPU system %",		NULL,		1.0,	true,	false },
	{ STAT_PID_TTIME,	"CPU total %",		NULL,		1.0,	true,	false },
	{ STAT_NULL,		"",			"",		1.0,	false,	false },

	{ STAT_READS_COMPLETED,	"Reads Completed",	NULL,		1.0,	true,	false },
	{ STAT_READS_MERGED,	"Reads Merged",		NULL,		1.0,	true,	false },
	{ STAT_SECTORS_READ,	"Sectors Read",		NULL,		1.0,	true,	false },
	{ STAT_READ_TIME_MS,	"Read Time",		"ms",		1.0,	true,	false },
	{ STAT_PID_IO_RCHAR,	"Read",			"MB",	  1048576.0,	true,	false },
	{ STAT_PID_IO_SYSCR,	"Read Syscalls",	NULL,		1.0,	true,	false },
	{ STAT_PID_IO_READ,	"Read (from device)",	"MB",	  1048576.0,	true,	false },
	{ STAT_NULL,		"",			"",		1.0,	false,	false },

	{ STAT_WRITES_COMPLETED, "Writes Completed",	NULL,		1.0,	true,	false },
	{ STAT_WRITES_MERGED,	"Writes Merged",	NULL,		1.0,	true,	false },
	{ STAT_SECTORS_WRITTEN,	"Sectors Written",	NULL,		1.0,	true,	false },
	{ STAT_WRITE_TIME_MS,	"Write Time",		"ms",		1.0,	true,	false },
	{ STAT_PID_IO_WCHAR,	"Write",		"MB",	  1048576.0,	true,	false },
	{ STAT_PID_IO_SYSCW,	"Write Syscalls",	NULL,		1.0,	true,	false },
	{ STAT_PID_IO_WRITE,	"Write (to device)",	"MB",	  1048576.0,	true,	false },
	{ STAT_PID_IO_CANCEL_WRITE, "Cancelled Write",	"MB",	  1048576.0,	true,	false },
	{ STAT_NULL,		"",			"",		1.0,	false,	false },

	{ STAT_RESPONSE_TIME,	"Response Time",	"us",	     0.001,	true,	false },
	{ STAT_IO_IN_PROGRESS,	"I/O In Progress",	NULL,		1.0,	true,	true },
	{ STAT_IO_TIME_SPENT_MS, "I/O Time Spent",	"ms",		1.0,	true,	false },
	{ STAT_IO_TIME_SPENT_WEIGHTED_MS, "I/O Time Spent (Weighted)", "ms",	1.0,	true,	true },
	{ STAT_NULL,		"",			"",		1.0,	false,	false },

	{ STAT_SLAB_BLKDEV_QUEUE, "Slab Blkdev Queue Objs", NULL,	1.0,	true,	false },
	{ STAT_SLAB_BLKDEV_REQUESTS, "Slab Blkdev Request Objs", NULL,	1.0,	true,	false },
	{ STAT_SLAB_BDEV_CACHE,	"Slab Bdev Cache Objs", NULL,		1.0,	true,	false },
	{ STAT_SLAB_BUFFER_HEAD,"Slab Buffer Head Objs", NULL,	1.0,	true,	false },
	{ STAT_SLAB_INODE_CACHE,"Slab Inode Cache Objs", NULL,	1.0,	true,	false },
	{ STAT_SLAB_DENTRY_CACHE,"Slab Dentry Cache Objs", NULL,	1.0,	true,	false },
	{ STAT_NULL,		"",			"",		1.0,	false,	false },

	{ STAT_MEM_TOTAL,	"Memory Total",		"MB",	     1024.0,	false,	false },
	{ STAT_MEM_FREE,	"Memory Free",		"MB",	     1024.0,	false,	false },
	{ STAT_MEM_AVAILABLE,	"Memory Available",	"MB",	     1024.0,	false,	false },
	{ STAT_MEM_BUFFERS,	"Memory Buffers",	"MB",	     1024.0,	false,	false },
	{ STAT_MEM_CACHED,	"Memory Cached",	"MB",        1024.0,	false,	false },
	{ STAT_MEM_DIRTY,	"Memory Dirty",		"MB",	     1024.0,	false,	false },
	{ STAT_MEM_WRITEBACK,	"Memory Writeback",	"MB",	     1024.0,	false,	false },

	{ STAT_MAX_VAL,		NULL,			NULL,		1.0,	false,	false }
};

typedef struct {
	double		val[STAT_MAX_VAL];
} stat_t;

typedef struct {
	const char ch;		/* Scaling suffix */
	const uint64_t scale;	/* Amount to scale by */
} scale_t;

static unsigned int opt_flags = OPT_CONT;

static void init_stats(stat_t *stat_vals)
{
	int i;

	for (i = 0; i < STAT_MAX_VAL; i++)
		stat_vals->val[i] = 0.0;
}

static int read_diskstats(const char *path, stat_t *stat_vals)
{
	FILE *fp;
	int rc = -1;
	struct stat buf;

	if (stat(path, &buf) < 0) {
		fprintf(stderr, "Cannot stat %s\n", path);
		return rc;
	}

	if ((fp = fopen("/proc/diskstats", "r")) == NULL) {
		fprintf(stderr, "Cannot open /proc/diskstats\n");
		return rc;
	}

	while (!feof(fp)) {
		char fbuf[4096];
		unsigned int major, minor;

		fgets(fbuf, sizeof(fbuf), fp);
		sscanf(fbuf, "%u %u", &major, &minor);
		if (major == major(buf.st_dev) &&
		    minor == minor(buf.st_dev)) {
			sscanf(fbuf, "%*d %*d %*s"
				" %lf %lf %lf %lf %lf"
				" %lf %lf %lf %lf %lf",
				&stat_vals->val[STAT_READS_COMPLETED],
				&stat_vals->val[STAT_READS_MERGED],
				&stat_vals->val[STAT_SECTORS_READ],
				&stat_vals->val[STAT_READ_TIME_MS],
				&stat_vals->val[STAT_WRITES_COMPLETED],
				&stat_vals->val[STAT_WRITES_MERGED],
				&stat_vals->val[STAT_SECTORS_WRITTEN],
				&stat_vals->val[STAT_WRITE_TIME_MS],
				&stat_vals->val[STAT_IO_IN_PROGRESS],
				&stat_vals->val[STAT_IO_TIME_SPENT_MS]);
			rc = 0;
			break;
		}
	}
	fclose(fp);
	return rc;
}

void calc_stats_delta(stat_t *start, stat_t *end, stat_t *diff)
{
	int s;
	for (s = 0; stat_table[s].stat != STAT_MAX_VAL; s++) {
		stat_val_t i = stat_table[s].stat;

		if (i == STAT_NULL)
			continue;
		if (stat_table[s].delta)
			diff->val[i] = end->val[i] - start->val[i];
	}
}

static int read_memstats(stat_t *stat_vals)
{
	FILE *fp;

	if ((fp = fopen("/proc/meminfo", "r")) == NULL) {
		fprintf(stderr, "Cannot get memory total\n");
		return -1;
	}

	while (!feof(fp)) {
		char fbuf[4096];

		fgets(fbuf, sizeof(fbuf), fp);
		if (!strncmp(fbuf, "MemTotal:", 9)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_MEM_TOTAL]);
			continue;
		}
		if (!strncmp(fbuf, "MemFree:", 8)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_MEM_FREE]);
			continue;
		}
		if (!strncmp(fbuf, "MemAvailable:", 13)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_MEM_AVAILABLE]);
			continue;
		}
		if (!strncmp(fbuf, "Buffers:", 8)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_MEM_BUFFERS]);
			continue;
		}
		if (!strncmp(fbuf, "Cached:", 7)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_MEM_CACHED]);
			continue;
		}
		if (!strncmp(fbuf, "Dirty:", 6)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_MEM_DIRTY]);
			continue;
		}
		if (!strncmp(fbuf, "Writeback:", 10)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_MEM_WRITEBACK]);
			continue;
		}
	}
	fclose(fp);

	return 0;
}

static int read_pid_proc_io(stat_t *stat_vals)
{
	FILE *fp;
	char path[PATH_MAX];

	snprintf(path, sizeof(path), "/proc/%i/io", getpid());
	if ((fp = fopen(path, "r")) == NULL) {
		fprintf(stderr, "Cannot get per process I/O stats\n");
		return -1;
	}

	while (!feof(fp)) {
		char fbuf[4096];

		fgets(fbuf, sizeof(fbuf), fp);
		if (!strncmp(fbuf, "rchar:", 6)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_PID_IO_RCHAR]);
			continue;
		}
		if (!strncmp(fbuf, "wchar:", 6)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_PID_IO_WCHAR]);
			continue;
		}
		if (!strncmp(fbuf, "syscr:", 6)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_PID_IO_SYSCR]);
			continue;
		}
		if (!strncmp(fbuf, "syscw:", 6)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_PID_IO_SYSCW]);
			continue;
		}
		if (!strncmp(fbuf, "read_bytes:", 11)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_PID_IO_READ]);
			continue;
		}
		if (!strncmp(fbuf, "write_bytes:", 12)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_PID_IO_WRITE]);
			continue;
		}
		if (!strncmp(fbuf, "cancelled_write_bytes:", 22)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_PID_IO_CANCEL_WRITE]);
			continue;
		}
	}
	fclose(fp);

	return 0;
}

static int read_pid_proc_stat(stat_t *stat_vals)
{
	char path[PATH_MAX];
	char comm[20];
	unsigned long utime, stime, clock_ticks;
	FILE *fp;

	clock_ticks = sysconf(_SC_CLK_TCK);
	snprintf(path, sizeof(path), "/proc/%i/stat", getpid());
	if ((fp = fopen(path, "r")) == NULL) {
		fprintf(stderr, "Cannot get per process stats\n");
		return -1;
	}
	if (fscanf(fp, "%*d (%20[^)]) %*c %*d %*d %*d %*d "
		       "%*d %*u %*u %*u %*u %*u %16lu %16lu",
                        comm, &utime, &stime) == 3) {
		stat_vals->val[STAT_PID_UTIME] = (double)(100.0 * utime) / clock_ticks;
		stat_vals->val[STAT_PID_STIME] = (double)(100.0 * stime) / clock_ticks;
		stat_vals->val[STAT_PID_TTIME] = (double)(100.0 * (stime + utime)) / clock_ticks;
	}
	fclose(fp);

	return 0;
}

static inline void calc_pid_proc_stat(double duration, stat_t *stat_vals)
{
	int i;

	for (i = STAT_PID_UTIME; i <= STAT_PID_TTIME; i++)
		stat_vals->val[i] /= duration;
}

static int read_slab_stat(stat_t *stat_vals)
{
	FILE *fp;
	if ((fp = fopen("/proc/slabinfo", "r")) == NULL) {
		fprintf(stderr, "Cannot get slabinfo stats\n");
		return -1;
	}

	while (!feof(fp)) {
		char fbuf[4096];

		fgets(fbuf, sizeof(fbuf), fp);
		if (!strncmp(fbuf, "blkdev_queue", 12)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_SLAB_BLKDEV_QUEUE]);
			continue;
		}
		if (!strncmp(fbuf, "blkdev_requests", 15)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_SLAB_BLKDEV_REQUESTS]);
			continue;
		}
		if (!strncmp(fbuf, "bdev_cache", 10)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_SLAB_BDEV_CACHE]);
			continue;
		}
		if (!strncmp(fbuf, "buffer_head", 11)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_SLAB_BUFFER_HEAD]);
			continue;
		}
		if (!strncmp(fbuf, "inode_cache", 11)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_SLAB_INODE_CACHE]);
			continue;
		}
		if (!strncmp(fbuf, "dentry", 6)) {
			sscanf(fbuf, "%*s %lf", &stat_vals->val[STAT_SLAB_DENTRY_CACHE]);
			continue;
		}
	}
	fclose(fp);

	return 0;
}


/*
 *  mcw()
 *	fast psuedo random number generator, see
 *	http://www.cse.yorku.ca/~oz/marsaglia-rng.html
 */
static uint32_t mwc(uint32_t *z, uint32_t *w)
{
	*z = 36969 * (*z & 65535) + (*z >> 16);
	*w = 18000 * (*w & 65535) + (*w >> 16);
	return (*z << 16) + *w;
}

static int drop_caches(void)
{
	int i;

	(void)sync();
	for (i = 1; i < 4; i++) {
		FILE *fp;
		if ((fp = fopen("/proc/sys/vm/drop_caches", "w")) == NULL) {
			fprintf(stderr, "Cannot drop caches, need to run as root\n");
			return -EACCES;
		}
		fprintf(fp, "%d\n", i);
		fclose(fp);
	}

	return 0;
}

static uint64_t get_mem_total(void)
{
	FILE *fp;
	uint64_t mem_total = 0;

	if ((fp = fopen("/proc/meminfo", "r")) == NULL) {
		fprintf(stderr, "Cannot get memory total\n");
		return 0;
	}
	while (!feof(fp)) {
		if (fscanf(fp, "MemTotal: %" SCNu64 , &mem_total) == 1)
			break;
	}
	fclose(fp);

	return mem_total * 1024;
}

/*
 *  size_to_str_h()
 *	report size in different units
 */
static char *size_to_str_h(const double val, const char *fmt, char *buf, size_t len)
{
	double s;
	char *units;

	memset(buf, 0, len);

	if (val < 9 * 1024ULL) {
		s = val;
		units = "B";
	} else if (val < 9 * 1024ULL * 1024ULL) {
		s = val / 1024ULL;
		units = "KB";
	} else if (val < 9 * 1024ULL * 1024ULL * 1024ULL) {
		s = val / (1024ULL * 1024ULL);
		units = "MB";
	} else {
		s = val / (1024ULL * 1024ULL * 1024ULL);
		units = "GB";
	}
	snprintf(buf, len, fmt, s);
	strncat(buf, " ", len);
	strncat(buf, units, len);
	return buf;
}

static char *size_to_str(const double val, const char *fmt, char *buf, size_t len)
{
	if (opt_flags & OPT_HUMAN_READABLE) {
		return size_to_str_h(val, fmt, buf, len);
	} else {
		snprintf(buf, len, fmt, val);

		return buf;
	}
}

/*
 *  timeval_to_double()
 *	convert timeval to seconds as a double
 */
static inline double timeval_to_double(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
}

/*
 *  count_bits()
 *	count bits set, from C Programming Language 2nd Ed
 */
static unsigned int count_bits(const unsigned int val)
{
	register unsigned int c, n = val;

	for (c = 0; n; c++)
		n &= n - 1;

	return c;
}

/*
 *  get_u64
 *	get a u64
 */
static uint64_t get_u64(const char *const str)
{
	uint64_t val;

	errno = 0;
	val = (uint64_t)strtoull(str, NULL, 10);
	if (errno) {
		fprintf(stderr, "Invalid value %s.\n", str);
		exit(EXIT_FAILURE);
	}
	return val;
}

static uint32_t get_u32(const char *const str)
{
	uint32_t val;

	errno = 0;
	val = (uint32_t)strtoul(str, NULL, 10);
	if (errno) {
		fprintf(stderr, "Invalid value %s.\n", str);
		exit(EXIT_FAILURE);
	}
	return val;
}

/*
 *  get_u64_scale()
 *	get a value and scale it by the given scale factor
 */
static uint64_t get_u64_scale(
	const char *const str,
	const scale_t scales[],
	const char *const msg)
{
	uint64_t val;
	size_t len = strlen(str);
	int i;
	char ch;

	if (len == 0) {
		fprintf(stderr, "Value %s is an invalid size.\n", str);
		exit(EXIT_FAILURE);
	}
	val = get_u64(str);
	len--;
	ch = str[len];

	if (isdigit(ch))
		return val;

	ch = tolower(ch);
	for (i = 0; scales[i].ch; i++) {
		if (ch == scales[i].ch)
			return val * scales[i].scale;
	}

	printf("Illegal %s specifier %c\n", msg, str[len]);
	exit(EXIT_FAILURE);
}

/*
 *  get_u64_byte()
 *	size in bytes, K bytes, M bytes or G bytes
 */
static uint64_t get_u64_byte(const char *const str)
{
	static const scale_t scales[] = {
		{ 'b', 	1 },
		{ 'k',  1 << 10 },
		{ 'm',  1 << 20 },
		{ 'g',  1 << 30 },
		{ 0,    0 },
	};

	return get_u64_scale(str, scales, "length");
}

static int write_init(test_context_t *test)
{
	int fd;

	/* Start off with clean file */
	(void)unlink(test->filename);
	fd = creat(test->filename, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		fprintf(stderr, "Cannot create file %s: %d %s\n",
			test->filename, errno, strerror(errno));
		return -errno;
	}
	(void)close(fd);
	(void)sync();

	return 0;
}

static int write_deinit(test_context_t *test)
{
	(void)unlink(test->filename);
	return 0;
}

static void *write_seq(void *ctxt)
{
	void *buffer;
	int fd;
	double time_start, time_end;
	uint64_t fs, ops = 0;
	test_context_t *test = (test_context_t *)ctxt;

	test->ret = 0;

	fd = open(test->filename, O_WRONLY | test->open_flags, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		fprintf(stderr, "Cannot open for writing: %s: %d %s\n",
			test->filename, errno, strerror(errno));
		test->ret = -errno;
		return NULL;
	}
	if (lseek(fd, (off_t)(test->instance * test->per_thread_file_size), SEEK_SET) < 0) {
		fprintf(stderr, "Cannot seek: %s: %d %s\n",
			test->filename, errno, strerror(errno));
		test->ret = -errno;
		close(fd);
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
		ssize_t n = write(fd, buffer, sz);
		if (n < 0) {
			fprintf(stderr, "Write failed: %d %s\n",
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

static void *write_rnd(void *ctxt)
{
	test_context_t *test = (test_context_t *)ctxt;
	void *buffer;
	int fd;
	double time_start, time_end;
	uint64_t fs, ops = 0;
	uint32_t z = 362436069, w = 521288629 + test->instance;
	off_t  offset = (off_t)(test->instance * test->per_thread_file_size);

	test->ret = 0;

	fd = open(test->filename, O_WRONLY | test->open_flags, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		fprintf(stderr, "Cannot open for writing: %s: %d %s\n",
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

		n = write(fd, buffer, sz);
		if (n < 0) {
			fprintf(stderr, "Write failed: %d %s\n",
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

static int read_init(test_context_t *test)
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

static int read_deinit(test_context_t *test)
{
	(void)unlink(test->filename);
	return 0;
}

static void *read_seq(void *ctxt)
{
	void *buffer;
	int fd;
	double time_start, time_end;
	uint64_t fs, ops = 0;
	test_context_t *test = (test_context_t *)ctxt;

	test->ret = 0;

	fd = open(test->filename, O_RDONLY | test->open_flags, S_IRUSR);
	if (fd < 0) {
		fprintf(stderr, "Cannot open for read: %s: %d %s\n",
			test->filename, errno, strerror(errno));
		test->ret = -errno;
		return NULL;
	}
	if (lseek(fd, (off_t)(test->instance * test->per_thread_file_size), SEEK_SET) < 0) {
		fprintf(stderr, "Cannot seek: %s: %d %s\n",
			test->filename, errno, strerror(errno));
		test->ret = -errno;
		close(fd);
		return NULL;
	}

	if (posix_memalign(&buffer, 4096, (size_t)test->block_size) < 0) {
		fprintf(stderr, "Cannot allocate block buffer: %d %s\n",
			errno, strerror(errno));
		test->ret = -ENOMEM;
		close(fd);
		return NULL;
	}

	time_start = timeval_to_double();
	fs = test->per_thread_file_size;

	while ((opt_flags & OPT_CONT) && (fs != 0)) {
		size_t sz = fs > test->block_size ? test->block_size : fs;
		ssize_t n = read(fd, buffer, sz);
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

static void *read_rnd(void *ctxt)
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

static void *read_write_rnd(void *ctxt)
{
	test_context_t *test = (test_context_t *)ctxt;
	void *buffer;
	int fd;
	double time_start, time_end;
	uint64_t fs, ops = 0;
	uint32_t z = 362436069, w = 521288629 + test->instance;
	off_t  offset = (off_t)(test->instance * test->per_thread_file_size);

	test->ret = 0;

	fd = open(test->filename, O_RDWR | test->open_flags, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		fprintf(stderr, "Cannot open for reading and writing: %s: %d %s\n",
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

		if ((mwc(&z, &w) & 255) > 127) {
			n = write(fd, buffer, sz);
			if (n < 0) {
				fprintf(stderr, "Write failed: %d %s\n",
					errno, strerror(errno));
				test->ret = -errno;
				goto out;
			}
		} else {
			n = read(fd, buffer, sz);
			if (n < 0) {
				fprintf(stderr, "Read failed: %d %s\n",
					errno, strerror(errno));
				test->ret = -errno;
				goto out;
			}
		}
		fs -= sz;
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

static void *rewrite_seq(void *ctxt)
{
	void *buffer;
	int fd;
	double time_start, time_end;
	uint64_t fs, ops = 0;
	test_context_t *test = (test_context_t *)ctxt;
	int i;

	test->ret = 0;

	fd = open(test->filename, O_WRONLY | test->open_flags, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		fprintf(stderr, "Cannot open for writing: %s: %d %s\n",
			test->filename, errno, strerror(errno));
		test->ret = -errno;
		return NULL;
	}
	if (lseek(fd, (off_t)(test->instance * test->per_thread_file_size), SEEK_SET) < 0) {
		fprintf(stderr, "Cannot seek: %s: %d %s\n",
			test->filename, errno, strerror(errno));
		test->ret = -errno;
		close(fd);
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

	for (i = 0; i < 2; i++) {
		fs = test->per_thread_file_size;
		while ((opt_flags & OPT_CONT) && (fs != 0)) {
			size_t sz = fs > test->block_size ? test->block_size : fs;
			ssize_t n = write(fd, buffer, sz);
			if (n < 0) {
				fprintf(stderr, "Write failed: %d %s\n",
					errno, strerror(errno));
				test->ret = -errno;
				goto out;
			}
			fs -= n;
			ops++;
		}
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

static void mk_filename(uint32_t *z, uint32_t *w, char *path, char *filename, size_t len)
{
	size_t i;
	char *ptr;

	ptr = filename + snprintf(filename, len, "%s/temp-%d-", path, getpid());

	for (i = 0; i < len - 1; i++)
		*ptr++ = 'a' + mwc(z, w) % 26;

	*ptr = '\0';
}

static void *write_many(void *ctxt)
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

static test_info_t test_info[] = {
	{ "Write",	write_seq,	write_init,	write_deinit,	"wr_seq",	"Write Sequential" },
	{ "Write",	write_rnd,	write_init,	write_deinit,	"wr_rnd",	"Write Random" },
	{ "Read",	read_seq,	read_init,	read_deinit,	"rd_seq",	"Read Sequential" },
	{ "Read",	read_rnd,	read_init,	read_deinit,	"rd_rnd",	"Read Random" },
	{ "Rd+Wr",	read_write_rnd,	read_init,	read_deinit,	"rdwr_rnd",	"Read+Write Random" },
	{ "Rewrite",	rewrite_seq,	write_init,	write_deinit,	"rewr_seq",	"Rewrite Sequentual" },
	{ "WrMany",	write_many,	NULL,		NULL,		"wr_many",	"Write Many" },
	{ NULL,		NULL,		NULL,		NULL,		NULL,		NULL }
};

void calculate_stats(
	uint32_t repeats,
	stat_t *stat_vals,
	stat_t *results)
{
	uint32_t s, r;

	for (s = 0; stat_table[s].stat != STAT_MAX_VAL; s++) {
		double sum = 0.0;
		stat_val_t i = stat_table[s].stat;

		if (i == STAT_NULL)
			continue;
		for (r = 0; r < repeats; r++)
			stat_vals[r].val[i] /= stat_table[s].scale;

		results[STAT_MIN].val[i] = stat_vals[0].val[i];
		results[STAT_MAX].val[i] = stat_vals[0].val[i];
		results[STAT_AVERAGE].val[i] = 0.0;

		for (r = 0; r < repeats; r++) {
			double val = stat_vals[r].val[i];
			results[STAT_AVERAGE].val[i] += val;
			if (results[STAT_MIN].val[i] > val)
				results[STAT_MIN].val[i] = val;
			if (results[STAT_MAX].val[i] < val)
				results[STAT_MAX].val[i] = val;
		}
		results[STAT_AVERAGE].val[i] /= (double)repeats;
		for (r = 0; r < repeats; r++) {
			double d = stat_vals[r].val[i] - results[STAT_AVERAGE].val[i];
			d = d * d;
			sum += d;
		}
		sum /= (double)repeats;
		results[STAT_STDDEV].val[i] = sqrt(sum);
	}
}

static void show_tests(void)
{
	int i;

	printf("\nTests available are:\n");
	for (i = 0; test_info[i].test; i++)
		printf("  %-9s - %s\n", test_info[i].tag, test_info[i].name);
}

/*
 *  show_usage()
 *	show options
 */
static void show_usage(void)
{
	printf("%s, version %s\n\n", TEST_NAME, VERSION);
	printf("Usage: %s [options]\n", TEST_NAME);
	printf("  -h\tprint this help.\n"
	       "  -H\tHuman readable sizes.\n"
	       "  -x\tname, name of test to execute.\n"
	       "  -r\trepeats, number of test repeats, default is 1.\n"
	       "  -t\tthreads, number of threaded workers, default is 1.\n"
	       "  -T\tshow per thread stats (just for debugging).\n"
	       "  -b\tsize, block size of writes.\n"
	       "  -a\tuse O_NOATIME.\n"
	       "  -d\tuse O_DIRECT.\n"
	       "  -s\tuse O_SYNC.\n"
	       "  -l\tlength, specify length of file.\n"
	       "  -n\tblocks, specify length by number of blocks.\n"
	       "  -p\tpathname, directory to write test file.\n"
	       "  -S\tdump out full statistics of performance.\n");
	show_tests();
	printf("\n");
}

void sighandler(int dummy)
{
	(void)dummy;

	opt_flags &= ~OPT_CONT;
}

int main(int argc, char **argv)
{
	int n, rc = EXIT_SUCCESS;
	uint32_t i, j, repeats = 1, r, num_threads = 1, t;
	char filename[PATH_MAX];
	char buf[64];
	char *pathname, *opt_test;
	stat_t *stat_vals, results[STAT_RESULT_MAX];
	test_info_t *ti = NULL;
	uint64_t mem_total;
	struct sigaction new_action, old_action;

	test_context_t tests[MAX_THREADS], test;

	memset(&test, 0, sizeof(test));

	for (;;) {
		int c = getopt(argc, argv, "adsb:l:n:hHp:r:t:Tx:");
		if (c == -1)
			break;
		switch (c) {
		case '?':
		case 'h':
			show_usage();
			exit(EXIT_SUCCESS);
		case 'r':
			repeats = get_u32(optarg);
			break;
		case 't':
			num_threads = get_u32(optarg);
			if (num_threads > MAX_THREADS) {
				fprintf(stderr, "Maximum of %d threads allowed\n", MAX_THREADS);
				exit(EXIT_FAILURE);
			}
			break;
		case 'T':
			opt_flags |= OPT_THREAD_STATS;
			break;
		case 'b':
			test.block_size = get_u64_byte(optarg);
			opt_flags |= OPT_BLOCK_SIZE;
			break;
		case 'a':
			test.open_flags |= O_NOATIME;
			break;
		case 'd':
			test.open_flags |= O_DIRECT;
			break;
		case 's':
			test.open_flags |= O_SYNC;
			break;
		case 'l':
			test.file_size = get_u64_byte(optarg);
			opt_flags |= OPT_FILE_SIZE;
			break;
		case 'n':
			test.blocks = get_u64(optarg);
			opt_flags |= OPT_BLOCKS;
			break;
		case 'p':
			pathname = optarg;
			break;
		case 'S':
			opt_flags |= OPT_STATS;
			break;
		case 'H':
			opt_flags |= OPT_HUMAN_READABLE;
			break;
		case 'x':
			opt_test = optarg;
			break;
		default:
			show_usage();
			exit(EXIT_FAILURE);
		}
	}

	if (pathname == NULL) {
		fprintf(stderr, "Must specify pathname with -p option\n");
		exit(EXIT_FAILURE);
	}
	if (opt_test == NULL) {
		fprintf(stderr, "Must specify test to run\n");
		exit(EXIT_FAILURE);
	}
	for (ti = test_info; ti->test; ti++) {
		if (!strcmp(opt_test, ti->tag))
			break;
	}
	if (!ti->test) {
		fprintf(stderr, "%s is not a valid test name\n", opt_test);
		show_tests();
		exit(EXIT_FAILURE);
	}

	n = count_bits(opt_flags & (OPT_BLOCK_SIZE | OPT_FILE_SIZE | OPT_BLOCKS));
	if (n != 2) {
		fprintf(stderr, "Must specify either -b and -l, -b and -n, -l and -n options\n");
		exit(EXIT_FAILURE);
	}

	if ((opt_flags & OPT_BLOCK_SIZE) == 0) {
		test.block_size = test.file_size / test.blocks;
		test.per_thread_file_size = test.file_size / num_threads;
	}
	if ((opt_flags & OPT_FILE_SIZE) == 0) {
		test.file_size = test.block_size * test.blocks;
		test.per_thread_file_size = (test.block_size * test.blocks) / num_threads;
		test.file_size = test.per_thread_file_size * num_threads;
	}
	if ((opt_flags & OPT_BLOCKS) == 0) {
		test.per_thread_file_size = test.file_size / num_threads;
		test.blocks = test.file_size / test.block_size;
	}
	test.per_thread_blocks = test.per_thread_file_size / test.block_size;
	test.d_per_thread_blocks = (double)test.per_thread_file_size / test.block_size;

	if ((mem_total = get_mem_total()) == 0) {
		exit(EXIT_FAILURE);
	}
	if (test.file_size * num_threads < (mem_total * 2)) {
		fprintf(stderr, "WARNING: Recommend file size to be at least %-s\n",
			size_to_str(mem_total * 2, "%.3f", buf, sizeof(buf)));
	}

	stat_vals = calloc((size_t)repeats, sizeof(stat_t));
	if (stat_vals == NULL) {
		fprintf(stderr, "Out of memory allocating stats\n");
		exit(EXIT_FAILURE);
	}

	printf("Running test %s (%s)\n", ti->tag, ti->name);
	printf("%s bytes: %" PRIu32 " threads x %" PRIu64 " byte sized blocks x %.1f blocks\n",
		size_to_str_h(test.file_size, "%.2f", buf, sizeof(buf)),
		num_threads, test.block_size, test.d_per_thread_blocks);

	snprintf(filename, sizeof(filename), "%s/temp-%d", pathname, getpid());

	test.filename = filename;
	test.pathname = pathname;
	test.test_info = ti;

	new_action.sa_handler = sighandler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;
	sigaction(SIGINT, &new_action, &old_action);

	printf("          Duration   %8.8s Rate %11.11ss  %s Resp.\n",
		ti->op_name, ti->op_name, ti->op_name);
	printf("           (secs)        (per sec)    (per sec)  Time (ms)\n");
	for (r = 0; (opt_flags && OPT_CONT) && (r < repeats); r++) {
		double duration;
		double time_start, time_end;
		uint64_t ops = 0;
		stat_t stat_start, stat_end;

		init_stats(&stat_vals[r]);
		init_stats(&stat_start);
		init_stats(&stat_end);

		if (ti->test_init) {
			if (ti->test_init(&test) < 0) {
				rc = EXIT_FAILURE;
				break;
			}
		}
		read_pid_proc_stat(&stat_start);
		read_slab_stat(&stat_start);
		read_diskstats(test.pathname, &stat_start);
		read_pid_proc_io(&stat_start);
		(void)drop_caches();

		time_start = timeval_to_double();
		for (t = 0; t < num_threads; t++) {
			tests[t] = test;
			tests[t].instance = t;

			if (pthread_create(&tests[t].thread, NULL, ti->test, &tests[t]) < 0) {
				fprintf(stderr, "Cannot start worker thread instance %" PRIu32 "\n", t);
				rc = EXIT_FAILURE;
				break;
			}
		}

		for (t = 0; t < num_threads; t++) {
			void *ret;
			test_context_t *test = &tests[t];

			pthread_join(test->thread, &ret);
		}
		time_end = timeval_to_double();
		duration = time_end - time_start;

		read_pid_proc_io(&stat_end);
		read_pid_proc_stat(&stat_end);
		read_slab_stat(&stat_end);
		read_diskstats(test.pathname, &stat_end);
		read_memstats(&stat_vals[r]);

		if (ti->test_deinit) {
			if (ti->test_deinit(&test) < 0) {
				rc = EXIT_FAILURE;
				break;
			}
		}
		calc_stats_delta(&stat_start, &stat_end, &stat_vals[r]);
		calc_pid_proc_stat(duration, &stat_vals[r]);

		if (!(opt_flags & OPT_CONT))
			break;

		for (t = 0; t < num_threads; t++) {
			test_context_t *test = &tests[t];
			ops += test->ops;

			if (opt_flags & OPT_THREAD_STATS) {
				printf("Thread %-2" PRIu32 " %8.3f %s %12.3f %12.7f\n",
					t,
					test->duration_s,
					size_to_str(test->rate, "%12.3f", buf, sizeof(buf)),
					test->op_rate,
					test->response_time_ms);
			}
		}

		stat_vals[r].val[STAT_DURATION] = duration;
		stat_vals[r].val[STAT_RATE] = (double)(ops * test.block_size) / duration;
		stat_vals[r].val[STAT_OP_RATE] = (double)ops / duration;
		stat_vals[r].val[STAT_RESPONSE_TIME] = 1000.0 * duration / (double)ops;

		printf("Round %-2" PRIu32 "  %8.3f %12s %12.3f %12.7f\n",
			r,
			duration,
			size_to_str(stat_vals[r].val[STAT_RATE], "%12.3f", buf, sizeof(buf)),
			stat_vals[r].val[STAT_OP_RATE],
			stat_vals[r].val[STAT_RESPONSE_TIME]);
	}

	if (!(opt_flags & OPT_CONT)) {
		fprintf(stderr, "Aborted!\n");
		goto out;
	}

	calculate_stats(repeats, stat_vals, results);

	printf("\n%25.25s %12s %12s %12s %12s\n",
		"", "Minimum", "Maximum", "Average", "Std.Dev.");
	for (j = 0; stat_table[j].stat != STAT_MAX_VAL; j++) {
		char buf[64];
		stat_val_t s = stat_table[j].stat;

		if (s == STAT_NULL) {
			printf("\n");
			continue;
		}

		if (stat_table[j].ignore)
			continue;
		if (stat_table[j].units)
			snprintf(buf, sizeof(buf), "%s (%s)", stat_table[j].label, stat_table[j].units);
		else
			snprintf(buf, sizeof(buf), "%s", stat_table[j].label);
		printf("%-25.25s ", buf);
		for (i = 0; i < STAT_RESULT_MAX; i++)
			printf("%12.3f ", results[i].val[s]);
		printf("\n");
	}

out:
	free(stat_vals);
	exit(rc);
}
