/** \file
 * Utility macros */
#pragma once

/** Mark argument as explicitly unused.
 * Useful in callbacks methods that take a void argument, to indicate that the
 * argument is not being used and prevent compiler warnings. */
#define LSDN_UNUSED(x) ((void)(x))

/** Throw a "not implemented" error.
 * Used in body of a function that is part of the API but has not been
 * implemented yet.  There should be no usages of this macro when the project is
 * done. */
#define	LSDN_NOT_IMPLEMENTED	do {\
	DEBUG_PRINTF("%s: not implemented", __func__); \
	exit(EXIT_FAILURE); \
} while (0);

/** Helper for `LSDN_ENUM`.
 * @see LSDN_ENUM */
#define _LSDN_DUMP_ENUM(val, str) val,
/** Helper for `LSDN_ENUM_NAMES`.
 * @see LSDN_ENUM
 * @see LSDN_ENUM_NAMES */
#define _LSDN_DUMP_NAMES(val, str) str,

/** Generate enum definition from `enumgen` list.
 * In several places we need to keep enum values associated with their string
 * names or format specifiers. We do that by defining a macro called
 * `lsdn_enumgen_enumname`. It should look like this:
 * ```
 * #define lsdn_enumgen_foobar(x) \
 * 	x(LSDNFOO_BAR, "foo with bar") \
 * 	x(LSDNFOO_BAZ, "bazinga") \
 * 	x(LSDNFOO_QUX, "zebra")
 * ```
 * Then, using `LSDN_ENUM(foobar, LSDNFOO)`, an enum called `lsdn_foobar`
 * is generated, with the specified values. In addition, a `LSDNFOO_COUNT` value
 * is added at end of the list. This is a guard value which corresponds to
 * the number of values.
 *
 * Using `LSDN_ENUM_NAMES(foobar)` creates a static NULL terminated array
 * `foobar_names[]` of associated strings. Indexing this array with the enum value
 * yields the corresponding string, so `foobar_names[LSDNFOO_BAZ]` is `"bazinga"`. */
#define LSDN_ENUM(name, prefix) \
enum lsdn_ ## name { \
	lsdn_enumgen_ ## name (_LSDN_DUMP_ENUM) \
	/** Guard value. See #LSDN_ENUM for details. */ \
	prefix ## _COUNT \
};

/** Generate descriptions from `enumgen` list.
 * Creates a `static const char * <name>_names[]` array with strings taken
 * from the corresponding `lsdn_enumgen` macro.
 *
 * See \ref LSDN_ENUM documentation for details. */
#define LSDN_ENUM_NAMES(name) \
static const char * name ## _names[] = { \
	lsdn_enumgen_ ## name (_LSDN_DUMP_NAMES) \
	NULL \
};
