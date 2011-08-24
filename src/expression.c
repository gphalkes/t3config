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
#include <string.h>

#include "config_internal.h"
#include "expression.h"

static t3_bool is_present(expr_node_t *expression, t3_config_t *config) {
	if (expression->type != EXPR_IDENT)
		return t3_true;

	return t3_config_get(config, expression->value.string) != NULL;
}

static t3_config_type_t operand_type(expr_node_t *expression, t3_config_t *config) {
	switch (expression->type) {
		case EXPR_STRING_CONST:
			return T3_CONFIG_STRING;
		case EXPR_INT_CONST:
			return T3_CONFIG_INT;
		case EXPR_NUMBER_CONST:
			return T3_CONFIG_NUMBER;
		case EXPR_BOOL_CONST:
			return T3_CONFIG_BOOL;
		case EXPR_IDENT:
			return t3_config_get_type(t3_config_get(config, expression->value.string));
		default:
			return T3_CONFIG_NONE;
	}
}

static const char *get_string_operand(expr_node_t *expression, t3_config_t *config) {
	if (expression->type == EXPR_IDENT)
		return t3_config_get_string(t3_config_get(config, expression->value.string));
	return expression->value.string;
}

static t3_bool get_bool_operand(expr_node_t *expression, t3_config_t *config) {
	if (expression->type == EXPR_IDENT)
		return t3_config_get_bool(t3_config_get(config, expression->value.string));
	return expression->value.boolean;
}

t3_bool _t3_evaluate_expr(expr_node_t *expression, t3_config_t *config) {
	switch (expression->type) {
		case EXPR_AND:
			return _t3_evaluate_expr(expression->value.operand[0], config) && _t3_evaluate_expr(expression->value.operand[1], config);
		case EXPR_OR:
			return _t3_evaluate_expr(expression->value.operand[0], config) || _t3_evaluate_expr(expression->value.operand[1], config);
		case EXPR_XOR:
			return _t3_evaluate_expr(expression->value.operand[0], config) ^ _t3_evaluate_expr(expression->value.operand[1], config);
		case EXPR_NOT:
			return !_t3_evaluate_expr(expression->value.operand[0], config);
		case EXPR_IDENT:
			return t3_config_get(config, expression->value.string) != NULL;

		/* FIXME: implement comparisons for < > <= >= */
/*		case EXPR_LT:
		case EXPR_LE:
		case EXPR_GT:
		case EXPR_GE: */


		case EXPR_NE:
		case EXPR_EQ: {
			t3_config_type_t type;

			if (!is_present(expression->value.operand[0], config) || !is_present(expression->value.operand[1], config))
				return t3_false;

			type = operand_type(expression->value.operand[0], config);
			if (operand_type(expression, config) == T3_CONFIG_STRING) {
				return (strcmp(get_string_operand(expression->value.operand[0], config),
					get_string_operand(expression->value.operand[1], config)) == 0) ^
					(expression->type == EXPR_NE);
			} else if (type == T3_CONFIG_BOOL) {
				return (get_bool_operand(expression->value.operand[0], config) ==
					get_bool_operand(expression->value.operand[1], config)) ^
					(expression->type == EXPR_NE);
			}
			/* FIXME: implement number and integer comparisons */
			return t3_false;
		}
		default:
			return t3_false;
	}
}

static t3_config_type_t operand_type_meta(expr_node_t *expression, t3_config_t *config) {
	static const struct {
		const char *name;
		t3_config_type_t type;
	} str2type[] = {
		{ "int", T3_CONFIG_INT },
		{ "bool", T3_CONFIG_BOOL },
		{ "number", T3_CONFIG_NUMBER },
		{ "string", T3_CONFIG_STRING },
		{ "section", T3_CONFIG_SECTION },
		{ "list", T3_CONFIG_LIST }
	};

	t3_config_t *allowed_keys, *key;
	const char *type;
	size_t i;

	switch (expression->type) {
		case EXPR_STRING_CONST:
			return T3_CONFIG_STRING;
		case EXPR_INT_CONST:
			return T3_CONFIG_INT;
		case EXPR_NUMBER_CONST:
			return T3_CONFIG_NUMBER;
		case EXPR_BOOL_CONST:
			return T3_CONFIG_BOOL;
		case EXPR_IDENT:
			if ((allowed_keys = t3_config_get(config, "allowed-keys")) != NULL) {
				if ((key = t3_config_get(allowed_keys, expression->value.string)) == NULL)
					return T3_CONFIG_NONE;
				if ((type = t3_config_get_string(t3_config_get(key, "type"))) == NULL)
					return T3_CONFIG_NONE;
			} else if ((type = t3_config_get_string(t3_config_get(config, "item-type"))) == NULL) {
				return T3_CONFIG_NONE;
			}
			for (i = 0; i < sizeof(str2type) / sizeof(str2type[0]); i++)
				if (strcmp(type, str2type[i].name) == 0)
					return str2type[i].type;
			return T3_CONFIG_NONE;
		default:
			return T3_CONFIG_NONE;
	}
}

t3_bool _t3_validate_expr(expr_node_t *expression, t3_config_t *config) {
	switch (expression->type) {
		case EXPR_AND:
		case EXPR_OR:
		case EXPR_XOR:
			return _t3_validate_expr(expression->value.operand[0], config) &&
				_t3_validate_expr(expression->value.operand[1], config);
		case EXPR_NOT:
			return _t3_validate_expr(expression->value.operand[0], config);
		case EXPR_IDENT: {
			t3_config_t *allowed_keys;
			if ((allowed_keys = t3_config_get(config, "allowed-keys")) != NULL)
				return t3_config_get(allowed_keys, expression->value.string) != NULL;
			return t3_true;
		}
		case EXPR_LT:
		case EXPR_LE:
		case EXPR_GT:
		case EXPR_GE:
		case EXPR_EQ:
		case EXPR_NE: {
			t3_config_type_t type = operand_type_meta(expression->value.operand[0], config);

			if (type != operand_type_meta(expression->value.operand[1], config))
				return t3_false;

			if (type == T3_CONFIG_STRING || type == T3_CONFIG_BOOL)
				return t3_true;

			if (expression->type == EXPR_EQ || expression->type == EXPR_NE)
				return t3_false;

			if (type == T3_CONFIG_INT || type == T3_CONFIG_NUMBER)
				return t3_true;
			return t3_false;
		}
		default:
			return t3_false;
	}
}
