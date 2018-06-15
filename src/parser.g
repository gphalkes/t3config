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

%options "abort thread-safe lowercase-symbols generate-lexer-wrapper=no";
%prefix _t3_config_;
%lexical _t3_config_yylex_wrapper;
%datatype "parse_context_t *", "t3config/config_internal.h";
%start _t3_config_parse, config;
%start _t3_config_parse_include, include_config;

%token INT, NUMBER, STRING, IDENTIFIER, BOOL_TRUE, BOOL_FALSE;

%top {
#include "t3config/expression.h"

struct _t3_config_this;
T3_CONFIG_LOCAL int _t3_config_parse(parse_context_t * LLuserData);
T3_CONFIG_LOCAL int _t3_config_parse_include(parse_context_t * LLuserData);
T3_CONFIG_LOCAL int _t3_config_parse_constraint(parse_context_t * LLuserData);
T3_CONFIG_LOCAL void _t3_config_abort(struct _t3_config_this *, int);
}

{
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "t3config/config.h"
#include "t3config/util.h"

static t3_config_t *allocate_item(struct _t3_config_this *LLthis, t3_bool allocate_name) {
	t3_config_t *result;

	if ((result = (t3_config_t *) malloc(sizeof(t3_config_t))) == NULL)
		LLabort(LLthis, T3_ERR_OUT_OF_MEMORY);

	result->next = NULL;
	result->type = T3_CONFIG_NONE;
	result->line_number = _t3_config_data->line_number;
	result->value.ptr = NULL;
	result->file_name = _t3_config_ref_file_name(_t3_config_data->included);

	if (allocate_name) {
		if ((result->name = _t3_config_strdup(_t3_config_get_text(_t3_config_data->scanner))) == NULL)
			LLabort(LLthis, T3_ERR_OUT_OF_MEMORY);
	} else {
		result->name = NULL;
	}

	return result;
}

static void set_value(struct _t3_config_this *LLthis, t3_config_t *item, t3_config_type_t type) {
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

			if (value == NULL)
				LLabort(LLthis, T3_ERR_OUT_OF_MEMORY);

			_t3_unescape(value, text);

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
			LLabort(LLthis, T3_ERR_INTERNAL);
	}
}

static t3_bool transform_percent_list(struct _t3_config_this *LLthis, t3_config_t *item, t3_config_t **last_dptr) {
	t3_config_t *list;

	if ((*last_dptr)->name[0] != '%')
		return t3_false;

	if ((list = t3_config_get(item, (*last_dptr)->name + 1)) == NULL) {
		/* No existing list found. Transform the current item into new plist. */
		list = allocate_item(LLthis, t3_false);
		list->type = T3_CONFIG_PLIST;
		list->name = (*last_dptr)->name;
		(*last_dptr)->name = NULL;
		memmove(list->name, list->name + 1, strlen(list->name));
		list->value.list = *last_dptr;
		*last_dptr = list;
		return t3_false;
	}

	if (list->type != T3_CONFIG_PLIST) {
		if (_t3_config_data->opts != NULL && (_t3_config_data->opts->flags & T3_CONFIG_VERBOSE_ERROR))
			_t3_config_data->error_extra = _t3_config_strdup((*last_dptr)->name);
		LLabort(LLthis, T3_ERR_DUPLICATE_KEY);
	}

	t3_config_add_existing(list, NULL, *last_dptr);
	*last_dptr = NULL;
	return t3_true;
}

T3_CONFIG_LOCAL int _t3_config_yylex_wrapper(struct _t3_config_this *LLthis);
int _t3_config_yylex_wrapper(struct _t3_config_this *LLthis) {
	if (LLreissue == LL_NEW_TOKEN) {
		/* Unfortunately, we have to initialize the LLthis member here, because
		   it may be used in the _t3_config_lex routine and this is the first
		   oportunity we have to initialize it. This does mean it will get set
		   over and over again, once for each token read. Unless we modify LLnextgen
		   to allow us to execute some code before the first LLread, we're stuck
		   with this. */
		_t3_config_data->LLthis = LLthis;
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

static file_name_t *new_file_name(const t3_config_t *config) {
	file_name_t *result;
	if ((result = malloc(sizeof(file_name_t))) == NULL)
		return NULL;

	if ((result->file_name = _t3_config_strdup(t3_config_get_string(config))) == NULL) {
		free(result);
		return NULL;
	}
	result->count = 1;
	return result;
}

static void include_file(struct _t3_config_this *LLthis, t3_config_t *item, t3_config_t *include) {
	/* Location to save data from current lexer. */
	yyscan_t scanner;
	int scan_type;
	int line_number;
	FILE *file;
	t3_config_t *included;

	yyscan_t new_scanner;
	FILE *new_file;
	int result;

	for (included = _t3_config_data->included; included != NULL; included = included->next) {
		if (strcmp(included->value.string, include->value.string) == 0) {
			/* It is an error to use recursive includes. */
			if (_t3_config_data->opts->flags & T3_CONFIG_VERBOSE_ERROR)
				_t3_config_data->error_extra = t3_config_take_string(include);

			/* Delete "include" because we no longer need it. */
			t3_config_delete(include);

			LLabort(LLthis, T3_ERR_RECURSIVE_INCLUDE);
		}
	}

	include->next = _t3_config_data->included;
	_t3_config_data->included = include;
	_t3_config_unref_file_name(include);
	if ((include->file_name = new_file_name(include)) == NULL)
		LLabort(LLthis, T3_ERR_OUT_OF_MEMORY);


	/* Use either the default or the user supplied include-callback function to open
	   the include file. */
	if (_t3_config_data->opts->flags & T3_CONFIG_INCLUDE_DFLT)
		new_file = t3_config_open_from_path(_t3_config_data->opts->include_callback.dflt.path,
			include->value.string, _t3_config_data->opts->include_callback.dflt.flags);
	else
		new_file = _t3_config_data->opts->include_callback.user.open(include->value.string,
			_t3_config_data->opts->include_callback.user.data);

	/* Abort if the include file could not be found. */
	if (new_file == NULL) {
		if (_t3_config_data->opts->flags & T3_CONFIG_VERBOSE_ERROR)
			_t3_config_data->error_extra = _t3_config_strdup(include->value.string);
		LLabort(LLthis, T3_ERR_ERRNO);
	}

	/* Initialize a new lexer. */
	if (_t3_config_lex_init_extra(_t3_config_data, &new_scanner) != 0)
		LLabort(LLthis, T3_ERR_OUT_OF_MEMORY);

	/* Replace the current context's lexer related values, after saving the current settings. */
	scanner = _t3_config_data->scanner;
	scan_type = _t3_config_data->scan_type;
	file = _t3_config_data->file;
	line_number = _t3_config_data->line_number;

	_t3_config_data->scanner = new_scanner;
	_t3_config_data->scan_type = SCAN_FILE;
	_t3_config_data->file = new_file;

	_t3_config_data->current_section = item;
	_t3_config_data->line_number = 1;
	/* Parse the included file. */
	result = _t3_config_parse_include(_t3_config_data);

	/* Destroy the new lexer, and reset all the lexer related values in the context. */
	_t3_config_lex_destroy(new_scanner);

	_t3_config_data->scanner = scanner;
	_t3_config_data->scan_type = scan_type;
	_t3_config_data->file = file;

	/* Close the include file. */
	fclose(new_file);

	/* Abort if the parse of the include file was not successful. */
	if (result != T3_ERR_SUCCESS)
		LLabort(LLthis, result);

	/* The line number may get used if LLabort is called above. Thus we only reset it
	   after we pass all possible LLabort calls. */
	_t3_config_data->line_number = line_number;

	/* Remove the current include from the list (stack) of included files. */
	_t3_config_data->included = _t3_config_data->included->next;
	/* Prevent deletion of the entire chain. */
	include->next = NULL;
	t3_config_delete(include);
}

}

//=========================== RULES ============================

config {
	_t3_config_data->result = allocate_item(LLthis, t3_false);
	((t3_config_t *) _t3_config_data->result)->line_number = 0;
} :
	section_contents(_t3_config_data->result)
;

include_config :
	section_contents(_t3_config_data->current_section)
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
	[
		'+'
		'\n'*
		STRING
		{
			/* We won't be adding the entire yytext, so we can safely ignore the
			   nul byte. */
			char *text = _t3_config_get_text(_t3_config_data->scanner);
			char *value = realloc(item->value.string, strlen(text) + strlen(item->value.string));
			if (value == NULL)
				LLabort(LLthis, T3_ERR_OUT_OF_MEMORY);

			item->value.string = value;
			value += strlen(value);
			_t3_unescape(value, text);
		}
	]*
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

section(t3_config_t *item)
 :
	'{'
	section_contents(item)
	'}'
;

section_contents(t3_config_t *item) {
	t3_config_t **next_ptr = &item->value.list;
	item->type = T3_CONFIG_SECTION;
	while (*next_ptr != NULL)
		next_ptr = &(*next_ptr)->next;
} :
	'\n'*
	[
		item(next_ptr)
		{
			if (_t3_config_data->opts != NULL && (_t3_config_data->opts->flags & (T3_CONFIG_INCLUDE_DFLT | T3_CONFIG_INCLUDE_USER)) &&
					strcmp((*next_ptr)->name, "%include") == 0)
			{
				t3_config_t *include = *next_ptr;
				*next_ptr = NULL;
				include_file(LLthis, item, include);
				while (*next_ptr != NULL)
					next_ptr = &(*next_ptr)->next;
			} else {
				if (t3_config_get(item, (*next_ptr)->name) != *next_ptr) {
					if (_t3_config_data->opts != NULL && (_t3_config_data->opts->flags & T3_CONFIG_VERBOSE_ERROR))
						_t3_config_data->error_extra = _t3_config_strdup((*next_ptr)->name);
					LLabort(LLthis, T3_ERR_DUPLICATE_KEY);
				}
				if (!transform_percent_list(LLthis, item, next_ptr))
					next_ptr = &(*next_ptr)->next;
			}
		}
		[ [';' | '\n'] '\n'* ] ..?
	]*
;

//=========================== CONSTRAINTS PARSER ============================
%token NE, LE, GE, DESCRIPTION;
%start _t3_config_parse_constraint, constraint;

{
static int get_priority(int operator) {
	switch (operator) {
		case '|':
		case '^':
		case '&':
			return 0;
		case '=':
		case NE:
		case '<':
		case LE:
		case '>':
		case GE:
			return 1;
		case '/':
			return 2;
		default:
			break;
	}
	/* This point should never be reached. */
	return -1;
}

static expr_type_t symb2expr(int symb) {
	static const struct {
		int symb;
		expr_type_t type;
	} map[] = {
		{ '=', EXPR_EQ },
		{ NE, EXPR_NE },
		{ '<', EXPR_LT },
		{ LE, EXPR_LE },
		{ '>', EXPR_GT },
		{ GE, EXPR_GE },
		{ '&', EXPR_AND },
		{ '|', EXPR_OR },
		{ '^', EXPR_XOR },
		{ '/', EXPR_PATH }
	};
	size_t i;

	for (i = 0; i < sizeof(map) / sizeof(map[0]); i++)
		if (map[i].symb == symb)
			return map[i].type;
	return EXPR_EQ;
}

static expr_node_t *new_expression(struct _t3_config_this *LLthis, expr_type_t type, expr_node_t *operand_0, expr_node_t *operand_1) {
	expr_node_t *result;
	/* We use a t3_config_t to reuse the conversion stuff we wrote for that. */
	t3_config_t config_node;

	/* Convert this first, such that when it jumps out because of insufficient
	   memory, we don't cause a memory leak. */
	switch (type) {
		case EXPR_INT_CONST:
			set_value(LLthis, &config_node, T3_CONFIG_INT);
			break;
		case EXPR_NUMBER_CONST:
			set_value(LLthis, &config_node, T3_CONFIG_NUMBER);
			break;
		case EXPR_STRING_CONST:
			set_value(LLthis, &config_node, T3_CONFIG_STRING);
			break;
		default:
			break;
	}

	if ((result = malloc(sizeof(expr_node_t))) == NULL)
		LLabort(LLthis, T3_ERR_OUT_OF_MEMORY);
	result->type = type;
	result->value.operand[0] = operand_0;
	result->value.operand[1] = operand_1;

	switch (type) {
		case EXPR_BOOL_CONST:
			result->value.boolean = LLsymb == BOOL_TRUE;
			break;
		case EXPR_INT_CONST:
			result->value.integer = config_node.value.integer;
			break;
		case EXPR_NUMBER_CONST:
			result->value.number = config_node.value.number;
			break;
		case EXPR_STRING_CONST:
			result->value.string = config_node.value.string;
			break;
		case EXPR_IDENT:
			if ((result->value.string = _t3_config_strdup(_t3_config_get_text(_t3_config_data->scanner))) == NULL) {
				free(result);
				LLabort(LLthis, T3_ERR_OUT_OF_MEMORY);
			}
			break;
		default:
			break;
	}
	return result;
}

}

constraint {
	expr_node_t *top;
	_t3_config_data->LLthis = LLthis;
	_t3_config_data->result = NULL;

	if ((top = malloc(sizeof(expr_node_t))) == NULL)
		LLabort(LLthis, T3_ERR_OUT_OF_MEMORY);
	_t3_config_data->result = top;
	top->type = EXPR_TOP;
	top->value.operand[0] = NULL;
	if ((top->value.operand[1] = malloc(sizeof(expr_node_t))) == NULL)
		LLabort(LLthis, T3_ERR_OUT_OF_MEMORY);
	top->value.operand[1]->type = EXPR_STRING_CONST;
	top->value.operand[1]->value.string = NULL;
} :
	[
		DESCRIPTION
		{
			if ((top->value.operand[1]->value.string = _t3_config_strdup(_t3_config_get_text(_t3_config_data->scanner) + 1)) == NULL)
				LLabort(LLthis, T3_ERR_OUT_OF_MEMORY);
			/* We can safely subtract one from the length of the string because we
			   know the closing '}' is there. */
			top->value.operand[1]->value.string[strlen(top->value.operand[1]->value.string) - 1] = 0;
		}
	]?
	expression(0, &top->value.operand[0])
	{
		if (top->value.operand[1] == NULL) {
			if ((top->value.operand[1] = malloc(sizeof(expr_node_t))) == NULL)
				LLabort(LLthis, T3_ERR_OUT_OF_MEMORY);
			top->value.operand[1]->type = EXPR_STRING_CONST;
			top->value.operand[1]->value.string = NULL;
		}
	}
;

expression(int priority, expr_node_t **node) {
	int operator;
} :
	factor(node)
	[
		%while (get_priority(LLsymb) >= priority)
		[ '=' | NE | '<' | LE | '>' | GE | '&' | '|' | '^' | '/' ]
		{
			*node = new_expression(LLthis, symb2expr(LLsymb), *node, NULL);
			operator = LLsymb;
		}
		expression(get_priority(operator) + 1, &(*node)->value.operand[1])
	]*
;

factor(expr_node_t **node):
	IDENTIFIER
	{
		*node = new_expression(LLthis, EXPR_IDENT, NULL, NULL);
	}
|
	'!'
	factor(node)
	{
		*node = new_expression(LLthis, EXPR_NOT, *node, NULL);
	}
|
	'('
	expression(0, node)
	')'
|
	STRING
	{
		*node = new_expression(LLthis, EXPR_STRING_CONST, NULL, NULL);
	}
|
	INT
	{
		*node = new_expression(LLthis, EXPR_INT_CONST, NULL, NULL);
	}
|
	NUMBER
	{
		*node = new_expression(LLthis, EXPR_NUMBER_CONST, NULL, NULL);
	}
|
	[ BOOL_FALSE | BOOL_TRUE ]
	{
		*node = new_expression(LLthis, EXPR_BOOL_CONST, NULL, NULL);
	}
|
	'/'
	[
		%prefer
		factor(node)
		{
			*node = new_expression(LLthis, EXPR_PATH, NULL, *node);
			(*node)->value.operand[0] = new_expression(LLthis, EXPR_PATH_ROOT, NULL, NULL);
		}
	|
		{
			*node = new_expression(LLthis, EXPR_PATH_ROOT, NULL, NULL);
		}
	]
|
	'['
	factor(node)
	']'
	{
		*node = new_expression(LLthis, EXPR_DEREF, *node, NULL);
	}
|
	'%'
	{
		*node = new_expression(LLthis, EXPR_THIS, NULL, NULL);
	}
|
	'#'
	[
		%prefer
		'('
		[
			{
				*node = new_expression(LLthis, EXPR_LIST, NULL, *node);
			}
			expression(0, &(*node)->value.operand[0])
			[
				','
				...
			]*
		]
		')'
	|
		%prefer
		factor(node)
	|
	]
	{
		*node = new_expression(LLthis, EXPR_LENGTH, *node, NULL);
	}
;

