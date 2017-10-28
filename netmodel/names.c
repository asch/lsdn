#include "private/names.h"
#include <string.h>
#include <stdlib.h>

void lsdn_names_init(struct lsdn_names *tab)
{
	lsdn_list_init(&tab->head);
}

void lsdn_names_free(struct lsdn_names *tab)
{

}

lsdn_err_t lsdn_name_set(struct lsdn_name *name, struct lsdn_names *table, const char* str)
{
	if(name->str == str)
		return LSDNE_OK;
	if(str && name->str && strcmp(name->str, str) == 0)
		return LSDNE_OK;

	if(str && lsdn_names_search(table, str))
		return LSDNE_DUPLICATE;

	char *namedup = strdup(str);
	if(str && !namedup)
		return LSDNE_NOMEM;

	free(name->str);
	name->str = namedup;
	lsdn_list_init_add(&table->head, &name->entry);

	return LSDNE_OK;
}

void lsdn_name_init(struct lsdn_name *name)
{
	name->str = NULL;
}

struct lsdn_name *lsdn_names_search(struct lsdn_names *table, const char* key)
{
	lsdn_foreach(table->head, entry, struct lsdn_name, name)
	{
		if(strcmp(name->str, key) == 0)
			return name;
	}
	return NULL;
}
