#ifndef slist_h_
#define slist_h_

extern struct slist_head;

void
slist_add(struct slist_head *prev, struct slist_head *node);

//delete (head, tail]
void
slist_del(struct slist_head *head, struct slist_head *tail);

void
slist_clear(struct slist_head *H);

/**
move (head, tail] to between prev and prev->next
*/
void
slist_move(struct slist_head *prev, struct slist_head *head, struct slist_head *tail);

/**
* slist_last - tests whether @list is the last entry in list @head
* @node: the entry to test
* @head: the head of the list
*/
bool
slist_last(const struct slist_head *node, const struct slist_head *H);

/**
* list_empty - tests whether a list is empty
* @head: the list to test.
*/
bool
list_empty(const struct slist_head *H);

/**
* slist_for_each    -    iterate over a list
* @pos:    the &struct list_head to use as a loop cursor.
* @head:    the head for your list.
*/
#define slist_for_each(curr, H) \
    for (struct slist_head* curr = (H)->next; curr != (H); curr = curr->next)

#endif//slist_head_h_