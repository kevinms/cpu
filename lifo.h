#ifndef __LIFO_H
#define __LIFO_H

typedef struct LifoNode {
	struct LifoNode *next;
	void *data;
} LifoNode;

typedef struct Lifo {
	struct LifoNode *head;
} Lifo;

/*
 * Allocates a Lifo (last in, first out) queue handle.
 *
 * On success, returns a pointer to the newly allocated queue handle.
 * On error, returns NULL.
 */
Lifo *lifoAlloc();

/*
 * Frees the Lifo handle.
 *
 * Attention:
 * This function does not free the data pointers. If the data pointer
 * needs to be freed, make sure to manually call lifoPop() to empty
 * the Lifo before freeing the handle.
 */
void lifoFree(Lifo **lifo);

/*
 * Pushes 'data' onto the queue.
 *
 * On success, returns 0.
 * On error, returns -1.
 */
int lifoPush(Lifo *lifo, void *data);

/*
 * Removes the most recently pushed 'data' from the queue.
 *
 * On success, returns the removed data or NULL if the queue is empty.
 * On error, returns (void *)-1.
 */
void *lifoPop(Lifo *lifo);

/*
 * Returns the most recently pushed 'data' from the queue without removing it.
 *
 * On success, returns the data or NULL if the queue is empty.
 * On error, returns (void *)-1.
 */
void *lifoPeek(Lifo *lifo);

#endif /* __LIFO_H */
