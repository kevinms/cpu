#include "list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct ListEntry *
listAppend(List *list, void *data)
{
	if (list == NULL) {
		fprintf(stderr, "Bad list pointer?! %p\n", list);
		return NULL;
	}

	ListEntry *entry = NULL;
	if ((entry = malloc(sizeof(*entry))) == NULL) {
		fprintf(stderr, "Can't allocate new list entry: %s\n", strerror(errno));
		return NULL;
	}
	entry->data = data;
	entry->prev = list->tail;
	entry->next = NULL;

	if (list->tail != NULL) {
		list->tail->next = entry;
	} else {
		list->head = entry;
	}
	list->tail = entry;
	list->len++;

	return entry;
}

/*
 * NULL     Error occurred.
 * non-NULL Returns the data from the entry that was removed.
 */
void *
listRemove(List *list, ListEntry *entry)
{
	if (list == NULL) {
		fprintf(stderr, "Bad list pointer?! %p\n", list);
		return NULL;
	}

	if (entry->prev == NULL) {
		list->head = entry->next;
	} else {
		entry->prev->next = entry->next;
	}

	if (entry->next == NULL) {
		list->tail = entry->prev;
	} else {
		entry->next->prev = entry->prev;
	}

	list->len--;

	void *data = entry->data;
	free(entry);

	return data;
}

void *
listNext(List *list, ListEntry **entry)
{
	if (list == NULL) {
		fprintf(stderr, "Bad list pointer?! %p\n", list);
		return (void *)-1;
	}
	if (entry == NULL) {
		fprintf(stderr, "Bad entry pointer?! %p\n", entry);
		return (void *)-1;
	}

	if (*entry == NULL) {
		*entry = list->head;
	} else {
		*entry = (*entry)->next;
	}

	if (*entry != NULL) {
		return (*entry)->data;
	}
	return NULL;
}
