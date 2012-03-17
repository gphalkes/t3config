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

%option reentrant
%option prefix="_t3_config_"
%option noyywrap
%option extra-type="parse_context_t *"

%{
#include "expression.h"
#include "parser.h"

/* Hide symbols from generated code. This is ugly, but unfortunately I have not
   found a better way to do this. */
#include "config_api.h"
#define YY_EXTRA_TYPE parse_context_t *
#include "lex_hide.h"
#undef YY_EXTRA_TYPE
T3_CONFIG_LOCAL int _t3_config_get_column(yyscan_t yyscanner);
T3_CONFIG_LOCAL void _t3_config_set_column(int column_no, yyscan_t yyscanner);

/* Provide our own input routine to support reading from buffer. */
#define YY_INPUT(buf, result, max_size) do { \
	if (yyextra->scan_type == SCAN_FILE) { \
		errno = 0; \
		while ((result = fread(buf, 1, max_size, yyextra->file)) == 0 && ferror(yyextra->file)) { \
			if (errno != EINTR) \
				_t3_config_abort(yyextra->LLthis, T3_ERR_ERRNO); \
			clearerr(yyextra->file); \
		} \
	} else { \
		if (yyextra->buffer_idx == yyextra->buffer_size)  {\
			result = YY_NULL; \
		} else { \
			result = 0; \
			while ((size_t) result < max_size && yyextra->buffer_idx < yyextra->buffer_size) \
				buf[result++] = yyextra->buffer[yyextra->buffer_idx++]; \
		} \
	} \
} while (0)

#define YY_FATAL_ERROR(x) do { \
	struct yyguts_t * yyg = (struct yyguts_t*) yyscanner; \
	_t3_config_abort(yyextra->LLthis, T3_ERR_INTERNAL);\
} while (0)
%}

%x CONSTRAINT
%x COMMENT

IDENT_PAT      [_a-zA-Z][-_0-9a-zA-Z]*
NUMBER_PAT     [-+]?([0-9]*\.[0-9]+([eE][-+][0-9]+)?)|(?i:nan|inf(inity)?)
%%
	/* Executed every time yylex is called. */
	if (yyextra->constraint_parser)
		BEGIN(CONSTRAINT);

<INITIAL,CONSTRAINT>{
(?i:(true|yes))                        return BOOL_TRUE;
(?i:(false|no))                        return BOOL_FALSE;
[-+]?[0-9]+|0x[0-9A-Fa-f]              return INT;
\"(\"\"|[^"\0\n])*\"                   return STRING;
'(''|[^'\0\n])*'                       return STRING;
{NUMBER_PAT}                           return NUMBER;
}

<INITIAL>{
%?{IDENT_PAT}                          return IDENTIFIER;
\r?\n                                  return '\n';
#                                      BEGIN(COMMENT);
}

<CONSTRAINT>{
{IDENT_PAT}                            return IDENTIFIER;
>=                                     return GE;
\<=                                    return LE;
!=                                     return NE;
\{[^}]+\}                              return DESCRIPTION;
}

<INITIAL,CONSTRAINT>{
[[:space:]]                            /* Ignore white space */
.                                      return (unsigned char) yytext[0];
}

<COMMENT>{
\r?\n                                  BEGIN(INITIAL); return '\n';
.                                      /* Ignore comment */
}
%%
