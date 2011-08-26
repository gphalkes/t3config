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
#include <stdlib.h>

#include "config_internal.h"
#include "util.h"

static t3_bool is_present(const expr_node_t *expression, const t3_config_t *config) {
	if (expression->type != EXPR_IDENT)
		return t3_true;

	return t3_config_get(config, expression->value.string) != NULL;
}

static t3_config_type_t operand_type(const expr_node_t *expression, const t3_config_t *config) {
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

static const char *get_string_operand(const expr_node_t *expression, const t3_config_t *config) {
	if (expression->type == EXPR_IDENT)
		return t3_config_get_string(t3_config_get(config, expression->value.string));
	return expression->value.string;
}

static t3_bool get_bool_operand(const expr_node_t *expression, const t3_config_t *config) {
	if (expression->type == EXPR_IDENT)
		return t3_config_get_bool(t3_config_get(config, expression->value.string));
	return expression->value.boolean;
}

static t3_config_int_t get_int_operand(const expr_node_t *expression, const t3_config_t *config) {
	if (expression->type == EXPR_IDENT)
		return t3_config_get_int(t3_config_get(config, expression->value.string));
	return expression->value.integer;
}

static double get_number_operand(const expr_node_t *expression, const t3_config_t *config) {
	if (expression->type == EXPR_IDENT)
		return t3_config_get_number(t3_config_get(config, expression->value.string));
	return expression->value.number;
}

t3_bool _t3_config_evaluate_expr(const expr_node_t *expression, const t3_config_t *config) {
	t3_config_type_t type;

	switch (expression->type) {
		case EXPR_TOP:
			return _t3_config_evaluate_expr(expression->value.operand[0], config);
		case EXPR_AND:
			return _t3_config_evaluate_expr(expression->value.operand[0], config) && _t3_config_evaluate_expr(expression->value.operand[1], config);
		case EXPR_OR:
			return _t3_config_evaluate_expr(expression->value.operand[0], config) || _t3_config_evaluate_expr(expression->value.operand[1], config);
		case EXPR_XOR:
			return _t3_config_evaluate_expr(expression->value.operand[0], config) ^ _t3_config_evaluate_expr(expression->value.operand[1], config);
		case EXPR_NOT:
			return !_t3_config_evaluate_expr(expression->value.operand[0], config);
		case EXPR_IDENT:
			return t3_config_get(config, expression->value.string) != NULL;

#define COMPARE(expr_type, operator) \
		case expr_type: \
			if (!is_present(expression->value.operand[0], config) || !is_present(expression->value.operand[1], config)) \
				return t3_false; \
			type = operand_type(expression->value.operand[0], config); \
			if (type == T3_CONFIG_INT) { \
				return get_int_operand(expression->value.operand[0], config) operator \
					get_int_operand(expression->value.operand[1], config); \
			} else if (type == T3_CONFIG_NUMBER) { \
				return get_number_operand(expression->value.operand[0], config) operator \
					get_number_operand(expression->value.operand[1], config); \
			} \
			return t3_false;
		COMPARE(EXPR_LT, <)
		COMPARE(EXPR_LE, <=)
		COMPARE(EXPR_GT, >)
		COMPARE(EXPR_GE, >=)

		case EXPR_NE:
		case EXPR_EQ:
			if (!is_present(expression->value.operand[0], config) || !is_present(expression->value.operand[1], config))
				return t3_false;

			type = operand_type(expression->value.operand[0], config);
			if (type == T3_CONFIG_STRING) {
				return (strcmp(get_string_operand(expression->value.operand[0], config),
					get_string_operand(expression->value.operand[1], config)) == 0) ^
					(expression->type == EXPR_NE);
			} else if (type == T3_CONFIG_BOOL) {
				return (get_bool_operand(expression->value.operand[0], config) ==
					get_bool_operand(expression->value.operand[1], config)) ^
					(expression->type == EXPR_NE);
			} else if (type == T3_CONFIG_INT) {
				return (get_int_operand(expression->value.operand[0], config) ==
					get_int_operand(expression->value.operand[1], config)) ^
					(expression->type == EXPR_NE);
			} else if (type == T3_CONFIG_NUMBER) {
				return (get_number_operand(expression->value.operand[0], config) ==
					get_number_operand(expression->value.operand[1], config)) ^
					(expression->type == EXPR_NE);
			}
			/* FIXME: implement number and integer comparisons */
			return t3_false;
		default:
			return t3_false;
	}
}

static t3_config_type_t operand_type_meta(const expr_node_t *expression, const t3_config_t *config) {
	t3_config_t *allowed_keys, *key;
	const char *type;

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
			return _t3_config_str2type(type);
		default:
			return T3_CONFIG_NONE;
	}
}

t3_bool _t3_config_validate_expr(const expr_node_t *expression, const t3_config_t *config) {
	switch (expression->type) {
		case EXPR_AND:
		case EXPR_OR:
		case EXPR_XOR:
			return _t3_config_validate_expr(expression->value.operand[0], config) &&
				_t3_config_validate_expr(expression->value.operand[1], config);
		case EXPR_TOP:
		case EXPR_NOT:
			return _t3_config_validate_expr(expression->value.operand[0], config);
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
		case EXPR_BOOL_CONST:
		case EXPR_INT_CONST:
		case EXPR_NUMBER_CONST:
		case EXPR_STRING_CONST:
			return t3_true;
		default:
			return t3_false;
	}
}

void _t3_config_delete_expr(expr_node_t *expr) {
	switch (expr->type) {
		case EXPR_TOP:
		case EXPR_AND:
		case EXPR_OR:
		case EXPR_XOR:
		case EXPR_LT:
		case EXPR_LE:
		case EXPR_GT:
		case EXPR_GE:
		case EXPR_EQ:
		case EXPR_NE:
			_t3_config_delete_expr(expr->value.operand[0]);
			_t3_config_delete_expr(expr->value.operand[1]);
			break;
		case EXPR_NOT:
			_t3_config_delete_expr(expr->value.operand[0]);
			break;
		case EXPR_STRING_CONST:
		case EXPR_IDENT:
			free(expr->value.string);
			break;
		default:
			break;
	}
	free(expr);
}
