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


#include "config_internal.h"

/*
basic_types {
	int { type = "int" }
	number { type = "number" }
	string { type = "string" }
	list { type = "list" }
	section { type = "section" }
}
*/

static const t3_config_item_t *resolve_type(t3_config_item_t *types, const char *name) {
	if ((type = t3_config_get(basic_types, type_name)) != NULL)
		return type;


	while ((type = t3_config_get(types, type_name)) != NULL) {
		if (t3_config_get(basic_types, type) != NULL)
			return type;
	}
	return NULL;
}

static int validate_key(t3_config_item_t *config, t3_config_item_t *key, t3_config_item_t *types) {
	t3_config_item_t *type = resolve_type(types, t3_config_get_string(t3_config_get(key, "type")));
	/* At this point, the type can be any of section (containing a base type),
	   a list of types, or a base type definition. */

}

static int validate_section_keys(t3_config_item_t *config, t3_config_item_t *key, t3_config_item_t *types) {
	t3_config_item_t *allowed_keys = t3_config_get(schema, "allowed_keys"), *unknown_keys = t3_config_get(schema, "unknown_keys");

	config = t3_config_get(config, NULL);
	while (config != NULL) {
		if (allowed_keys != NULL && (key = t3_config_get(allowed_keys, config->name)) != NULL)
			validate_key(config, key, schema);
		else if (unknown_keys != NULL)
			validate_key(config, unknown_keys, schema);
	}
}

int t3_config_validate(t3_config_item_t *config, t3_config_item_t *schema) {
	if (schema->type != T3_CONFIG_SCHEMA)
		return T3_ERR_INVALID_SCHEMA;
	return validate_section_keys(config, schema, t3_config_get(schema, "types"));
}
