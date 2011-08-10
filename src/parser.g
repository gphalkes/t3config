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

%options "abort thread-safe lowercase-symbols generate-lexer-wrapper=no";
%prefix _t3_config_;
%lexical _t3_config_yylex_wrapper;
%datatype "parse_context_t *", "config_internal.h";
%start _t3_config_parse, config;

%token INT, NUMBER, STRING, IDENTIFIER, BOOL_TRUE, BOOL_FALSE;

%top {
#include "config_api.h"
#include "config_internal.h"

struct _t3_config_this;
T3_CONFIG_LOCAL int _t3_config_parse(parse_context_t * LLuserData);
T3_CONFIG_LOCAL void _t3_config_abort(struct _t3_config_this *, int);
}

{
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include "config.h"

static t3_config_t *allocate_item(struct _t3_config_this *LLthis, t3_bool allocate_name) {
	t3_config_t *result;

	if ((result = (t3_config_t *) malloc(sizeof(t3_config_t))) == NULL)
		LLabort(LLthis, T3_ERR_OUT_OF_MEMORY);

	result->next = NULL;
	result->type = T3_CONFIG_NONE;

	if (allocate_name) {
		if ((result->name = strdup(_t3_config_get_text(_t3_config_data->scanner))) == NULL)
			LLabort(LLthis, T3_ERR_OUT_OF_MEMORY);
	} else {
		result->name = NULL;
	}

	return result;
}

static void set_value(struct _t3_config_this *LLthis, t3_config_t *item, t3_config_item_type_t type) {
	switch (type) {
		case T3_CONFIG_BOOL:
			item->type = type;
			item->value.boolean = LLsymb == BOOL_TRUE;
			break;
		case T3_CONFIG_INT: {
			long value;
			errno = 0;
			value = strtol(_t3_config_get_text(_t3_config_data->scanner), NULL, 0);
/*
			if (errno == ERANGE
#if T3_CONFIG_INT_MAX < LONG_MAX || T3_CONFIG_INT_MIN > LONG_MIN
				|| value > T3_CONFIG_INT_MAX || value < T3_CONFIG_INT_MIN
#endif
			)
				LLabort(LLthis, T3_ERR_OUT_OF_RANGE);
*/
			item->type = type;
			item->value.integer = (int) value;
			break;
		}
		case T3_CONFIG_NUMBER: {
			double value;
			errno = 0;
			value = _t3_config_strtod(_t3_config_get_text(_t3_config_data->scanner));
/*			if (errno == ERANGE)
				LLabort(LLthis, T3_ERR_OUT_OF_RANGE);*/
			item->type = type;
			item->value.number = value;
			break;
		}
		case T3_CONFIG_STRING: {
			/* Don't need to allocate full yytext, because we drop the quotes. */
			char *text = _t3_config_get_text(_t3_config_data->scanner);
			char *value = malloc(strlen(text));
			size_t i, j;

			for (i = 1, j = 0; !(text[i] == text[0] && text[i + 1] == 0); i++, j++) {
				value[j] = text[i];
				/* Because the only quotes that can occur in the string itself
				   are doubled (checked by lexing), we don't have to check the
				   next character. */
				if (text[i] == text[0])
					i++;
			}
			value[j] = 0;
			item->type = type;
			item->value.string = value;
			break;
		}
		case T3_CONFIG_LIST:
		case T3_CONFIG_SECTION:
			item->type = type;
			item->value.list = NULL;
			break;
		default:
			LLabort(LLthis, T3_ERR_UNKNOWN);
	}
}

T3_CONFIG_LOCAL int _t3_config_yylex_wrapper(struct _t3_config_this *LLthis);
int _t3_config_yylex_wrapper(struct _t3_config_this *LLthis) {
	if (LLreissue == LL_NEW_TOKEN) {
		/* Increase line number when the last token was a newline, instead of
		   when we find a newline, to improve error location reporting. */
		if (LLsymb == '\n')
			_t3_config_data->line_number++;
		return _t3_config_lex(_t3_config_data->scanner);
	} else {
		int LLretval = LLreissue;
		LLreissue = LL_NEW_TOKEN;
		return LLretval;
	}
}

T3_CONFIG_LOCAL void LLmessage(struct _t3_config_this *LLthis, int LLtoken);
void LLmessage(struct _t3_config_this *LLthis, int LLtoken) {
	(void) LLtoken;
	LLabort(LLthis, T3_ERR_PARSE_ERROR);
}

}

//=========================== RULES ============================

config {
	t3_config_t **next_ptr;
	_t3_config_data->LLthis = LLthis;
	_t3_config_data->config = allocate_item(LLthis, t3_false);
	_t3_config_data->config->type = T3_CONFIG_SECTION;
	_t3_config_data->config->value.list = NULL;
	next_ptr = &_t3_config_data->config->value.list;
} :
	'\n' *
	[
		item(next_ptr)
		{
			if (t3_config_get(_t3_config_data->config, (*next_ptr)->name) != *next_ptr)
				LLabort(LLthis, T3_ERR_DUPLICATE_KEY);
			next_ptr = &(*next_ptr)->next;
		}
		[ '\n'+ ] ..?
	]*
;

value(t3_config_t *item) {
	t3_config_t **next_ptr;
} :
	INT
	{
		set_value(LLthis, item, T3_CONFIG_INT);
	}
|
	[ BOOL_TRUE | BOOL_FALSE ]
	{
		set_value(LLthis, item, T3_CONFIG_BOOL);
	}
|
	NUMBER
	{
		set_value(LLthis, item, T3_CONFIG_NUMBER);
	}
|
	STRING
	{
		set_value(LLthis, item, T3_CONFIG_STRING);
	}
|
	'('
	{
		set_value(LLthis, item, T3_CONFIG_LIST);
		item->value.list = NULL;
		next_ptr = &item->value.list;
	}
	'\n'*
	[
		{
			*next_ptr = allocate_item(LLthis, t3_false);
		}
		[
			value(*next_ptr)
		|
			section(*next_ptr)
		]
		{
			next_ptr = &(*next_ptr)->next;
		}
		'\n'*
		[
			','
			'\n'*
			...
		]*
	]*
	')'
;

item(t3_config_t **item) :
	IDENTIFIER
	{
		*item = allocate_item(LLthis, t3_true);
	}
	[
		section(*item)
	|
		'='
		value(*item)
	]
;

section(t3_config_t *item) {
	t3_config_t **next_ptr = &item->value.list;
	item->type = T3_CONFIG_SECTION;
	item->value.list = NULL;
} :
	'{'
	'\n'*
	[
		item(next_ptr)
		{
			if (t3_config_get(item, (*next_ptr)->name) != *next_ptr)
				LLabort(LLthis, T3_ERR_DUPLICATE_KEY);
			next_ptr = &(*next_ptr)->next;
		}
		[ [';' | '\n'] '\n'* ] ..?
	]*
	'}'
;
