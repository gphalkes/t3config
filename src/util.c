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
#ifdef USE_XLOCALE_H
#include <xlocale.h>
#endif
#include "util.h"

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

void _t3_unescape(char *dest, const char *src) {
	size_t i, j;

	for (i = 1, j = 0; !(src[i] == src[0] && src[i + 1] == 0); i++, j++) {
		dest[j] = src[i];
		/* Because the only quotes that can occur in the string itself
		   are doubled (checked by lexing), we don't have to check the
		   next character. */
		if (src[i] == src[0])
			i++;
	}
	dest[j] = 0;
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

t3_config_type_t _t3_config_str2type(const char *name) {
	static const struct {
		const char *name;
		t3_config_type_t type;
	} map[] = {
		{ "int", T3_CONFIG_INT },
		{ "bool", T3_CONFIG_BOOL },
		{ "number", T3_CONFIG_NUMBER },
		{ "string", T3_CONFIG_STRING },
		{ "section", T3_CONFIG_SECTION },
		{ "list", T3_CONFIG_LIST }
	};

	size_t i;
	for (i = 0; i < sizeof(map) / sizeof(map[0]); i++)
		if (strcmp(name, map[i].name) == 0)
			return map[i].type;
	return T3_CONFIG_NONE;
}
