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
#ifdef USE_XLOCALE_H
#include <xlocale.h>
#endif

#include "config_internal.h"
#include "parser.h"

#ifdef USE_GETTEXT
#include <libintl.h>
#define _(x) dgettext("LIBT3", (x))
#else
#define _(x) (x)
#endif

static void write_section(t3_config_t *config, FILE *file, int indent);

#ifndef HAS_STRDUP
/** strdup implementation if none is provided by the environment. */
char *_t3_config_strdup(const char *str) {
	char *result;
	size_t len = strlen(str) + 1;

	if ((result = malloc(len)) == NULL)
		return NULL;
	memcpy(result, str, len);
	return result;
}
#endif

t3_config_t *t3_config_new(void) {
	t3_config_t *result;

	if ((result = malloc(sizeof(t3_config_t))) == NULL)
		return NULL;
	result->name = NULL;
	result->type = T3_CONFIG_SECTION;
	result->value.list = NULL;
	result->next = NULL;
	return result;
}

/** Read config, either from file or from buffer. */
static t3_config_t *config_read(parse_context_t *context, t3_config_error_t *error) {
	int retval;

	/* Initialize lexer. */
	if (_t3_config_lex_init_extra(context, &context->scanner) != 0) {
		if (error != NULL) {
			error->error = T3_ERR_OUT_OF_MEMORY;
			error->line_number = 0;
		}
		return NULL;
	}

	/* Perform parse. */
	if ((retval = _t3_config_parse(context)) != 0) {
		if (error != NULL) {
			error->error = retval;
			error->line_number = _t3_config_get_extra(context->scanner)->line_number;
		}
		/* On failure, we free all memory allocated by the partial parse ... */
		t3_config_delete(context->config);
		/* ... and set context->config to NULL so we return NULL at the end. */
		context->config = NULL;
	}
	/* Free memory allocated by lexer. */
	_t3_config_lex_destroy(context->scanner);
	return context->config;
}


t3_config_t *t3_config_read_file(FILE *file, t3_config_error_t *error, void *opts) {
	parse_context_t context;

	(void) opts;

	context.scan_type = SCAN_FILE;
	context.file = file;
	context.line_number = 1;
	context.config = NULL;
	return config_read(&context, error);
}

t3_config_t *t3_config_read_buffer(const char *buffer, size_t size, t3_config_error_t *error, void *opts) {
	parse_context_t context;

	(void) opts;

	context.scan_type = SCAN_BUFFER;
	context.buffer = buffer;
	context.buffer_size = size;
	context.buffer_idx = 0;
	context.line_number = 1;
	context.config = NULL;
	return config_read(&context, error);
}

#ifdef HAS_USELOCALE
/** Locale independent strtod implemenation. */
double _t3_config_strtod(char *text) {
	double result;
	locale_t prev_locale, c_locale;

	c_locale = newlocale(LC_ALL, "C", (locale_t) 0);
	prev_locale = uselocale(c_locale);

	result = strtod(text, NULL);

	uselocale(prev_locale);
	freelocale(c_locale);

	return result;
}
#else
/** Locale independent strtod implemenation.

    @internal
    Wraps the locale dependent strtod implementation from the C library. The
    rationale for not using dtoa.c is that it has too many configuration
    options with platform dependencies such as endianess. The C library
    implementers have already figured all that out, so we just use their
    strtod by replacing the period by localeconv()->decimal_point.
    Furthermore, we would have to track updates to those sources as well,
    as bug-fixes are still being applied to this piece of 20 year old code ...
*/
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

	/* If we reach this point, the value contains a decimal point and
	   decimal_point is more than a single byte. This may for example occur
	   if the decimal separater is a Unicode character above U+007F, or in some
	   multi-byte character set. So, we create a representation of the value in
	   our local buffer, which allows us to paste in the decimal_separator.

	   First of course, we copy a sign, if that is present ...
	*/
	if (*text == '-') {
		buffer[idx++] = '-';
		text++;
	}

	/* ... then we skip all leading zeros. */
	while (*text == '0') text++;

	/* We already know that there is a decimal point in the text, so that is
	   the last character we can possibly stop at. Now if there are more than 50
	   digits in the input, we can safely assume that the result is too large.
	   In that case we just emulate the behaviour of strtod, instead of trying
	   to feed it some silly value.
	*/
	if ((decimal_point - text) > 50) {
		errno = ERANGE;
		/* If we already copied a sign bit, pass -inf. */
		return idx == 0 ? HUGE_VAL : - HUGE_VAL;
	}

	/* Now we can copy all the text up to the decimal point ... */
	memcpy(buffer, text, decimal_point - text);
	idx += decimal_point - text;
	/* ... and paste decimal_point after it. */
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
#endif

/** Write indentation to the output. */
static void write_indent(FILE *file, int indent) {
	static const char tabs[8] = "\t\t\t\t\t\t\t\t";
	while (indent > (int) sizeof(tabs)) {
		fwrite(tabs, 1, sizeof(tabs), file);
		indent -= sizeof(tabs);
	}
	fwrite(tabs, 1, indent, file);
}

/** Write a single integer to the output. */
static void write_int(FILE *file, t3_config_int_t value) {
	fprintf(file, "%" T3_CONFIG_INT_PRI "d", value);
}

#ifdef HAS_USELOCALE
/** Write a floating point number to the output. */
static void write_number(FILE *file, double value) {
	char buffer[160];
	locale_t prev_locale, c_locale;

	/* Make sure that we have standard representations for not-a-number and
	   infinity. Especially NaN is allowed to have extra characters in the C
	   specification.
	*/
	if (isnan(value)) {
		fprintf(file, "%sNaN", signbit(value) ? "-" : "");
		return;
	} else if (isinf(value)) {
		fprintf(file, "%sInfinity", signbit(value) ? "-" : "");
		return;
	}

	c_locale = newlocale(LC_ALL, "C", (locale_t) 0);
	prev_locale = uselocale(c_locale);

	/* Print to buffer, because we want to add .0 if there is no decimal point. */
	sprintf(buffer, "%.18g", value);

	uselocale(prev_locale);
	freelocale(c_locale);

	/* If there is no decimal point, add .0 */
	if (strchr(buffer, '.') == NULL)
		strcat(buffer, ".0");
	fputs(buffer, file);
	return;
}
#else
/** Write a floating point number to the output. */
static void write_number(FILE *file, double value) {
	char buffer[160], *decimal_point;
	struct lconv *ldata = localeconv();

	/* Make sure that we have standard representations for not-a-number and
	   infinity. Especially NaN is allowed to have extra characters in the C
	   specification.
	*/
	if (isnan(value)) {
		fprintf(file, "%sNaN", signbit(value) ? "-" : "");
		return;
	} else if (isinf(value)) {
		fprintf(file, "%sInfinity", signbit(value) ? "-" : "");
		return;
	}

	sprintf(buffer, "%.18g", value);
	/* Replace locale dependent decimal point with '.' */
	if (strcmp(ldata->decimal_point, ".") != 0) {
		decimal_point = strstr(buffer, ldata->decimal_point);
		if (decimal_point != NULL) {
			memmove(decimal_point + 1, decimal_point + strlen(ldata->decimal_point),
				strlen(decimal_point + strlen(ldata->decimal_point)) + 1);
			*decimal_point = '.';
		}
	}

	/* If there is no decimal point, add .0 */
	if (strchr(buffer, '.') == NULL)
		strcat(buffer, ".0");
	fputs(buffer, file);
}
#endif

/** Determine the number of quote characters in a string.
    @param value The string to check.
    @param quote_char The quote character.
    @return The number of occurences of @p quote_char in @p value.
*/
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

/** Write a string to the output, quoting and escaping as necessary.
    This routine optimizes its use of quotes by counting the number of quotes
    in the string and using the quotes that occur least in the string itself.
*/
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

/** Write a list to the output. */
static void write_list(t3_config_t *config, FILE *file, int indent) {
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

/** Write a section to the output. */
static void write_section(t3_config_t *config, FILE *file, int indent) {
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
				/* This can only happen if the client screws up the list, which
				   the interface does not allow by itself. */
				break;
		}
		config = config->next;
	}
}

int t3_config_write_file(t3_config_t *config, FILE *file) {
	if (config->type != T3_CONFIG_SECTION)
		return T3_ERR_BAD_ARG;

	write_section(config->value.list, file, 0);
	return ferror(file) ? T3_ERR_ERRNO : T3_ERR_SUCCESS;
}


void t3_config_delete(t3_config_t *config) {
	t3_config_t *ptr = config;

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

	if (list == NULL || (list->type != T3_CONFIG_SECTION && list->type != T3_CONFIG_LIST))
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
    @p config must be either ::T3_CONFIG_LIST or ::T3_CONFIG_SECTION .
*/
static t3_config_t *config_add(t3_config_t *config, const char *name, t3_config_item_type_t type) {
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

	if (config->value.list == NULL) {
		config->value.list = result;
	} else {
		t3_config_t *ptr = config->value.list;
		while (ptr->next != NULL) ptr = ptr->next;
		ptr->next = result;
	}

	return result;
}

/** Check whether @p config is either ::T3_CONFIG_LIST or ::T3_CONFIG_SECTION . */
static t3_bool can_add(t3_config_t *config, const char *name) {
	return config != NULL &&
		((config->type == T3_CONFIG_SECTION && name != NULL) ||
		(config->type == T3_CONFIG_LIST && name == NULL));
}

/** Check whether @p name is a valid key. */
static t3_bool check_name(const char *name) {
	return name == NULL || (strspn(name, "-_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") == strlen(name) &&
		strchr("-0123456789", name[0]) == NULL);
}

/** Add or replace an item.
    If an item with @p name already exists in the list in @p config, it will
    be stripped of its values, and returned. Otherwise a new item is created.
*/
static t3_config_t *add_or_replace(t3_config_t *config, const char *name, t3_config_item_type_t type) {
	t3_config_t *item;
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
ADD(string, const char *, T3_CONFIG_STRING,
	if ((item->value.string = strdup(value)) == NULL) { t3_config_erase(config, name); return T3_ERR_OUT_OF_MEMORY; }
)

/** Add a list or section. */
static t3_config_t *t3_config_add_aggregate(t3_config_t *config, const char *name, int *error, t3_config_item_type_t type) {
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
ADD_AGGREGATE(section, T3_CONFIG_SECTION)

int t3_config_add_existing(t3_config_t *config, const char *name, t3_config_t *value) {
	char *item_name = NULL;
	if (!can_add(config, name) || !check_name(name))
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

t3_config_t *t3_config_get(const t3_config_t *config, const char *name) {
	t3_config_t *result;
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

t3_config_item_type_t t3_config_get_type(const t3_config_t *config) {
	return config != NULL ? config->type : T3_CONFIG_NONE;
}

const char *t3_config_get_name(const t3_config_t *config) {
	return config != NULL ? config->name : NULL;
}

#define GET(name_type, arg_type, TYPE, value_name, deflt) \
arg_type t3_config_get_##name_type(const t3_config_t *config) { \
	return config != NULL && config->type == TYPE ? config->value.value_name : deflt; \
}

GET(bool, t3_bool, T3_CONFIG_BOOL, boolean, t3_false)
GET(int, t3_config_int_t, T3_CONFIG_INT, integer, 0)
GET(number, double, T3_CONFIG_NUMBER, number, 0.0)
GET(string, const char *, T3_CONFIG_STRING, string, NULL)

t3_config_t *t3_config_get_next(const t3_config_t *config) {
	return config != NULL ? config->next : NULL;
}

t3_config_t *t3_config_find(const t3_config_t *config,
		t3_bool (*predicate)(t3_config_t *, void *), void *data, t3_config_t *start_from)
{
	t3_config_t *item;
	if (config == NULL || (config->type != T3_CONFIG_LIST && config->type != T3_CONFIG_SECTION))
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

