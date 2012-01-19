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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "config_internal.h"
#include "util.h"
#include "expression.h"
#include "parser.h"

#ifdef USE_GETTEXT
#include <libintl.h>
#define _(x) dgettext("LIBT3", (x))
#else
#define _(x) (x)
#endif


t3_config_t *t3_config_new(void) {
	t3_config_t *result;

	if ((result = malloc(sizeof(t3_config_t))) == NULL)
		return NULL;
	result->name = NULL;
	result->type = T3_CONFIG_SECTION;
	result->value.list = NULL;
	result->next = NULL;
	result->file_name = NULL;
	return result;
}

/** Read config, either from file or from buffer. */
static t3_config_t *config_read(parse_context_t *context, t3_config_error_t *error) {
	int retval;

	context->line_number = 1;
	context->result = NULL;
	context->constraint_parser = t3_false;
	context->error_extra = NULL;
	context->included = NULL;

	/* Initialize lexer. */
	if (_t3_config_lex_init_extra(context, &context->scanner) != 0) {
		if (error != NULL) {
			error->error = T3_ERR_OUT_OF_MEMORY;
			error->line_number = 0;
			if (context->opts != NULL) {
				if (context->opts->flags & T3_CONFIG_VERBOSE_ERROR)
					error->extra = NULL;
				if (context->opts->flags & T3_CONFIG_ERROR_FILE_NAME)
					error->file_name = NULL;
			}
		}
		return NULL;
	}

	/* Perform parse. */
	if ((retval = _t3_config_parse(context)) != 0) {
		if (error != NULL) {
			error->error = retval;
			error->line_number = _t3_config_get_extra(context->scanner)->line_number;
			if (context->opts != NULL) {
				if (context->opts->flags & T3_CONFIG_VERBOSE_ERROR)
					error->extra = context->error_extra;
				if (context->opts->flags & T3_CONFIG_ERROR_FILE_NAME)
					error->file_name = t3_config_take_string(context->included);
			}
		}
		/* On failure, we free all memory allocated by the partial parse ... */
		t3_config_delete(context->result);
		/* ... and set context->config to NULL so we return NULL at the end. */
		context->result = NULL;
	}
	/* Delete the chain of included files (if there still is one after an error). */
	t3_config_delete(context->included);
	/* Free memory allocated by lexer. */
	_t3_config_lex_destroy(context->scanner);
	return context->result;
}


t3_config_t *t3_config_read_file(FILE *file, t3_config_error_t *error, const t3_config_opts_t *opts) {
	parse_context_t context;

	(void) opts;

	context.scan_type = SCAN_FILE;
	context.file = file;
	context.opts = opts;
	return config_read(&context, error);
}

t3_config_t *t3_config_read_buffer(const char *buffer, size_t size, t3_config_error_t *error, const t3_config_opts_t *opts) {
	parse_context_t context;

	(void) opts;

	context.scan_type = SCAN_BUFFER;
	context.buffer = buffer;
	context.buffer_size = size;
	context.buffer_idx = 0;
	context.opts = opts;
	return config_read(&context, error);
}

void t3_config_delete(t3_config_t *config) {
	t3_config_t *ptr = config;

	while (config != NULL) {
		config = ptr->next;
		switch ((int) ptr->type) {
			case T3_CONFIG_STRING:
				free(ptr->value.string);
				break;
			case T3_CONFIG_LIST:
			case T3_CONFIG_PLIST:
			case T3_CONFIG_SECTION:
			case T3_CONFIG_SCHEMA:
				t3_config_delete(ptr->value.list);
				break;
			case T3_CONFIG_EXPRESSION:
				_t3_config_delete_expr(ptr->value.expr);
				break;
			default:
				break;
		}
		_t3_config_unref_file_name(ptr);
		free(ptr->name);
		free(ptr);
		ptr = config;
	}
}

t3_config_t *t3_config_unlink(t3_config_t *config, const char *name) {
	t3_config_t *ptr, *prev;

	if (config == NULL || config->type != T3_CONFIG_SECTION)
		return NULL;

	prev = NULL;
	ptr = config->value.list;

	/* Find the named item in the list, keeping a reference to the item preceeding it. */
	while (ptr != NULL && strcmp(ptr->name, name) != 0) {
		prev = ptr;
		ptr = ptr->next;
	}

	if (ptr == NULL)
		return NULL;

	if (prev == NULL)
		config->value.list = ptr->next;
	else
		prev->next = ptr->next;
	ptr->next = NULL;
	return ptr;
}

t3_config_t *t3_config_unlink_from_list(t3_config_t *list, t3_config_t *item) {
	t3_config_t *ptr, *prev;

	if (list == NULL || (list->type != T3_CONFIG_SECTION && list->type != T3_CONFIG_LIST && list->type != T3_CONFIG_PLIST))
		return NULL;

	prev = NULL;
	ptr = list->value.list;

	/* Find the referenced item in the list, keeping a reference to the item preceeding it. */
	while (ptr != NULL && ptr != item) {
		prev = ptr;
		ptr = ptr->next;
	}

	if (ptr == NULL)
		return NULL;

	if (prev == NULL)
		list->value.list = ptr->next;
	else
		prev->next = ptr->next;
	ptr->next = NULL;
	return item;
}

void t3_config_erase(t3_config_t *config, const char *name) {
	t3_config_delete(t3_config_unlink(config, name));
}

void t3_config_erase_from_list(t3_config_t *list, t3_config_t *item) {
	t3_config_delete(t3_config_unlink_from_list(list, item));
}

/** Allocate a new item and link it to the end of the list.
    @p config must be either ::T3_CONFIG_LIST, ::T3_CONFIG_PLIST or ::T3_CONFIG_SECTION .
*/
static t3_config_t *config_add(t3_config_t *config, const char *name, t3_config_type_t type) {
	t3_config_t *result;

	if ((result = malloc(sizeof(t3_config_t))) == NULL)
		return NULL;
	if (name == NULL) {
		result->name = NULL;
	} else {
		if ((result->name = strdup(name)) == NULL) {
			free(result);
			return NULL;
		}
	}
	result->type = type;
	result->next = NULL;
	result->line_number = 0;
	result->file_name = NULL;

	if (config->value.list == NULL) {
		config->value.list = result;
	} else {
		t3_config_t *ptr = config->value.list;
		while (ptr->next != NULL) ptr = ptr->next;
		ptr->next = result;
	}

	return result;
}

/** Check whether @p config is either ::T3_CONFIG_LIST, ::T3_CONFIG_PLIST or ::T3_CONFIG_SECTION . */
static t3_bool can_add(t3_config_t *config, const char *name) {
	return config != NULL &&
		((config->type == T3_CONFIG_SECTION && name != NULL) ||
		((config->type == T3_CONFIG_LIST || config->type == T3_CONFIG_PLIST) && name == NULL));
}

static t3_bool istrcmp(const char *name, const char *str) {
	for ( ; *name != 0 && *str != 0; name++, str++) {
		if (((*name) | ('a' ^ 'A')) != ((*str) | ('a' ^ 'A')))
			return t3_false;
	}
	return *name == *str;
}

/** Check whether @p name is a valid key. */
static t3_bool check_name(const char *name) {
	if (name == NULL)
		return t3_true;
	if (!(strspn(name, "-_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") == strlen(name) &&
			strchr("-0123456789", name[0]) == NULL))
		return t3_false;

	if (istrcmp(name, "yes") || istrcmp(name, "no") || istrcmp(name, "true") || istrcmp(name, "false") ||
			istrcmp(name, "nan") || istrcmp(name, "inf") || istrcmp(name, "infinity"))
		return t3_false;

	return t3_true;
}

/** Add or replace an item.
    If an item with @p name already exists in the list in @p config, it will
    be stripped of its values, and returned. Otherwise a new item is created.
*/
static t3_config_t *add_or_replace(t3_config_t *config, const char *name, t3_config_type_t type) {
	t3_config_t *item;
	if (name == NULL || (item = t3_config_get(config, name)) == NULL)
		return config_add(config, name, type);

	if (item->type == T3_CONFIG_STRING)
		free(item->value.string);
	else if (item->type == T3_CONFIG_SECTION || item->type == T3_CONFIG_LIST || item->type == T3_CONFIG_PLIST)
		t3_config_delete(item->value.list);

	item->type = type;
	return item;
}

#define ADD(name_type, arg_type, TYPE, value_set) \
int t3_config_add_##name_type(t3_config_t *config, const char *name, arg_type value) { \
	t3_config_t *item; \
	if (!can_add(config, name) || !check_name(name)) \
		return T3_ERR_BAD_ARG; \
	if ((item = add_or_replace(config, name, TYPE)) == NULL) \
		return T3_ERR_OUT_OF_MEMORY; \
	value_set \
	return T3_ERR_SUCCESS; \
}

#define ADD_SIMPLE(name_type, arg_type, TYPE, value_name) ADD(name_type, arg_type, TYPE, item->value.value_name = value;)

ADD_SIMPLE(bool, t3_bool, T3_CONFIG_BOOL, boolean)
ADD_SIMPLE(int, t3_config_int_t, T3_CONFIG_INT, integer)
ADD_SIMPLE(number, double, T3_CONFIG_NUMBER, number)

int t3_config_add_string(t3_config_t *config, const char *name, const char *value) { \
	t3_config_t *item;
	char *value_copy;

	if (!can_add(config, name) || !check_name(name))
		return T3_ERR_BAD_ARG;
	if (strchr(value, '\n') != NULL)
		return T3_ERR_BAD_ARG;
	if ((value_copy = strdup(value)) == NULL)
		return T3_ERR_OUT_OF_MEMORY;

	if ((item = add_or_replace(config, name, T3_CONFIG_STRING)) == NULL) {
		free(value_copy);
		return T3_ERR_OUT_OF_MEMORY;
	}
	item->value.string = value_copy;
	return T3_ERR_SUCCESS;
}

/** Add a list or section. */
static t3_config_t *t3_config_add_aggregate(t3_config_t *config, const char *name, int *error, t3_config_type_t type) {
	t3_config_t *item;
	if (!can_add(config, name) || !check_name(name)) {
		if (error != NULL)
			*error = T3_ERR_BAD_ARG;
		return NULL;
	}
	if ((item = add_or_replace(config, name, type)) == NULL) {
		if (error != NULL)
			*error = T3_ERR_OUT_OF_MEMORY;
		return NULL;
	}
	item->value.list = NULL;
	return item;
}

#define ADD_AGGREGATE(type, TYPE) \
t3_config_t *t3_config_add_##type(t3_config_t *config, const char *name, int *error) { \
	return t3_config_add_aggregate(config, name, error, TYPE); \
}

ADD_AGGREGATE(list, T3_CONFIG_LIST)
ADD_AGGREGATE(plist, T3_CONFIG_PLIST)
ADD_AGGREGATE(section, T3_CONFIG_SECTION)

int t3_config_add_existing(t3_config_t *config, const char *name, t3_config_t *value) {
	char *item_name = NULL;
	if (!can_add(config, name) || !check_name(name) || value->next != NULL)
		return T3_ERR_BAD_ARG;

	if (name != NULL) {
		if ((item_name = strdup(name)) == NULL)
			return T3_ERR_OUT_OF_MEMORY;
	}

	free(value->name);
	value->name = item_name;

	if (config->value.list == NULL) {
		config->value.list = value;
	} else {
		t3_config_t *ptr = config->value.list;
		while (ptr->next != NULL) ptr = ptr->next;
		ptr->next = value;
	}

	return T3_ERR_SUCCESS;
}

int t3_config_set_list_type(t3_config_t *config, t3_config_type_t type) {
	if (config == NULL ||
			(config->type != T3_CONFIG_LIST && config->type != T3_CONFIG_PLIST) ||
			(type != T3_CONFIG_LIST && type != T3_CONFIG_PLIST))
		return T3_ERR_BAD_ARG;
	config->type = type;
	return T3_ERR_SUCCESS;
}

t3_config_t *t3_config_get(const t3_config_t *config, const char *name) {
	t3_config_t *result;
	if (config == NULL || (config->type != T3_CONFIG_SECTION && config->type != T3_CONFIG_LIST &&
			config->type != T3_CONFIG_PLIST && (int) config->type != T3_CONFIG_SCHEMA))
		return NULL;
	if (name != NULL && (config->type == T3_CONFIG_LIST || config->type == T3_CONFIG_PLIST))
		return NULL;
	if (name == NULL)
		return config->value.list;

	result = config->value.list;
	while (result != NULL && strcmp(result->name, name) != 0)
		result = result->next;
	return result;
}

t3_config_type_t t3_config_get_type(const t3_config_t *config) {
	return config != NULL ? config->type : T3_CONFIG_NONE;
}

t3_bool t3_config_is_list(const t3_config_t *config) {
	return config != NULL && (config->type == T3_CONFIG_LIST || config->type == T3_CONFIG_PLIST);
}

const char *t3_config_get_name(const t3_config_t *config) {
	return config != NULL ? config->name : NULL;
}

int t3_config_get_line(const t3_config_t *config) {
	return config == NULL ? 0 : config->line_number;
}

#define GET(name_type, arg_type, TYPE, value_name, dflt) \
arg_type t3_config_get_##name_type(const t3_config_t *config) { \
	return config != NULL && config->type == TYPE ? config->value.value_name : dflt; \
}

GET(bool, t3_bool, T3_CONFIG_BOOL, boolean, t3_false)
GET(int, t3_config_int_t, T3_CONFIG_INT, integer, 0)
GET(number, double, T3_CONFIG_NUMBER, number, 0.0)
GET(string, const char *, T3_CONFIG_STRING, string, NULL)

#define GET_DFLT(name_type, arg_type, TYPE, value_name) \
arg_type t3_config_get_##name_type##_dflt(const t3_config_t *config, arg_type dflt) { \
	return config != NULL && config->type == TYPE ? config->value.value_name : dflt; \
}

GET_DFLT(bool, t3_bool, T3_CONFIG_BOOL, boolean)
GET_DFLT(int, t3_config_int_t, T3_CONFIG_INT, integer)
GET_DFLT(number, double, T3_CONFIG_NUMBER, number)
GET_DFLT(string, const char *, T3_CONFIG_STRING, string)

char *t3_config_take_string(t3_config_t *config) {
	char *retval;

	if (config == NULL || config->type != T3_CONFIG_STRING)
		return NULL;

	retval = config->value.string;
	config->value.string = NULL;
	config->type = T3_CONFIG_NONE;
	return retval;
}

t3_config_t *t3_config_get_next(const t3_config_t *config) {
	return config != NULL ? config->next : NULL;
}

int t3_config_get_length(const t3_config_t *config) {
	int count = 0;
	if (config == NULL || (config->type != T3_CONFIG_LIST && config->type != T3_CONFIG_SECTION && config->type != T3_CONFIG_PLIST))
		return 0;
	for (config = config->value.list; config != NULL; config = config->next, count++) {}
	return count;
}

t3_config_t *t3_config_find(const t3_config_t *config,
		t3_bool (*predicate)(const t3_config_t *, const void *), const void *data, t3_config_t *start_from)
{
	t3_config_t *item;
	if (config == NULL || (config->type != T3_CONFIG_LIST && config->type != T3_CONFIG_SECTION && config->type != T3_CONFIG_PLIST))
		return NULL;

	item = config->value.list;
	if (start_from != NULL) {
		for (; item != start_from && item != NULL; item = item->next) {}

		if (item == NULL)
			return NULL;
		item = item->next;
	}

	for (; item != NULL && !predicate(item, data); item = item->next) {}

	return item;
}

long t3_config_get_version(void) {
	return T3_CONFIG_VERSION;
}

const char *t3_config_strerror(int error) {
	switch (error) {
		default:
			return t3_config_strerror_base(error);
		case T3_ERR_OUT_OF_RANGE:
			return _("value out of range");
		case T3_ERR_PARSE_ERROR:
			return _("parse error");
		case T3_ERR_DUPLICATE_KEY:
			return _("duplicate key");
		case T3_ERR_INVALID_CONSTRAINT:
			return _("invalid constraint");
		case T3_ERR_INVALID_KEY_TYPE:
			return _("key has invalid type");
		case T3_ERR_INVALID_KEY:
			return _("key is not allowed here");
		case T3_ERR_CONSTRAINT_VIOLATION:
			return _("schema constraint violated");
		case T3_ERR_RECURSIVE_TYPE:
			return _("recursive type definition");
		case T3_ERR_RECURSIVE_INCLUDE:
			return _("recursive include");
	}
}

int t3_config_get_line_number(const t3_config_t *config) {
	return config == NULL ? -1 : config->line_number;
}

const char *t3_config_get_file_name(const t3_config_t *config) {
	return config != NULL && config->file_name != NULL ? config->file_name->file_name : NULL;
}
