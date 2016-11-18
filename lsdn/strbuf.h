#ifndef STRBUF_H
#define	STRBUF_H

#include <stdarg.h>
#include <stddef.h>


#define	STRBUF_INIT_SIZE 1024


struct strbuf {
	char *str;
	size_t size;	/* excl. the '\0' */
	size_t offset;	/* str[offset] is the last written '\0' at all times */
};


/**
 * @brief Initialize empty string buffer.
 */
void strbuf_init(struct strbuf *buf);

/**
 * @brief Resize the buffer.
 */
void strbuf_resize(struct strbuf *buf, size_t new_size);

/**
 * @brief Print at specific offset of the buffer.
 */
void strbuf_printf(struct strbuf *buf, size_t offset, const char *format, ...);

/**
 * @brief Print at specific offset of the buffer (va_list).
 */
void strbuf_vprintf_at(struct strbuf *buf, size_t offset, const char *format, va_list args);

/**
 * @brief Append string to the end of the buffer.
 */
void strbuf_append(struct strbuf *buf, const char *format, ...);

/**
 * @brief Copy the string and return pointer to the copy.
 */
char *strbuf_copy(struct strbuf *buf);

/**
 * @brief Prepend a string before current value stored in the buffer.
 */
void strbuf_prepend(struct strbuf *buf, const char *format, ...);

/**
 * @brief Free all memory used by the buffer.
 */
void strbuf_free(struct strbuf *buf);

#endif
