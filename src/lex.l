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

%option reentrant
%option prefix="_t3_config_"
%option noyywrap
%option extra-type="parse_context_t *"

%{
#include "parser.h"
#include "config_api.h"
#define YY_EXTRA_TYPE parse_context_t *
#include "lex_hide.h"
#undef YY_EXTRA_TYPE

#define YY_INPUT(buf, result, max_size) do { \
	if (yyextra->scan_type == SCAN_FILE) { \
		errno = 0; \
		while ((result = fread(buf, 1, max_size, yyextra->file)) == 0 && ferror(yyextra->file)) { \
			if (errno != EINTR) \
				_t3_config_abort(yyextra->LLthis, T3_ERR_ERRNO); \
			clearerr(yyextra->file); \
		} \
	} else { \
		if (yyextra->buffer[yyextra->buffer_idx] == 0)  {\
			result = YY_NULL; \
		} else { \
			result = 0; \
			while ((size_t) result < max_size && (buf[result++] = yyextra->buffer[yyextra->buffer_idx++]) != 0) {} \
		} \
	} \
} while (0)

%}

%%

-?[0-9]+|0x[0-9A-Fa-f]                 return INT;
\"(\"\"|[^"])*\"                       return STRING;
'(''|[^'])*'                           return STRING;
-?[0-9]*\.[0-9]+(e[+-][0-9]+)?         return NUMBER;
(?i:(true|yes))                        return BOOL_TRUE;
(?i:(false|no))                        return BOOL_FALSE;
[_a-zA-Z][_0-9a-zA-Z]*                 return IDENTIFIER;
\n                                     yyextra->line_number++; return '\n';
[[:space:]]                            /* Ignore white space */
#.*                                    /* Ignore comments */
.                                      return (unsigned char) yytext[0];

%%

