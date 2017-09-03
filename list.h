#ifndef _LIST_H_
#define _LIST_H_

typedef struct ListEntry {
	void *data;
	struct ListEntry *next;
	struct ListEntry *prev;
} ListEntry;

typedef struct List {
	ListEntry *head;
	ListEntry *tail;
	int len;
} List;

extern struct ListEntry *listAppend(List *list, void *data);
extern void *listRemove(List *list, ListEntry *entry);
extern void *listNext(List *list, ListEntry **entry);

#endif /* _LIST_H_ */
