#include <stdio.h>
#include <assert.h>
#include "uitl/types.h"
#include "uitl/slist.h"

typedef uint8 GIDX;

typedef struct Entity{
	struct Entity* next;
	uint32 id;
	uint16 r;//视野半径
	GIDX gidx;
	uint16 x;
	uint16 y;
}*EntityPtr;

typedef struct Grid{
	EntityPtr watchers;
	EntityPtr markers;
}*GridPtr;

typedef struct AOI{
	uint16 mx;
	uint16 my;
	uint16 gxbit;
	uint16 gybit;
	GIDX gxlen;
	GIDX gylen;
	GridPtr vGrid;
}*AOIPtr;

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
static void
_free_entity(EntityPtr e){
	EntityPtr t = NULL;
	while (e) {
		t = e;
		e = e->next;
		free(t);
	}
}
static GIDX
_grid_idx0(AOIPtr thiz, GIDX gx, GIDX gy) {
	return gx * thiz->gylen + gy;
}
static inline GIDX
_grid_idx(AOIPtr thiz, uint16 x, uint16 y) {
	return _grid_idx0(thiz, x >> thiz->gxbit, y >> thiz->gybit);
}
static inline bool
_in_sight(EntityPtr w, EntityPtr e){
	return w->r >= abs(w->x - e->x) && w->r >= abs(w->y - e->y);
}
static GIDX
_iter_watcher(EntityPtr w, EntityPtr e, aoi_cb cb) {
	while (w) {
		if (_in_sight(w, e))//todo 自己格子内不需要比较
			cb(w->id, e->id);
		w = w->next;
	}
}
static void
_iter_grid_row(AOIPtr thiz, GIDX gidx, EntityPtr e, aoi_cb cb) {
	_iter_watcher(thiz->vGrid[gidx].watchers, e, cb);
	if (gidx % thiz->gylen != 0)
		_iter_watcher(thiz->vGrid[gidx - 1].watchers, e, cb);
	if ((gidx + 1) % thiz->gylen != 0)
		_iter_watcher(thiz->vGrid[gidx + 1].watchers, e, cb);
}
static void
_iter_grid9(AOIPtr thiz, EntityPtr e, aoi_cb cb){
	_iter_grid_row(AOIPtr thiz, e->gidx, e, cb);
	if (e->gidx >= thiz->gylen)
		_iter_grid_row(AOIPtr thiz, e->gidx - thiz->gylen, e, cb);
	if (e->gidx < (thiz->gylen - thiz->gxlen))
		_iter_grid_row(AOIPtr thiz, e->gidx + thiz->gylen, e, cb);
}
static void
_add_entity(EntityPtr *head, EntityPtr e) {
	e->next = *head;
	*head = e;
}
static void
_del_entity(EntityPtr *head, EntityPtr prev, EntityPtr e) {
	if (prev)
		prev->next = e->next;
	else
		*head = e->next;
}

AOIPtr
aoi_init(uint16 mx, uint16 my, uint8 gx_bit, uint8 gy_bit){
	AOIPtr thiz = malloc(sizeof(*thiz));
	thiz->mx = mx;
	thiz->my = my;
	thiz->gxbit = gx_bit;
	thiz->gybit = gy_bit;
	thiz->gxlen = ((mx - 1) >> gx_bit) + 1;
	thiz->gylen = ((my - 1) >> gy_bit) + 1;
	thiz->vGrid = calloc(sizeof(*thiz->vGrid), thiz->gxlen * thiz->gylen);
}
void
aoi_free(AOIPtr thiz){
	assert(thiz);
	for (uint8 i = 0; i < thiz->gxlen * thiz->gylen; ++i){
		_free_entity(thiz->vGrid[i].watchers);
		_free_entity(thiz->vGrid[i].markers);
	}
	free(thiz->vGrid);
	free(thiz);
}
GIDX
aoi_add(AOIPtr thiz, uint32 entity, uint16 x, uint16 y, uint16 r, bool w){
	assert(x < thiz->mx && y < thiz->my);
	EntityPtr e = malloc(sizeof(*e));
	e->id = entity;
	e->x = x;
	e->y = y;
	e->r = r;
	e->gidx = _grid_idx(thiz, x, y);
	_iter_grid9(thiz, e, cb_appear);
	_add_entity(w ? &thiz->vGrid[e->gidx].watchers : &thiz->vGrid[e->gidx].markers, e);
	return e->gidx;
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
		_del_entity(w ? &thiz->vGrid[gidx].watchers : &thiz->vGrid[gidx].markers, prev, e);
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