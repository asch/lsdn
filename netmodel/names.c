/** \file
 * Name-related function definitions. */
#include "private/names.h"
#include "include/util.h"
#include <string.h>
#include <stdlib.h>

/** Set up a list of names.
 * Intended to be called on a member variable of another struct.
 * @param tab The list variable to initialize. */
void lsdn_names_init(struct lsdn_names *tab)
{
	lsdn_list_init(&tab->head);
}

/** Free a list of names.
 * Doesn't actually do anything. */
void lsdn_names_free(struct lsdn_names *tab)
{
	LSDN_UNUSED(tab);
}

/** Update an existing name.
 * If the new name is the same as the old name, returns `LSDNE_OK` immediately. 
 * Otherwise checks for uniqueness within `table` and if that succeeds,
 * assigns the new name to the `name` variable.
 * @param name Name struct.
 * @param[in] table List of names.
 * @param[in] str New name.
 * @return `LSDNE_OK` if the update was successful. 
 * @return `LSDNE_DUPLICATE` if the name already exists in `table`. 
 * @return `LSDNE_NOMEM` if memory allocation failed. */
lsdn_err_t lsdn_name_set(struct lsdn_name *name, struct lsdn_names *table, const char* str)
{
	if (!str) {
		lsdn_name_free(name);
		return LSDNE_OK;
	}
	if (name->str == str)
		return LSDNE_OK;
	if (name->str && strcmp(name->str, str) == 0)
		return LSDNE_OK;

	if (lsdn_names_search(table, str))
		return LSDNE_DUPLICATE;

	char *namedup = strdup(str);
	if (!namedup)
		return LSDNE_NOMEM;

	lsdn_name_free(name);

	name->str = namedup;
	lsdn_list_add(&table->head, &name->entry);

	return LSDNE_OK;
}

/** Set up an individual name.
 * @param name Pointer to a name struct. */
void lsdn_name_init(struct lsdn_name *name)
{
	lsdn_list_init(&name->entry);
	name->str = NULL;
}

/** Free a name.
 * Frees the associated string and removes the name from the list.
 * @param name Pointer to a name struct. */
void lsdn_name_free(struct lsdn_name *name)
{
	free(name->str);
	name->str = NULL;
	if (!lsdn_is_list_empty(&name->entry))
		lsdn_list_remove(&name->entry);
}

/** Find a name struct corresponding to a given string.
 * Walks the list of names and checks whether the string exists in it.
 * @param[in] table List of names.
 * @param[in] key Search string.
 * @return Name struct corresponding to `key`, if found.
 * @return `NULL` if the name is not found in the list. */
struct lsdn_name *lsdn_names_search(struct lsdn_names *table, const char* key)
{
	lsdn_foreach(table->head, entry, struct lsdn_name, name)
	{
		if(strcmp(name->str, key) == 0)
			return name;
	}
	return NULL;
}
