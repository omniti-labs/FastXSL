#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#include <libxml/hash.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/valid.h>
#include <libxml/xmlerror.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/uri.h>
#include <libxml/xpointer.h>
#include <libxml/xinclude.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/xsltutils.h>
#include <libxslt/documents.h>
#include <libxslt/transform.h>
#include <libxslt/imports.h>
#include <libxslt/keys.h>
#include <libxslt/security.h>
#include <libxslt/extensions.h>

xsltDocumentPtr
HACK_xsltNewDocument(xsltTransformContextPtr ctxt, xmlDocPtr doc) {
    xsltDocumentPtr cur;

    cur = (xsltDocumentPtr) xmlMalloc(sizeof(xsltDocument));
    if (cur == NULL) {
        xsltTransformError(ctxt, NULL, (xmlNodePtr) doc,
                "xsltNewDocument : malloc failed\n");
        return(NULL);
    }
    memset(cur, 0, sizeof(xsltDocument));
    cur->doc = doc;
    if (ctxt != NULL) {
/* THIS IS THE WHOLE REASON FOR THE HACK */
/* This cannot run, as it will cause some wicked freeing
        if (! XSLT_IS_RES_TREE_FRAG(doc)) {
            cur->next = ctxt->docList;
            ctxt->docList = cur;
        }
*/
#ifdef XSLT_REFACTORED_KEYCOMP
        /*
        * A key with a specific name for a specific document
        * will only be computed if there's a call to the key()
        * function using that specific name for that specific
        * document. I.e. computation of keys will be done in
        * xsltGetKey() (keys.c) on an on-demand basis.
        */
#else
        /*
        * Old behaviour.
        */
        xsltInitCtxtKeys(ctxt, cur);
#endif
    }
    return(cur);
}

xmlDocPtr
HACK_xsltDocDefaultLoaderFunc(const xmlChar * URI, xmlDictPtr dict,
				int options, void *ctxt, xsltLoadType type)
{
    xmlParserCtxtPtr pctxt;
    xmlParserInputPtr inputStream;
    xmlDocPtr doc;

    pctxt = xmlNewParserCtxt();
    if (pctxt == NULL)
        return(NULL);
    if ((dict != NULL) && (pctxt->dict != NULL)) {
        xmlDictFree(pctxt->dict);
        pctxt->dict = NULL;
    }
    if (dict != NULL) {
        pctxt->dict = dict;
        xmlDictReference(pctxt->dict);
#ifdef WITH_XSLT_DEBUG
        xsltGenericDebug(xsltGenericDebugContext,
                     "Reusing dictionary for document\n");
#endif
    }
    xmlCtxtUseOptions(pctxt, options);
    inputStream = xmlLoadExternalEntity((const char *) URI, NULL, pctxt);
    if (inputStream == NULL) {
        xmlFreeParserCtxt(pctxt);
        return(NULL);
    }
    inputPush(pctxt, inputStream);
    if (pctxt->directory == NULL)
        pctxt->directory = xmlParserGetDirectory((const char *) URI);

    xmlParseDocument(pctxt);

    if (pctxt->wellFormed) {
        doc = pctxt->myDoc;
    }
    else {
        doc = NULL;
        xmlFreeDoc(pctxt->myDoc);
        pctxt->myDoc = NULL;
    }
    xmlFreeParserCtxt(pctxt);

    return(doc);
}

xsltDocumentPtr
HACK_xsltLoadDocument(xsltTransformContextPtr ctxt, const xmlChar *URI) {
    xsltDocumentPtr ret;
    xmlDocPtr doc;

    if ((ctxt == NULL) || (URI == NULL))
        return(NULL);

    /*
     * Security framework check
     */
    if (ctxt->sec != NULL) {
        int res;

        res = xsltCheckRead(ctxt->sec, ctxt, URI);
        if (res == 0) {
            xsltTransformError(ctxt, NULL, NULL,
                 "xsltLoadDocument: read rights for %s denied\n",
                             URI);
            return(NULL);
        }
    }

    /*
     * Walk the context list to find the document if preparsed
     */
/* HACK point.. we assume it is not prepared..
   we'd have returned it from cache if it were */
/*
    ret = ctxt->docList;
    while (ret != NULL) {
        if ((ret->doc != NULL) && (ret->doc->URL != NULL) &&
            (xmlStrEqual(ret->doc->URL, URI)))
            return(ret);
        ret = ret->next;
    }
*/
    doc = HACK_xsltDocDefaultLoaderFunc(URI, /* We don't want this, it's not shared: ctxt->dict */ NULL, ctxt->parserOptions,
                                        (void *) ctxt, XSLT_LOAD_DOCUMENT);

    if (doc == NULL)
        return(NULL);

    if (ctxt->xinclude != 0) {
#ifdef LIBXML_XINCLUDE_ENABLED
#if LIBXML_VERSION >= 20603
        xmlXIncludeProcessFlags(doc, ctxt->parserOptions);
#else
        xmlXIncludeProcess(doc);
#endif
#else
        xsltTransformError(ctxt, NULL, NULL,
            "xsltLoadDocument(%s) : XInclude processing not compiled in\n",
                         URI);
#endif
    }
    /*
     * Apply white-space stripping if asked for
     */
    if (xsltNeedElemSpaceHandling(ctxt))
        xsltApplyStripSpaces(ctxt, xmlDocGetRootElement(doc));
    if (ctxt->debugStatus == XSLT_DEBUG_NONE)
        xmlXPathOrderDocElems(doc);

    ret = HACK_xsltNewDocument(ctxt, doc);
    return(ret);
}

void
HACK_xsltDocumentFunctionLoadDocument(xmlXPathParserContextPtr ctxt, xmlChar* URI)
{
    xsltTransformContextPtr tctxt;
    xmlURIPtr uri;
    xmlChar *fragment;
    xsltDocumentPtr idoc; /* document info */
    xmlDocPtr doc;
    xmlXPathContextPtr xptrctxt = NULL;
    xmlXPathObjectPtr resObj = NULL;

    tctxt = xsltXPathGetTransformContext(ctxt);
    if (tctxt == NULL) {
        xsltTransformError(NULL, NULL, NULL,
            "document() : internal error tctxt == NULL\n");
        valuePush(ctxt, xmlXPathNewNodeSet(NULL));
        return;
    }

    uri = xmlParseURI((const char *) URI);
    if (uri == NULL) {
        xsltTransformError(tctxt, NULL, NULL,
            "document() : failed to parse URI\n");
        valuePush(ctxt, xmlXPathNewNodeSet(NULL));
        return;
    }

    /*
     * check for and remove fragment identifier
     */
    fragment = (xmlChar *)uri->fragment;
    if (fragment != NULL) {
        uri->fragment = NULL;
        URI = xmlSaveUri(uri);
        idoc = HACK_xsltLoadDocument(tctxt, URI);
        xmlFree(URI);
    } else
        idoc = HACK_xsltLoadDocument(tctxt, URI);
    xmlFreeURI(uri);

    if (idoc == NULL) {
        if ((URI == NULL) ||
            (URI[0] == '#') ||
            (xmlStrEqual(tctxt->style->doc->URL, URI)))
        {
            /*
            * This selects the stylesheet's doc itself.
            */
            doc = tctxt->style->doc;
        } else {
            valuePush(ctxt, xmlXPathNewNodeSet(NULL));

            if (fragment != NULL)
                xmlFree(fragment);

            return;
        }
    } else
        doc = idoc->doc;

    if (fragment == NULL) {
        valuePush(ctxt, xmlXPathNewNodeSet((xmlNodePtr) doc));
        return;
    }

    /* use XPointer of HTML location for fragment ID */
#ifdef LIBXML_XPTR_ENABLED
    xptrctxt = xmlXPtrNewContext(doc, NULL, NULL);
    if (xptrctxt == NULL) {
        xsltTransformError(tctxt, NULL, NULL,
            "document() : internal error xptrctxt == NULL\n");
        goto out_fragment;
    }

    resObj = xmlXPtrEval(fragment, xptrctxt);
    xmlXPathFreeContext(xptrctxt);
#endif
    xmlFree(fragment);

    if (resObj == NULL)
        goto out_fragment;

    switch (resObj->type) {
        case XPATH_NODESET:
            break;
        case XPATH_UNDEFINED:
        case XPATH_BOOLEAN:
        case XPATH_NUMBER:
        case XPATH_STRING:
        case XPATH_POINT:
        case XPATH_USERS:
        case XPATH_XSLT_TREE:
        case XPATH_RANGE:
        case XPATH_LOCATIONSET:
            xsltTransformError(tctxt, NULL, NULL,
                "document() : XPointer does not select a node set: #%s\n",
                fragment);
        goto out_object;
    }

    valuePush(ctxt, resObj);
    return;

out_object:
    xmlXPathFreeObject(resObj);

out_fragment:
    valuePush(ctxt, xmlXPathNewNodeSet(NULL));
}

void
HACK_xsltDocumentFunction(xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlXPathObjectPtr obj, obj2 = NULL;
    xmlChar *base = NULL, *URI;


    if ((nargs < 1) || (nargs > 2)) {
        xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
                         "document() : invalid number of args %d\n",
                         nargs);
        ctxt->error = XPATH_INVALID_ARITY;
        return;
    }
    if (ctxt->value == NULL) {
        xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
                         "document() : invalid arg value\n");
        ctxt->error = XPATH_INVALID_TYPE;
        return;
    }

    if (nargs == 2) {
        if (ctxt->value->type != XPATH_NODESET) {
            xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
                             "document() : invalid arg expecting a nodeset\n");
            ctxt->error = XPATH_INVALID_TYPE;
            return;
        }

        obj2 = valuePop(ctxt);
    }

    if (ctxt->value->type == XPATH_NODESET) {
        int i;
        xmlXPathObjectPtr newobj, ret;

        obj = valuePop(ctxt);
        ret = xmlXPathNewNodeSet(NULL);

        if (obj->nodesetval) {
            for (i = 0; i < obj->nodesetval->nodeNr; i++) {
                valuePush(ctxt,
                          xmlXPathNewNodeSet(obj->nodesetval->nodeTab[i]));
                xmlXPathStringFunction(ctxt, 1);
                if (nargs == 2) {
                    valuePush(ctxt, xmlXPathObjectCopy(obj2));
                } else {
                    valuePush(ctxt,
                              xmlXPathNewNodeSet(obj->nodesetval->
                                                 nodeTab[i]));
                }
                HACK_xsltDocumentFunction(ctxt, 2);
                newobj = valuePop(ctxt);
                ret->nodesetval = xmlXPathNodeSetMerge(ret->nodesetval,
                                                       newobj->nodesetval);
                xmlXPathFreeObject(newobj);
            }
        }

        xmlXPathFreeObject(obj);
        if (obj2 != NULL)
            xmlXPathFreeObject(obj2);
        valuePush(ctxt, ret);
        return;
    }
    /*
     * Make sure it's converted to a string
     */
    xmlXPathStringFunction(ctxt, 1);
    if (ctxt->value->type != XPATH_STRING) {
        xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
                         "document() : invalid arg expecting a string\n");
        ctxt->error = XPATH_INVALID_TYPE;
        if (obj2 != NULL)
            xmlXPathFreeObject(obj2);
        return;
    }
    obj = valuePop(ctxt);
    if (obj->stringval == NULL) {
        valuePush(ctxt, xmlXPathNewNodeSet(NULL));
    } else {
        if ((obj2 != NULL) && (obj2->nodesetval != NULL) &&
            (obj2->nodesetval->nodeNr > 0) &&
            IS_XSLT_REAL_NODE(obj2->nodesetval->nodeTab[0])) {
            xmlNodePtr target;

            target = obj2->nodesetval->nodeTab[0];
            if ((target->type == XML_ATTRIBUTE_NODE) ||
                (target->type == XML_PI_NODE)) {
                target = ((xmlAttrPtr) target)->parent;
            }
            base = xmlNodeGetBase(target->doc, target);
        } else {
            xsltTransformContextPtr tctxt;

            tctxt = xsltXPathGetTransformContext(ctxt);
            if ((tctxt != NULL) && (tctxt->inst != NULL)) {
                base = xmlNodeGetBase(tctxt->inst->doc, tctxt->inst);
            } else if ((tctxt != NULL) && (tctxt->style != NULL) &&
                       (tctxt->style->doc != NULL)) {
                base = xmlNodeGetBase(tctxt->style->doc,
                                      (xmlNodePtr) tctxt->style->doc);
            }
        }
        URI = xmlBuildURI(obj->stringval, base);
        if (base != NULL)
            xmlFree(base);
        if (URI == NULL) {
            valuePush(ctxt, xmlXPathNewNodeSet(NULL));
        } else {
            HACK_xsltDocumentFunctionLoadDocument( ctxt, URI );
            xmlFree(URI);
        }
    }
    xmlXPathFreeObject(obj);
    if (obj2 != NULL)
        xmlXPathFreeObject(obj2);
}

