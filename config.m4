dnl $Id: config.m4,v 1.1.1.1 2004/02/17 23:31:43 sterling Exp $

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

  PHP_NEW_EXTENSION(fastxsl, fastxsl.c fl_hash.c, $ext_shared)
fi
