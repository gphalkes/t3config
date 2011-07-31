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
#include <locale.h>
#include <errno.h>
#include <math.h>

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

t3_config_item_t *t3_config_read_buffer(const char *buffer, size_t size, t3_config_error_t *error) {
	parse_context_t context;

	context.scan_type = SCAN_BUFFER;
	context.buffer = buffer;
	context.buffer_size = size;
	context.buffer_idx = 0;
	context.line_number = 1;
	context.config = NULL;
	return config_read(&context, error);
}

double _t3_config_strtod(char *text) {
	struct lconv *ldata = localeconv();
	char buffer[160], *decimal_point;
	size_t idx = 0;

	/* This routine has the following problem to solve: the values we read from
	   the configuration file use a period as decimal separator. However, strtod
	   is locale dependent, and expects decimal_separator (although some
	   implementations also accept a period when decimal_separator is different).
	   Thus if the decimal_separator is not a period, it needs to be replaced.

	   As an extra obstacle, I don't want an out-of-memory condition to make this
	   routine fail, so malloc is out.

	   Now, we do already know that the input is actually a valid textual
	   representation of a floating point value, because it has already been
	   identified as such by the lexer.

	   So here we go:
	*/

	/* If decimal_point happens to be a period, just call strtod ... */
	if (strcmp(ldata->decimal_point, ".") == 0)
		return strtod(text, NULL);

	/* ... and also call strtod if there is no decimal point to begin with ... */
	if ((decimal_point = strchr(text, '.')) == NULL)
		return strtod(text, NULL);

	/* ... and if decimal_point is a single character, just replace it in the
	   string we got from the lexer and call strtod. */
	if (strlen(ldata->decimal_point) == 1) {
		*decimal_point = ldata->decimal_point[0];
		return strtod(text, NULL);
	}

	/* decimal_point is more than a single byte. This may for example occur
	   if the decimal separater is a Unicode character above U+007F, or in some
	   multi-byte character set. So, we create a representation of it in our
	   local buffer, which allows us to do paste in the decimal_separator.

	   First of course, we copy a sign, if that is present ...
	*/
	if (*text == '-') {
		buffer[idx++] = '-';
		text++;
	}

	/* ... then we skip all leading zeros. */
	while (*text == '0') text++;

	/* We already know that there is a decimal point in the text, so that last
	   character we can possibly stop at. Now if there are more than 60
	   digits in the input, we can safely assume that the result is too large.
	   In that case we just emulate the behaviour of strtod, instead of trying
	   to feed it some silly value.
	*/
	if ((decimal_point - text) > 60) {
		errno = ERANGE;
		/* If we already copied a sign bit, pass -inf. */
		return idx == 0 ? HUGE_VAL : - HUGE_VAL;
	}
	/* Now we can copy all the text up to the decimal point ... */
	memcpy(buffer, text, decimal_point - text);
	idx += decimal_point - text;
	/* ... and paste the decimal_point after it. */
	strcat(buffer + idx, ldata->decimal_point);
	idx += strlen(ldata->decimal_point);

	/* That leaves us with the text after the decimal point. We copy all digits
	   up until we fill our buffer with 131 characters. That should leave more
	   than enough digits to leave the precision intact. */
	text = decimal_point + 1;
	while (idx < 130 && *text != 0) {
		if (*text == 'e' || *text == 'E')
			break;
		buffer[idx++] = *text;
	}
	/* Skip all further digits, and stop at either an 'e' character, or the end
	   of the string. */
	while (*text != 'e' && *text != 'E' && *text != 0) text++;
	/* If we found an 'e' character, copy the exponent up until a buffer fill
	   of 159 characters (thus leaving just enough space for the nul byte). */
	if (*text != 0) {
		while (idx < 158 && *text != 0)
			buffer[idx++] = *text;
	}
	/* Terminate the string, and pass it to strtod, which should now be able
	   to parse it correctly. */
	buffer[idx] = 0;
	return strtod(buffer, NULL);
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
	fprintf(file, "%d", value);
}

static void write_number(FILE *file, double value) {
	char buffer[160], *decimal_point;
	struct lconv *ldata = localeconv();

	/* Make sure that we have standard representations for not-a-number and
	   infinity. Especially NaN is allowed to have extra characters. */
	if (isnan(value)) {
		fprintf(file, "%sNaN", signbit(value) ? "-" : "");
		return;
	} else if (isinf(value)) {
		fprintf(file, "%sInfinity", signbit(value) ? "-" : "");
		return;
	}

	snprintf(buffer, sizeof(buffer), "%g", value);
	/* Replace locale dependent decimal point with '.' */
	if (strcmp(ldata->decimal_point, ".") != 0) {
		decimal_point = strstr(buffer, ldata->decimal_point);
		if (decimal_point != NULL) {
			memmove(decimal_point + 1, decimal_point + strlen(ldata->decimal_point),
				strlen(decimal_point + strlen(ldata->decimal_point)) + 1);
			*decimal_point = '.';
		}
	}

	if (strchr(buffer, '.') == NULL)
		strcat(buffer, ".0");
	fputs(buffer, file);
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

t3_config_item_t *t3_config_unlink(t3_config_item_t *config, const char *name) {
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

t3_config_item_t *t3_config_unlink_from_list(t3_config_item_t *list, t3_config_item_t *item) {
	t3_config_item_t *ptr, *prev;

	if (list == NULL || (list->type != T3_CONFIG_SECTION && list->type != T3_CONFIG_LIST))
		return NULL;

	prev = NULL;
	ptr = list->value.list;

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

void t3_config_erase(t3_config_item_t *config, const char *name) {
	t3_config_delete(t3_config_unlink(config, name));
}

void t3_config_erase_from_list(t3_config_item_t *list, t3_config_item_t *item) {
	t3_config_delete(t3_config_unlink_from_list(list, item));
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

static t3_bool can_add(t3_config_item_t *config, const char *name) {
	return config != NULL &&
		((config->type == T3_CONFIG_SECTION && name != NULL) ||
		(config->type == T3_CONFIG_LIST && name == NULL));
}

static t3_config_item_t *add_or_replace(t3_config_item_t *config, const char *name, t3_config_item_type_t type) {
	t3_config_item_t *item;
	if (name == NULL || (item = t3_config_get(config, name)) == NULL)
		return config_add(config, name, type);

	if (item->type == T3_CONFIG_STRING)
		free(item->value.string);
	else if (item->type == T3_CONFIG_SECTION || item->type == T3_CONFIG_LIST)
		t3_config_delete(item->value.list);

	item->type = type;
	return item;
}

#define ADD(name_type, arg_type, TYPE, value_set) \
int t3_config_add_##name_type(t3_config_item_t *config, const char *name, arg_type value) { \
	t3_config_item_t *item; \
	if (!can_add(config, name)) \
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
ADD(string, const char *, T3_CONFIG_STRING,
	if ((item->value.string = strdup(value)) == NULL) { t3_config_erase(config, name); return T3_ERR_OUT_OF_MEMORY; }
)

t3_config_item_t *t3_config_add_list(t3_config_item_t *config, const char *name, int *error) {
	t3_config_item_t *item;
	if (!can_add(config, name)) {
		if (error != NULL)
			*error = T3_ERR_BAD_ARG;
		return NULL;
	}
	if ((item = add_or_replace(config, name, T3_CONFIG_LIST)) == NULL) {
		if (error != NULL)
			*error = T3_ERR_OUT_OF_MEMORY;
		return NULL;
	}
	item->value.list = NULL;
	return item;
}

t3_config_item_t *t3_config_add_section(t3_config_item_t *config, const char *name, int *error) {
	t3_config_item_t *item;
	if (!can_add(config, name)) {
		if (error != NULL)
			*error = T3_ERR_BAD_ARG;
		return NULL;
	}
	if ((item = add_or_replace(config, name, T3_CONFIG_SECTION)) == NULL) {
		if (error != NULL)
			*error = T3_ERR_OUT_OF_MEMORY;
		return NULL;
	}
	item->value.list = NULL;
	return item;
}

int t3_config_add_existing(t3_config_item_t *config, const char *name, t3_config_item_t *value) {
	char *item_name = NULL;
	if (!can_add(config, name))
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
		t3_config_item_t *ptr = config->value.list;
		while (ptr->next != NULL) ptr = ptr->next;
		ptr->next = value;
	}
	return T3_ERR_SUCCESS;
}

t3_config_item_t *t3_config_get(const t3_config_item_t *config, const char *name) {
	t3_config_item_t *result;
	if (config == NULL || (config->type != T3_CONFIG_SECTION && config->type != T3_CONFIG_LIST))
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

#define GET(name_type, arg_type, TYPE, value_name, deflt) \
arg_type t3_config_get_##name_type(const t3_config_item_t *config) { \
	return config != NULL && config->type == TYPE ? config->value.value_name : deflt; \
}

GET(bool, t3_bool, T3_CONFIG_BOOL, boolean, t3_false)
GET(int, t3_config_int_t, T3_CONFIG_INT, integer, 0)
GET(number, double, T3_CONFIG_NUMBER, number, 0.0)
GET(string, const char *, T3_CONFIG_STRING, string, NULL)

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

