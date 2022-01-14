/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/**
 * $Id$
 *
 * @file xlat_expr.c
 * @brief Tokenizers and support functions for xlat expressions
 *
 * @copyright 2021 The FreeRADIUS server project
 * @copyright 2021 Network RADIUS SAS (legal@networkradius.com)
 */

RCSID("$Id$")

#include <freeradius-devel/server/base.h>
#include <freeradius-devel/unlang/xlat_priv.h>
#include <freeradius-devel/util/calc.h>

static xlat_arg_parser_t const binary_op_xlat_args[] = {
	{ .required = true, .type = FR_TYPE_VOID },
	{ .required = true, .type = FR_TYPE_VOID },
	XLAT_ARG_PARSER_TERMINATOR
};

static xlat_action_t xlat_binary_op(TALLOC_CTX *ctx, fr_dcursor_t *out,
				    UNUSED xlat_ctx_t const *xctx,
				    request_t *request, fr_value_box_list_t *in,
				    fr_token_t op)
{
	int rcode;
	fr_value_box_t	*dst, *a, *b;

	MEM(dst = fr_value_box_alloc_null(ctx));

	a = fr_dlist_head(in);
	b = fr_dlist_next(in, a);

	rcode = fr_value_calc_binary_op(dst, dst, FR_TYPE_NULL, a, op, b);
	if (rcode < 0) {
		talloc_free(dst);
		RPEDEBUG("Failed calculating result");
		return XLAT_ACTION_FAIL;
	}

	fr_dcursor_append(out, dst);
	return XLAT_ACTION_DONE;
}

#define XLAT_BINARY_FUNC(_name, _op)  \
static xlat_action_t xlat_func_ ## _name(TALLOC_CTX *ctx, fr_dcursor_t *out, \
				   UNUSED xlat_ctx_t const *xctx, \
				   request_t *request, fr_value_box_list_t *in)  \
{ \
	return xlat_binary_op(ctx, out, xctx, request, in, _op); \
}

XLAT_BINARY_FUNC(add, T_ADD)
XLAT_BINARY_FUNC(sub, T_SUB)
XLAT_BINARY_FUNC(mul, T_MUL)
XLAT_BINARY_FUNC(div, T_DIV)
XLAT_BINARY_FUNC(and, T_AND)
XLAT_BINARY_FUNC(or,  T_OR)
XLAT_BINARY_FUNC(prepend,  T_OP_PREPEND)

XLAT_BINARY_FUNC(cmp_eq,  T_OP_CMP_EQ)
XLAT_BINARY_FUNC(cmp_ne,  T_OP_NE)
XLAT_BINARY_FUNC(cmp_lt,  T_OP_LT)
XLAT_BINARY_FUNC(cmp_le,  T_OP_LE)
XLAT_BINARY_FUNC(cmp_gt,  T_OP_GT)
XLAT_BINARY_FUNC(cmp_ge,  T_OP_GE)

static xlat_arg_parser_t const short_circuit_xlat_args[] = {
	{ .required = true, .type = FR_TYPE_BOOL },
	{ .required = true, .type = FR_TYPE_BOOL },
	XLAT_ARG_PARSER_TERMINATOR
};

static xlat_action_t xlat_func_logical_and(TALLOC_CTX *ctx, fr_dcursor_t *out,
					   UNUSED xlat_ctx_t const *xctx,
					   UNUSED request_t *request, fr_value_box_list_t *in)
{
	fr_value_box_t	*dst, *a, *b;

	MEM(dst = fr_value_box_alloc(ctx, FR_TYPE_BOOL, NULL, false));

	a = fr_dlist_head(in);
	b = fr_dlist_next(in, a);

	/*
	 *	@todo - short-circuit stuff
	 */
	dst->vb_bool = a->vb_bool && b->vb_bool;

	fr_dcursor_append(out, dst);
	return XLAT_ACTION_DONE;
}


static xlat_action_t xlat_func_logical_or(TALLOC_CTX *ctx, fr_dcursor_t *out,
					  UNUSED xlat_ctx_t const *xctx,
					  UNUSED request_t *request, fr_value_box_list_t *in)
{
	fr_value_box_t	*dst, *a, *b;

	MEM(dst = fr_value_box_alloc(ctx, FR_TYPE_BOOL, NULL, false));

	a = fr_dlist_head(in);
	b = fr_dlist_next(in, a);

	/*
	 *	@todo - short-circuit stuff
	 */
	dst->vb_bool = a->vb_bool || b->vb_bool;

	fr_dcursor_append(out, dst);
	return XLAT_ACTION_DONE;
}


static xlat_arg_parser_t const unary_not_xlat_args[] = {
	{ .required = true, .type = FR_TYPE_BOOL },
	XLAT_ARG_PARSER_TERMINATOR
};

static xlat_action_t xlat_func_unary_not(TALLOC_CTX *ctx, fr_dcursor_t *out,
					 UNUSED xlat_ctx_t const *xctx,
					 UNUSED request_t *request, fr_value_box_list_t *in)
{
	fr_value_box_t *dst, *a;

	a = fr_dlist_head(in);
	MEM(dst = fr_value_box_alloc(ctx, FR_TYPE_BOOL, NULL, a->tainted));
	dst->vb_bool = !a->vb_bool;

	fr_dcursor_append(out, dst);
	return XLAT_ACTION_DONE;
}

static xlat_arg_parser_t const unary_sub_xlat_args[] = {
	{ .required = true, .concat = true },
	XLAT_ARG_PARSER_TERMINATOR
};

static xlat_action_t xlat_func_unary_sub(TALLOC_CTX *ctx, fr_dcursor_t *out,
					 UNUSED xlat_ctx_t const *xctx,
					 UNUSED request_t *request, fr_value_box_list_t *in)
{
	int rcode;
	fr_value_box_t	*dst, a, *b;

	MEM(dst = fr_value_box_alloc_null(ctx));

	fr_value_box_init(&a, FR_TYPE_INT64, NULL, false);
	b = fr_dlist_head(in);

	rcode = fr_value_calc_binary_op(dst, dst, FR_TYPE_NULL, &a, T_SUB, b);
	if (rcode < 0) {
		talloc_free(dst);
		RPEDEBUG("Failed calculating result");
		return XLAT_ACTION_FAIL;
	}

	fr_dcursor_append(out, dst);
	return XLAT_ACTION_DONE;
}

#undef XLAT_REGISTER_BINARY_OP
#define XLAT_REGISTER_BINARY_OP(_name) \
do { \
	if (!(xlat = xlat_register(NULL, STRINGIFY(_name), xlat_func_ ## _name, NULL))) return -1; \
	xlat_func_args(xlat, binary_op_xlat_args); \
} while (0)

int xlat_register_expressions(void)
{
	xlat_t *xlat;

	XLAT_REGISTER_BINARY_OP(add);
	XLAT_REGISTER_BINARY_OP(sub);
	XLAT_REGISTER_BINARY_OP(mul);
	XLAT_REGISTER_BINARY_OP(div);
	XLAT_REGISTER_BINARY_OP(and);
	XLAT_REGISTER_BINARY_OP(or);
	XLAT_REGISTER_BINARY_OP(prepend);

	XLAT_REGISTER_BINARY_OP(cmp_eq);
	XLAT_REGISTER_BINARY_OP(cmp_ne);
	XLAT_REGISTER_BINARY_OP(cmp_lt);
	XLAT_REGISTER_BINARY_OP(cmp_le);
	XLAT_REGISTER_BINARY_OP(cmp_gt);
	XLAT_REGISTER_BINARY_OP(cmp_ge);

	/*
	 *	&&, ||
	 */
	if (!(xlat = xlat_register(NULL, "land", xlat_func_logical_and, NULL))) return -1;
	xlat_func_args(xlat, short_circuit_xlat_args);

	if (!(xlat = xlat_register(NULL, "lor", xlat_func_logical_or, NULL))) return -1;
	xlat_func_args(xlat, short_circuit_xlat_args);

	/*
	 *	-EXPR
	 *	!EXPR
	 */
	if (!(xlat = xlat_register(NULL, "minus", xlat_func_unary_sub, NULL))) return -1;
	xlat_func_args(xlat, unary_sub_xlat_args);
	
	if (!(xlat = xlat_register(NULL, "not", xlat_func_unary_not, NULL))) return -1;
	xlat_func_args(xlat, unary_not_xlat_args);

	return 0;
}

/*
 *	Must use the same names as above.
 */
static const fr_sbuff_term_elem_t binary_ops[T_TOKEN_LAST] = {
	[ T_ADD ]		= L("add"),
	[ T_SUB ]		= L("sub"),
	[ T_MUL ]		= L("mul"),
	[ T_DIV ]		= L("div"),
	[ T_AND ]		= L("and"),
	[ T_OR ]		= L("or"),

	[ T_LAND ]		= L("land"),
	[ T_LOR ]		= L("lor"),

	[ T_OP_CMP_EQ ]		= L("cmp_eq"),
	[ T_OP_NE ]		= L("cmp_ne"),
	[ T_OP_LT ]		= L("cmp_lt"),
	[ T_OP_LE ]		= L("cmp_le"),
	[ T_OP_GT ]		= L("cmp_gt"),
	[ T_OP_GE ]		= L("cmp_ge"),
};


/*
 *	Allow for BEDMAS ordering.  Gross ordering is first number,
 *	fine ordering is second number.  Unused operators 
 */
#define P(_x, _y) (((_x) << 4) | (_y))

static const int precedence[T_TOKEN_LAST] = {
	[T_INVALID]	= 0,

	// += -= *= /= %= <<= >>= &= ^= |=

	[T_LOR]		= P(2,0),
	[T_LAND]	= P(2,1),

	[T_OR]		= P(3,0),
	// ^ (3,1)
	[T_AND]		= P(3,2),

	[T_OP_CMP_EQ]	= P(4,0),
	[T_OP_NE]	= P(4,0),

	[T_OP_LT]	= P(5,0),
	[T_OP_LE]	= P(5,0),
	[T_OP_GT]	= P(5,0),
	[T_OP_GE]	= P(5,0),

	[T_RSHIFT]	= P(6,0),
	[T_LSHIFT]	= P(6,0),

	[T_ADD]		= P(7,0),
	[T_SUB]		= P(7,1),

	[T_MUL]		= P(8,0),
	[T_DIV]		= P(8,1),

	[T_LBRACE]	= P(9,0),
};

#define fr_sbuff_skip_whitespace(_x) \
	do { \
		while (isspace((int) *fr_sbuff_current(_x))) fr_sbuff_advance(_x, 1); \
	} while (0)


static ssize_t tokenize_expression(TALLOC_CTX *ctx, xlat_exp_t **head, xlat_flags_t *flags, fr_sbuff_t *input,
				   fr_sbuff_parse_rules_t const *p_rules, tmpl_rules_t const *t_rules,
				   fr_token_t prev, fr_type_t type, fr_sbuff_parse_rules_t const *bracket_rules);


/*
 *	Look for prefix operators
 *
 *	+ = ignore
 *	- = unary_sub(next)
 *	! = unary_not(next)
 *	~ = unary_xor(0, next)
 *	(expr) = recurse, and parse expr
 *
 *	as a special case, <type> is a cast.  Which lets us know how
 *	to parse the next thing we get.  Otherwise, parse the thing as
 *	int64_t.
 *
 *	@todo - have updated parse rules which stop at any of the
 *	operators we accept.  This allows us to parse attribute
 *	references.
 */
static ssize_t tokenize_field(TALLOC_CTX *ctx, xlat_exp_t **head, xlat_flags_t *flags, fr_sbuff_t *input,
			      fr_sbuff_parse_rules_t const *p_rules, tmpl_rules_t const *t_rules,
			      fr_type_t type, fr_sbuff_parse_rules_t const *bracket_rules)
{
	ssize_t		slen;
	xlat_exp_t	*node = NULL;
	xlat_exp_t	*unary = NULL;
	xlat_t		*func = NULL;
	fr_sbuff_t	in = FR_SBUFF(input);

	/*
	 *	Handle !-~ by adding a unary function to the xlat
	 *	node, with the first argument being the _next_ thing
	 *	we allocate.
	 */
	if (fr_sbuff_next_if_char(&in, '!')) { /* unary not */
		func = xlat_func_find("not", 3);
		fr_assert(func != NULL);
	}
	else if (fr_sbuff_next_if_char(&in, '-')) { /* unary minus */
		func = xlat_func_find("minus", 5);
		fr_assert(func != NULL);
	}
	else if (fr_sbuff_next_if_char(&in, '+')) { /* ignore unary + */
		/* nothing */
	}

	/*
	 *	Maybe we have a unary not / etc.  If so, make sure
	 *	that we return that, and not the child node
	 */
	if (func) {
		MEM(unary = xlat_exp_alloc(ctx, XLAT_FUNC, func->name, strlen(func->name)));
		unary->call.func = func;
		unary->flags = func->flags;
		ctx = unary;
	}
	
	/*
	 *	Allow for casts, if the caller hasn't already
	 *	specified that.
	 */
	if (type == FR_TYPE_VOID) {
		char end = '\0';
		fr_sbuff_marker_t marker;

		fr_sbuff_marker(&marker, &in);
		if (fr_sbuff_is_char(&in, '(')) { /* <cast> is yucky.  (cast) is friendly */
			fr_sbuff_advance(&in, 1);
			end = ')';

		} else if (fr_sbuff_is_char(&in, '<')) {
			fr_sbuff_advance(&in, 1);
			end = '>';

		} else {
			goto check_more;
		}

		fr_sbuff_skip_whitespace(&in);

		fr_sbuff_out_by_longest_prefix(&slen, &type, fr_value_box_type_table, &in, FR_TYPE_VOID);
		if (type == FR_TYPE_VOID) {
			fr_sbuff_set(&in, &marker);
			goto check_more;
		}

		if (!fr_type_is_leaf(type)) {
			fr_strerror_printf("Cannot cast to structural data type");
			fr_sbuff_set(&in, &marker);
			FR_SBUFF_ERROR_RETURN(&in);
		}

		fr_sbuff_skip_whitespace(&in);
		if (!fr_sbuff_is_char(&in, end)) {
			fr_strerror_printf("Unexpected text after cast data type");
			FR_SBUFF_ERROR_RETURN(&in);
		}

		fr_sbuff_advance(&in, 1);
	}

	/*
	 *	If we have '(', then recurse for other expressions
	 */
check_more:
	if (fr_sbuff_next_if_char(&in, '(')) {
		/*
		 *	Tokenize the sub-expression, ensuring that we stop at ')'.
		 */
		slen = tokenize_expression(ctx, &node, flags, &in, bracket_rules, t_rules, T_INVALID, type, bracket_rules);
		if (slen <= 0) {
			FR_SBUFF_ERROR_RETURN_ADJ(&in, slen);
		}

		if (!fr_sbuff_next_if_char(&in, ')')) {
			fr_strerror_printf("Failed to find trailing ')'");
			FR_SBUFF_ERROR_RETURN_ADJ(&in, -slen);
		}

		goto done;
	}

	/*
	 *	Parse an attribute string.
	 */
	if (fr_sbuff_is_char(&in, '&')) {
		tmpl_t *vpt = NULL;

		MEM(node = xlat_exp_alloc_null(ctx));
		xlat_exp_set_type(node, XLAT_ATTRIBUTE);

		slen = tmpl_afrom_attr_substr(node, NULL, &vpt, &in, p_rules, t_rules);
		if (slen <= 0) {
			talloc_free(node);
			FR_SBUFF_ERROR_RETURN_ADJ(&in, slen);
		}

		xlat_exp_set_name_buffer_shallow(node, vpt->name);
		node->attr = vpt;

		goto done;
	}

	/*
	 *	Parse %{...}
	 */
	if (fr_sbuff_adv_past_str_literal(&in, "%{")) {
		if (xlat_tokenize_expansion(ctx, &node, flags, &in, t_rules) < 0) {
			return -1;
		}

		goto done;
	}

#if 0
	/*
	 *	@todo - support %(xlat:...)
	 *
	 *	HOWEVER this use-case overlaps a bit with remainder,
	 *	followed by something. i.e. "foo % (bar)".  So we have
	 *	to be a bit careful about that.
	 */
	if (fr_sbuff_is_str(&in, "%(", 2)) {
		fr_sbuff_marker_t marker;

		fr_sbuff_marker(&marker, &in);

		fr_sbuff_advance(&in, 2);

		//

	}
#endif

	/*
	 *	Else it's nothing we recognize.  Do some quick checks
	 *	to see what it might be.
	 */
	if (type == FR_TYPE_VOID) {
		/*
		 *	@todo - we want to parse strings as "foo", but
		 *	this doesn't quite seem to work yet.  Deeper
		 *	investigation is warranted.
		 */
		if (fr_sbuff_is_char(&in, '"') || fr_sbuff_is_char(&in, '\'') || fr_sbuff_is_char(&in, '`')) {
			type = FR_TYPE_STRING;
		} else {			
			type = FR_TYPE_INT64;
		}
	}

	fr_assert(fr_type_is_leaf(type));

	/*
	 *	Parse the thing as a value-box of the given type.
	 */
	{
		fr_sbuff_marker_t marker;
		char *p;

		fr_sbuff_marker(&marker, &in);

		MEM(node = xlat_exp_alloc_null(ctx));
		xlat_exp_set_type(node, XLAT_BOX);

		slen = fr_value_box_from_substr(node, &node->data, type, NULL, &in, p_rules, false);
		if (slen <= 0) {
			fr_strerror_printf("Failed parsing value");
			FR_SBUFF_ERROR_RETURN_ADJ(&in, slen);
		}

		MEM(p = talloc_array(node, char, slen + 1));
		p[slen] = '\0';
		memcpy(p, fr_sbuff_current(&marker), slen);
		xlat_exp_set_name_buffer_shallow(node, p);
		goto done;
	}

done:
	if (unary) {
		unary->child = node;
		xlat_flags_merge(&unary->flags, &node->flags);
		node = unary;
	}

	fr_assert(node != NULL);
	*head = node;
	return fr_sbuff_set(input, &in);

}

/*
 *	Lexigraphically sorted!
 */
static fr_table_num_sorted_t const expr_assignment_op_table[] = {
	{ L("!="),	T_OP_NE			},

	{ L("&"),	T_AND			},
	{ L("*"),	T_MUL			},
	{ L("+"),	T_ADD			},
	{ L("-"),	T_SUB			},
	{ L("/"),	T_DIV			},

	{ L("|"),	T_OR			},

	{ L("<"),	T_OP_LT			},
	{ L("<<"),	T_LSHIFT    		},
	{ L("<="),	T_OP_LE			},

	{ L("="),	T_OP_EQ			},
	{ L("=="),	T_OP_CMP_EQ		},

	{ L(">"),	T_OP_GT			},
	{ L(">="),	T_OP_GE			},
	{ L(">>"),	T_RSHIFT    		},
};
static size_t const expr_assignment_op_table_len = NUM_ELEMENTS(expr_assignment_op_table);

/** Tokenize a mathematical operation.
 *
 *  @todo - take the tokenization process from rlm_expr, and cond_tokenize.c
 *
 *  @todo - convert rlm_expr to the new value.
 *
 *	(EXPR)
 *	!EXPR
 *	A OP B
 */
static ssize_t tokenize_expression(TALLOC_CTX *ctx, xlat_exp_t **head, xlat_flags_t *flags, fr_sbuff_t *input,
				   fr_sbuff_parse_rules_t const *p_rules, tmpl_rules_t const *t_rules,
				   fr_token_t prev, fr_type_t type, fr_sbuff_parse_rules_t const *bracket_rules)
{
	xlat_exp_t	*lhs, *rhs, *node;
	xlat_t		*func = NULL;
	fr_token_t	op;
	ssize_t		slen;
	fr_sbuff_marker_t  marker;
	fr_sbuff_t	in = FR_SBUFF(input);

	fr_sbuff_skip_whitespace(&in);

	/*
	 *	Get the LHS of the operation.
	 */
	slen = tokenize_field(ctx, &lhs, flags, &in, p_rules, t_rules, type, bracket_rules);
	if (slen <= 0) return slen;

redo:
	fr_assert(lhs != NULL);

	fr_sbuff_skip_whitespace(&in);

	/*
	 *	No more input, we're done.
	 */
	if (fr_sbuff_extend(&in) == 0) {
		*head = lhs;
		return fr_sbuff_set(input, &in);
	}

	/*
	 *	')' is a terminal, even if we didn't expect it.
	 *	Because if we didn't expect it, then it's an error.
	 *
	 *	If we did expect it, then we return whatever we found,
	 *	and let the caller eat the ')'.
	 */
	if (fr_sbuff_is_char(&in, ')')) {
		if (!bracket_rules) {
			fr_strerror_printf("Unexpected ')'");
			FR_SBUFF_ERROR_RETURN(&in);
		}

		*head = lhs;
		return fr_sbuff_set(input, &in);
	}
	fr_sbuff_skip_whitespace(&in);
			
	/*
	 *	Remember where we were after parsing the LHS.
	 */
	fr_sbuff_marker(&marker, &in);

	/*
	 *	Get the operator.
	 */
	fr_sbuff_out_by_longest_prefix(&slen, &op, expr_assignment_op_table, &in, T_INVALID);
	if (op == T_INVALID) {
		talloc_free(lhs);
		fr_strerror_printf("Expected operator at '%.4s'", fr_sbuff_current(&in));
		FR_SBUFF_ERROR_RETURN(&in);
	}	

	if (!binary_ops[op].str) {
		fr_strerror_printf("Invalid operator '%s'", fr_tokens[op]);
		FR_SBUFF_ERROR_RETURN_ADJ(&in, -slen);
	}

	fr_assert(precedence[op] != 0);

	/*
	 *	a * b + c ... = (a * b) + c ...
	 *
	 *	Feed the current expression to the caller, who will
	 *	take care of continuing.
	 */
	if (precedence[op] <= precedence[prev]) {
		*head = lhs;
		return fr_sbuff_set(input, &marker);
	}

	/*
	 *	@todo - if the LHS is typed, perhaps try to parse the
	 *	RHS as the given type?
	 */

	/*
	 *	We now parse the RHS, allowing a (perhaps different) cast on the RHS.
	 */
	slen = tokenize_expression(ctx, &rhs, flags, &in, p_rules, t_rules, op, FR_TYPE_VOID, bracket_rules);
	if (slen <= 0) {
		talloc_free(lhs);
		FR_SBUFF_ERROR_RETURN_ADJ(&in, slen);
	}

	func = xlat_func_find(binary_ops[op].str, binary_ops[op].len);
	fr_assert(func != NULL);

	/*
	 *	Create the function node, with the LHS / RHS arguments.
	 */
	MEM(node = xlat_exp_alloc(ctx, XLAT_FUNC, fr_tokens[op], strlen(fr_tokens[op])));
	node->call.func = func;
	node->flags = func->flags;
	node->child = lhs;
	lhs->next = rhs;

	xlat_flags_merge(&node->flags, &lhs->flags);
	xlat_flags_merge(&node->flags, &rhs->flags);

	lhs = node;
	goto redo;
}

static const fr_sbuff_term_t bracket_terms = FR_SBUFF_TERMS(
	L(")"),
);

static const fr_sbuff_term_t operator_terms = FR_SBUFF_TERMS(
	L(" "),
	L("\t"),
	L("\r"),
	L("\n"),
	L("+"),
	L("-"),
	L("/"),
	L("*"),
	L(":"),
	L("="),
	L("%"),
	L("!"),
	L("~"),
	L("&"),
	L("|"),
	L("^"),
	L(">"),
	L("<"),
);

ssize_t xlat_tokenize_expression(TALLOC_CTX *ctx, xlat_exp_t **head, xlat_flags_t *flags, fr_sbuff_t *in,
				 fr_sbuff_parse_rules_t const *p_rules, tmpl_rules_t const *t_rules)
{
	ssize_t slen;
	fr_sbuff_parse_rules_t *bracket_rules = NULL;
	fr_sbuff_parse_rules_t *terminal_rules = NULL;
	xlat_flags_t my_flags = { 0 };

	/*
	 *	If the terminal rules don't include a ')',
	 *	then add it in.
	 */
	MEM(bracket_rules = talloc_zero(ctx, fr_sbuff_parse_rules_t));
	MEM(terminal_rules = talloc_zero(ctx, fr_sbuff_parse_rules_t));
	if (p_rules) {
		*bracket_rules = *p_rules;
		*terminal_rules = *p_rules;

		if (p_rules->terminals) {
			MEM(terminal_rules->terminals = fr_sbuff_terminals_amerge(bracket_rules,
										  p_rules->terminals,
										  &operator_terms));
		} else {
			terminal_rules->terminals = &operator_terms;
		}
	} else {
		terminal_rules->terminals = &operator_terms;
	}
	MEM(bracket_rules->terminals = fr_sbuff_terminals_amerge(bracket_rules,
								 terminal_rules->terminals,
								 &bracket_terms));

	if (!flags) flags = &my_flags;
	
	slen = tokenize_expression(ctx, head, flags, in, terminal_rules, t_rules, T_INVALID, FR_TYPE_VOID,
				   bracket_rules);
	talloc_free(bracket_rules);
	talloc_free(terminal_rules);
	return slen;
}
