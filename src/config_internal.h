/* Copyright (C) 2011-2012 G.P. Halkes
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
#ifndef T3_CONFIG_INTERNAL_H
#define T3_CONFIG_INTERNAL_H
#include "config.h"

#include "expression.h"

typedef struct {
	char *file_name;
	int count;
} file_name_t;

struct t3_config_t {
	t3_config_type_t type;
	int line_number;
	struct t3_config_t *next;
	char *name;
	file_name_t *file_name;
	union {
		const void *ptr; /* First member can be assigned. */
		char *string;
		int integer;
		double number;
		struct t3_config_t *list;
		t3_bool boolean;
		expr_node_t *expr;
	} value;
};

enum {
	T3_CONFIG_SCHEMA = 128,
	T3_CONFIG_EXPRESSION,
	T3_CONFIG_ANY
};

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

#define SCAN_FILE 0
#define SCAN_BUFFER 1

typedef struct {
	yyscan_t scanner;
	void *result;

	int scan_type;
	FILE *file;
	const char *buffer;
	size_t buffer_size,
		buffer_idx;

	int line_number;
	void *LLthis;
	t3_bool constraint_parser;
	const t3_config_opts_t *opts;
	char *error_extra;

	t3_config_t *current_section; /* Used only for including files, to hold the current section. */
	t3_config_t *included; /* Holds a list of included files (strings). */
} parse_context_t;

T3_CONFIG_LOCAL char *_t3_config_get_text(yyscan_t scanner);
T3_CONFIG_LOCAL parse_context_t *_t3_config_get_extra(yyscan_t scanner);
T3_CONFIG_LOCAL void _t3_config_set_extra(parse_context_t *extra, yyscan_t scanner);
T3_CONFIG_LOCAL int _t3_config_lex(yyscan_t scanner);
T3_CONFIG_LOCAL int _t3_config_lex_init(yyscan_t *scanner);
T3_CONFIG_LOCAL int _t3_config_lex_init_extra(parse_context_t *extra, yyscan_t *scanner);
T3_CONFIG_LOCAL int _t3_config_lex_destroy(yyscan_t scanner);
T3_CONFIG_LOCAL void _t3_config_set_in(FILE *in_str, yyscan_t scanner);
#endif
