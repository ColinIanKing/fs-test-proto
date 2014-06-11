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
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "fs-test.h"
#include "fs-dump-results.h"

static const char *stat_result_table[] = {
	"Minimum",
	"Maximum",
	"Average",
	"StdDev"
};

static void label_to_str(char *dst, const char *src, const size_t len)
{
	const char *ptr1 = src;
	char *ptr2 = dst;
	ssize_t n = len;

	while (*ptr1) {
		if (n <= 0)
			return;
		if (isalnum(*ptr1))
			*ptr2++ = *ptr1;
		else if (isblank(*ptr1))
			*ptr2++ = '_';
		else if (*ptr1 == '%') {
			size_t sz = len > 7 ? 7 : len;
			strncpy(ptr2, "percent", sz);
			ptr2 += sz;
			n -= sz;
		} else if (*ptr1 == '/') {
			size_t sz = len > 5 ? 5 : len;
			strncpy(ptr2, "_per_", sz);
			ptr2 += sz;
			n -= sz;
		}
		ptr1++;
		n--;
	}
	*ptr2 = '\0';
}

int dump_results_csv(FILE *fp, stat_t *results)
{
	char buf[64];
	int i, j;

	/* Headings First */
	for (j = 0; stat_table[j].stat != STAT_MAX_VAL; j++) {
		if (stat_table[j].stat == STAT_NULL || stat_table[j].ignore)
			continue;
		if (stat_table[j].units)
			snprintf(buf, sizeof(buf), "%s (%s)", stat_table[j].label, stat_table[j].units);
		else
			snprintf(buf, sizeof(buf), "%s", stat_table[j].label);

		fprintf(fp, ", %s", buf);
	}
	fprintf(fp, "\n");

	/* Now the data */
	for (i = 0; i < STAT_RESULT_MAX; i++) {
		fprintf(fp, "%s", stat_result_table[i]);

		for (j = 0; stat_table[j].stat != STAT_MAX_VAL; j++) {
			stat_val_t s = stat_table[j].stat;

			if (s == STAT_NULL || stat_table[j].ignore)
				continue;

			fprintf(fp, ", %.3f", results[i].val[s]);
		}
		fprintf(fp, "\n");
	}
	return 0;
}

int dump_results_yaml(FILE *fp, stat_t *results)
{
	int i, j;

	fprintf(fp, "---\n");
	fprintf(fp, "fs-test-results:\n");
	for (j = 0; stat_table[j].stat != STAT_MAX_VAL; j++) {
		char buf1[128], buf2[256];
		stat_val_t s = stat_table[j].stat;

		if (s == STAT_NULL || stat_table[j].ignore)
			continue;

		if (stat_table[j].units)
			snprintf(buf1, sizeof(buf1), "%s (%s)", stat_table[j].label, stat_table[j].units);
		else
			snprintf(buf1, sizeof(buf1), "%s", stat_table[j].label);

		label_to_str(buf2, buf1, sizeof(buf2));

		fprintf(fp, "  - metric: %s\n", buf2);
		for (i = 0; i < STAT_RESULT_MAX; i++) {
			fprintf(fp, "    %s: %.3f\n",
				stat_result_table[i],
				results[i].val[s]);
		}
	}

	return 0;
}


int dump_results_json(FILE *fp, stat_t *results)
{
	int i, j;
	bool first = true;

	fprintf(fp, "{\n");
	fprintf(fp, "  \"fs-test-results\":\n");
	fprintf(fp, "  [\n");
	for (j = 0; stat_table[j].stat != STAT_MAX_VAL; j++) {
		char buf1[128], buf2[256];
		stat_val_t s = stat_table[j].stat;

		if (s == STAT_NULL || stat_table[j].ignore)
			continue;

		if (stat_table[j].units)
			snprintf(buf1, sizeof(buf1), "%s (%s)", stat_table[j].label, stat_table[j].units);
		else
			snprintf(buf1, sizeof(buf1), "%s", stat_table[j].label);

		label_to_str(buf2, buf1, sizeof(buf2));

		if (!first)
			fprintf(fp, ",\n");
		fprintf(fp, "    {\n");
		fprintf(fp, "      \"metric\":\"%s\"\n", buf2);
		for (i = 0; i < STAT_RESULT_MAX; i++) {
			fprintf(fp, "      \"%s\":%.3f\n",
				stat_result_table[i],
				results[i].val[s]);
		}
		fprintf(fp, "    }");

		first = false;
	}

	fprintf(fp, "\n");
	fprintf(fp, "  ]\n");
	fprintf(fp, "}\n");

	return 0;
}

int dump_results(const char *filename, stat_t *results)
{
	FILE *fp;
	int rc;
	const char *dot = strrchr(filename, '.');

	if (!dot)
		dot = "";

	if ((fp = fopen(filename, "w")) == NULL) {
		fprintf(stderr, "Cannot write output to %s\n", filename);
		return -1;
	}

	if (!strcmp(dot, ".csv"))
		rc = dump_results_csv(fp, results);
	else if (!strcmp(dot, ".yaml"))
		rc = dump_results_yaml(fp, results);
	else if (!strcmp(dot, ".json"))
		rc = dump_results_json(fp, results);
	else
		rc = dump_results_csv(fp, results);

	fclose(fp);

	return rc;
}
