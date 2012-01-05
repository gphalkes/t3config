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

static t3_config_t *config = NULL;
static t3_config_schema_t *schema = NULL;

static void cleanup(void) {
	t3_config_delete(config);
	t3_config_delete_schema(schema);
}

int main(int argc, char *argv[]) {
	t3_config_error_t error;
	FILE *file = stdin;
	t3_config_opts_t opts;
	const char *path[2] = { NULL, NULL };
	int c;

	setlocale(LC_ALL, "");
	opts.flags = T3_CONFIG_VERBOSE_ERROR | T3_CONFIG_ERROR_FILE_NAME;

	atexit(cleanup);
	while ((c = getopt(argc, argv, "s:hi::")) >= 0) {
		switch (c) {
			case 's': {
				FILE *schema_file;
				if (schema != NULL)
					fatal("More than one schema option\n");
				if ((schema_file = fopen(optarg, "r")) == NULL)
					fatal("Error opening schema '%s': %m\n", optarg);
				if ((schema = t3_config_read_schema_file(schema_file, &error, &opts)) == NULL)
					fatal("%s:%d: error loading schema '%s': %s: %s\n", error.file_name, error.line_number,
						optarg, t3_config_strerror(error.error), error.extra);
				fclose(schema_file);
				break;
			}
			case 'h':
				printf("Usage: test [-s <schema>] [<input>]\n");
				exit(EXIT_SUCCESS);
			case 'i':
				opts.flags |= T3_CONFIG_INCLUDE_DFLT;
				path[0] = optarg == 0 ? "." : optarg;
				opts.include_callback.dflt.path = path;
				opts.include_callback.dflt.flags = 0;
				break;
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
		fatal("%s:%d: error loading input: %s: %s\n", error.file_name, error.line_number,
			t3_config_strerror(error.error), error.extra);
	fclose(file);
	if (schema != NULL && !t3_config_validate(config, schema, &error, T3_CONFIG_VERBOSE_ERROR | T3_CONFIG_ERROR_FILE_NAME))
		fatal("%s:%d: error validating input: %s: %s\n", error.file_name, error.line_number,
			t3_config_strerror(error.error), error.extra == NULL ? "" : error.extra);

	t3_config_write_file(config, stdout);
	return EXIT_SUCCESS;
}

