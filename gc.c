#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
struct object {
	struct object *prev;
	struct object *next;
	uint16_t color; // 0: WHITE, 1: GRAY, 2: BLACK, 2+n: n entry references
	uint16_t fieldcount;
	uint32_t datacount;
};
typedef void *(*actr)(size_t);
typedef void (*dctr)(void*);
struct object sentinel = {&sentinel, &sentinel, 3, 0};
uint64_t total = 0, next_gc = 1024;
actr alloctor = malloc;
dctr dellctor = free;
void obj_ref(struct object *obj) {
	assert(obj->color != 0xFFFF);
	if (++obj->color<3) {
		obj->color = 3;
	}
}
void obj_deref(struct object *obj) {
	assert(obj->color >= 3);
	obj->color--;
}
struct object *obj_field_get(struct object *obj, uint16_t field) {
	assert(field < obj->fieldcount);
	return ((struct object *)(obj + sizeof(struct object)))[field];
}
void obj_field_put(struct object *obj, uint16_t field, struct object *value) {
	assert(field < obj->fieldcount);
	((struct object *)(obj + sizeof(struct object)))[field] = value;
}
void *obj_data_ref(struct object *obj, uint32_t offset) {
	assert(offset < obj->datacount);
	return obj + sizeof(struct object) + obj->fieldcount * sizeof(struct object *);
}
void gc_mark_iter(struct object *obj) {
	struct object *objs = ((void *) obj) + sizeof(struct obj);
	uint16_t i;
	for (i=0; i<obj->fieldcount; i++) {
		if (objs[i].color == 0) {
			objs[i].color = 1;
		}
	}
}
void do_gc() {
	struct object *cur;
	for (cur=sentinel.next; cur != &sentinel; cur = cur->next) {
		if (cur->color < 3) {
			cur->color = 0;
		}
	}
	for (cur=sentinel.next; cur != &sentinel; cur = cur->next) {
		if (cur->color >= 3) {
			gc_mark_iter(cur);
		}
	}
	uint8_t did_anything;
	do {
		did_anything = 0;
		for (cur=sentinel.next; cur != &sentinel; cur = cur->next) {
			if (cur->color == 1) {
				gc_mark_iter(cur);
				cur->color = 2;
				did_anything = 1;
			}
		}
	} while(did_anything);
	for (cur=sentinel.next; cur != &sentinel; ) {
		assert(cur->color != 1);
		if (cur->color == 0) {
			cur->prev->next = cur->next;
			cur->next->prev = cur->prev;
			struct object *tofree = cur;
			total -= tofree->datacount + tofree->fieldcount * sizeof(struct object *) + sizeof(struct object);
			cur = cur->next;
			free(tofree);
		} else {
			cur = cur->next;
		}
	}
	next_gc = total + (total >> 1);
}
struct object *obj_new(uint16_t fieldcount, uint32_t datacount) {
	size_t cnt = datacount + fieldcount * sizeof(struct object *) + sizeof(struct object);
	assert(cnt > datacount && cnt > fieldcount * sizeof(struct object *) && cnt > sizeof(struct object));
	if (total + cnt >= next_gc) {
		do_gc();
	}
	total += cnt;
	struct object *out = alloctor(cnt);
	assert(out != NULL);
	(out->prev = sentinel.prev)->next = out;
	(out->next = &sentinel)->prev = out;
	out->color = 3;
	out->fieldcount = fieldcount;
	out->datacount = datacount;
	return out;
}
struct object *instance_new_raw(struct object *class) {
	uint16_t fieldcount = *(uint16_t *) obj_data_ref(class, 0);
	assert(fieldcount > 0);
	uint32_t datacount = *(uint32_t *) obj_data_ref(class, 2);
	struct object *nobj = obj_new(fieldcount, datacount);
	obj_field_put(nobj, 0, class);
}
struct object *array_new(struct object *class, uint32_t length) {
	uint16_t fieldcount = *(uint16_t *) obj_data_ref(class, 0);
	assert(fieldcount == 0); // means that it's an array. it does actually have one entry.
	uint32_t datacount = *(uint32_t *) obj_data_ref(class, 2);
	assert(datacount > 0);
	struct object *nobj = obj_new(1, length * datacount);
}
int main(int argc, char *argv[]) {
	struct object *class = obj_new(1, 6);
	obj_field_put(class, 0, class); // of its own type
	*((uint16_t *)obj_dat_ref(class, 0)) = 1;
	*((uint32_t *)obj_dat_ref(class, 2)) = 6;
	struct object *bytearray_class = instance_new_raw(class);
	*((uint16_t *)obj_dat_ref(bytearray_class, 0)) = 0;
	*((uint32_t *)obj_dat_ref(bytearray_class, 2)) = 1; // one byte per entry
	struct object *string = instance_new_raw(class);
	*((uint16_t *)obj_dat_ref(string, 0)) = 2;
	*((uint32_t *)obj_dat_ref(string, 2)) = 0;
	return 0;
}

