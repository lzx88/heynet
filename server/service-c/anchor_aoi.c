#include <stdio.h>
#include <assert.h>
#include "uitl/types.h"
#include "uitl/slist.h"

typedef struct Entity{
	struct Entity* next;
	uint32 id;
	uint16 radius;
}*EntityPtr;

#define X 0
#define Y 1
#define XY 2
#define NEXT 0
#define PREV 1
#define PN 2
#define MAX_REDIUS 100//最大视野半径

typedef struct OLNode{
	struct OLNode* list[XY][PN];
	uint16 x;
	uint16 y;
	EntityPtr entitys;
}*OLNodePtr;

typedef struct OlHeader{
	OLNodePtr* ptr;
	uint16 size;
}*OlHeaderPtr;

typedef struct AOI{
	OlHeader header[XY];
}*AOIPtr;

static void
_init_olheader(OlHeaderPtr header, uint16 size){
	header->size = size;
	header->ptr = calloc(sizeof(*header->ptr), size);
}
static void
_free_entity(EntityPtr e){
	EntityPtr t = NULL;
	while (e) {
		t = e;
		e = t->next;
		free(t);
	}
}
static void
_free_olnode(OLNodePtr node, uint8 dir){
	assert(dir < XY);
	OLNodePtr t = NULL;
	while (node) {
		t = node;
		node = t->list[dir][NEXT];
		_free_entity(t->entitys);
		free(t);
	}
}
static OLNodePtr*
_get_olhead(AOIPtr thiz, uint8 dir, uint16 i){
	assert(dir < XY);
	assert(i < thiz->header[dir].size);
	return &thiz->header[dir].ptr[i];
}
static bool
_is_empty_olnode(OLNodePtr node){
	return node->entitys == NULL;
}
static bool
_is_empty_olhead(AOIPtr thiz, uint8 dir, uint16 i){
	uint8 direct;
	switch (dir) {
	case X: direct = Y; break;
	case Y: direct = X; break;
	default: assert(false);
	}
	OLNodePtr node = *_get_olhead(thiz, dir, i);
	while (node) {
		if (!_is_empty_olnode(node))
			return false;
		node = node->list[direct][NEXT];
	}
	return true;
}

static OLNodePtr
_add_anchor(AOIPtr thiz, uint16 x, uint16 y){
	OLNodePtr* xpnode = _get_olhead(thiz, X, x);

	OLNodePtr target = *xpnode;
	while (target && target->y < y)
		target = target->list[Y].[NEXT];
	if (target && target->y == y)
		return target;

	OLNodePtr node = *xpnode;
	for (int iy = 0; iy < thiz->header[Y].size; ++iy){
		if (node && node->y == iy){
			node = node->list[Y].[NEXT];
			continue;
		}
		if (!_is_empty_olhead(thiz, Y, iy)){
			target = malloc(sizeof(*target));
			target->entitys = NULL;
			target->x = x;
			target->y = iy;
			target->list[]
			if (node == NULL){

			}
		}


	}



	return newnode;
}


void
aoi_free(AOIPtr thiz){
	assert(thiz);
	for (uint16 i = 0; i < thiz->header[X].size; ++i)
		_free_olnode(thiz->header[X].ptr[i], Y);
	free(thiz->header[X].ptr);
	free(thiz->header[Y].ptr);
	free(thiz);
	thiz = NULL;
}
AOIPtr
aoi_init(uint16 mx, uint16 my){
	AOIPtr thiz = malloc(sizeof(*thiz));
	_init_olheader(&thiz->header[X], mx);
	_init_olheader(&thiz->header[Y], my);
}

static void
_entity_exist(EntityPtr head, uint32 id){
	while (head){
		if (head->id == id)
			return true;
		head = head->next;
	}
	return false;
}

static void
_add_entity(EntityPtr *head, EntityPtr e){
	assert(!_entity_exist(*head, e->id));
	e->next = *head;
	*head = e;
}

static void
_del_entity(EntityPtr* head, uint32 id){
	EntityPtr* p = head;
	EntityPtr e = *head;
	while (e){
		if (e->id == id){
			*p = e->next;
			free(e);
			break;
		}
		p = &e->next;
		e = e->next;
	}
}


void (aoi_cb*)(uint32 watcher, uint32 entity);

void
cb_appear(uint32 watcher, uint32 entity){
	printf("%d watch %d appear!", watcher, entity);
}
void
cb_disappear(uint32 watcher, uint32 entity){
	printf("%d watch %d disappear!", watcher, entity);
}
void
cb_move(uint32 watcher, uint32 entity){
	printf("%d watch %d move!", watcher, entity);
}

void (_iterquad*)(OLNodePtr origin, EntityPtr entity, aoi_cb cb, uint8 xy, uint8 pn, uint8 xy2, uint8 pn2, _iterquad iter_cb);
void
_iter_entity(EntityPtr entity, EntityPtr target, uint16 distance, aoi_cb cb){
	while (entity){
		if (entity->radius >= distance)
			cb(entity->id, target->id);
		entity = entity->next;
	}
}
void
_iter_quad(OLNodePtr origin, EntityPtr entity, aoi_cb cb, uint8 xy, uint8 pn, uint8 xy2, uint8 pn2, _iterquad iter_cb){
	OLNodePtr header = origin;
	for (;;){
		header = header->list[xy][pn];
		if (header == NULL)
			break;
		uint16 distance = xy == X ? abs(header->x - origin->x) : abs(header->y - origin->y);
		if (distance > MAX_REDIUS)
			break;
		_iter_entity(header->entitys, entity, distance, cb);
		if (iter_cb)
			iter_cb(header, entity, cb, xy2, pn2, 0, 0, NULL);
	}
}
void
_iter_watcher(OLNodePtr origin, EntityPtr entity, aoi_cb cb){
	_iter_entity(origin, entity, 0, cb);
	_iter_quad(origin, entity, cb, X, PREV, Y, PREV, _iter_quad);
	_iter_quad(origin, entity, cb, Y, PREV, X, NEXT, _iter_quad);
	_iter_quad(origin, entity, cb, X, NEXT, Y, NEXT, _iter_quad);
	_iter_quad(origin, entity, cb, Y, NEXT, X, PREV, _iter_quad);
}

void
aoi_add(AOIPtr thiz, uint32 entity, uint16 x, uint16 y, uint16 radius){
	assert(x < thiz->header[X].size && y < thiz->header[Y].size && radius <= MAX_REDIUS);
	OLNodePtr header = _add_anchor(thiz, x, y);
	assert(header);
	EntityPtr e = malloc(sizeof(*e));
	e->id = entity;
	e->radius = radius;
	_iter_watcher(header, e, cb_appear);
	_add_entity(&header->entitys, e);
}

void
aoi_del(AOIPtr thiz, uint32 entity, uint16 x, uint16 y){
	assert(x < thiz->header[X].size && y < thiz->header[Y].size);
	_del_entity(thiz->xHeader[x], e, x, y);
}

void
aoi_move(AOIPtr thiz, GIDX gidx, bool w, uint32 entity, uint16 x, uint16 y){
	assert(x < thiz->mx && y < thiz->my);
	assert(gidx < (thiz->gxlen * thiz->gylen));
	EntityPtr e = w ? thiz->vGrid[gidx]->watchers : thiz->vGrid[gidx]->markers;
	EntityPtr prev = NULL;
	while (e && e->id != entity) {//iterator one grid //todo
		prev = e;
		e = e->next;
	}
	assert(e->id == entity && e->gidx == gidx);
	assert(e->x != x || e->y != y);
	gidx = _grid_idx(thiz, x, y);
	if (e->gidx != gidx) {
		_entity_del(w ? &thiz->vGrid[gidx].watchers : &thiz->vGrid[gidx].markers, prev, e);
		_iter_grid9(thiz, e, cb_disappear);
		e->x = x;
		e->y = y;
		e->gidx = gidx;
		_iter_grid9(thiz, e, cb_appear);
		_add_entity(w ? &thiz->vGrid[e->gidx].watchers : &thiz->vGrid[e->gidx].markers, e);
	}
	else {
		e->x = x;
		e->y = y;
		_iter_grid9(thiz, e->gidx, e->id, cb_move);//todo 移动事件中增加出视野 进视野判断
	}
}