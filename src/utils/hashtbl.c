#include "hashtbl.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

hashtbl_t *hashtbl_init(int log_size, int obj_offset, int auto_grow)
{
	hashtbl_t *hashtbl;

	if (log_size < 0 || log_size > 20) {
		return NULL;
	}

	if (obj_offset < 0 || obj_offset > 65535) {
		return NULL;
	}

	hashtbl = (hashtbl_t *)malloc(sizeof(hashtbl_t));
	if (!hashtbl) {
		return NULL;
	}

	hashtbl->tbl = (hashtbl_link_t **)malloc(sizeof(hashtbl_link_t *) * (1 << log_size));
	if (!hashtbl->tbl) {
		free(hashtbl);
		return NULL;
	}

	memset(hashtbl->tbl, 0, sizeof(hashtbl_link_t *) * (1 << log_size));
	hashtbl->log_size = log_size;
	hashtbl->auto_grow = (auto_grow) ? 1 : 0;
	hashtbl->obj_offset = obj_offset;
	hashtbl->num_items = 0;

	return hashtbl;
}

int hashtbl_insert(hashtbl_t *hashtbl, int key, void *obj)
{
	int idx = key & ((1 << hashtbl->log_size) - 1);
	hashtbl_link_t *head = hashtbl->tbl[idx];
	hashtbl_link_t *link = OBJ2LINK(hashtbl, obj);

	link->next = head;
	if (head)
		head->prev = link;
	link->prev = NULL;
	link->hash_value = key;
	hashtbl->tbl[idx] = link;
	hashtbl->num_items++;

	return 0;
}

int hashtbl_remove(hashtbl_t *hashtbl, int key, void *obj)
{
	int idx = key & ((1 << hashtbl->log_size) - 1);
	hashtbl_link_t *head = hashtbl->tbl[idx];
	hashtbl_link_t *link = OBJ2LINK(hashtbl, obj);
	hashtbl_link_t *iter;

	/*Check if it's in hash table*/
	for (iter = head; iter; iter = iter->next) {
		if (iter == link)
			break;
	}
	if (iter == NULL)
		return 0;

	if (link->next) {
		link->next->prev = link->prev;
	}
	if (link->prev) {
		link->prev->next = link->next;
	}
	if (link == head) {
		hashtbl->tbl[idx] = link->next;
	}

	link->next = link->prev = NULL;
	hashtbl->num_items--;
	return 0;
}

void *hashtbl_find(hashtbl_t *hashtbl, void *target_obj, int key, hashtbl_comp_func func)
{
	int idx = key & ((1 << hashtbl->log_size) - 1);
	hashtbl_link_t *head = hashtbl->tbl[idx];
	hashtbl_link_t *iter;

	for (iter = head; iter; iter = iter->next) {
		if (iter->hash_value != key)
			continue;
		if ((func)(LINK2OBJ(hashtbl, iter), target_obj))
			return LINK2OBJ(hashtbl, iter);
	}
	return NULL;
}

int hashtbl_free_all_objects(hashtbl_t *hashtbl, hashtbl_traverse_func func, void *args)
{
	if (NULL == hashtbl || (hashtbl->num_items == 0)) {
		return 0;
	}

	hashtbl_link_t *head = NULL;
	hashtbl_link_t *next = NULL;
	tk_uint32_t bucket_num = (1 << hashtbl->log_size);
	tk_uint32_t i = 0;

	for (i = 0; i < bucket_num; i++) {
		head = hashtbl->tbl[i];
		if (NULL == head) {
			continue;
		}

		while (head) {
			next = head->next;
			if (func) {
				(func)(LINK2OBJ(hashtbl, head), args);
			}
			head = next;
		}
		hashtbl->tbl[i] = NULL;
	}

	hashtbl->num_items = 0;

	return 0;
}

int hashtbl_destroy(hashtbl_t *hashtbl)
{
	free(hashtbl->tbl);
	free(hashtbl);
	return 0;
}

int hashtbl_reset(hashtbl_t *hashtbl)
{
	memset(hashtbl->tbl, 0, sizeof(hashtbl_link_t *) * (1 << hashtbl->log_size));
	hashtbl->num_items = 0;
	return 0;
}

int hashtbl_traverse_each(hashtbl_t *hashtbl, hashtbl_traverse_func func, void *args)
{
	if (NULL == hashtbl || NULL == func || (hashtbl->num_items == 0)) {
		return 0;
	}

	hashtbl_link_t *head = NULL;
	hashtbl_link_t *iter = NULL;
	tk_uint32_t bucket_num = (1 << hashtbl->log_size);
	tk_uint32_t i = 0;

	for (i = 0; i < bucket_num; i++) {
		head = hashtbl->tbl[i];
		if (NULL == head) {
			continue;
		}

		for (iter = head; iter; iter = iter->next) {
			if (func) {
				if ((func)(LINK2OBJ(hashtbl, iter), args))
					return -1;
			}
		}
	}

	return 0;
}

int hashtbl_traverse_each_safe(hashtbl_t *hashtbl, hashtbl_traverse_func func, void *args)
{
	if (NULL == hashtbl || NULL == func || (hashtbl->num_items == 0)) {
		return 0;
	}

	hashtbl_link_t *head = NULL;
	hashtbl_link_t *next = NULL;
	tk_uint32_t bucket_num = (1 << hashtbl->log_size);
	tk_uint32_t i = 0;

	for (i = 0; i < bucket_num; i++) {
		head = hashtbl->tbl[i];
		if (NULL == head) {
			continue;
		}

		while (head) {
			next = head->next;
			if (func) {
				(func)(LINK2OBJ(hashtbl, head), args);
			}
			head = next;
		}
	}

	return 0;
}