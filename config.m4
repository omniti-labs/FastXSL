dnl $Id: config.m4,v 1.1.1.1 2004/02/17 23:31:43 sterling Exp $

AC_DEFUN(PHP_EXSLT_CHECK_VERSION,[
  old_CPPFLAGS=$CPPFLAGS
  CPPFLAGS=-I$DOMEXSLT_DIR/include
  AC_MSG_CHECKING(for libexslt version)
  AC_EGREP_CPP(yes,[
#include <libexslt/exsltconfig.h>
#if LIBEXSLT_VERSION >= 600
  yes
#endif
  ],[
    AC_MSG_RESULT(>= 1.0.3)
  ],[
    AC_MSG_ERROR(libxslt version 1.0.3 or greater required.)
  ])
  CPPFLAGS=$old_CPPFLAGS
])

PHP_ARG_WITH(fastxsl, for fastxsl support,
[  --with-fastxsl             Include fastxsl support])

if test "$PHP_FASTXSL" != "no"; then
  SEARCH_PATH="/usr/local /usr" 
  SEARCH_FOR="/include/libxslt/xslt.h" 
  if test -r $PHP_FASTXSL/; then 
    FASTXSL_DIR=$PHP_FASTXSL
  else
    AC_MSG_CHECKING([for fastxsl files in default path])
    for i in $SEARCH_PATH ; do
      if test -r $i/$SEARCH_FOR; then
        FASTXSL_DIR=$i
        AC_MSG_RESULT(found in $i)
      fi
    done
  fi
  
  if test -z "$FASTXSL_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the fastxsl distribution])
  fi

  PHP_ADD_INCLUDE($FASTXSL_DIR/include/libxml2)
  PHP_ADD_INCLUDE($FASTXSL_DIR/include)
  AC_MSG_CHECKING(whether to support libmm based shared template cache)
  AC_ARG_ENABLE(libmm,
  [  --enable-libmm Enable libmm based shared template cache],[
    if test "$enableval" = "yes" ; then
      AC_DEFINE(FASTXSL_MM, 1, [ ])
      AC_MSG_RESULT(yes)
      PHP_ADD_LIBRARY_WITH_PATH(mm,   $FASTXSL_DIR/lib, FASTXSL_SHARED_LIBADD)
    else
      AC_MSG_RESULT(no)
    fi
  ],[
    AC_MSG_RESULT(no)
  ])
  PHP_ADD_LIBRARY_WITH_PATH(xslt, $FASTXSL_DIR/lib, FASTXSL_SHARED_LIBADD)
  PHP_ADD_LIBRARY_WITH_PATH(xml2, $FASTXSL_DIR/lib, FASTXSL_SHARED_LIBADD)
  AC_DEFINE(HAVE_FASTXSLLIB,1,[ ])
  PHP_SUBST(FASTXSL_SHARED_LIBADD)

  PHP_NEW_EXTENSION(fastxsl, fastxsl.c fl_hash.c xsltDocHack.c, $ext_shared)
fi

PHP_ARG_WITH(exslt, for DOM EXSLT support,
[  --with-exslt[=DIR]    FastXSL: Include DOM EXSLT support (requires libxslt >= 1.0.18).
                            DIR is the libexslt install directory.], no, no)

if test "$PHP_EXSLT" != "no"; then
  if test -r $PHP_EXSLT/include/libexslt/exslt.h; then
    DOMEXSLT_DIR=$PHP_EXSLT
  else
    for i in /usr/local /usr; do
      test -r $i/include/libexslt/exslt.h && DOMEXSLT_DIR=$i
    done
  fi

  if test -z "$DOMEXSLT_DIR"; then
    AC_MSG_RESULT(not found)
    AC_MSG_ERROR(Please reinstall the libexslt distribution)
  fi

  PHP_EXSLT_CHECK_VERSION

  PHP_ADD_LIBRARY_WITH_PATH(exslt, $DOMEXSLT_DIR/lib, FASTXSL_SHARED_LIBADD)

  PHP_ADD_INCLUDE($DOMEXSLT_DIR/include)

  AC_DEFINE(HAVE_DOMEXSLT,1,[ ])

  PHP_SUBST(FASTXSL_SHARED_LIBADD)

fi
