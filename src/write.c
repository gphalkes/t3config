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
#include <stdio.h>
#include <math.h>
#include <locale.h>
#include <string.h>
#ifdef USE_XLOCALE_H
#include <xlocale.h>
#endif

#include "config.h"
#include "config_internal.h"

static void write_list(t3_config_t *config, FILE *file, int indent);
static void write_section(t3_config_t *config, FILE *file, int indent);

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

/** Write a single value out to file. */
static void write_value(t3_config_t *config, FILE *file, int indent) {
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
			case T3_CONFIG_PLIST:
				fputs("( ", file);
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

}

/** Write a list to the output. */
static void write_list(t3_config_t *config, FILE *file, int indent) {
	t3_bool first = t3_true;
	while (config != NULL) {
		if (first)
			first = t3_false;
		else
			fputs(", ", file);

		write_value(config, file, indent);

		config = config->next;
	}
}

/** Write a plist to the output. */
static void write_plist(t3_config_t *config, FILE *file, int indent) {
	t3_config_t *base = config;
	config = config->value.list;

	while (config != NULL) {
		write_indent(file, indent);
		fputc('%', file);
		fputs(base->name, file);

		switch (config->type) {
			case T3_CONFIG_BOOL:
			case T3_CONFIG_INT:
			case T3_CONFIG_NUMBER:
			case T3_CONFIG_STRING:
			case T3_CONFIG_PLIST:
			case T3_CONFIG_LIST:
				fputs(" = ", file);
				write_value(config, file, indent);
				fputc('\n', file);
				break;
			case T3_CONFIG_SECTION:
				fputc(' ', file);
				write_value(config, file, indent);
				fputc('\n', file);
				break;
			default:
				/* This can only happen if the client screws up the list, which
				   the interface does not allow by itself. */
				break;
		}
		config = config->next;
	}
}

/** Write a section to the output. */
static void write_section(t3_config_t *config, FILE *file, int indent) {
	while (config != NULL) {
		if (config->type == T3_CONFIG_PLIST) {
			write_plist(config, file, indent);
			config = config->next;
			continue;
		}

		write_indent(file, indent);
		fputs(config->name, file);
		switch (config->type) {
			case T3_CONFIG_BOOL:
			case T3_CONFIG_INT:
			case T3_CONFIG_NUMBER:
			case T3_CONFIG_STRING:
			case T3_CONFIG_LIST:
				fputs(" = ", file);
				write_value(config, file, indent);
				fputc('\n', file);
				break;
			case T3_CONFIG_SECTION:
				fputc(' ', file);
				write_value(config, file, indent);
				fputc('\n', file);
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
