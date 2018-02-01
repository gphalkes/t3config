#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <t3config/config.h>
#include <unistd.h>

#define STRING_DFLT(x, dflt) ((x) != NULL ? (x) : (dflt))

static t3_config_t *config;
static t3_config_schema_t *schema;

/* Alert the user of a fatal error and quit. */
static void fatal(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(EXIT_FAILURE);
}

/* Cleanup memory used by config and schema. */
static void cleanup(void) {
  /* Both t3_config_delete and t3_config_delete_schema allow NULL pointers to be passed. */
  t3_config_delete(config);
  t3_config_delete_schema(schema);
}

int main(int argc, char *argv[]) {
  t3_config_error_t error;
  FILE *file = stdin;
  t3_config_opts_t opts;
  const char *path[2] = {NULL, NULL};
  const char *name = "<stdin>";
  int c;

  /* Set locale */
  setlocale(LC_ALL, "");
  /* Request as much information about errors as possible. */
  opts.flags = T3_CONFIG_VERBOSE_ERROR | T3_CONFIG_ERROR_FILE_NAME;

  /* Make sure we free memory on exit (useful for debugging purposes). */
  atexit(cleanup);
  while ((c = getopt(argc, argv, "s:hi::")) >= 0) {
    switch (c) {
      case 's': {
        /* Load a schema. */
        FILE *schema_file;
        if (schema != NULL) fatal("more than one schema option\n");
        if ((schema_file = fopen(optarg, "r")) == NULL)
          fatal("error opening schema '%s': %m\n", optarg);
        if ((schema = t3_config_read_schema_file(schema_file, &error, &opts)) == NULL)
          fatal("%s:%d: error loading schema '%s': %s: %s\n", STRING_DFLT(error.file_name, optarg),
                error.line_number, optarg, t3_config_strerror(error.error),
                STRING_DFLT(error.extra, ""));
        fclose(schema_file);
        break;
      }
      case 'h':
        printf("Usage: test [-s <schema>] [-i[<include dir>]] [<input>]\n");
        exit(EXIT_SUCCESS);
      case 'i':
        /* Configure the options for allowing the include mechanism. */
        opts.flags |= T3_CONFIG_INCLUDE_DFLT;
        path[0] = optarg == 0 ? "." : optarg;
        opts.include_callback.dflt.path = path;
        opts.include_callback.dflt.flags = 0;
        break;
      default:
        break;
    }
  }

  /* Open the input file if necessary. */
  if (argc - optind == 1) {
    if ((file = fopen(argv[optind], "r")) == NULL) fatal("could not open input '%s': %m\n");
    name = argv[optind];
  } else if (argc != optind) {
    fatal("more than one input specified\n");
  }

  /* Read the configuration. */
  if ((config = t3_config_read_file(file, &error, &opts)) == NULL)
    fatal("%s:%d: error loading input: %s: %s\n", STRING_DFLT(error.file_name, name),
          error.line_number, t3_config_strerror(error.error), STRING_DFLT(error.extra, ""));
  /* Close the file now that we are done with it. */
  fclose(file);

  /* Use the schema (if any) to validate the input. */
  if (schema != NULL &&
      !t3_config_validate(config, schema, &error,
                          T3_CONFIG_VERBOSE_ERROR | T3_CONFIG_ERROR_FILE_NAME))
    fatal("%s:%d: error validating input: %s: %s\n", STRING_DFLT(error.file_name, name),
          error.line_number, t3_config_strerror(error.error), STRING_DFLT(error.extra, ""));

  /* Write the input to screen for checking the result. */
  t3_config_write_file(config, stdout);
  return EXIT_SUCCESS;
}
