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
  | Author: Sterling Hughes <sterling@php.net>                           |
  +----------------------------------------------------------------------+

  $Id: fastxsl.c,v 1.1.1.1 2004/02/17 23:31:44 sterling Exp $ 
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libxml/xmlmemory.h>
#include <libxml/debugXML.h>
#include <libxml/HTMLtree.h>
#include <libxml/xmlIO.h>
#include <libxml/xinclude.h>
#include <libxml/catalog.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include <mm.h>
#include <sys/types.h>
#include <unistd.h>

#include "fl_hash.h"
#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_fastxsl.h"

ZEND_DECLARE_MODULE_GLOBALS(fastxsl);

static int le_fastxsl_stylesheet;
#define le_fastxsl_stylesheet_name "FastXSL Stylesheet"
static int le_fastxsl_document;
#define le_fastxsl_document_name "FastXSL Document"

#define FASTXSL_PRM_ALLOC 1
#define FASTXSL_SHARED_ALLOC 2

static php_ss_wrapper *
SS_Wrapper_Alloc(int shared TSRMLS_DC)
{
	php_ss_wrapper *wrapper;

	if (shared) {
		if (FASTXSL_SHARED_ALLOC) {
			wrapper = (php_ss_wrapper *) mm_calloc(FASTXSL_G(cache)->mm, 1, sizeof(php_ss_wrapper));
		}
		wrapper->persistant = 1;
	} else {
		wrapper = (php_ss_wrapper *) calloc(1, sizeof(php_ss_wrapper));
	}

	return wrapper;
}

static php_xd_wrapper *
XD_Wrapper_Alloc(void)
{
	php_xd_wrapper *wrapper;

	wrapper = (php_xd_wrapper *) calloc(1, sizeof(php_xd_wrapper));
	return wrapper;
}

static xmlFreeFunc    free_ptr;
static xmlMallocFunc  malloc_ptr;
static xmlReallocFunc realloc_ptr;
static xmlStrdupFunc  strdup_ptr;

static void
ShmCache_Free(void *ptr)
{
	TSRMLS_FETCH();
	FASTXSL_G(tmp_allocated_size) -= mm_sizeof(FASTXSL_G(cache)->mm, ptr);
	
	mm_free(FASTXSL_G(cache)->mm, ptr);
}

static void *
ShmCache_Malloc(size_t size)
{
	void *ptr;
	TSRMLS_FETCH();

	ptr = mm_malloc(FASTXSL_G(cache)->mm, size);
	if (!ptr) {
		php_error(E_ERROR, "Ran out of Shared memory to allocate data for FastXSL cache, "
				           "in function %s() cannot allocate %d bytes (%d available, %d allocated)", 
						   get_active_function_name(TSRMLS_C), size, mm_available(FASTXSL_G(cache)->mm), 
						   mm_maxsize() - mm_available(FASTXSL_G(cache)->mm));
		return NULL;
	}

	FASTXSL_G(tmp_allocated_size) += size;

	return ptr;
}

static void *
ShmCache_Calloc(size_t nmemb, size_t size) 
{
	void *ptr;

	ptr = ShmCache_Malloc(nmemb * size);
	memset(ptr, 0, nmemb * size);

	return ptr;
}

static void *
ShmCache_Realloc(void *ptr, size_t size)
{
	void *newptr;
	long  oldsize;
	TSRMLS_FETCH();

	oldsize = mm_sizeof(FASTXSL_G(cache)->mm, ptr);
	newptr = mm_realloc(FASTXSL_G(cache)->mm, ptr, size);
	if (!newptr) {
		TSRMLS_FETCH();
		php_error(E_ERROR, "Ran out of Shared memory to allocate data for FastXSL cache, "
				           "in function %s() cannot allocate %d bytes (%d available, %d allocated)", 
						   get_active_function_name(TSRMLS_C), size, mm_available(FASTXSL_G(cache)->mm), 
						   mm_maxsize() - mm_available(FASTXSL_G(cache)->mm));
		return NULL;
	}
	FASTXSL_G(tmp_allocated_size) += (size - oldsize);

	return newptr;
}

static char *
ShmCache_Strdup(const char *string)
{
	char *newstring;
	int   string_length;

	string_length = strlen(string);
	newstring = ShmCache_Malloc(string_length);
	memcpy(newstring, string, string_length);
	newstring[string_length] = 0;

	return newstring;
}

static void
Php_Free(void *ptr)
{
	efree(ptr);
}

static void *
Php_Malloc(size_t size)
{
	return emalloc(size);
}

static void *
Php_Realloc(void *ptr, size_t size)
{
	return erealloc(ptr, size);
}

static char *
Php_Strdup(const char *string)
{
	return estrdup(string);
}

static void
ShmCache_UseAllocationFunctions(void)
{
	xmlMemSetup(ShmCache_Free, ShmCache_Malloc, ShmCache_Realloc, ShmCache_Strdup);
}

static void
Php_UseAllocationFunctions(void)
{
	xmlMemSetup(Php_Free, Php_Malloc, Php_Realloc, Php_Strdup);
}

static void
Xml_UseAllocationFunctions(void)
{
	xmlMemSetup(free_ptr, malloc_ptr, realloc_ptr, strdup_ptr);
}

static php_ss_wrapper *
ShmCache_Stylesheet_ParseAndStore(char *filename, size_t filename_len, int mtime TSRMLS_DC)
{
	php_ss_wrapper *wrapper;
	
	wrapper = SS_Wrapper_Alloc(FASTXSL_SHARED_ALLOC TSRMLS_CC);

	ShmCache_UseAllocationFunctions();
	FASTXSL_G(tmp_allocated_size) = 0;
	wrapper->ss = xsltParseStylesheetFile(filename);
	if (!wrapper->ss) {
		Xml_UseAllocationFunctions();
		return NULL;
	}
	Xml_UseAllocationFunctions();
	wrapper->mtime = mtime;
	wrapper->allocsize = FASTXSL_G(tmp_allocated_size);
	
	fl_hash_add(FASTXSL_G(cache)->table, filename, filename_len, wrapper);

	return wrapper;
}

static void
ShmCache_Stylesheet_Free(php_ss_wrapper *wrapper TSRMLS_DC)
{
	if (wrapper->ss) {
		ShmCache_UseAllocationFunctions();
		xsltFreeStylesheet(wrapper->ss);
		Xml_UseAllocationFunctions();
	}
	mm_free(FASTXSL_G(cache)->mm, wrapper);
}

static void
ShmCache_Stylesheet_Delete(char *filename, size_t filename_len)
{
	php_ss_wrapper *wrapper;

	wrapper = fl_hash_find(FASTXSL_G(cache)->table, filename, filename_len);
	if (wrapper) {
		fl_hash_delete(FASTXSL_G(cache)->table, filename, filename_len);
		ShmCache_Stylesheet_Free(wrapper);
	}
}

static php_ss_wrapper *
PrmCache_Stylesheet_ParseAndStore(char *filename, size_t filename_len, int mtime)
{
	php_ss_wrapper *wrapper;

	wrapper = SS_Wrapper_Alloc(FASTXSL_PRM_ALLOC TSRMLS_CC);

	wrapper->ss = xsltParseStylesheetFile(filename);
	if (!wrapper->ss) {
		return NULL;
	}
	wrapper->mtime = mtime;
	
	fl_hash_add(FASTXSL_G(cache)->prmtable, filename, filename_len, wrapper);

	return wrapper;
}

static void
PrmCache_Stylesheet_Free(php_ss_wrapper *wrapper)
{
	if (wrapper->ss) {
		xsltFreeStylesheet(wrapper->ss);
	}
	free(wrapper);
}

static void
PrmCache_Stylesheet_Delete(char *filename, size_t filename_len TSRMLS_DC)
{
	php_ss_wrapper *wrapper;

	wrapper = fl_hash_find(FASTXSL_G(cache)->prmtable, filename, filename_len);
	if (wrapper) {
		fl_hash_delete(FASTXSL_G(cache)->prmtable, filename, filename_len);
		PrmCache_Stylesheet_Free(wrapper TSRMLS_CC);
	}
}

/* {{{ proto array fastxsl_prmcache_getstatistics(void)
   Get an array of statistics regarding the fastxsl documents in the process resident memory cache */
PHP_FUNCTION(fastxsl_prmcache_getstatistics)
{
	php_ss_wrapper *wrapper;	

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

}
/* }}} */

/* {{{ proto array fastxsl_shmcache_getstatistics(void)
   Get an array of statistics regarding the documents in the shared memory cache */
PHP_FUNCTION(fastxsl_shmcache_getstatistics)
{
	php_ss_wrapper *ss_wrapper;
	FL_Bucket      *bucket;
	zval           *files_array;
	zval           *info_array;
	int             i;
	long            allocated_bytes = 0;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RD);
	
	array_init(return_value);

	MAKE_STD_ZVAL(files_array);
	array_init(files_array);

	for (i = 0; i < FL_HASH_SIZE; i++) {
		for (bucket = FASTXSL_G(cache)->table->buckets[i]; bucket != NULL; bucket = bucket->next) {
			ss_wrapper = (php_ss_wrapper *) bucket->data;

			MAKE_STD_ZVAL(info_array);
			array_init(info_array);

			add_assoc_long(info_array, "allocated", ss_wrapper->allocsize);
			add_assoc_long(info_array, "mtime", ss_wrapper->mtime);

			add_assoc_zval(files_array, bucket->key, info_array);

			allocated_bytes += ss_wrapper->allocsize;
		}
	}
	add_assoc_zval(return_value, "files", files_array);

	add_assoc_long(return_value, "shm_allocated", allocated_bytes);
	add_assoc_long(return_value, "shm_maxsize", (long) mm_maxsize());

	mm_unlock(FASTXSL_G(cache)->mm);
}
/* }}} */

/* {{{ proto resource fastxsl_stylesheet_parsefile(string filename)
   Parse a stylesheet file located at filename. */
PHP_FUNCTION(fastxsl_stylesheet_parsefile)
{
	php_ss_wrapper *wrapper;
	char           *filename;
	size_t          filename_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &filename, 
				&filename_len) == FAILURE) {
		return;
	}

	wrapper = SS_Wrapper_Alloc(0 TSRMLS_CC);
	wrapper->ss = xsltParseStylesheetFile((const xmlChar *) filename);
	if (!wrapper->ss) {
		RETURN_FALSE;
	}

	ZEND_REGISTER_RESOURCE(return_value, wrapper, le_fastxsl_stylesheet);
}
/* }}} */

/* {{{ proto resource fastxsl_xml_parsestring(string text)
  Parse a string containing an XML document and return a resource pointer to the resulting libxml2 tree */ 
PHP_FUNCTION(fastxsl_xml_parsestring)
{
	php_xd_wrapper *wrapper;
	char           *text;
	size_t          text_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &text, &text_len) == FAILURE) {
		return;
	}

	wrapper = XD_Wrapper_Alloc();
	wrapper->xd = xmlParseDoc(text);
	if (!wrapper->xd) {
		RETURN_FALSE;
	}

	ZEND_REGISTER_RESOURCE(return_value, wrapper, le_fastxsl_document);
}
/* }}} */

/* {{{ proto resource fastxsl_xml_parsefile(string filename)
   Parse an XML file into a FastXSL resource */
PHP_FUNCTION(fastxsl_xml_parsefile)
{
	php_xd_wrapper *wrapper;
	char           *filename;
	size_t          filename_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &filename, 
				&filename_len) == FAILURE) {
		return;
	}

	wrapper = XD_Wrapper_Alloc();
	wrapper->xd = xmlParseFile((const xmlChar *) filename);
	if (!wrapper->xd) {
		RETURN_FALSE;
	}

	ZEND_REGISTER_RESOURCE(return_value, wrapper, le_fastxsl_document);
}
/* }}} */

static int
ParseTransformParameters(zval *z_parameters, char ***parameters TSRMLS_DC)
{
	zval         **current;
	HashTable     *h_parameters;
	HashPosition   pos;
	char          *key;
	int            key_length;
	unsigned long  ival;
	int            index = 0;

	h_parameters = Z_ARRVAL_P(z_parameters);

	if (zend_hash_num_elements(h_parameters) == 0) {
		*parameters = NULL;
		return SUCCESS;
	}
	
	*parameters = calloc(1, (zend_hash_num_elements(h_parameters) * (2 * sizeof(char *))) + 1);
	if (!*parameters) {
		php_error(E_WARNING, "Cannot allocate parameters array to pass to FastXSL");
		return FAILURE;
	}
	
	for (zend_hash_internal_pointer_reset_ex(h_parameters, &pos);
	     zend_hash_get_current_data_ex(h_parameters, (void **) &current, &pos) == SUCCESS;
		 zend_hash_move_forward_ex(h_parameters, &pos)) {
		if (zend_hash_get_current_key_ex(h_parameters, &key, &key_length, 
					&ival, 0, &pos) == HASH_KEY_IS_LONG) {
			efree(*parameters);
			*parameters = NULL;

			php_error(E_WARNING, 
					"Parameters array passed to %s() may not contain numeric keys", 
					get_active_function_name(TSRMLS_C));
			return FAILURE;
		}

		convert_to_string_ex(current);

		(*parameters)[index++] = key;
		(*parameters)[index++] = Z_STRVAL_PP(current);
	}
	(*parameters)[index] = NULL;

	return SUCCESS;
}

/* {{{ proto resource fastxsl_shmcache_transform(string filename, resource xmldoc[, array parameters])
   Transform a XML document, "xmldoc", by a XSL stylesheet "filename" with transform "parameters." */ 
PHP_FUNCTION(fastxsl_shmcache_transform)
{
	char            **parameters = NULL;
	php_xd_wrapper   *xd_wrapper;
	php_xd_wrapper   *result_wrapper;
	php_ss_wrapper   *ss_wrapper;
	char             *ss_filename;
	size_t            ss_filename_len;
	zval             *z_xd_wrapper;
	zval             *z_parameters;
	struct stat       sb;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz|z", &ss_filename, &ss_filename_len,
				&z_xd_wrapper, &z_parameters) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(xd_wrapper, php_xd_wrapper *, &z_xd_wrapper, -1, "FastXSL XML Document", 
			le_fastxsl_document);

	if (ZEND_NUM_ARGS() > 2 && Z_TYPE_P(z_parameters) == IS_ARRAY) {
		if (ParseTransformParameters(z_parameters, &parameters TSRMLS_CC) == FAILURE) {
			RETURN_FALSE;
		}
	}

	result_wrapper = XD_Wrapper_Alloc();

	mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RW);
	if (!FASTXSL_G(nostat)) {
		if (stat(ss_filename, &sb) == -1) {
			ShmCache_Stylesheet_Delete(ss_filename, ss_filename_len TSRMLS_CC);
			mm_unlock(FASTXSL_G(cache)->mm);
			free(parameters);
			RETURN_FALSE;
		}
	} else {
		sb.st_mtime = 0;
	}
	
	ss_wrapper = fl_hash_find(FASTXSL_G(cache)->table, ss_filename, ss_filename_len);
	if (!ss_wrapper) {
		ss_wrapper = ShmCache_Stylesheet_ParseAndStore(ss_filename, ss_filename_len, sb.st_mtime);
		if (!ss_wrapper) {
			mm_unlock(FASTXSL_G(cache)->mm);
			free(parameters);
			RETURN_FALSE;
		}
	} else {
		if (!FASTXSL_G(nostat)) {
			if (ss_wrapper->mtime != sb.st_mtime) {
				ShmCache_Stylesheet_Delete(ss_filename, ss_filename_len TSRMLS_CC);
				ss_wrapper = ShmCache_Stylesheet_ParseAndStore(ss_filename, ss_filename_len, sb.st_mtime TSRMLS_CC);
				if (!ss_wrapper) {
					mm_unlock(FASTXSL_G(cache)->mm);
					free(parameters);
					RETURN_FALSE;
				}
			}
		}
	}

	result_wrapper->xd = xsltApplyStylesheet(ss_wrapper->ss, xd_wrapper->xd, 
			(const char **) parameters);

	mm_unlock(FASTXSL_G(cache)->mm);
	
	if (parameters) 
		free(parameters);
	
	if (!result_wrapper->xd) {
		RETURN_FALSE;
	}	
	
	ZEND_REGISTER_RESOURCE(return_value, result_wrapper, le_fastxsl_document);
}
/* }}} */

/* {{{ proto resource fastxsl_prmcache_transform(string filename, resource xmldoc[, array parameters])
   Transform a XML document, "xmldoc", by a XSL stylesheet "filename" with transform "parameters." */
PHP_FUNCTION(fastxsl_prmcache_transform)
{
	char            **parameters = NULL;
	php_xd_wrapper   *xd_wrapper;
	php_xd_wrapper   *result_wrapper;
	php_ss_wrapper   *ss_wrapper;
	char             *ss_filename;
	size_t            ss_filename_len;
	zval             *z_xd_wrapper;
	zval             *z_parameters;
	struct stat       sb;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz|z", &ss_filename, &ss_filename_len,
				&z_xd_wrapper, &z_parameters) == FAILURE) {
		return;
	}
	ZEND_FETCH_RESOURCE(xd_wrapper, php_xd_wrapper *, &z_xd_wrapper, -1, "FastXSL XML Document", 
			le_fastxsl_document);

	if (ZEND_NUM_ARGS() > 2 && Z_TYPE_P(z_parameters) == IS_ARRAY) {
		if (ParseTransformParameters(z_parameters, &parameters TSRMLS_CC) == FAILURE) {
			RETURN_FALSE;
		}
	}

	result_wrapper = XD_Wrapper_Alloc();

	if (!FASTXSL_G(nostat)) {
		if (stat(ss_filename, &sb) == -1) {
			PrmCache_Stylesheet_Delete(ss_filename, ss_filename_len TSRMLS_CC);
			free(parameters);
			RETURN_FALSE;
		}
	} else {
		sb.st_mtime = 0;
	}
	
	ss_wrapper = fl_hash_find(FASTXSL_G(cache)->prmtable, ss_filename, ss_filename_len);
	if (!ss_wrapper) {
		ss_wrapper = PrmCache_Stylesheet_ParseAndStore(ss_filename, ss_filename_len, sb.st_mtime TSRMLS_CC);
		if (!ss_wrapper) {
			free(parameters);
			RETURN_FALSE;
		}
	} else {
		if (!FASTXSL_G(nostat)) {
			if (ss_wrapper->mtime != sb.st_mtime) {
				PrmCache_Stylesheet_Delete(ss_filename, ss_filename_len TSRMLS_CC);
				ss_wrapper = PrmCache_Stylesheet_ParseAndStore(ss_filename, ss_filename_len, sb.st_mtime TSRMLS_CC);
				if (!ss_wrapper) {
					free(parameters);
					RETURN_FALSE;
				}
			}
		}
	}

	result_wrapper->xd = xsltApplyStylesheet(ss_wrapper->ss, xd_wrapper->xd, 
			(const char **) parameters);

	if (parameters) 
		free(parameters);
	
	if (!result_wrapper->xd) {
		RETURN_FALSE;
	}	
	
	ZEND_REGISTER_RESOURCE(return_value, result_wrapper, le_fastxsl_document);
}

/* {{{ proto void fastxsl_nocache_transform(resource stylesheet, resource xmldoc[, array parameters])
   Transform the stylesheet document by the xml document with parameters. */
PHP_FUNCTION(fastxsl_nocache_transform)
{
	char           **parameters = NULL;
	php_xd_wrapper  *xd_wrapper;
	php_xd_wrapper  *result_wrapper;
	php_ss_wrapper  *ss_wrapper;
	zval            *z_xd_wrapper;
	zval            *z_ss_wrapper;
	zval            *z_parameters;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|z", &z_ss_wrapper, 
				&z_xd_wrapper, &z_parameters) == FAILURE) {
		return;
	}
	ZEND_FETCH_RESOURCE(ss_wrapper, php_ss_wrapper *, &z_ss_wrapper, -1, "FastXSL Stylesheet", 
			le_fastxsl_stylesheet);
	ZEND_FETCH_RESOURCE(xd_wrapper, php_xd_wrapper *, &z_xd_wrapper, -1, "FastXSL XML Document", 
			le_fastxsl_document);

	if (ZEND_NUM_ARGS() > 2 && Z_TYPE_P(z_parameters) == IS_ARRAY) {
		if (ParseTransformParameters(z_parameters, &parameters TSRMLS_CC) == FAILURE) {
			RETURN_FALSE;
		}
	}

	result_wrapper = XD_Wrapper_Alloc();
	
	result_wrapper->xd = xsltApplyStylesheet(ss_wrapper->ss, xd_wrapper->xd, 
			(const char **) parameters);
	if (parameters) 
		free(parameters);

	if (!result_wrapper->xd) {
		RETURN_FALSE;
	}	

	
	ZEND_REGISTER_RESOURCE(return_value, result_wrapper, le_fastxsl_document);
}
/* }}} */

/* {{{ proto void fastxsl_nocache_profile(resource stylesheet, resource xmldoc[, array parameters, string filename])
   Profile the stylesheet document by the xml document with parameters and output the results to filename (or stderr, if filename doesn't exist). */
PHP_FUNCTION(fastxsl_nocache_profile)
{
	char           **parameters = NULL;
	char            *filename = "php://stderr";
	php_xd_wrapper  *xd_wrapper;
	php_xd_wrapper  *result_wrapper;
	php_ss_wrapper  *ss_wrapper;
	zval            *z_xd_wrapper;
	zval            *z_ss_wrapper;
	zval            *z_parameters;
	FILE            *dbgprof;
	int              filename_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|zs", &z_ss_wrapper, 
				&z_xd_wrapper, &z_parameters, &filename, &filename_len) == FAILURE) {
		return;
	}
	ZEND_FETCH_RESOURCE(ss_wrapper, php_ss_wrapper *, &z_ss_wrapper, -1, "FastXSL Stylesheet", 
			le_fastxsl_stylesheet);
	ZEND_FETCH_RESOURCE(xd_wrapper, php_xd_wrapper *, &z_xd_wrapper, -1, "FastXSL XML Document", 
			le_fastxsl_document);

	if (ZEND_NUM_ARGS() > 2 && Z_TYPE_P(z_parameters) == IS_ARRAY) {
		if (ParseTransformParameters(z_parameters, &parameters TSRMLS_CC) == FAILURE) {
			RETURN_FALSE;
		}
	}

	if (!strcmp(filename, "php://stdout")) {
		dbgprof = stdout;
	} else if (!strcmp(filename, "php://stderr")) {
		dbgprof = stderr;
	} else {
		dbgprof = fopen(filename, "w");
	}

	result_wrapper = XD_Wrapper_Alloc();

	result_wrapper->xd = xsltProfileStylesheet(ss_wrapper->ss, xd_wrapper->xd, 
			(const char **) parameters, dbgprof);
	
	if (parameters) 
		free(parameters);

	fclose(dbgprof);
	
	if (!result_wrapper->xd) {
		RETURN_FALSE;
	}	
	
	ZEND_REGISTER_RESOURCE(return_value, result_wrapper, le_fastxsl_document);
}
/* }}} */

/* {{{ proto string fastxsl_nocache_tostring(resource stylesheet, resource xmldoc)
   Return the contents of an XML stylesheet result as a string */
PHP_FUNCTION(fastxsl_nocache_tostring)
{
	zval           *z_xd_wrapper;
	zval           *z_ss_wrapper;
	php_ss_wrapper *ss_wrapper;
	php_xd_wrapper *xd_wrapper;
	xmlChar        *result = NULL;
	int             length;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &z_ss_wrapper, 
				&z_xd_wrapper) == FAILURE) {
		return;
	}
	ZEND_FETCH_RESOURCE(ss_wrapper, php_ss_wrapper *, &z_ss_wrapper, -1, "FastXSL XML Stylesheet", 
			le_fastxsl_stylesheet);
	ZEND_FETCH_RESOURCE(xd_wrapper, php_xd_wrapper *, &z_xd_wrapper, -1, "FastXSL XML Document", 
			le_fastxsl_document);

	xsltSaveResultToString(&result, &length, xd_wrapper->xd, ss_wrapper->ss);

	if (result) {
		RETVAL_STRINGL((char *) result, length, 1);
		xmlFree(result);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string fastxsl_shmcache_tostring(string filename, resource xmldoc)
   Return the string representation of xmldoc which is the result of an XSLT transformation on filename */
PHP_FUNCTION(fastxsl_shmcache_tostring)
{
	zval           *z_xd_wrapper;
	php_ss_wrapper *ss_wrapper;
	php_xd_wrapper *xd_wrapper;
	xmlChar        *result = NULL;
	char           *ss_filename;
	size_t          ss_filename_len;
	int             length;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &ss_filename, &ss_filename_len,
				&z_xd_wrapper) == FAILURE) {
		return;
	}
	ZEND_FETCH_RESOURCE(xd_wrapper, php_xd_wrapper *, &z_xd_wrapper, -1, "FastXSL XML Document", 
			le_fastxsl_document);

	mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RD);
	ss_wrapper = fl_hash_find(FASTXSL_G(cache)->table, ss_filename, ss_filename_len);
	if (!ss_wrapper) {
		mm_unlock(FASTXSL_G(cache)->mm);
		RETURN_FALSE;
	}
	xsltSaveResultToString(&result, &length, xd_wrapper->xd, ss_wrapper->ss);
	mm_unlock(FASTXSL_G(cache)->mm);
	if (result) {
		RETVAL_STRINGL((char *) result, length, 1);
		xmlFree(result);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string fastxsl_prmcache_tostring(string filename, resource xmldoc)
   Return the string representation of xmldoc which is the result of an XSLT transformation on filename */
PHP_FUNCTION(fastxsl_prmcache_tostring)
{
	zval           *z_xd_wrapper;
	php_ss_wrapper *ss_wrapper;
	php_xd_wrapper *xd_wrapper;
	xmlChar        *result = NULL;
	char           *ss_filename;
	size_t          ss_filename_len;
	int             length;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &ss_filename, &ss_filename_len,
				&z_xd_wrapper) == FAILURE) {
		return;
	}
	ZEND_FETCH_RESOURCE(xd_wrapper, php_xd_wrapper *, &z_xd_wrapper, -1, "FastXSL XML Document", 
			le_fastxsl_document);

	ss_wrapper = fl_hash_find(FASTXSL_G(cache)->prmtable, ss_filename, ss_filename_len);
	if (!ss_wrapper) {
		RETURN_FALSE;
	}
	xsltSaveResultToString(&result, &length, xd_wrapper->xd, ss_wrapper->ss);
	
	if (result) {
		RETVAL_STRINGL((char *) result, length, 1);
		xmlFree(result);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

function_entry fastxsl_functions[] = {
	PHP_FE(fastxsl_stylesheet_parsefile,         NULL)
	PHP_FE(fastxsl_xml_parsefile,                NULL)
	PHP_FE(fastxsl_xml_parsestring,              NULL)
	PHP_FE(fastxsl_shmcache_transform,           NULL)
	PHP_FE(fastxsl_prmcache_transform,           NULL)
	PHP_FE(fastxsl_nocache_transform,            NULL)
	PHP_FE(fastxsl_nocache_profile,              NULL)
	PHP_FE(fastxsl_nocache_tostring,             NULL)
	PHP_FE(fastxsl_shmcache_tostring,            NULL)
	PHP_FE(fastxsl_prmcache_tostring,            NULL)
	PHP_FE(fastxsl_shmcache_getstatistics,       NULL)
	PHP_FE(fastxsl_prmcache_getstatistics,       NULL)
	{NULL, NULL, NULL}
};


zend_module_entry fastxsl_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"fastxsl",
	fastxsl_functions,
	PHP_MINIT(fastxsl),
	NULL,
	NULL,	
	NULL,
	PHP_MINFO(fastxsl),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1",
#endif
	STANDARD_MODULE_PROPERTIES
};


#ifdef COMPILE_DL_FASTXSL
ZEND_GET_MODULE(fastxsl)
#endif

static void
SS_Wrapper_Dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	php_ss_wrapper *wrapper = (php_ss_wrapper *) rsrc->ptr;
	
	if (wrapper->persistant) {
		return;
	}
	
	if (wrapper->ss)
		xsltFreeStylesheet(wrapper->ss);

	free(wrapper);
}

static void
XD_Wrapper_Dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	php_xd_wrapper *wrapper = (php_xd_wrapper *) rsrc->ptr;
	
	if (wrapper->xd)
		xmlFreeDoc(wrapper->xd);
	free(wrapper);
}

static int 
Stream_MatchWrapper(const char *filename)
{
	TSRMLS_FETCH();
	return php_stream_locate_url_wrapper(filename, NULL, STREAM_LOCATE_WRAPPERS_ONLY TSRMLS_CC) ? 1 : 0;
}

static void *
Stream_XmlWrite_OpenWrapper(const char *filename)
{
	TSRMLS_FETCH();
	return php_stream_open_wrapper((char *) filename, "wb", ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
}

static int
Stream_XmlWrite_WriteWrapper(void *context, const char *buffer, int length)
{
	TSRMLS_FETCH();
	return php_stream_write((php_stream *) context, (char *) buffer, length);
}

static int
Stream_CloseWrapper(void *context)
{
	TSRMLS_FETCH();
	return php_stream_close((php_stream *) context);
}


extern int xmlLoadExtDtdDefaultValue;

/**
 * Allocators for the fl_hash storage 
 */
static FL_Allocator mm_allocators = {ShmCache_Free, ShmCache_Malloc, ShmCache_Calloc};
static FL_Allocator normal_allocators = {free, malloc, calloc};

static void
php_fastxsl_init_globals(zend_fastxsl_globals *globals)
{
	memset(globals, 0, sizeof(zend_fastxsl_globals));
	
	globals->cache = calloc(1, sizeof(fl_cache));
}


static void
php_fastxsl_destroy_globals(zend_fastxsl_globals *globals)
{
	fl_cache *cache;
	
	cache = globals->cache;
	if (cache) {
		if (cache->owner != getpid()) {
			return;
		}

		mm_lock(cache->mm, MM_LOCK_RW);
		fl_hash_free(cache->table);
		mm_unlock(cache->mm);

		mm_destroy(cache->mm);

		fl_hash_free(cache->prmtable);
	}
}

PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("fastxsl.shmpath", "/tmp/fastxsl_mem", PHP_INI_SYSTEM, OnUpdateString, shmpath, zend_fastxsl_globals, fastxsl_globals)
	STD_PHP_INI_BOOLEAN("fastxsl.nostat", "0", PHP_INI_ALL, OnUpdateInt, nostat, zend_fastxsl_globals, fastxsl_globals) 
PHP_INI_END()

PHP_MINIT_FUNCTION(fastxsl)
{
	char   *shmpath;
	size_t  shmpath_len;
	char    euid[30];

	ZEND_INIT_MODULE_GLOBALS(fastxsl, php_fastxsl_init_globals, php_fastxsl_destroy_globals);

	REGISTER_INI_ENTRIES();
	
	le_fastxsl_stylesheet = zend_register_list_destructors_ex(SS_Wrapper_Dtor, NULL, 
			le_fastxsl_stylesheet_name, module_number);
	le_fastxsl_document   = zend_register_list_destructors_ex(XD_Wrapper_Dtor, NULL, 
			le_fastxsl_document_name,   module_number);

	xsltRegisterAllExtras();
	xmlSubstituteEntitiesDefault(1);
	xmlLoadExtDtdDefaultValue = 1;

	xmlMemGet(&free_ptr, &malloc_ptr, &realloc_ptr, &strdup_ptr);
	xmlRegisterOutputCallbacks(Stream_MatchWrapper, Stream_XmlWrite_OpenWrapper, 
			                   Stream_XmlWrite_WriteWrapper, Stream_CloseWrapper);
	

	if (!sprintf(euid, "%d", geteuid())) {
		return FAILURE;
	}

	shmpath_len = strlen(FASTXSL_G(shmpath)) + strlen(euid);
	shmpath = do_alloca(shmpath_len + 1);

	strcpy(shmpath, FASTXSL_G(shmpath));
	strcat(shmpath, euid);
	
	FASTXSL_G(cache)->owner = getpid();
	FASTXSL_G(cache)->mm = mm_create(0, shmpath);

	free_alloca(shmpath);
	if (!FASTXSL_G(cache)->mm) {
		return FAILURE;
	}

	mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RW);
	FASTXSL_G(cache)->table = fl_hash_new(&mm_allocators, NULL);
	mm_unlock(FASTXSL_G(cache)->mm);
	FASTXSL_G(cache)->prmtable = fl_hash_new(&normal_allocators, NULL);

	return SUCCESS;
}

PHP_MINFO_FUNCTION(fastxsl)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "fastxsl support", "enabled");
	php_info_print_table_end();

}
