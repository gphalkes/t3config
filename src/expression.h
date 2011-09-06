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
#ifndef T3_CONFIG_EXPRESSION_H
#define T3_CONFIG_EXPRESSION_H

typedef struct expr_node_t expr_node_t;

#include "config_api.h"
#include "config_internal.h"

typedef enum {
	EXPR_TOP,
	EXPR_AND,
	EXPR_OR,
	EXPR_XOR,
	EXPR_NOT,

	EXPR_IDENT,

	EXPR_EQ,
	EXPR_NE,

	EXPR_LT,
	EXPR_LE,
	EXPR_GT,
	EXPR_GE,

	EXPR_STRING_CONST,
	EXPR_INT_CONST,
	EXPR_NUMBER_CONST,
	EXPR_BOOL_CONST,

	EXPR_PATH,
	EXPR_PATH_ROOT,
	EXPR_DEREF,
	EXPR_THIS,

	EXPR_LENGTH,
	EXPR_LIST,
	/* Used only to store the result of resolve, such that each lookup_node
	   only has to be called onde. */
	EXPR_CONFIG
} expr_type_t;


struct expr_node_t {
	expr_type_t type;
	union {
		struct expr_node_t *operand[2];
		char *string;
		t3_config_int_t integer;
		double number;
		t3_bool boolean;
		const t3_config_t *config;
	} value;
};


T3_CONFIG_LOCAL t3_bool _t3_config_evaluate_expr(const expr_node_t *expression, const t3_config_t *config, const t3_config_t *root);
T3_CONFIG_LOCAL t3_bool _t3_config_validate_expr(const expr_node_t *expression, const t3_config_t *config, const t3_config_t *root);
T3_CONFIG_LOCAL void _t3_config_delete_expr(expr_node_t *expr);
#endif
