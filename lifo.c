#include "lifo.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

Lifo *
lifoAlloc()
{
	Lifo *lifo;

	lifo = malloc(sizeof(*lifo));
	if (lifo == NULL) {
		fprintf(stderr, "Can't allocate Lifo structure: %s\n", strerror(errno));
		return NULL;
	}
	memset(lifo, 0, sizeof(*lifo));

	return lifo;
}

void
lifoFree(Lifo **lifo)
{
	if (lifo == NULL || *lifo == NULL) {
		return;
	}

	while (lifoPop(*lifo) != NULL);

	free(*lifo);
	*lifo = NULL;
}

int
lifoPush(Lifo *lifo, void *data)
{
	if (lifo == NULL || data == NULL) {
		fprintf(stderr, "%s(%p,%p): Invalid arguments?!\n",
				__func__, lifo, data);
		return -1;
	}

	LifoNode *n;

	n = malloc(sizeof(*n));
	if (n == NULL) {
		fprintf(stderr, "Can't allocate LifoNode structure: %s\n",
				strerror(errno));
		return -1;
	}
	n->next = lifo->head;
	n->data = data;
	lifo->head = n;

	return 0;
}

void *
lifoPop(Lifo *lifo)
{
	if (lifo == NULL) {
		fprintf(stderr, "%s(%p): Invalid arguments?!\n", __func__, lifo);
		return (void *)-1;
	}
	if (lifo->head == NULL) {
		return NULL;
	}

	LifoNode *n = lifo->head;
	lifo->head = n->next;
	void *data = n->data;
	free(n);

	return data;
}

void *
lifoPeek(Lifo *lifo)
{
	if (lifo == NULL) {
		fprintf(stderr, "%s(%p): Invalid arguments?!\n", __func__, lifo);
		return (void *)-1;
	}
	if (lifo->head == NULL) {
		return NULL;
	}

	return lifo->head->data;
}
