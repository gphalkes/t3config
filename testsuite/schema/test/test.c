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
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>
#include <stdarg.h>
#include "config.h"

t3_config_schema_t *_t3_config_config2schema(t3_config_t *config, t3_config_error_t *error);

static const char meta_schema_buffer[] = {
#include "meta_schema.bytes"
};

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

int main(int argc, char *argv[]) {
	t3_config_error_t error;
	t3_config_t *test, *testcase;
	t3_config_schema_t *meta_schema, *schema;
	int failed = 0, testnr;
	FILE *file = stdin;

	setlocale(LC_ALL, "nl_NL.UTF-8");

	if (argc == 2) {
		if ((file = fopen(argv[optind], "r")) == NULL)
			fatal("Could not open input '%s': %m\n");
	} else if (argc > 2) {
		fatal("Usage: test [<test file>]\n");
	}
	/* Read test file. */
	if ((test = t3_config_read_file(file, &error, NULL)) == NULL)
		fatal("Error loading input: %s %s @ %d\n", t3_config_strerror(error.error),
			error.extra == NULL ? "" : error.extra, error.line_number);
	fclose(file);

	if ((meta_schema = t3_config_read_schema_buffer(meta_schema_buffer, sizeof(meta_schema_buffer), &error, NULL)) == NULL)
		fatal("Could not load meta schema: %s @ %d (%s)\n", t3_config_strerror(error.error), error.line_number, error.extra);

	if (!t3_config_validate(test, meta_schema, &error))
		fatal("test does not conform to schema: %s @ %d (%s)\n", t3_config_strerror(error.error), error.line_number, error.extra);
	t3_config_delete_schema(meta_schema);

	if ((schema = _t3_config_config2schema(t3_config_get(test, "schema"), &error)) == NULL)
		fatal("test schema can not be converted: %s @ %d (%s)\n", t3_config_strerror(error.error), error.line_number, error.extra);

	for (testcase = t3_config_get(t3_config_get(test, "correct"), NULL), testnr = 1;
			testcase != NULL; testcase = t3_config_get_next(testcase), testnr++)
	{
		if (!t3_config_validate(testcase, schema, &error)) {
			failed++;
			fprintf(stderr, "Correct test %d failed: %s @ %d (%s)\n", testnr, t3_config_strerror(error.error),
				error.line_number, error.extra);
		}
	}

	for (testcase = t3_config_get(t3_config_get(test, "incorrect"), NULL), testnr = 1;
			testcase != NULL; testcase = t3_config_get_next(testcase), testnr++)
	{
		if (t3_config_validate(testcase, schema, &error)) {
			failed++;
			fprintf(stderr, "Incorrect test %d failed (i.e. passed validation)\n", testnr);
		}
	}
	t3_config_delete(test);
	if (failed != 0)
		fatal("%d tests failed\n", failed);
	return EXIT_SUCCESS;
}
