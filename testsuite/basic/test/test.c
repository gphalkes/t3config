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
#include <libgen.h>
#include "t3config/config.h"

static const char *path[2];
static const t3_config_opts_t opts = { T3_CONFIG_VERBOSE_ERROR | T3_CONFIG_INCLUDE_DFLT, {{ path, 0 }} };

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

static void compare_config(t3_config_t *a, t3_config_t *b) {
	if (t3_config_get_type(a) != t3_config_get_type(b))
		fatal("Different type at lines %d/%d: %d != %d\n",
			t3_config_get_line(a), t3_config_get_line(a),
			t3_config_get_type(a), t3_config_get_type(b));
	if ((t3_config_get_name(a) == NULL && t3_config_get_name(b) != NULL) ||
			(t3_config_get_name(a) != NULL && t3_config_get_name(b) == NULL) ||
			(t3_config_get_name(a) != NULL && strcmp(t3_config_get_name(a), t3_config_get_name(b)) != 0))
		fatal("Differently named keys at lines %d/%d: %s != %s\n",
			t3_config_get_line(a), t3_config_get_line(a),
			t3_config_get_name(a), t3_config_get_name(b));

	switch (t3_config_get_type(a)) {
		case T3_CONFIG_INT:
			if (t3_config_get_int(a) != t3_config_get_int(b))
				fatal("Different int value at lines %d/%d %d != %d\n",
					t3_config_get_line(a), t3_config_get_line(a),
					t3_config_get_int(a), t3_config_get_int(b));
			break;
		case T3_CONFIG_NUMBER:
			if (t3_config_get_number(a) != t3_config_get_number(b))
				fatal("Different number value at lines %d/%d: %.18f != %.18f\n",
					t3_config_get_line(a), t3_config_get_line(a),
					t3_config_get_number(a), t3_config_get_number(b));
			break;
		case T3_CONFIG_BOOL:
			if (t3_config_get_bool(a) != t3_config_get_bool(b))
				fatal("Different bool value at lines %d/%d: %d != %d\n",
					t3_config_get_line(a), t3_config_get_line(a),
					t3_config_get_bool(a), t3_config_get_bool(b));
			break;
		case T3_CONFIG_STRING:
			if (strcmp(t3_config_get_string(a), t3_config_get_string(b)) != 0)
				fatal("Different string value at lines %d/%d: %s != %s\n",
					t3_config_get_line(a), t3_config_get_line(a),
					t3_config_get_string(a), t3_config_get_string(b));
			break;
		case T3_CONFIG_LIST:
		case T3_CONFIG_PLIST:
		case T3_CONFIG_SECTION: {
			t3_config_t *a_sub, *b_sub;
			for (a_sub = t3_config_get(a, NULL), b_sub = t3_config_get(b, NULL);
					a_sub != NULL && b_sub != NULL; a_sub = t3_config_get_next(a_sub), b_sub = t3_config_get_next(b_sub))
				compare_config(a_sub, b_sub);
			if (a_sub != NULL || b_sub != NULL)
				fatal("Different (p)list/section lengths for (p)list/sections starting at %d/%d\n",
					t3_config_get_line(a), t3_config_get_line(a));
			break;
		}
		default:
			fatal("Unhandled config type %d\n", t3_config_get_type(a));
	}
}

int main(int argc, char *argv[]) {
	t3_config_error_t error;
	FILE *file = stdin;
	t3_config_t *config, *reread;

	setlocale(LC_ALL, "nl_NL.UTF-8");

	if (argc == 2) {
		if ((file = fopen(argv[optind], "r")) == NULL)
			fatal("Could not open input '%s': %m\n");
	} else if (argc > 2) {
		fatal("Usage: test [<test file>]\n");
	}

	path[0] = dirname(dirname(strdup(argv[optind])));
	path[1] = NULL;

	/* Read file. */
	if ((config = t3_config_read_file(file, &error, &opts)) == NULL)
		fatal("Error loading input: %s %s @ %d\n", t3_config_strerror(error.error),
			error.extra == NULL ? "" : error.extra, error.line_number);
	fclose(file);

	/* Write new file. */
	if ((file = fopen("out", "w+")) == NULL)
		fatal("Could not open output: %m\n");
	t3_config_write_file(config, file);
	fclose(file);

	if ((file = fopen("out", "r")) == NULL)
		fatal("Could not re-open output: %m\n");
	if ((reread = t3_config_read_file(file, &error, NULL)) == NULL)
		fatal("Error re-loading output: %s @ %d\n", t3_config_strerror(error.error), error.line_number);
	fclose(file);

	compare_config(config, reread);
	t3_config_delete(config);
	t3_config_delete(reread);

	return EXIT_SUCCESS;
}
