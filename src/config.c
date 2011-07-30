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
#include <stdlib.h>
#include <string.h>
#include "config_internal.h"
#include "parser.h"

#ifdef USE_GETTEXT
#include <libintl.h>
#define _(x) dgettext("LIBT3", (x))
#else
#define _(x) (x)
#endif

static void write_section(t3_config_item_t *config, FILE *file, int indent);

#ifndef HAVE_STRDUP
char *_t3_config_strdup(const char *str) {
	char *result;
	size_t len = strlen(str) + 1;

	if ((result = malloc(len)) == NULL)
		return NULL;
	memcpy(result, str, len);
	return result;
}
#endif

t3_config_item_t *t3_config_new(void) {
	t3_config_item_t *result;

	if ((result = malloc(sizeof(t3_config_item_t))) == NULL)
		return NULL;
	result->name = NULL;
	result->type = T3_CONFIG_SECTION;
	result->value.list = NULL;
	result->next = NULL;
	return result;
}

static t3_config_item_t *config_read(parse_context_t *context, t3_config_error_t *error) {
	int retval;

	if (_t3_config_lex_init_extra(context, &context->scanner) != 0) {
		if (error != NULL) {
			error->error = T3_ERR_OUT_OF_MEMORY;
			error->line_number = 0;
		}
		return NULL;
	}

	if ((retval = _t3_config_parse(context)) != 0) {
		if (error != NULL) {
			error->error = retval;
			error->line_number = _t3_config_get_extra(context->scanner)->line_number;
		}
		t3_config_delete(context->config);
		context->config = NULL;
	}
	_t3_config_lex_destroy(context->scanner);
	return context->config;
}


t3_config_item_t *t3_config_read_file(FILE *file, t3_config_error_t *error) {
	parse_context_t context;

	context.scan_type = SCAN_FILE;
	context.file = file;
	context.line_number = 1;
	context.config = NULL;
	return config_read(&context, error);
}

t3_config_item_t *t3_config_read_buffer(const char *buffer, t3_config_error_t *error) {
	parse_context_t context;

	context.scan_type = SCAN_BUFFER;
	context.buffer = buffer;
	context.buffer_idx = 0;
	context.line_number = 1;
	context.config = NULL;
	return config_read(&context, error);
}

static void write_indent(FILE *file, int indent) {
	static const char tabs[8] = "\t\t\t\t\t\t\t\t";
	while (indent > 8) {
		fwrite(tabs, 1, 8, file);
		indent -= 8;
	}
	fwrite(tabs, 1, indent, file);
}

static void write_int(FILE *file, t3_config_int_t value) {
	char buffer[80], *buffer_ptr = buffer + 78;
	int digit;

	if (value == 0) {
		fputc('0', file);
		return;
	} else if (value < 0) {
		fputc('-', file);
	}

	buffer[79] = 0;
	while (value != 0) {
		digit = value % 10;
		value /= 10;
		*buffer_ptr-- = '0' + (digit < 0 ? -digit : digit);
	}

	fputs(buffer_ptr + 1, file);
}

static void write_number(FILE *file, double value) {
	//FIXME: replace by locale independent version!
	fprintf(file, "%#g", value);
}

static int count_quotes(const char *value, char quote_char) {
	const char *quote;
	int count = 0;

	quote = strchr(value, quote_char);
	while (quote != NULL) {
		count++;
		quote = strchr(quote + 1, quote_char);
	}
	return count;
}

static void write_string(FILE *file, const char *value) {
	const char *quote;
	char quote_char = '"';
	int single_count, double_count;

	double_count = count_quotes(value, '"');
	if (double_count != 0) {
		single_count = count_quotes(value, '\'');
		if (single_count < double_count)
			quote_char = '\'';
	}

	fputc(quote_char, file);
	while ((quote = strchr(value, quote_char)) != NULL) {
		fwrite(value, 1, quote - value, file);
		fputc(quote_char, file);
		fputc(quote_char, file);
		value = quote + 1;
	}
	fwrite(value, 1, strlen(value), file);
	fputc(quote_char, file);
}

static void write_list(t3_config_item_t *config, FILE *file, int indent) {
	t3_bool first = t3_true;
	while (config != NULL) {
		if (first)
			first = t3_false;
		else
			fputc(',', file);

		fputc(' ', file);

		switch (config->type) {
			case T3_CONFIG_BOOL:
				fputs(config->value.boolean ? "true" : "false", file);
				break;
			case T3_CONFIG_INT:
				write_int(file, config->value.integer);
				break;
			case T3_CONFIG_NUMBER:
				write_number(file, config->value.number);
				break;
			case T3_CONFIG_STRING:
				write_string(file, config->value.string);
				break;
			case T3_CONFIG_LIST:
				fputc('(', file);
				write_list(config->value.list, file, indent + 1);
				fputs(" )", file);
				break;
			case T3_CONFIG_SECTION:
				fputs("{\n", file);
				write_section(config->value.list, file, indent + 1);
				write_indent(file, indent);
				fputc('}', file);
				break;
			default:
				/* This can only happen if the client screws up the list. */
				break;
		}
		config = config->next;
	}
}

static void write_section(t3_config_item_t *config, FILE *file, int indent) {
	while (config != NULL) {
		write_indent(file, indent);
		fputs(config->name, file);
		switch (config->type) {
			case T3_CONFIG_BOOL:
				fputs(" = ", file);
				fputs(config->value.boolean ? "true" : "false", file);
				fputc('\n', file);
				break;
			case T3_CONFIG_INT:
				fputs(" = ", file);
				write_int(file, config->value.integer);
				fputc('\n', file);
				break;
			case T3_CONFIG_NUMBER:
				fputs(" = ", file);
				write_number(file, config->value.number);
				fputc('\n', file);
				break;
			case T3_CONFIG_STRING:
				fputs(" = ", file);
				write_string(file, config->value.string);
				fputc('\n', file);
				break;
			case T3_CONFIG_LIST:
				fputs(" = (", file);
				write_list(config->value.list, file, indent);
				fputs(" )\n", file);
				break;
			case T3_CONFIG_SECTION:
				fputs(" {\n", file);
				write_section(config->value.list, file, indent + 1);
				write_indent(file, indent);
				fputs("}\n", file);
				break;
			default:
				/* This can only happen if the client screws up the list. */
				break;
		}
		config = config->next;
	}
}

int t3_config_write_file(t3_config_item_t *config, FILE *file) {
	if (config->type != T3_CONFIG_SECTION)
		return T3_ERR_BAD_ARG;

	write_section(config->value.list, file, 0);
	return ferror(file) ? T3_ERR_ERRNO : T3_ERR_SUCCESS;
}


void t3_config_delete(t3_config_item_t *config) {
	t3_config_item_t *ptr = config;

	while (config != NULL) {
		config = ptr->next;
		switch (ptr->type) {
			case T3_CONFIG_STRING:
				free(ptr->value.string);
				break;
			case T3_CONFIG_LIST:
			case T3_CONFIG_SECTION:
			case T3_CONFIG_SCHEMA:
				t3_config_delete(ptr->value.list);
				break;
			default:
				break;
		}
		free(ptr->name);
		free(ptr);
		ptr = config;
	}
}

t3_config_item_t *t3_config_remove(t3_config_item_t *config, const char *name) {
	t3_config_item_t *ptr, *prev;

	if (config == NULL || config->type != T3_CONFIG_SECTION)
		return NULL;

	prev = NULL;
	ptr = config->value.list;

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

void t3_config_prune(t3_config_item_t *config, const char *name) {
	t3_config_delete(t3_config_remove(config, name));
}

static t3_config_item_t *config_add(t3_config_item_t *config, const char *name, t3_config_item_type_t type) {
	t3_config_item_t *result;

	if ((result = malloc(sizeof(t3_config_item_t))) == NULL)
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
	if (config->value.list == NULL) {
		config->value.list = result;
	} else {
		t3_config_item_t *ptr = config->value.list;
		while (ptr->next != NULL) ptr = ptr->next;
		ptr->next = result;
	}

	return result;
}

#define CHECK_ARGS(_config, _name) do { \
	int retval; \
	if ((retval = check_args(_config, _name)) != T3_ERR_SUCCESS) \
		return retval; \
} while (0)

static int check_args(t3_config_item_t *config, const char *name) {
	if (!((config->type == T3_CONFIG_SECTION && name != NULL) || (config->type == T3_CONFIG_LIST && name == NULL)))
		return T3_ERR_BAD_ARG;
	if (name == NULL)
		return T3_ERR_SUCCESS;
	if (t3_config_get(config, name) != NULL)
		return T3_ERR_DUPLICATE_KEY;
	return T3_ERR_SUCCESS;
}

int t3_config_add_bool(t3_config_item_t *config, const char *name, t3_bool value) {
	t3_config_item_t *item;
	CHECK_ARGS(config, name);
	if ((item = config_add(config, name, T3_CONFIG_BOOL)) == NULL)
		return T3_ERR_OUT_OF_MEMORY;
	item->value.boolean = value;
	return T3_ERR_SUCCESS;
}

int t3_config_add_int(t3_config_item_t *config, const char *name, t3_config_int_t value) {
	t3_config_item_t *item;
	CHECK_ARGS(config, name);
	if ((item = config_add(config, name, T3_CONFIG_INT)) == NULL)
		return T3_ERR_OUT_OF_MEMORY;
	item->value.integer = value;
	return T3_ERR_SUCCESS;
}

int t3_config_add_number(t3_config_item_t *config, const char *name, double value) {
	t3_config_item_t *item;
	CHECK_ARGS(config, name);
	if ((item = config_add(config, name, T3_CONFIG_NUMBER)) == NULL)
		return T3_ERR_OUT_OF_MEMORY;
	item->value.number = value;
	return T3_ERR_SUCCESS;
}

int t3_config_add_string(t3_config_item_t *config, const char *name, const char *value) {
	t3_config_item_t *item;
	CHECK_ARGS(config, name);
	if ((item = config_add(config, name, T3_CONFIG_STRING)) == NULL)
		return T3_ERR_OUT_OF_MEMORY;
	if ((item->value.string = strdup(value)) == NULL) {
		item->type = T3_CONFIG_NONE;
		t3_config_remove(config, name);
		t3_config_delete(item);
		return T3_ERR_OUT_OF_MEMORY;
	}
	return T3_ERR_SUCCESS;
}

int t3_config_add_list(t3_config_item_t *config, const char *name) {
	t3_config_item_t *item;
	CHECK_ARGS(config, name);
	if ((item = config_add(config, name, T3_CONFIG_LIST)) == NULL)
		return T3_ERR_OUT_OF_MEMORY;
	item->value.list = NULL;
	return T3_ERR_SUCCESS;
}

int t3_config_add_section(t3_config_item_t *config, const char *name) {
	t3_config_item_t *item;
	CHECK_ARGS(config, name);
	if (config->type == T3_CONFIG_LIST)
		return T3_ERR_BAD_ARG;
	if ((item = config_add(config, name, T3_CONFIG_SECTION)) == NULL)
		return T3_ERR_OUT_OF_MEMORY;
	item->value.list = NULL;
	return T3_ERR_SUCCESS;
}

t3_config_item_t *t3_config_get(const t3_config_item_t *config, const char *name) {
	t3_config_item_t *result;
	if (config->type != T3_CONFIG_SECTION && config->type != T3_CONFIG_LIST && config->type != T3_CONFIG_SCHEMA)
		return NULL;
	if (name != NULL && config->type == T3_CONFIG_LIST)
		return NULL;
	if (name == NULL)
		return config->value.list;

	result = config->value.list;
	while (result != NULL && strcmp(result->name, name) != 0)
		result = result->next;
	return result;
}

t3_config_item_type_t t3_config_get_type(const t3_config_item_t *config) {
	return config != NULL ? config->type : T3_CONFIG_NONE;
}

const char *t3_config_get_name(const t3_config_item_t *config) {
	return config != NULL ? config->name : NULL;
}

t3_bool t3_config_get_bool(const t3_config_item_t *config) {
	return config != NULL && config->type == T3_CONFIG_BOOL ? config->value.boolean : 0;
}

t3_config_int_t t3_config_get_int(const t3_config_item_t *config) {
	return config != NULL && config->type == T3_CONFIG_INT ? config->value.integer : 0;
}

double t3_config_get_number(const t3_config_item_t *config) {
	return config != NULL && config->type == T3_CONFIG_NUMBER ? config->value.number : 0.0;
}

const char *t3_config_get_string(const t3_config_item_t *config) {
	return config != NULL && config->type == T3_CONFIG_STRING ? config->value.string : NULL;
}

t3_config_item_t *t3_config_get_list(const t3_config_item_t *config) {
	return config != NULL && config->type == T3_CONFIG_LIST ? config->value.list : NULL;
}

t3_config_item_t *t3_config_get_section(const t3_config_item_t *config) {
	return config != NULL && (config->type == T3_CONFIG_SECTION || config->type == T3_CONFIG_SCHEMA) ? config->value.list : NULL;
}

t3_config_item_t *t3_config_get_next(const t3_config_item_t *config) {
	return config != NULL ? config->next : NULL;
}

int t3_config_get_version(void) {
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
	}
}

