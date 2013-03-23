/* Copyright (C) 2011 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <errno.h>
#include "t3config/config.h"

static struct { const char *name; t3_bool expected_result; } tests[] = {
	{ "bar", t3_true },

	{ ".", t3_false },
	{ "./", t3_false },
	{ "/.", t3_false },
	{ "..", t3_false },
	{ "../", t3_false },
	{ "/..", t3_false },
	{ "../foo", t3_false },
	{ "/foo", t3_false },
	{ "/foo/.", t3_false },
	{ "/foo/..", t3_false },
	{ "foo/.", t3_false },
	{ "foo/..", t3_false },

	{ "foo/../bar", t3_true },

	{ "foo/../../bar", t3_false },

	{ "foo/../blah/../bar", t3_true },

	{ "foo/../blah/../../bar", t3_false },
	{ "foo/../../blah/../bar", t3_false },

	{ "foo/./../bar", t3_true },

	{ "foo/./../../bar", t3_false },

	{ "foo/./../blah/../bar", t3_true },

	{ "foo/./../blah/../../bar", t3_false },
	{ "foo/./../../blah/../bar", t3_false },
};

static const char *path[2] = { ".", NULL };

#if 0
/** Alert the user of a fatal error and quit.
    @param fmt The format string for the message. See fprintf(3) for details.
    @param ... The arguments for printing.
*/
static void fatal(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(EXIT_FAILURE);
}
#endif

int main(int argc, char *argv[]) {
	FILE *result;
	size_t i, failed = 0;

	(void) argc;
	(void) argv;

	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		result = t3_config_open_from_path(path, tests[i].name, T3_CONFIG_CLEAN_NAME);
		if (result == NULL) {
			if ((errno != EINVAL) != tests[i].expected_result) {
				printf("Name '%s' was incorrectly %s\n", tests[i].name, tests[i].expected_result ? "rejected" : "accepted");
				failed++;
			}
		} else {
			if (!tests[i].expected_result) {
				printf("Name '%s' was incorrectly accepted\n", tests[i].name);
				failed++;
			}
			fclose(result);
		}
	}
	if (failed != 0)
		fprintf(stderr, "%zd out of %zd failed\n", failed, i);
	else
		fprintf(stderr, "Testsuite passed correctly\n");
	return EXIT_SUCCESS;
}
