#include "include/errors.h"
#include "include/lsdn.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>


#define mk_diag_fmt(x, fmt) fmt,

static const char* error_fmt[] = {
	lsdn_foreach_problem(mk_diag_fmt)
};

#define CAST(x) ((x)subj->ptr)
void format_subject(FILE* out, const struct lsdn_problem_ref *subj)
{
	switch(subj->type){
	case LSDNS_IF:
		fputs(CAST(struct lsdn_if*)->ifname, out);
		break;
	case LSDNS_NET:
		if (CAST(struct lsdn_net*)->name.str) {
			fputs(CAST(struct lsdn_net*)->name.str, out);
			break;
		}
	case LSDNS_VIRT:
		if (CAST(struct lsdn_virt*)->name.str) {
			fputs(CAST(struct lsdn_virt*)->name.str, out);
			break;
		}
	case LSDNS_PHYS:
		if (CAST(struct lsdn_phys*)->name.str) {
			fputs(CAST(struct lsdn_phys*)->name.str, out);
			break;
		}
	default:
		fprintf(out, "0x%p", subj);
	}
}

void lsdn_problem_format(FILE* out, const struct lsdn_problem *problem)
{
	size_t i = 0;
	const char* fmt = error_fmt[problem->code];
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

/* The variadic arguments are pairs of subject type and pointers. The las type must be LSDNS_END */
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

void lsdn_problem_stderr_handler(const struct lsdn_problem *problem, void *user)
{
	lsdn_problem_format(stderr, problem);
}
