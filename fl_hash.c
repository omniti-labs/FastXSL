/*
  +----------------------------------------------------------------------+
  | PHP Version 4                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2003 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 2.02 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available at through the world-wide-web at                           |
  | http://www.php.net/license/2_02.txt.                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Thies C. Arntzen <thies@php.net>                             |
  |         Sterling Hughes <sterling@php.net>                           |
  +----------------------------------------------------------------------+

  $Id: fl_hash.c,v 1.1.1.1 2004/02/17 23:31:44 sterling Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "php_fastxsl.h"
#include "fl_hash.h"

FL_Hash *fl_hash_new(FL_Allocator *mems, FL_HashDtor dtor)
{
	FL_Hash *table;

	table = mems->calloc_func(1, sizeof(FL_Hash));
	table->dtor = dtor;
	table->mems = mems;

	return table;
}

void fl_hash_free(FL_Hash *table) 
{
	FL_Bucket *cur;
	FL_Bucket *next;
	FL_Allocator *mems;
	int     i;

	mems = table->mems;
	
	for (i = 0; i < FL_HASH_SIZE; i++) {
		cur = table->buckets[i];
		while (cur) {
			next = cur->next;

			if (table->dtor) table->dtor(cur->data);
			mems->free_func(cur->key);
			mems->free_func(cur);

			cur = next;
		}
	}
	mems->free_func(table);
}

static unsigned int hash_hash(const char *key, int length)
{
	unsigned int hash = 0;

	while (--length)
		hash = hash * 65599 + *key++;

	return hash;
}

void *fl_hash_find(FL_Hash *table, const void *key, int length)
{
	FL_Bucket       *b;
	unsigned int  hash = hash_hash(key, length) % FL_HASH_SIZE;
	for (b = table->buckets[ hash ]; b; b = b->next) {
		if (hash != b->hash) continue;
		if (length != b->length) continue;
		if (memcmp(key, b->key, length)) continue;
		return b->data;
	}

	return NULL;
}

int fl_hash_add(FL_Hash *table, char *key, int length, void *data)
{
	FL_Bucket       *b, *ptr;
	unsigned int  hash;
	
	hash = hash_hash(key, length) % FL_HASH_SIZE;
	/* check hash for dupes */
	ptr = table->buckets[ hash ];
	while(ptr) {
		if(!strcmp(ptr->key, key)) {
			return 0;
		}
		ptr = ptr->next;
	}

	b = table->mems->malloc_func(sizeof(FL_Bucket));
	b->key = table->mems->malloc_func(length + 1);
	memcpy(b->key, key, length);
	b->key[length] = 0;
	b->length = length;
	b->hash = hash;
	b->data = data;
	b->next = table->buckets[ hash ];

	table->buckets[ hash ] = b;
	table->nElements++;
	return 1;
}

void fl_hash_delete(FL_Hash *table, char *key, int length)
{
	FL_Bucket       *b; 
	FL_Bucket       *prev = NULL;
	unsigned int  hash;

	hash = hash_hash(key, length) % FL_HASH_SIZE;
	for (b = table->buckets[ hash ]; b; b = b->next) {
		if (hash != b->hash || length != b->length || memcmp(key, b->key, length)) {
			prev = b;
			continue;
		}

		/* unlink */
		if (prev) {
			prev->next = b->next;
		} else {
			table->buckets[hash] = b->next;
		}
		
		if (table->dtor) table->dtor(b->data);
		table->mems->free_func(b->key);
		table->mems->free_func(b);

		break;
	}
}

/*
Local variables:
tab-width: 4
c-basic-offset: 4
End:
vim600: noet sw=4 ts=4 fdm=marker
vim<600: noet sw=4 ts=4
*/
