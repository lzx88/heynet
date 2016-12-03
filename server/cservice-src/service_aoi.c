#include <stdio.h>
#include <assert.h>
#include "uitl/types.h"
#include "uitl/slist.h"

typedef uint8 GIDX;

typedef struct Entity{
	struct Entity* next;
	uint32 id;
	uint16 x;
	uint16 y;
	GIDX gx;
	GIDX gy;
	uint16 r;//视野半径
}*EntityPtr;

typedef struct Grid{
	EntityPtr watchers;
	EntityPtr markers;
}*GridPtr;

typedef struct AOI{
	uint16 mx;
	uint16 my;
	uint16 gx_bit;
	uint16 gy_bit;
	GIDX xlen;
	GIDX ylen;
	GridPtr vGrid;
}*AOIPtr;

void (aoi_cb*)(uint32 watcher, uint32 entity);
void cb_appear(uint32 watcher, uint32 entity){
	printf("%d watch %d appear!", watcher, entity);
}
void cb_disappear(uint32 watcher, uint32 entity){
	printf("%d watch %d disappear!", watcher, entity);
}
void cb_move(uint32 watcher, uint32 entity){
	printf("%d watch %d move!", watcher, entity);
}

AOIPtr
aoi_init(uint16 mx, uint16 my, uint8 gx_bit, uint8 gy_bit){
	AOIPtr thiz = malloc(sizeof(*thiz));
	thiz->mx = mx;
	thiz->my = my;
	thiz->gx_bit = gx_bit;
	thiz->gy_bit = gy_bit;
	thiz->xlen = ((mx - 1) >> gx_bit) + 1;
	thiz->ylen = ((my - 1) >> gy_bit) + 1;
	thiz->vGrid = calloc(sizeof(*thiz->vGrid), thiz->xlen * thiz->ylen);
}

void
aoi_free(AOIPtr thiz){
	assert(thiz);
	EntityPtr e = NULL, t = NULL;
	for (uint8 i = 0; i < thiz->xlen * thiz->ylen; ++i){
		e = thiz->vGrid[i]->watchers;
		while (e){
			t = e;
			e = e->next;
			free(t);
		}
		e = thiz->vGrid[i]->markers;
		while (e){
			t = e;
			e = e->next;
			free(t);
		}
	}
	free(thiz->vGrid);
	free(thiz);
}

static GIDX
_grid_idx(AOIPtr thiz, uint16 x, uint16 y){
	return (x >> thiz->gx_bit) * thiz->ylen + (y >> thiz->gy_bit);
}

static GIDX
_iter_watcher(EntityPtr pEntity, uint32 eid, aoi_cb cb){
	while (pEntity){
		if (pEntity->id != eid)//??
			cb(pEntity->id, eid);
		pEntity = pEntity->next;
	}
}

static void
_iter_grid_row(AOIPtr thiz, GIDX egidx, uint32 eid, aoi_cb cb){
	_iter_watcher(thiz->vGrid[egidx]->watchers, eid, cb);
	GIDX n = (thiz->my >> thiz->gy_bit);
	if (egidx > 0 && ((egidx - 1) % n) != (n - 1))
		_iter_watcher(thiz->vGrid[egidx - 1]->watchers, eid, cb);
	if ((egidx + 1) % n != 0)
		_iter_watcher(thiz->vGrid[egidx + 1]->watchers, eid, cb);
}

static void
_iter_grid9(AOIPtr thiz, GIDX egidx, uint32 eid, aoi_cb cb){
	GIDX n = (thiz->my >> thiz->gy_bit);
	_iter_grid_row(AOIPtr thiz, egidx, eid, cb);
	if (egidx >= n)
		_iter_grid_row(AOIPtr thiz, egidx - n, eid, cb);
	if (egidx < (thiz->glen - n))
		_iter_grid_row(AOIPtr thiz, egidx + n, eid, cb);
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

	EntityPtr h = NULL;
	if (w)
		h = thiz->vGrid[e->gidx]->watchers;
	else
		h = thiz->vGrid[e->gidx]->markers;

	e->next = h;
	h = e;
	_iter_grid9(thiz, e->gidx, e->id, cb_appear);
	return e->gidx;
}

void
aoi_move(AOIPtr thiz, GIDX gx, GIDX gy, bool w, uint32 entity, uint16 x, uint16 y){
	assert(x < thiz->mx && y < thiz->my);
	assert(gx < thiz->xlen && gy < thiz->ylen);
	EntityPtr e = NULL;
	if (w)
		e = thiz->vGrid[gx * thiz->ylen + ]->watchers;
	else
		e = thiz->vGrid[gidx]->markers;
	while (e){//tofix
		if (e->id == entity)
			break;
		e = e->next;
	}
	assert(e->id == entity && e->gidx == gidx);
	assert(e->x != x || e->y != y);
	e->x = x;
	e->y = y;
	e->gidx = _grid_idx(thiz, x, y);
	if (e->gidx != gidx){
		_iter_grid9(thiz, gidx, e->id, cb_disappear);
		_iter_grid9(thiz, e->gidx, e->id, cb_appear);
	}
	else
		_iter_grid9(thiz, e->gidx, e->id, cb_move);
}