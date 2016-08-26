#include "strbuf.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>


void strbuf_resize(struct strbuf *buf, size_t new_size)
{
	buf->str = realloc_safe(buf->str, new_size + 1);

	buf->size = new_size;
	buf->offset = buf->offset < buf->size ? buf->offset : buf->size;
	buf->str[buf->offset] = '\0';
}

void strbuf_init(struct strbuf *buf)
{
	buf->offset = 0;
	buf->str = NULL;
	strbuf_resize(buf, STRBUF_INIT_SIZE);
}

void strbuf_vprintf_at(struct strbuf *buf, size_t offset, char *format, va_list args)
{
	va_list args2;
	va_copy(args2, args);

	size_t len = vsnprintf(NULL, 0, format, args2);
	if (offset + len > buf->size) {
		size_t new_size = 2 * buf->size;
		if (new_size < len + offset) new_size = len + offset;

		strbuf_resize(buf, new_size);
	}

	vsnprintf(buf->str + offset, len + 1, format, args);
	buf->offset = len + offset;

	va_end(args2);
	va_end(args);
}

void strbuf_printf(struct strbuf *buf, size_t offset, char *format, ...)
{
	va_list args;
	va_start(args, format);

	strbuf_vprintf_at(buf, offset, format, args);

	va_end(args);
}

void strbuf_append(struct strbuf *buf, char *format, ...)
{
	va_list args;
	va_start(args, format);

	strbuf_vprintf_at(buf, buf->offset, format, args);

	va_end(args);
}

void strbuf_prepend(struct strbuf *buf, char *format, ...)
{
	va_list args;
	va_start(args, format);

	char *orig_str = strbuf_copy(buf);
	strbuf_vprintf_at(buf, 0, format, args);
	strbuf_append(buf, "%s", orig_str);

	free(orig_str);
}

char *strbuf_copy(struct strbuf *buf)
{
	char *str = malloc(buf->offset + 1);
	for (unsigned i = 0; i < buf->offset; i++) {
		str[i] = buf->str[i];
	}
	str[buf->offset] = '\0';

	return str;
}

void strbuf_free(struct strbuf *buf)
{
	free(buf->str);
}
