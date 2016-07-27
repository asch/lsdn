#ifndef STRBUF_H
#define	STRBUF_H

#include <stdarg.h>
#include <stddef.h>



#define	STRBUF_INIT_SIZE 1024
#define	STRBUF_GROWTH_RATE 2


typedef struct {
	char *str;
	size_t size;	/* excl. the '\0' */
	size_t offset;	/* offset always points to the last written '\0' */
} strbuf;


/*
 * Initialize empty string buffer.
 */
void strbuf_init(strbuf *buf);


/*
 * Resize the buffer.
 */
void strbuf_resize(strbuf *buf, size_t new_size);


/*
 * Print at specific offset of the buffer.
 */
void strbuf_printf(strbuf *buf, size_t offset, char *format, ...);


/*
 * Print at specific offset of the buffer (va_list).
 */
void strbuf_vprintf_at(strbuf *buf, size_t offset, char *format, va_list args);


/*
 * Append string to the end of the buffer.
 */
void strbuf_append(strbuf *buf, char *format, ...);


/*
 * Copy the string and return pointer to the copy.
 */
char *strbuf_copy(strbuf *buf);


/*
 * Free all memory used by the buffer.
 */
void strbuf_free(strbuf *buf);


void strbuf_prepend(strbuf *buf, char *format, ...);


#endif
