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

  $Id: php_fastxsl.h,v 1.1.1.1 2004/02/17 23:31:44 sterling Exp $ 
*/

#include "php.h"

#ifndef PHP_FASTXSL_H
#define PHP_FASTXSL_H

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

extern zend_module_entry fastxsl_module_entry;
#define phpext_fastxsl_ptr &fastxsl_module_entry

#ifdef PHP_WIN32
#define PHP_FASTXSL_API __declspec(dllexport)
#else
#define PHP_FASTXSL_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(fastxsl);
PHP_MSHUTDOWN_FUNCTION(fastxsl);
PHP_MINFO_FUNCTION(fastxsl);

typedef struct {
	MM   *mm;
	FL_Hash *table;
	pid_t owner;
	FL_Hash *prmtable;
} fl_cache;

typedef struct {
	xmlDocPtr xd;
} php_xd_wrapper;

typedef struct {
	xsltStylesheetPtr ss;
	int allocsize;
	time_t mtime;
	int persistant;
} php_ss_wrapper;

typedef struct {
	fl_cache *cache;
	char     *shmpath;
	long      nostat;
	long      tmp_allocated_size;
} zend_fastxsl_globals;

#ifdef ZTS
#define FASTXSL_G(v) TSRMG(fastxsl_globals_id, zend_fastxsl_globals *, v)
#else
#define FASTXSL_G(v) (fastxsl_globals.v)
#endif

#endif


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
