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
  |         George Schlossnagle <george@omniti.com>                      |
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
#include <libxml/xpathInternals.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <libxslt/extensions.h>
#include <libxslt/functions.h>

#ifdef FASTXSL_MM
#include <mm.h>
#endif
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>


#include "fl_hash.h"
#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_fastxsl.h"

ZEND_DECLARE_MODULE_GLOBALS(fastxsl)

static int le_fastxsl_stylesheet;
#define le_fastxsl_stylesheet_name "FastXSL Stylesheet"
static int le_fastxsl_document;
#define le_fastxsl_document_name "FastXSL Document"

#define FASTXSL_PRM_ALLOC 1
#define FASTXSL_SHARED_ALLOC 2

void fastxsl_errorfunc(void *ctx, const char *msg, ...);
static void ShmCache_Stylesheet_Free(php_ss_wrapper *wrapper TSRMLS_DC);
static void _SS_Wrapper_Dtor(php_ss_wrapper *wrapper);
static void _XD_Wrapper_Dtor(php_xd_wrapper *wrapper);

extern void HACK_xsltDocumentFunction(xmlXPathParserContextPtr ctxt, int nargs);

static php_ss_wrapper *
SS_Wrapper_Alloc(int shared TSRMLS_DC)
{
	php_ss_wrapper *wrapper;

	if (shared) {
#ifdef FASTXSL_MM
		if (shared == FASTXSL_SHARED_ALLOC) {
			wrapper = (php_ss_wrapper *) mm_calloc(FASTXSL_G(cache)->mm, 1, sizeof(php_ss_wrapper));
		} else {
			wrapper = (php_ss_wrapper *) calloc(1, sizeof(php_ss_wrapper));
		}
		if(!wrapper) {
			mm_display_info(FASTXSL_G(cache)->mm);
		}
		wrapper->persistant = 1;
#else
		wrapper = (php_ss_wrapper *) calloc(1, sizeof(php_ss_wrapper));
		wrapper->persistant = 1;
#endif
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
static xmlMallocFunc  mallocatomic_ptr;
static xmlReallocFunc realloc_ptr;
static xmlStrdupFunc  strdup_ptr;

#ifdef FASTXSL_MM
void
ShmCache_Free(void *ptr)
{
	TSRMLS_FETCH();
	FASTXSL_G(tmp_allocated_size) -= mm_sizeof(FASTXSL_G(cache)->mm, ptr);
	mm_free(FASTXSL_G(cache)->mm, ptr);
}

void *
ShmCache_Malloc(size_t size)
{
	void *ptr;
	TSRMLS_FETCH();
	ptr = mm_malloc(FASTXSL_G(cache)->mm, size);
	if (!ptr) {
		php_error(E_ERROR, "Ran out of Shared memory to allocate data for FastXSL cache, "
				           "in function %s() cannot allocate %ld bytes (%ld available, %ld allocated)", 
						   get_active_function_name(TSRMLS_C), size, mm_available(FASTXSL_G(cache)->mm), 
						   mm_maxsize() - mm_available(FASTXSL_G(cache)->mm));
		return NULL;
	}

	FASTXSL_G(tmp_allocated_size) += size;
	return ptr;
}

void *
ShmCache_Calloc(size_t nmemb, size_t size) 
{
	void *ptr;

	ptr = ShmCache_Malloc(nmemb * size);
	memset(ptr, 0, nmemb * size);
	return ptr;
}

void *
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

char *
ShmCache_Strdup(const char *string)
{
	char *newstring;
	int   string_length;

	string_length = strlen(string);
	newstring = ShmCache_Malloc(string_length + 1);
	memcpy(newstring, string, string_length);
	newstring[string_length] = 0;

	return newstring;
}
#endif

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

#ifdef FASTXSL_MM
static void
ShmCache_UseAllocationFunctions(void)
{
	xmlMemSetup(ShmCache_Free, ShmCache_Malloc, ShmCache_Realloc, ShmCache_Strdup);
}
#endif

static void
Php_UseAllocationFunctions(void)
{
	xmlMemSetup(Php_Free, Php_Malloc, Php_Realloc, Php_Strdup);
}

static void
Xml_UseAllocationFunctions(void)
{
	xmlMemSetup(free_ptr, malloc_ptr,  realloc_ptr, strdup);
}

#ifdef FASTXSL_MM
static php_ss_wrapper *
ShmCache_Stylesheet_ParseAndStore(char *filename, size_t filename_len, int mtime TSRMLS_DC)
{
	php_ss_wrapper *wrapper;
	int rv;
	wrapper = SS_Wrapper_Alloc(FASTXSL_SHARED_ALLOC TSRMLS_CC);

	ShmCache_UseAllocationFunctions();
	wrapper->alloc_type = FASTXSL_SHMALLOC;
	FASTXSL_G(tmp_allocated_size) = 0;
	zend_set_timeout(0);
	wrapper->data.ss = xsltParseStylesheetFile((unsigned char *)filename);
	wrapper->data_type = FASTXSL_STYLESHEET;
	Xml_UseAllocationFunctions();
	if (!wrapper->data.ss) {
		_SS_Wrapper_Dtor(wrapper);
		return NULL;
	}
	wrapper->mtime = mtime;
	wrapper->allocsize = FASTXSL_G(tmp_allocated_size);
	mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RD);
	rv = fl_hash_add(FASTXSL_G(cache)->table, filename, filename_len, wrapper);
	mm_unlock(FASTXSL_G(cache)->mm);
	if(rv == 0) {
		/* we failed */
		php_ss_wrapper *fallback;
		mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RD);
		fallback = fl_hash_find(FASTXSL_G(cache)->table, filename, 
					filename_len);
		mm_unlock(FASTXSL_G(cache)->mm);
		if(fallback && fallback->data_type == FASTXSL_STYLESHEET) {
			ShmCache_Stylesheet_Free(wrapper);
			wrapper = fallback;
		} else {
		}
	} else {
	}

	return wrapper;
}

static void
ShmCache_Stylesheet_Free(php_ss_wrapper *wrapper TSRMLS_DC)
{
	if (wrapper->data_type == FASTXSL_STYLESHEET) {
		if (wrapper->data.ss) {
			ShmCache_UseAllocationFunctions();
			xsltFreeStylesheet(wrapper->data.ss);
			Xml_UseAllocationFunctions();
		}
		mm_free(FASTXSL_G(cache)->mm, wrapper);
	}
}

static void
ShmCache_XPathObject_Free(php_ss_wrapper *wrapper TSRMLS_DC)
{
	if (wrapper->data_type == FASTXSL_XPATHOBJ) {
		if (wrapper->data.ss) {
			ShmCache_UseAllocationFunctions();
			xmlXPathFreeObject(wrapper->data.op);
			Xml_UseAllocationFunctions();
		}
		mm_free(FASTXSL_G(cache)->mm, wrapper);
	}
}

static void
ShmCache_Document_Delete(char *filename, size_t filename_len)
{
	php_ss_wrapper *wrapper;

	mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RD);
	wrapper = fl_hash_find(FASTXSL_G(cache)->table, filename, filename_len);
	mm_unlock(FASTXSL_G(cache)->mm);
	if (wrapper) {
		mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RD);
		fl_hash_delete(FASTXSL_G(cache)->table, filename, filename_len);
		mm_unlock(FASTXSL_G(cache)->mm);
		switch(wrapper->data_type) {
			case FASTXSL_STYLESHEET:
				ShmCache_Stylesheet_Free(wrapper);
				break;
			case FASTXSL_XPATHOBJ:
				ShmCache_XPathObject_Free(wrapper);
				break;
			default:
				break;
		}
	}
}
#endif

#ifdef FASTXSL_MM
/* {{{ proto void fastxsl_CachedDocumentFunction(xmlXPathParserContextPtr ctxt, int nargs)
   Emulate xsltDocumentFunction but leverage the MM shared cache for speed. */
void
fastxsl_CachedDocumentFunction(xmlXPathParserContextPtr ctxt, int nargs)
{
	xmlXPathFunction func;
	xmlXPathObjectPtr idoc, obj;
	xsltTransformContextPtr tctxt;
	xmlChar *base = NULL, *URI;
	char *ss_filename;
	int ss_filename_len;
	struct stat sb;
	php_ss_wrapper *ss_wrapper;

	xmlXPathStringFunction(ctxt, 1);
	if (ctxt->value->type != XPATH_STRING) {
		xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
						 "document() : invalid arg expecting a string\n");
		goto error;
	}
	obj = ctxt->value;

	/* Translate this document as would xsltLoadDocument */
	tctxt = xsltXPathGetTransformContext(ctxt);
	if ((tctxt != NULL) && (tctxt->inst != NULL)) {
		base = xmlNodeGetBase(tctxt->inst->doc, tctxt->inst);
	}
	else {
		base = xmlNodeGetBase(tctxt->style->doc,
					(xmlNodePtr) tctxt->style->doc);
	}
	URI = xmlBuildURI(obj->stringval, base);
	if (base != NULL)               
		 xmlFree(base);

	ss_filename_len = strlen((char *)URI);
	ss_filename = alloca(ss_filename_len + 1);
	strcpy(ss_filename, (char *)URI);

	sb.st_mtime = 0;

	mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RW);
	ss_wrapper = fl_hash_find(FASTXSL_G(cache)->table, ss_filename, ss_filename_len);
	if(ss_wrapper) ss_wrapper->hits++;
	mm_unlock(FASTXSL_G(cache)->mm);

	if (!ss_wrapper) {
		int lockfd;
		lockfd = open(ss_filename, O_RDONLY);
		if(lockfd < 0) goto error;

                /* It would be really bad if we longjmp'd out with the lock */
		zend_set_timeout(0);
		/* Lock, but we may not be the only one... */
        	ACQUIRE(lockfd);
		/* We have the lock, lets see if someone beat us here */
		mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RW);
		ss_wrapper = fl_hash_find(FASTXSL_G(cache)->table, ss_filename, ss_filename_len);
		if(ss_wrapper) ss_wrapper->hits++;
		mm_unlock(FASTXSL_G(cache)->mm);

		if(ss_wrapper) {
			/* Someone beat us */
		}
		else {
	        	ss_wrapper = SS_Wrapper_Alloc(FASTXSL_SHARED_ALLOC TSRMLS_CC);
        		if(ss_wrapper) {
				int rv;
				if (!FASTXSL_G(nostat)) {
					if (fstat(lockfd, &sb) == -1) {
						ShmCache_UseAllocationFunctions();
						ShmCache_Document_Delete(ss_filename, ss_filename_len TSRMLS_CC);
						Xml_UseAllocationFunctions();
						RELEASE(lockfd);
						close(lockfd);
						goto error;
					}
				}
				ss_wrapper->alloc_type = FASTXSL_SHMALLOC;
		 		ss_wrapper->data_type = FASTXSL_XPATHOBJ;
				ShmCache_UseAllocationFunctions();
				FASTXSL_G(tmp_allocated_size) = 0;
				valuePush(ctxt, xmlXPathObjectCopy(ctxt->value));
				HACK_xsltDocumentFunction(ctxt, nargs);
				ss_wrapper->data.op = xmlXPathObjectCopy(ctxt->value);
				valuePop(ctxt);
				ss_wrapper->allocsize = FASTXSL_G(tmp_allocated_size);
				Xml_UseAllocationFunctions();
				ss_wrapper->mtime = sb.st_mtime;
				mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RD);
				rv = fl_hash_add(FASTXSL_G(cache)->table, ss_filename, ss_filename_len, ss_wrapper);
				mm_unlock(FASTXSL_G(cache)->mm);
				if(rv == 0) {
					/* we failed */
					php_ss_wrapper *fallback;
					mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RD);
					fallback = fl_hash_find(FASTXSL_G(cache)->table, ss_filename, ss_filename_len);
					mm_unlock(FASTXSL_G(cache)->mm);
					if(fallback && fallback->data_type == FASTXSL_XPATHOBJ) {
						ShmCache_UseAllocationFunctions();
						ShmCache_XPathObject_Free(ss_wrapper);
						Xml_UseAllocationFunctions();
						ss_wrapper = fallback;
					} else {
					}
				} else {
				}
			}
		}
		RELEASE(lockfd);
		close(lockfd);
		if (!ss_wrapper) {
			Xml_UseAllocationFunctions();
			goto error;
		}
	}
	else {
		if (!FASTXSL_G(nostat)) {
			int lockfd;
			lockfd = open(ss_filename, O_RDONLY);
			if(lockfd < 0) goto error;

			if (fstat(lockfd, &sb) == -1) {
				ShmCache_UseAllocationFunctions();
				ShmCache_Document_Delete(ss_filename, ss_filename_len TSRMLS_CC);
				Xml_UseAllocationFunctions();
				close(lockfd);
				goto error;
			}
			if (ss_wrapper->mtime != sb.st_mtime) {
                		/* It would be really bad if we longjmp'd out with the lock */
				zend_set_timeout(0);
				ACQUIRE(lockfd);
				ShmCache_UseAllocationFunctions();
				ShmCache_Document_Delete(ss_filename, ss_filename_len TSRMLS_CC);
				Xml_UseAllocationFunctions();
	        		ss_wrapper = SS_Wrapper_Alloc(FASTXSL_SHARED_ALLOC TSRMLS_CC);
        			if(ss_wrapper) {
					int rv;
					ss_wrapper->alloc_type = FASTXSL_SHMALLOC;
        				ss_wrapper->data_type = FASTXSL_STYLESHEET;
					ShmCache_UseAllocationFunctions();
					FASTXSL_G(tmp_allocated_size) = 0;
					valuePush(ctxt, xmlXPathObjectCopy(ctxt->value));
					HACK_xsltDocumentFunction(ctxt, nargs);
					ss_wrapper->data.op = xmlXPathObjectCopy(ctxt->value);
					valuePop(ctxt);
					ss_wrapper->allocsize = FASTXSL_G(tmp_allocated_size);
					Xml_UseAllocationFunctions();
					ss_wrapper->mtime = sb.st_mtime;
					mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RD);
					rv = fl_hash_add(FASTXSL_G(cache)->table, ss_filename, ss_filename_len, ss_wrapper);
					mm_unlock(FASTXSL_G(cache)->mm);
					if(rv == 0) {
						/* we failed */
						php_ss_wrapper *fallback;
						mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RD);
						fallback = fl_hash_find(FASTXSL_G(cache)->table, ss_filename, ss_filename_len);
						mm_unlock(FASTXSL_G(cache)->mm);
						if(fallback && fallback->data_type == FASTXSL_XPATHOBJ) {
							ShmCache_UseAllocationFunctions();
							ShmCache_XPathObject_Free(ss_wrapper);
							Xml_UseAllocationFunctions();
							ss_wrapper = fallback;
						} else {
						}
					} else {
					}
				}
				RELEASE(lockfd);
				close(lockfd);
				if (!ss_wrapper) {
					goto error;
				}
			}
		}
	}
	Xml_UseAllocationFunctions();

	valuePop(ctxt);
	valuePush(ctxt, xmlXPathObjectCopy(ss_wrapper->data.op));
	return;
error:
	ctxt->error = XPATH_INVALID_TYPE;
	return;
}
/* }}} */
#endif

static php_ss_wrapper *
PrmCache_Stylesheet_ParseAndStore(char *filename, size_t filename_len, int mtime TSRMLS_DC)
{
	php_ss_wrapper *wrapper;

	wrapper = (php_ss_wrapper *) SS_Wrapper_Alloc(FASTXSL_PRM_ALLOC TSRMLS_CC);
	wrapper->alloc_type = FASTXSL_PRMALLOC;

	wrapper->data.ss = xsltParseStylesheetFile((xmlChar *)filename);
	wrapper->data_type = FASTXSL_STYLESHEET;
	if (!wrapper->data.ss) {
		return NULL;
	}
	wrapper->mtime = mtime;
	
	if(fl_hash_add(FASTXSL_G(cache)->prmtable, filename, filename_len, wrapper) == 0) {
		/* we failed */
		php_ss_wrapper *fallback;
		mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RD);
		fallback = fl_hash_find(FASTXSL_G(cache)->table, filename, filename_len);
		mm_unlock(FASTXSL_G(cache)->mm);
		if(fallback) {
			ShmCache_Stylesheet_Free(wrapper);
			wrapper = fallback;
		}
	}

	return wrapper;
}

static void
PrmCache_Stylesheet_Free(php_ss_wrapper *wrapper TSRMLS_DC)
{
	if (wrapper->data_type == FASTXSL_STYLESHEET) {
		if (wrapper->data.ss) {
			xsltFreeStylesheet(wrapper->data.ss);
		}
		free(wrapper);
	}
}

static void
PrmCache_XPathObject_Free(php_ss_wrapper *wrapper TSRMLS_DC)
{
	if (wrapper->data_type == FASTXSL_XPATHOBJ) {
		if (wrapper->data.op) {
			xmlXPathFreeObject(wrapper->data.op);
		}
		free(wrapper);
	}
}

static void
PrmCache_Document_Delete(char *filename, size_t filename_len TSRMLS_DC)
{
	php_ss_wrapper *wrapper;

	wrapper = fl_hash_find(FASTXSL_G(cache)->prmtable, filename, filename_len);
	if (wrapper) {
		fl_hash_delete(FASTXSL_G(cache)->prmtable, filename, filename_len);
		switch(wrapper->data_type) {
			case FASTXSL_STYLESHEET:
				PrmCache_Stylesheet_Free(wrapper TSRMLS_CC);
				break;
			case FASTXSL_XPATHOBJ:
				PrmCache_XPathObject_Free(wrapper TSRMLS_CC);
				break;
			default:
				break;
		}
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
#ifdef FASTXSL_MM
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
			add_assoc_long(info_array, "hits", ss_wrapper->hits);
			add_assoc_long(info_array, "mtime", ss_wrapper->mtime);

			add_assoc_zval(files_array, bucket->key, info_array);

			allocated_bytes += ss_wrapper->allocsize;
		}
	}
	add_assoc_zval(return_value, "files", files_array);

	add_assoc_long(return_value, "apparent_allocated", allocated_bytes);
	add_assoc_long(return_value, "real_allocated", mm_maxsize() - mm_available(FASTXSL_G(cache)->mm));
	add_assoc_long(return_value, "shm_maxsize", (long) mm_maxsize());
	mm_unlock(FASTXSL_G(cache)->mm);
}

/* {{{ proto array fastxsl_version(void)
   Returns the version number */
PHP_FUNCTION(fastxsl_version)
{
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	RETURN_STRING(PHP_FASTXSL_VERSION, 1);
}

PHP_FUNCTION(fastxsl_xmlMemoryDump)
{
  chdir("/tmp");
  xmlMemDisplay(stderr);
}
#endif
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

	wrapper = (php_ss_wrapper *) SS_Wrapper_Alloc(0 TSRMLS_CC);
	wrapper->alloc_type = FASTXSL_PRMALLOC;
	Xml_UseAllocationFunctions();
	wrapper->data.ss = xsltParseStylesheetFile((xmlChar*)filename);
	if (!wrapper->data.ss) {
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
	wrapper->alloc_type = FASTXSL_PRMALLOC;
	Xml_UseAllocationFunctions();
	wrapper->xd = xmlParseDoc((unsigned char *)text);
	if (!wrapper->xd) {
		_XD_Wrapper_Dtor(wrapper);
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
	wrapper->alloc_type = FASTXSL_PRMALLOC;
	wrapper->xd = xmlParseFile((const char *) filename);
	if (!wrapper->xd) {
		_XD_Wrapper_Dtor(wrapper);
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
	unsigned int   key_length;
	unsigned long  ival;
	int            index = 0;

	h_parameters = Z_ARRVAL_P(z_parameters);

	if (zend_hash_num_elements(h_parameters) == 0) {
		*parameters = NULL;
		return SUCCESS;
	}
	
	*parameters = calloc(1, (1 + zend_hash_num_elements(h_parameters) * 2) * sizeof(char *));
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
#ifdef FASTXSL_MM
PHP_FUNCTION(fastxsl_shmcache_transform)
{
	xsltTransformContextPtr ctxt;
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
	result_wrapper->alloc_type = FASTXSL_PRMALLOC;

	sb.st_mtime = 0;

	mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RW);
	ss_wrapper = fl_hash_find(FASTXSL_G(cache)->table, ss_filename, ss_filename_len);
	if(ss_wrapper) ss_wrapper->hits++;
	mm_unlock(FASTXSL_G(cache)->mm);

	if (!ss_wrapper) {
		int lockfd;
		lockfd = open(ss_filename, O_RDONLY);
		if(lockfd < 0) RETURN_FALSE;

                /* It would be really bad if we longjmp'd out with the lock */
		zend_set_timeout(0);
		/* Create a wait queue -- no herds */
		ACQUIRE(lockfd);

		/* One last check */
		mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RW);
		ss_wrapper = fl_hash_find(FASTXSL_G(cache)->table, ss_filename, ss_filename_len);
		if(ss_wrapper) ss_wrapper->hits++;
		mm_unlock(FASTXSL_G(cache)->mm);

		if(ss_wrapper) {
			/* Someone beat us to it */
		}
		else {
			if (fstat(lockfd, &sb) != -1) {
				ShmCache_UseAllocationFunctions();
				ss_wrapper = ShmCache_Stylesheet_ParseAndStore(ss_filename, ss_filename_len, sb.st_mtime);
				Xml_UseAllocationFunctions();
			}
			if (!ss_wrapper) {
				RELEASE(lockfd);
				close(lockfd);
				goto error;
			}
		}
		RELEASE(lockfd);
		close(lockfd);
	} else {
		if (!FASTXSL_G(nostat)) {
			int lockfd;
			lockfd = open(ss_filename, O_RDONLY);
			if(lockfd < 0) RETURN_FALSE;
			
			if (fstat(lockfd, &sb) == -1) {
				ShmCache_UseAllocationFunctions();
				ShmCache_Document_Delete(ss_filename, ss_filename_len TSRMLS_CC);
				Xml_UseAllocationFunctions();
				close(lockfd);
				goto error;
			}

                	/* It would be really bad if we longjmp'd out with the lock */
			zend_set_timeout(0);
			ACQUIRE(lockfd);
			if (ss_wrapper->mtime != sb.st_mtime) {
				ShmCache_UseAllocationFunctions();
				ShmCache_Document_Delete(ss_filename, ss_filename_len TSRMLS_CC);
				ss_wrapper = ShmCache_Stylesheet_ParseAndStore(ss_filename, ss_filename_len, sb.st_mtime TSRMLS_CC);
				Xml_UseAllocationFunctions();
				if (!ss_wrapper) {
					RELEASE(lockfd);
					close(lockfd);
					goto error;
				}
			}
			RELEASE(lockfd);
			close(lockfd);
		}
	}

	ctxt = xsltNewTransformContext(ss_wrapper->data.ss, xd_wrapper->xd);
#ifdef FASTXSL_MM
	if(FASTXSL_G(replace_document_function)) {
		xmlXPathFunction func;
		func = xmlXPathFunctionLookup(ctxt->xpathCtxt, (const xmlChar *)"document");
		xmlXPathRegisterFunc(ctxt->xpathCtxt, (const xmlChar *) "original_document", func);
		xmlXPathRegisterFunc(ctxt->xpathCtxt, (const xmlChar *) "document", NULL);
		xmlXPathRegisterFunc(ctxt->xpathCtxt, (const xmlChar *) "document",
					fastxsl_CachedDocumentFunction);
	}
	xmlXPathRegisterFunc(ctxt->xpathCtxt, (const xmlChar *) "cached_document",
				fastxsl_CachedDocumentFunction);
#endif
	result_wrapper->xd = xsltApplyStylesheetUser(ss_wrapper->data.ss, xd_wrapper->xd, 
			(const char **) parameters, NULL, NULL, ctxt);
	xsltFreeTransformContext(ctxt);
	
	if (parameters) 
		free(parameters);
	
	if (!result_wrapper->xd) {
		RETURN_FALSE;
	}	
	
	ZEND_REGISTER_RESOURCE(return_value, result_wrapper, le_fastxsl_document);
	return;

	error:
		Xml_UseAllocationFunctions();
		if (parameters) free(parameters);
		_XD_Wrapper_Dtor(result_wrapper);
		RETURN_FALSE;
}
#endif
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
	result_wrapper->alloc_type = FASTXSL_PRMALLOC;

	if (!FASTXSL_G(nostat)) {
		if (stat(ss_filename, &sb) == -1) {
			PrmCache_Document_Delete(ss_filename, ss_filename_len TSRMLS_CC);
			_XD_Wrapper_Dtor(result_wrapper);
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
			_XD_Wrapper_Dtor(result_wrapper);
			free(parameters);
			RETURN_FALSE;
		}
	} else {
		if (!FASTXSL_G(nostat)) {
			if (ss_wrapper->mtime != sb.st_mtime) {
				PrmCache_Document_Delete(ss_filename, ss_filename_len TSRMLS_CC);
				ss_wrapper = PrmCache_Stylesheet_ParseAndStore(ss_filename, ss_filename_len, sb.st_mtime TSRMLS_CC);
				if (!ss_wrapper) {
					_XD_Wrapper_Dtor(result_wrapper);
					free(parameters);
					RETURN_FALSE;
				}
			}
		}
	}
	ss_wrapper->hits++;
	result_wrapper->xd = xsltApplyStylesheet(ss_wrapper->data.ss, xd_wrapper->xd, 
			(const char **) parameters);
	////xmlCleanupParserr();

	if (parameters) 
		free(parameters);
	
	if (!result_wrapper->xd) {
		_XD_Wrapper_Dtor(result_wrapper);
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
	result_wrapper->alloc_type = FASTXSL_PRMALLOC;
	result_wrapper->xd = xsltApplyStylesheet(ss_wrapper->data.ss, xd_wrapper->xd, 
			(const char **) parameters);
	if (parameters) 
		free(parameters);

	if (!result_wrapper->xd) {
		_XD_Wrapper_Dtor(result_wrapper);
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
	result_wrapper->alloc_type = FASTXSL_PRMALLOC;

	result_wrapper->xd = xsltProfileStylesheet(ss_wrapper->data.ss, xd_wrapper->xd, 
			(const char **) parameters, dbgprof);
	if (parameters) 
		free(parameters);

	fclose(dbgprof);
	
	if (!result_wrapper->xd) {
		_XD_Wrapper_Dtor(result_wrapper);
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

	xsltSaveResultToString(&result, &length, xd_wrapper->xd, ss_wrapper->data.ss);

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
#ifdef FASTXSL_MM
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
	Xml_UseAllocationFunctions();
	mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RD);
	ss_wrapper = fl_hash_find(FASTXSL_G(cache)->table, ss_filename, ss_filename_len);
	mm_unlock(FASTXSL_G(cache)->mm);
	if (!ss_wrapper) {
		RETURN_FALSE;
	}
	ss_wrapper->hits++;
	xsltSaveResultToString(&result, &length, xd_wrapper->xd, ss_wrapper->data.ss);
	if (result) {
		RETVAL_STRINGL((char *) result, length, 1);
		xmlFree(result);
	} else {
		RETVAL_FALSE;
	}
	Xml_UseAllocationFunctions();
}
#endif
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
	ss_wrapper->hits++;
	xsltSaveResultToString(&result, &length, xd_wrapper->xd, ss_wrapper->data.ss);
	
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
	PHP_FE(fastxsl_version,                      NULL)
#ifdef FASTXSL_MM
	PHP_FE(fastxsl_shmcache_transform,           NULL)
	PHP_FE(fastxsl_shmcache_tostring,            NULL)
	PHP_FE(fastxsl_shmcache_getstatistics,       NULL)
	PHP_FE(fastxsl_xmlMemoryDump,                NULL)
#endif
	PHP_FE(fastxsl_prmcache_transform,           NULL)
	PHP_FE(fastxsl_nocache_transform,            NULL)
	PHP_FE(fastxsl_nocache_profile,              NULL)
	PHP_FE(fastxsl_nocache_tostring,             NULL)
	PHP_FE(fastxsl_prmcache_tostring,            NULL)
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
	PHP_MSHUTDOWN(fastxsl),
	PHP_RINIT(fastxsl),
	PHP_RSHUTDOWN(fastxsl),
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
_SS_Wrapper_Dtor(php_ss_wrapper *wrapper)
{
	if(wrapper->data.ss) {
		if(wrapper->alloc_type == FASTXSL_SHMALLOC) {
			//xmlCleanupParserr();
			ShmCache_UseAllocationFunctions();
		} else {
			Xml_UseAllocationFunctions();
		}
		xsltFreeStylesheet(wrapper->data.ss);
		if(wrapper->alloc_type == FASTXSL_SHMALLOC) {
			//xmlCleanupParserr();
			Xml_UseAllocationFunctions();
		}
	}
	if(wrapper->persistant) {
		mm_free(FASTXSL_G(cache)->mm, wrapper);
	} else {
		free(wrapper);
	}
}

static void
SS_Wrapper_Dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	php_ss_wrapper *wrapper = (php_ss_wrapper *) rsrc->ptr;
	if (wrapper->persistant) {
		return;
	}
	_SS_Wrapper_Dtor(wrapper);
}

static void _XD_Wrapper_Dtor(php_xd_wrapper *wrapper) {
	if (wrapper->xd) {
		if(wrapper->alloc_type == FASTXSL_SHMALLOC) {
			ShmCache_UseAllocationFunctions();
		} else {
			Xml_UseAllocationFunctions();
		}
		xmlFreeDoc(wrapper->xd);
		Xml_UseAllocationFunctions();
	}
	free(wrapper);
}

static void
XD_Wrapper_Dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	php_xd_wrapper *wrapper = (php_xd_wrapper *) rsrc->ptr;
	_XD_Wrapper_Dtor(wrapper);
}

void fastxsl_errorfunc(void *ctx, const char *msg, ...)
{
	char *frag;
	int fraglen;
	int output = 0;
	va_list args;
	TSRMLS_FETCH();

	va_start(args, msg);
	fraglen = vspprintf(&frag, 0, msg, args);
	while(fraglen && frag[fraglen - 1] == '\n') {
		frag[--fraglen] = '\0';
		output = 1;
	}
	if(fraglen) {
		if(FASTXSL_G(errbuf)) {
			FASTXSL_G(errbuf) = erealloc(FASTXSL_G(errbuf), fraglen + strlen(FASTXSL_G(errbuf)));
			strcat(FASTXSL_G(errbuf), frag);
		} else {
			FASTXSL_G(errbuf) = frag;
		}
	}
	va_end(args);
	if(output) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, FASTXSL_G(errbuf));
		efree(FASTXSL_G(errbuf));
		FASTXSL_G(errbuf) = NULL;
	}
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
#ifdef FASTXSL_MM
static FL_Allocator mm_allocators = {ShmCache_Free, ShmCache_Malloc, ShmCache_Calloc};
#endif
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
		fl_hash_free(cache->prmtable);
	}
}

/* {{{ php function -> xslt exporting
 */
 
static void fastxsl_ext_function(xmlXPathParserContextPtr ctxt, int nargs)
{
	xsltTransformContextPtr tctxt;
	zval **params;
	zval *param;
	zval *function = NULL;
	zval *ret = NULL;
	xmlChar *fname;
	int param_count = nargs - 1;
	int i;
	TSRMLS_FETCH();
	
	tctxt = (xsltTransformContextPtr) xsltXPathGetTransformContext(ctxt);
	////xmlCleanupParserr();
	if (tctxt == NULL) {
		xsltGenericError(xsltGenericErrorContext,
		"fastxsl extension functions: failed to get the transformation context\n");
		return;
	}
	/* allocate for args.  first position is function name */
	params = emalloc(sizeof(zval *) * param_count);
	for(i = param_count - 1; i >=0; i--) {
		xmlXPathObjectPtr obj;
		xmlChar *tmp;
		obj = valuePop(ctxt);
		MAKE_STD_ZVAL(param);
		switch(obj->type) {
			case XPATH_UNDEFINED:
				ZVAL_NULL(param);
				break;
			case XPATH_NODESET:
				tmp = xmlXPathCastToString(obj);
				ZVAL_STRING(param, (char *) tmp, 1);
				xmlFree(tmp);
				break;
			case XPATH_BOOLEAN:
				ZVAL_BOOL(param, obj->boolval);
				break;
			case XPATH_NUMBER:
				ZVAL_DOUBLE(param, obj->floatval);
				break;
			case XPATH_STRING:
				ZVAL_STRING(param, (char *)obj->stringval, 1);
				break;
			default:
				zend_error(E_WARNING, "inexact type conversion %d", obj->type);
				tmp = xmlXPathCastToString(obj);
				ZVAL_STRING(param, (char *)tmp, 1);
				xmlFree(tmp);
				break;
		}
		params[i] = param;
		xmlXPathFreeObject(obj);
	}
	fname = xmlXPathPopString(ctxt);
	if(!fname) {
		xsltGenericError(xsltGenericDebugContext,
			"passed function name is not a string");
		xmlXPathReturnEmptyString(ctxt);
		goto cleanup;
	}
	MAKE_STD_ZVAL(function);
	ZVAL_STRING(function, (char *)fname, 1);
	xmlFree(fname);
	MAKE_STD_ZVAL(ret);
	ZVAL_FALSE(ret);
	if(call_user_function(EG(function_table), NULL, function, ret, 
	                         param_count, params TSRMLS_CC) == FAILURE) {
			xsltGenericError(xsltGenericDebugContext,
				"function evaluation error");
		xmlXPathReturnEmptyString(ctxt);
		goto cleanup;
	}
	switch(ret->type) {
		case IS_BOOL:
			xmlXPathReturnBoolean(ctxt, Z_BVAL_P(ret));
			break;
		case IS_LONG:
			xmlXPathReturnNumber(ctxt, (float) Z_LVAL_P(ret));
			break;
		case IS_DOUBLE:
			xmlXPathReturnNumber(ctxt, Z_DVAL_P(ret));
			break;
		case IS_STRING:
			xmlXPathReturnString(ctxt, xmlStrdup((xmlChar *) Z_STRVAL_P(ret)));
			break;
		default:
			convert_to_string_ex(&ret);
			xmlXPathReturnString(ctxt, xmlStrdup((xmlChar *) Z_STRVAL_P(ret)));
			break;
	}
cleanup:
	if(params) {
		for(i=0; i < param_count; i++) {
			zval_ptr_dtor(&params[i]);
		}
		efree(params); params = NULL;
	}
	if(function) {
		efree(function); function = NULL;
	}
	if(ret) {
		efree(ret); ret = NULL;
	}
	return;
}		
/* }}} */


PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("fastxsl.shmpath", "/tmp/fastxsl_mem", PHP_INI_SYSTEM, OnUpdateString, shmpath, zend_fastxsl_globals, fastxsl_globals)
	STD_PHP_INI_BOOLEAN("fastxsl.nostat", "0", PHP_INI_ALL, OnUpdateLong, nostat, zend_fastxsl_globals, fastxsl_globals) 
	STD_PHP_INI_BOOLEAN("fastxsl.replace_document_function", "0", PHP_INI_ALL, OnUpdateLong, replace_document_function, zend_fastxsl_globals, fastxsl_globals)
	STD_PHP_INI_BOOLEAN("fastxsl.memalloc", "0", PHP_INI_SYSTEM, OnUpdateLong, memalloc, zend_fastxsl_globals, fastxsl_globals) 
	STD_PHP_INI_BOOLEAN("fastxsl.register_functions", "0", PHP_INI_ALL, OnUpdateLong, register_functions, zend_fastxsl_globals, fastxsl_globals) 
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
#ifdef FASTXSL_MM
	if (!sprintf(euid, "%d", geteuid())) {
		return FAILURE;
	}

	shmpath_len = strlen(FASTXSL_G(shmpath)) + strlen(euid);
	shmpath = do_alloca(shmpath_len + 1);

	strcpy(shmpath, FASTXSL_G(shmpath));
	strcat(shmpath, euid);
	
	FASTXSL_G(cache)->owner = getpid();
	FASTXSL_G(cache)->mm = mm_create(FASTXSL_G(memalloc), shmpath);
	free_alloca(shmpath);
	if (!FASTXSL_G(cache)->mm) {
		return FAILURE;
	}

	mm_lock(FASTXSL_G(cache)->mm, MM_LOCK_RW);
	FASTXSL_G(cache)->table = fl_hash_new(&mm_allocators, NULL);
	mm_unlock(FASTXSL_G(cache)->mm);
#endif
	FASTXSL_G(cache)->prmtable = fl_hash_new(&normal_allocators, NULL);
	//xmlGcMemGet(&free_ptr, &malloc_ptr, &mallocatomic_ptr, &realloc_ptr, &strdup_ptr);
	xmlMemGet(&free_ptr, &malloc_ptr, &realloc_ptr, &strdup_ptr);
	//xmlCleanupParserr();
	ShmCache_UseAllocationFunctions();
	xsltRegisterAllExtras();
#if HAVE_DOMEXSLT
	exsltRegisterAll();
#endif
	xmlSubstituteEntitiesDefault(1);
	xmlLoadExtDtdDefaultValue = 1;

	xmlRegisterOutputCallbacks(Stream_MatchWrapper, Stream_XmlWrite_OpenWrapper, 
			                   Stream_XmlWrite_WriteWrapper, Stream_CloseWrapper);
	xsltSetGenericErrorFunc(NULL, fastxsl_errorfunc);
	xmlSetGenericErrorFunc(NULL, fastxsl_errorfunc);
	
	if(FASTXSL_G(register_functions)) {
		xsltRegisterExtModuleFunction ((const xmlChar *) "function",
					   (const xmlChar *) "http://php.net/fastxsl",
					   fastxsl_ext_function);
	}
	xsltRegisterExtModuleFunction ((const xmlChar *) "cached_document",
				   (const xmlChar *) "http://php.net/fastxsl/cached_document",
				   fastxsl_CachedDocumentFunction);
	//xmlCleanupParserr();
	Xml_UseAllocationFunctions();
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(fastxsl)
{
  UNREGISTER_INI_ENTRIES();
}
PHP_RINIT_FUNCTION(fastxsl)
{
#ifdef FASTXSL_MM
  /* just in case, set back the allocators */
  Xml_UseAllocationFunctions();
#endif
}
PHP_RSHUTDOWN_FUNCTION(fastxsl)
{
#ifdef FASTXSL_MM
  /* just in case, set back the allocators */
  Xml_UseAllocationFunctions();
#endif
}

PHP_MINFO_FUNCTION(fastxsl)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "fastxsl support", "enabled");
	php_info_print_table_row(2, "libxml Version", LIBXML_DOTTED_VERSION);
	php_info_print_table_row(2, "libxslt Version", LIBXSLT_DOTTED_VERSION);
#if HAVE_DOMEXSLT
	php_info_print_table_row(2, "libexslt Version", LIBEXSLT_DOTTED_VERSION);
#endif
	php_info_print_table_end();
}
