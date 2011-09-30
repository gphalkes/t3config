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
	t3_config_t *config;
	t3_config_schema_t *schema = NULL;
	t3_config_opts_t opts;
	int c;

	setlocale(LC_ALL, "");
	opts.flags = T3_CONFIG_VERBOSE_ERROR;

	while ((c = getopt(argc, argv, "s:h")) >= 0) {
		switch (c) {
			case 's': {
				FILE *schema_file;
				if (schema != NULL)
					fatal("More than one schema option\n");
				if ((schema_file = fopen(optarg, "r")) == NULL)
					fatal("Error opening schema '%s': %m\n", optarg);
				if ((schema = t3_config_read_schema_file(schema_file, &error, &opts)) == NULL)
					fatal("Error loading schema '%s': %s %s @ %d\n", optarg, t3_config_strerror(error.error), error.extra, error.line_number);
				fclose(schema_file);
				break;
			}
			case 'h':
				printf("Usage: test [-s <schema>] [<input>]\n");
				exit(EXIT_SUCCESS);
			default:
				break;
		}
	}

	if (argc - optind == 1) {
		if ((file = fopen(argv[optind], "r")) == NULL)
			fatal("Could not open input '%s': %m\n");
	} else if (argc != optind) {
		fatal("More than one input specified\n");
	}

	if ((config = t3_config_read_file(file, &error, &opts)) == NULL)
		fatal("Error loading input: %s %s @ %d\n", t3_config_strerror(error.error), error.extra, error.line_number);
	fclose(file);
	if (schema != NULL && !t3_config_validate(config, schema, &error, &opts))
		fatal("Error validating input: %s %s @ %d\n", t3_config_strerror(error.error),
			error.extra == NULL ? "" : error.extra, error.line_number);

	t3_config_write_file(config, stdout);
	t3_config_delete(config);
	t3_config_delete_schema(schema);
	return EXIT_SUCCESS;
}

