struct slist_head{
	struct slist_head *next;
};

static void
_add(struct slist_head *prev, struct slist_head *head, struct slist_head *tail){
	tail->next = prev->next;
	prev->next = head;
}

void
slist_add(struct slist_head *node, struct slist_head *add){
	_add(node, add, add);
}

void
slist_del(struct slist_head *head, struct slist_head *tail){
	head->next = tail->next;
}

void
slist_clear(struct slist_head *head){
	head->next = head;
}

void
slist_move(struct slist_head *node, struct slist_head *head, struct slist_head *tail){
	slist_del(head, tail);
	_add(node, head->next, tail);
}

bool
slist_last(const struct slist_head *node, const struct slist_head *H){
	return node->next == H;
}

bool
slist_empty(const struct slist_head *H){
	return H->next == H;
}