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

  $Id: fl_hash.h,v 1.1.1.1 2004/02/17 23:31:44 sterling Exp $
*/

#ifndef _FL_HASH_H
#define _FL_HASH_H

typedef void (*FL_HashDtor)(void *);
typedef void (*FL_Free)(void *);
typedef void *(*FL_Malloc)(size_t);
typedef void *(*FL_Calloc)(size_t, size_t);

typedef struct {
	FL_Free   free_func;
	FL_Malloc malloc_func;
	FL_Calloc calloc_func;
} FL_Allocator;

typedef struct _FL_Bucket {
	struct _FL_Bucket *next;
	char *key;
	int length;
	unsigned int hash;
	void *data;
} FL_Bucket;

#define FL_HASH_SIZE 1009
typedef struct _Hash {
	 FL_Bucket *buckets[ FL_HASH_SIZE ];
	 FL_Allocator *mems;
	 FL_HashDtor dtor;
	 int nElements;
} FL_Hash;

FL_Hash *fl_hash_new(FL_Allocator *, FL_HashDtor);
void fl_hash_free(FL_Hash *table);
void *fl_hash_find(FL_Hash *table, const void *key, int length);
void fl_hash_add(FL_Hash *table, char *key, int length, void *data);
void fl_hash_delete(FL_Hash *table, char *key, int length);
#endif

/*
Local variables:
tab-width: 4
c-basic-offset: 4
End:
vim600: noet sw=4 ts=4 fdm=marker
vim<600: noet sw=4 ts=4
*/
