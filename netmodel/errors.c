/** \file
 * Error formatting functions.
 */
#include "include/errors.h"
#include "include/lsdn.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include "private/lsdn.h"


/** List of errors and their format strings. */
LSDN_ENUM_NAMES(problem_code);

/** Extract pointer to the appropriate type from `lsdn_problem_ref`.
 * Used as a shorthand for typecasting `ref->ptr`, which is a void pointer in order
 * to simplify assigning to it. */
#define CAST(x) ((x)subj->ptr)

/** Convert subject name to string.
 * @param[in] subj reference to a problem item. 
 * @param[out] out output stream. */
static void format_subject(FILE* out, const struct lsdn_problem_ref *subj)
{
	bool printed = false;
	switch(subj->type){
	case LSDNS_IF:
		fputs(CAST(struct lsdn_if*)->ifname, out);
		break;
	case LSDNS_NET:
		if (CAST(struct lsdn_net*)->name.str) {
			fputs(CAST(struct lsdn_net*)->name.str, out);
			printed = true;
		}
		break;
	case LSDNS_VIRT:
		if (CAST(struct lsdn_virt*)->name.str) {
			fputs(CAST(struct lsdn_virt*)->name.str, out);
			printed = true;
		}
		break;
	case LSDNS_PHYS:
		if (CAST(struct lsdn_phys*)->name.str) {
			fputs(CAST(struct lsdn_phys*)->name.str, out);
			printed = true;
		}
		break;
	case LSDNS_ATTR:
		fputs(CAST(const char*), out);
		printed = true;
		break;
	default:
		break;
	}
	if (!printed)
		fprintf(out, "0x%p", subj);
}

/** Print problem description.
 * Constructs a string describing the problem and writes it out to `out`.
 * @param[out] out output stream.
 * @param[in] problem problem description. */
void lsdn_problem_format(FILE* out, const struct lsdn_problem *problem)
{
	size_t i = 0;
	const char* fmt = problem_code_names[problem->code];
	while(*fmt){
		if (*fmt == '%'){
			fmt++;
			assert(*fmt == 'o');
			assert(i < problem->refs_count);
			format_subject(out, &problem->refs[i++]);
		}else{
			fputc(*fmt, out);
		}
		fmt++;
	}
	fputc('\n', stderr);
}

/** Report problems to caller.
 * Walks the list of variadic arguments and invokes the problem callback for each.
 * Also prepares data for the `lsdn_problem_format` function.
 * @param ctx Current context.
 * @param code Problem code.
 * @param ...  Variadic list of pairs of `lsdn_problem_ref_type`
 *   and a pointer to the specified object.
 *   The last element must be a single `LSDNS_END` value. */
void lsdn_problem_report(struct lsdn_context *ctx, enum lsdn_problem_code code, ...)
{
	va_list args;
	va_start(args, code);
	struct lsdn_problem *problem = &ctx->problem;
	problem->refs_count = 0;
	problem->refs = ctx->problem_refs;
	problem->code = code;
	while(1){
		enum lsdn_problem_ref_type subtype = va_arg(args, enum lsdn_problem_ref_type);
		if(subtype == LSDNS_END)
			break;
		assert(problem->refs_count < LSDN_MAX_PROBLEM_REFS);
		problem->refs[problem->refs_count++] =
			(struct lsdn_problem_ref) {subtype, va_arg(args, void*)};
	}
	va_end(args);

	if(ctx->problem_cb)
		ctx->problem_cb(problem, ctx->problem_cb_user);
	ctx->problem_count++;
}

/** Problem handler that dumps problem descriptions to `stderr`.
 * Can be used as a callback in `lsdn_commit`, `lsdn_validate`, and any other place that takes
 * `lsdn_problem_cb`.
 *
 * When a problem is encountered, this handler will dump a human-readable description to `stderr`.
 * @param problem problem description.
 * @param user user data for the callback. Unused. */
void lsdn_problem_stderr_handler(const struct lsdn_problem *problem, void *user)
{
	LSDN_UNUSED(user);
	lsdn_problem_format(stderr, problem);
}
