/* Copyright (C) 2019 G.P. Halkes
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
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <t3config/config.h>

/* FIXME: allow using a path to search for includes. */

#ifdef USE_GETTEXT
#include <libintl.h>
#define _(x) gettext(x)
#else
#define _(x) (x)
#endif

/* This header must be included after all the others to prevent issues with the
   definition of _. */
/* clang-format off */
#include "optionMacros.h"
/* clang-format on */

int option_verbose;
const char *option_schema;
const char *option_config;

#ifdef __GNUC__
void fatal(const char *fmt, ...) __attribute__((noreturn));
#endif
void fatal(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(EXIT_FAILURE);
}

/* clang-format off */
static PARSE_FUNCTION(parse_args)
  OPTIONS
    OPTION('v', "verbose", NO_ARG)
      option_verbose = 1;
    END_OPTION
    OPTION('s', "schema", REQUIRED_ARG)
      if (option_schema != NULL) {
        fatal(_("Only one schema option allowed\n"));
      }
      option_schema = optArg;
    END_OPTION
    OPTION('c', "config", REQUIRED_ARG)
      if (option_config != NULL || option_config != NULL) {
        fatal(_("Only one config option allowed\n"));
      }
      option_config = optArg;
    END_OPTION
    OPTION('h', "help", NO_ARG)
      printf("Usage: t3config_test [<options>]\n"
        "  -c<config>,--config=<config>    Load config from <config>\n"
        "  -s<schema>,--schema=<schema>    Load schema from <schema>\n"
        "  -v,--verbose                    Enable verbose output mode\n"
      );
      exit(EXIT_SUCCESS);
    END_OPTION
    DOUBLE_DASH
      NO_MORE_OPTIONS;
    END_OPTION

    fatal(_("No such option %.*s\n"), OPTPRARG);
  NO_OPTION
    fatal(_("Invalid argument %s\n"), optArg);
  END_OPTIONS

  if (!option_schema && !option_config) {
    fatal(_("Nothing to do (no schema or config selected)."));
  }
END_FUNCTION
/* clang-format on */


int main(int argc, char *argv[]) {
  t3_config_schema_t *schema = NULL;
  t3_config_t *config = NULL;
  t3_config_error_t error;
  t3_config_opts_t opts;

  opts.flags = T3_CONFIG_VERBOSE_ERROR | T3_CONFIG_ERROR_FILE_NAME;

#ifdef USE_GETTEXT
  setlocale(LC_ALL, "");
  bindtextdomain("t3highlight", LOCALEDIR);
  textdomain("t3highlight");
#endif

  parse_args(argc, argv);

  if (option_schema) {
    FILE *file = fopen(option_schema, "r");
    schema = t3_config_read_schema_file(file, &error, &opts);
    if (!schema) {
      fatal(_("%s:%d: Could not load schema: %s: %s\n"), error.file_name, error.line_number, t3_config_strerror(error.error), error.extra ? error.extra : "");
    }
    fclose(file);
  }

  if (option_config) {
    FILE *file = fopen(option_config, "r");
    config = t3_config_read_file(file, &error, &opts);
    if (!config) {
      fatal(_("%s:%d: Could not load config: %s: %s\n"), error.file_name, error.line_number, t3_config_strerror(error.error), error.extra ? error.extra : "");
    }
    fclose(file);
    if (schema) {
      if (!t3_config_validate(config, schema, &error, T3_CONFIG_VERBOSE_ERROR | T3_CONFIG_ERROR_FILE_NAME)) {
        fatal(_("%s:%d: Validation of the config failed: %s: %s\n"), error.file_name, error.line_number, t3_config_strerror(error.error), error.extra ? error.extra : "");
      }
    }
  }
  return 0;
}
