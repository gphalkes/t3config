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
#ifndef T3_CONFIG_INTERNAL_H
#define T3_CONFIG_INTERNAL_H
#include "config.h"

struct t3_config_t {
	t3_config_item_type_t type;
	struct t3_config_t *next;
	char *name;
	union {
		void *ptr; /* First member can be assigned. */
		char *string;
		int integer;
		double number;
		struct t3_config_t *list;
		t3_bool boolean;
	} value;
};

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

#define SCAN_FILE 0
#define SCAN_BUFFER 1

typedef struct {
	yyscan_t scanner;
	t3_config_t *config;
	int scan_type;
	FILE *file;
	const char *buffer;
	size_t buffer_size,
		buffer_idx;
	int line_number;
	int value_count;
	void *LLthis;
} parse_context_t;

#ifdef HAVE_STRDUP
#define _t3_config_strdup strdup
#else
T3_CONFIG_LOCAL char *_t3_config_strdup(const char *str);
#endif

T3_CONFIG_LOCAL char *_t3_config_get_text(yyscan_t scanner);
T3_CONFIG_LOCAL parse_context_t *_t3_config_get_extra(yyscan_t scanner);
T3_CONFIG_LOCAL void _t3_config_set_extra(parse_context_t *extra, yyscan_t scanner);
T3_CONFIG_LOCAL int _t3_config_lex(yyscan_t scanner);
T3_CONFIG_LOCAL int _t3_config_lex_init(yyscan_t *scanner);
T3_CONFIG_LOCAL int _t3_config_lex_init_extra(parse_context_t *extra, yyscan_t *scanner);
T3_CONFIG_LOCAL int _t3_config_lex_destroy(yyscan_t scanner);
T3_CONFIG_LOCAL void _t3_config_set_in(FILE *in_str, yyscan_t scanner);
T3_CONFIG_LOCAL double _t3_config_strtod(char *text);
#endif
