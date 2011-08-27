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
	FILE *file = stdin;
	t3_config_schema_t *schema;

	setlocale(LC_ALL, "nl_NL.UTF-8");

	if (argc == 2) {
		if ((file = fopen(argv[optind], "r")) == NULL)
			fatal("Could not open input '%s': %m\n");
	} else if (argc > 2) {
		fatal("Usage: test [<test file>]\n");
	}
	/* Read file. */
	if ((schema = t3_config_read_schema_file(file, &error, NULL)) == NULL)
		fatal("Error loading input: %s @ %d\n", t3_config_strerror(error.error), error.line_number);
	fclose(file);

	t3_config_delete_schema(schema);

	return EXIT_SUCCESS;
}
