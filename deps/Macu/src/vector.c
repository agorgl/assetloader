#include "vector.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define VECTOR_INIT_CAPACITY 10

void vector_init(struct vector* v, size_t item_sz)
{
    memset(v, 0, sizeof(struct vector));
    v->item_sz = item_sz;
    v->capacity = VECTOR_INIT_CAPACITY;
    v->size = 0;
    v->data = malloc(v->capacity * item_sz);
    memset(v->data, 0, v->capacity * item_sz);
}

void vector_destroy(struct vector* v)
{
    free(v->data);
    v->data = 0;
    v->capacity = 0;
    v->size = 0;
    v->item_sz = 0;
}

static void vector_grow(struct vector* v)
{
    v->capacity = (v->capacity * 2 + 1);
    v->data = realloc(v->data, v->capacity * v->item_sz);
    memset(v->data + v->size * v->item_sz, 0, (v->capacity - v->size) * v->item_sz);
}

void vector_append(struct vector* v, void* thing)
{
    if (v->capacity - v->size < 1) {
        vector_grow(v);
    }
    v->size++;
    memcpy(v->data + v->item_sz * (v->size - 1), thing, v->item_sz);
}

void vector_set(struct vector* v, size_t index, void* value)
{
    memcpy(v->data + index * v->item_sz, value, v->item_sz);
}

void vector_insert(struct vector* v, size_t index, void* thing)
{
    assert(index < v->size);

    if (v->capacity - v->size < 1) {
        vector_grow(v);
    }

    if (index < v->size) {
        memmove(
            v->data + (index + 1) * v->item_sz,
            v->data + (index + 0) * v->item_sz,
            (v->size - index) * v->item_sz
        );
        v->size++;
    }
    vector_set(v, index, thing);
}

void vector_remove_range(struct vector* v, size_t start, size_t end)
{
    assert(start < v->size);
    assert(end < v->size + 1);
    assert(start < end);
    assert(end - start <= v->size);

    memmove(
        v->data + start * v->item_sz,
        v->data + end * v->item_sz,
        (v->size - end) * v->item_sz
    );
    v->size -= end - start;
}

void vector_remove(struct vector* v, size_t index)
{
    vector_remove_range(v, index, index + 1);
}

void* vector_at(struct vector* v, size_t index)
{
    return v->data + v->item_sz * index;
}

void vector_clear(struct vector* v)
{
    v->size = 0;
}
