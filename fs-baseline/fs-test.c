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

#include "fs-test.h"
#include "fs-write-setup.h"
#include "fs-write-seq.h"
#include "fs-write-rnd.h"
#include "fs-write-many.h"
#include "fs-rewrite-seq.h"
#include "fs-read-setup.h"
#include "fs-read-seq.h"
#include "fs-read-rnd.h"
#include "fs-read-write-rnd.h"
#include "fs-noop.h"
#include "fs-dump-results.h"

#define TEST_NAME		"write-test"

#define MAX_THREADS		99

const stat_table_t stat_table[] = {
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
	{ STAT_IO_IN_PROGRESS,	"IO In Progress",	NULL,		1.0,	true,	true },
	{ STAT_IO_TIME_SPENT_MS, "IO Time Spent",	"ms",		1.0,	true,	false },
	{ STAT_IO_TIME_SPENT_WEIGHTED_MS, "IO Time Spent (Weighted)", "ms",	1.0,	true,	true },
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

static test_info_t test_info[] = {
	{ "Write",	write_seq,	write_init,	write_deinit,	"wr_seq",	"Write Sequential" },
	{ "Write",	write_rnd,	write_init,	write_deinit,	"wr_rnd",	"Write Random" },
	{ "Read",	read_seq,	read_init,	read_deinit,	"rd_seq",	"Read Sequential" },
	{ "Read",	read_rnd,	read_init,	read_deinit,	"rd_rnd",	"Read Random" },
	{ "Rd+Wr",	read_write_rnd,	read_init,	read_deinit,	"rdwr_rnd",	"Read+Write Random" },
	{ "Rewrite",	rewrite_seq,	write_init,	write_deinit,	"rewr_seq",	"Rewrite Sequentual" },
	{ "WrMany",	write_many,	NULL,		NULL,		"wr_many",	"Write Many" },
	{ "Noop",	noop,		NULL,		NULL,		"noop",		"No I/O ops" },
	{ NULL,		NULL,		NULL,		NULL,		NULL,		NULL }
};

typedef struct {
	const char ch;		/* Scaling suffix */
	const uint64_t scale;	/* Amount to scale by */
} scale_t;

unsigned int opt_flags = OPT_CONT;
static char *opt_ofilename = NULL;

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
uint32_t mwc(uint32_t *z, uint32_t *w)
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
		int c = getopt(argc, argv, "adsb:l:n:hHp:r:t:Tx:o:");
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
		case 'o':
			opt_ofilename = optarg;
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

	if (opt_ofilename)
		dump_results(opt_ofilename, results);

out:
	free(stat_vals);
	exit(rc);
}
