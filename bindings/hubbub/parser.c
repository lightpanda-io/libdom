/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <stdio.h>
#include <string.h>

#include <hubbub/errors.h>
#include <hubbub/hubbub.h>
#include <hubbub/parser.h>

#include <dom/dom.h>

#include "parser.h"
#include "utils.h"

#include "core/document.h"

#include <libwapcaplet/libwapcaplet.h>

/**
 * libdom Hubbub parser context
 */
struct dom_hubbub_parser {
	hubbub_parser *parser;		/**< Hubbub parser instance */
	hubbub_tree_handler tree_handler;
					/**< Hubbub parser tree handler */

	struct dom_document *doc;	/**< DOM Document we're building */

	dom_hubbub_encoding_source encoding_source;
					/**< The document's encoding source */
	const char *encoding; 		/**< The document's encoding */

	bool complete;			/**< Indicate stream completion */

	struct dom_implementation *impl;/**< DOM implementation */

	dom_alloc alloc;		/**< Memory (de)allocation function */
	void *pw;			/**< Pointer to client data */

	dom_msg msg;		/**< Informational messaging function */
	void *mctx;		/**< Pointer to client data */

    struct lwc_context_s *ctx;  /**< The string intern context */
};


/* The callbacks declarations */
static hubbub_error create_comment(void *parser, const hubbub_string *data, 
		void **result);
static hubbub_error create_doctype(void *parser, const hubbub_doctype *doctype,
		void **result);
static hubbub_error create_element(void *parser, const hubbub_tag *tag, 
		void **result);
static hubbub_error create_text(void *parser, const hubbub_string *data, 
		void **result);
static hubbub_error ref_node(void *parser, void *node);
static hubbub_error unref_node(void *parser, void *node);
static hubbub_error append_child(void *parser, void *parent, void *child, 
		void **result);
static hubbub_error insert_before(void *parser, void *parent, void *child, 
		void *ref_child, void **result);
static hubbub_error remove_child(void *parser, void *parent, void *child, 
		void **result);
static hubbub_error clone_node(void *parser, void *node, bool deep, 
		void **result);
static hubbub_error reparent_children(void *parser, void *node, 
		void *new_parent);
static hubbub_error get_parent(void *parser, void *node, bool element_only,
		void **result);
static hubbub_error has_children(void *parser, void *node, bool *result);
static hubbub_error form_associate(void *parser, void *form, void *node);
static hubbub_error add_attributes(void *parser, void *node,
		const hubbub_attribute *attributes, uint32_t n_attributes);
static hubbub_error set_quirks_mode(void *parser, hubbub_quirks_mode mode);
static hubbub_error change_encoding(void *parser, const char *charset);

static hubbub_tree_handler tree_handler = {
	create_comment,
	create_doctype,
	create_element,
	create_text,
	ref_node,
	unref_node,
	append_child,
	insert_before,
	remove_child,
	clone_node,
	reparent_children,
	get_parent,
	has_children,
	form_associate,
	add_attributes,
	set_quirks_mode,
	change_encoding,
	NULL
};

static bool __initialised = false;

/**
 * Create a Hubbub parser instance
 *
 * \param aliases  Path to encoding alias mapping file
 * \param enc      Source charset, or NULL
 * \param fix_enc  Whether fix the encoding
 * \param alloc    Memory (de)allocation function
 * \param pw       Pointer to client-specific private data
 * \param msg      Informational message function
 * \param mctx     Pointer to client-specific private data
 * \return Pointer to instance, or NULL on memory exhaustion
 */
dom_hubbub_parser *dom_hubbub_parser_create(const char *aliases, 
		const char *enc, bool fix_enc,
		dom_alloc alloc, void *pw, dom_msg msg, void *mctx,
		lwc_context *ctx)
{
	dom_hubbub_parser *parser;
	hubbub_parser_optparams params;
	hubbub_error error;
	dom_exception err;
	struct dom_string *features;

	if (__initialised == false) {
		error = hubbub_initialise(aliases,
				(hubbub_allocator_fn) alloc, pw);
		if (error != HUBBUB_OK) {
			msg(DOM_MSG_ERROR, mctx,
					"Failed initialising hubbub");
			return NULL;
		}

		__initialised = true;
	}


	parser = alloc(NULL, sizeof(dom_hubbub_parser), pw);
	if (parser == NULL) {
		msg(DOM_MSG_CRITICAL, mctx, "No memory for parsing context");
		return NULL;
	}

	parser->parser = NULL;
	parser->doc = NULL;
	parser->encoding = enc;
	parser->encoding_source = enc != NULL ? ENCODING_SOURCE_HEADER
					      : ENCODING_SOURCE_DETECTED;
	parser->complete = false;
	parser->impl = NULL;

	parser->alloc = alloc;
	parser->pw = pw;
	parser->msg = msg;
	parser->mctx = mctx;
    parser->ctx = ctx;

	error = hubbub_parser_create(enc, fix_enc, alloc, pw, &parser->parser);
	if (error != HUBBUB_OK)	 {
		parser->alloc(parser, 0, parser->pw);
		msg(DOM_MSG_CRITICAL, mctx, "Can't create parser");
		return NULL;
	}

	/* Create string representation of the features we want */
	err = dom_string_create(alloc, pw,
			(const uint8_t *) "HTML", SLEN("HTML"), &features);
	if (err != DOM_NO_ERR) {
		hubbub_parser_destroy(parser->parser);
		alloc(parser, 0, pw);
		msg(DOM_MSG_CRITICAL, mctx, "No memory for feature string");
		return NULL;
	}

 	/* Now, try to get an appropriate implementation from the registry */
	err = dom_implregistry_get_dom_implementation(features,
			&parser->impl);
	if (err != DOM_NO_ERR) {
		dom_string_unref(features);
		hubbub_parser_destroy(parser->parser);
		alloc(parser, 0, pw);
		msg(DOM_MSG_ERROR, mctx, "No suitable DOMImplementation");
		return NULL;
	}

	/* No longer need the feature string */
	dom_string_unref(features);

	/* TODO: Just pass the dom_events_default_action_fetcher a NULL,
	 * we should pass the real function when we integrate libDOM with
	 * Netsurf */
	err = dom_implementation_create_document(parser->impl, NULL, NULL, NULL,
			alloc, pw, ctx, NULL, &parser->doc);
	if (err != DOM_NO_ERR) {
		hubbub_parser_destroy(parser->parser);
		alloc(parser, 0, pw);
		msg(DOM_MSG_ERROR, mctx, "Can't create DOM document");
		return NULL;
	}

	parser->tree_handler = tree_handler;
	parser->tree_handler.ctx = (void *) parser;

	params.tree_handler = &parser->tree_handler;
	hubbub_parser_setopt(parser->parser, HUBBUB_PARSER_TREE_HANDLER,
			&params);

	dom_node_ref((struct dom_node *) parser->doc);
	params.document_node = parser->doc;
	hubbub_parser_setopt(parser->parser, HUBBUB_PARSER_DOCUMENT_NODE,
			&params);

	return parser;
}

/**
 * Destroy a Hubbub parser instance
 *
 * \param parser  The Hubbub parser object
 */
void dom_hubbub_parser_destroy(dom_hubbub_parser *parser)
{
	dom_implementation_unref(parser->impl);
	hubbub_parser_destroy(parser->parser);
	parser->parser = NULL;

	if (parser->doc != NULL) {
		dom_node_unref((struct dom_node *) parser->doc);
		parser->doc = NULL;
	}

	parser->alloc(parser, 0, parser->pw);
}

/**
 * Parse data with Hubbub parser
 *
 * \param parser  The parser object
 * \param data    The data to be parsed
 * \param len     The length of the data to be parsed
 * \return DOM_HUBBUB_OK on success,
 *         DOM_HUBBUB_HUBBUB_ERR | <hubbub_error> on failure
 */
dom_hubbub_error dom_hubbub_parser_parse_chunk(dom_hubbub_parser *parser,
		uint8_t *data, size_t len)
{
	hubbub_error err;

	err = hubbub_parser_parse_chunk(parser->parser, data, len);
	if (err != HUBBUB_OK)
		return DOM_HUBBUB_HUBBUB_ERR | err;

	return DOM_HUBBUB_OK;
}

/**
 * Notify the parser to complete parsing
 *
 * \param parser  The parser object
 * \return DOM_HUBBUB_OK                          on success, 
 *         DOM_HUBBUB_HUBBUB_ERR | <hubbub_error> on underlaying parser failure
 *         DOMHUBBUB_UNKNOWN | <lwc_error>        on libwapcaplet failure
 */
dom_hubbub_error dom_hubbub_parser_completed(dom_hubbub_parser *parser)
{
	hubbub_error err;
	lwc_string *name = NULL;
	lwc_error lerr;

	err = hubbub_parser_completed(parser->parser);
	if (err != DOM_HUBBUB_OK) {
		parser->msg(DOM_MSG_ERROR, parser->mctx,
				"hubbub_parser_completed failed: %d", err);
		return DOM_HUBBUB_HUBBUB_ERR | err;
	}

	parser->complete = true;

	lerr = lwc_context_intern(parser->ctx, "id", strlen("id"), &name);
	if (lerr != lwc_error_ok)
		return HUBBUB_UNKNOWN;
	
	_dom_document_set_id_name(parser->doc, name);
	lwc_context_string_unref(parser->ctx, name);

	return DOM_HUBBUB_OK;
}

/**
 * Fetch the Document object from the parser
 *
 * \param parser  The parser object
 * \return the created document on success, NULL on failure
 */
struct dom_document *dom_hubbub_parser_get_document(dom_hubbub_parser *parser)
{
	struct dom_document *doc = NULL; 

	if (parser->complete) {
		doc = parser->doc; 
		parser->doc = NULL; 
	}

	return doc;
}

/**
 * Retrieve the encoding
 *
 * \param parser  The parser object
 * \param source  The encoding_source
 * \return the encoding name
 */
const char *dom_hubbub_parser_get_encoding(dom_hubbub_parser *parser, 
		dom_hubbub_encoding_source *source)
{
	*source = parser->encoding_source;

	return parser->encoding != NULL ? parser->encoding
					: "Windows-1252";
}


/*--------------------- The callbacks definitions --------------------*/
static hubbub_error create_comment(void *parser, const hubbub_string *data,
		void **result)
{
	dom_hubbub_parser *dom_parser = (dom_hubbub_parser *) parser;
	dom_exception err;
	struct dom_string *str;
	struct dom_comment *comment;

	*result = NULL;

	err = dom_string_create(dom_parser->alloc, dom_parser->pw, data->ptr,
			data->len, &str);
	if (err != DOM_NO_ERR) {
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Can't create comment node text");
		return HUBBUB_UNKNOWN;
	}

	err = dom_document_create_comment(dom_parser->doc, str, &comment);
	if (err != DOM_NO_ERR) {
		dom_string_unref(str);
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Can't create comment node with text '%.*s'",
				data->len, data->ptr);
		return HUBBUB_UNKNOWN;
	}

	*result = comment;

	dom_string_unref(str);

	return HUBBUB_OK;
}

static hubbub_error create_doctype(void *parser, const hubbub_doctype *doctype,
		void **result)
{
	dom_hubbub_parser *dom_parser = (dom_hubbub_parser *) parser;
	dom_exception err;
	struct dom_string *qname, *public_id = NULL, *system_id = NULL;
	struct dom_document_type *dtype;

	*result = NULL;

	err = dom_string_create(dom_parser->alloc, dom_parser->pw,
			doctype->name.ptr, doctype->name.len, &qname);
	if (err != DOM_NO_ERR) {
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Can't create doctype name");
		goto fail;
	}

	if (doctype->public_missing == false) {
		err = dom_string_create(dom_parser->alloc, dom_parser->pw,
				doctype->public_id.ptr, 
				doctype->public_id.len, &public_id);
	} else {
		err = dom_string_create(dom_parser->alloc, dom_parser->pw,
				NULL, 0, &public_id);
	}
	if (err != DOM_NO_ERR) {
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Can't create doctype public id");
		goto clean1;
	}

	if (doctype->system_missing == false) {
		err = dom_string_create(dom_parser->alloc, dom_parser->pw,
				doctype->system_id.ptr,
				doctype->system_id.len, &system_id);
	} else {
		err = dom_string_create(dom_parser->alloc, dom_parser->pw,
				NULL, 0, &system_id);
	}
	if (err != DOM_NO_ERR) {
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Can't create doctype system id");
		goto clean2;
	}

	err = dom_implementation_create_document_type(dom_parser->impl, qname,
			public_id, system_id, dom_parser->alloc, 
			dom_parser->pw, dom_parser->ctx, &dtype);
	if (err != DOM_NO_ERR) {
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Can't create the document type");
		goto clean3;
	}

	*result = dtype;

clean3:
	dom_string_unref(system_id);

clean2:
	dom_string_unref(public_id);

clean1:
	dom_string_unref(qname);

fail:
	if (*result == NULL)
		return HUBBUB_UNKNOWN;
	else
		return HUBBUB_OK;
}

static hubbub_error create_element(void *parser, const hubbub_tag *tag,
		void **result)
{
	dom_hubbub_parser *dom_parser = (dom_hubbub_parser *) parser;
	dom_exception err;
	struct dom_string *name;
	struct dom_element *element = NULL;
	hubbub_error herr;

	*result = NULL;

	err = dom_string_create(dom_parser->alloc, dom_parser->pw, 
			tag->name.ptr, tag->name.len, &name);
	if (err != DOM_NO_ERR) {
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Can't create element name");
		goto fail;
	}

	if (tag->ns == HUBBUB_NS_NULL) {
		err = dom_document_create_element(dom_parser->doc, name,
				&element);
		if (err != DOM_NO_ERR) {
			dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
					"Can't create the DOM element");
			goto clean1;
		}
	} else {
		err = dom_document_create_element_ns(dom_parser->doc,
				dom_namespaces[tag->ns], name, &element);
		if (err != DOM_NO_ERR) {
			dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
					"Can't create the DOM element");
			goto clean1;
		}
	}

	*result = element;
	if (element != NULL) {
		if (tag->n_attributes != 0) {
			herr = add_attributes(parser, element, tag->attributes,
					tag->n_attributes);
			if (herr != HUBBUB_OK)
				return herr;
		}
	}

clean1:
	dom_string_unref(name);

fail:
	if (*result == NULL)
		return HUBBUB_UNKNOWN;
	else
		return HUBBUB_OK;
}

static hubbub_error create_text(void *parser, const hubbub_string *data,
		void **result)
{
	dom_hubbub_parser *dom_parser = (dom_hubbub_parser *) parser;
	dom_exception err;
	struct dom_string *str;
	struct dom_text *text = NULL;

	*result = NULL;

	err = dom_string_create(dom_parser->alloc, dom_parser->pw, data->ptr,
			data->len, &str);
	if (err != DOM_NO_ERR) {
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Can't create text '%.*s'", data->len, 
				data->ptr);
		goto fail;
	}

	err = dom_document_create_text_node(dom_parser->doc, str, &text);
	if (err != DOM_NO_ERR) {
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Can't create the DOM text node");
		goto clean1;
	}

	*result = text;
clean1:
	dom_string_unref(str);

fail:
	if (*result == NULL)
		return HUBBUB_UNKNOWN;
	else
		return HUBBUB_OK;

}

static hubbub_error ref_node(void *parser, void *node)
{
	struct dom_node *dnode = (struct dom_node *) node;

	UNUSED(parser);

	dom_node_ref(dnode);

	return HUBBUB_OK;
}

static hubbub_error unref_node(void *parser, void *node)
{
	struct dom_node *dnode = (struct dom_node *) node;

	UNUSED(parser);

	dom_node_unref(dnode);

	return HUBBUB_OK;
}

static hubbub_error append_child(void *parser, void *parent, void *child,
		void **result)
{
	dom_hubbub_parser *dom_parser = (dom_hubbub_parser *) parser;
	dom_exception err;

	err = dom_node_append_child((struct dom_node *) parent, 
			(struct dom_node *) child,
			(struct dom_node **) result);
	if (err != DOM_NO_ERR) {
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Can't append child '%p' for parent '%p'",
				child, parent);
		return HUBBUB_UNKNOWN;
	}

	return HUBBUB_OK;
}

static hubbub_error insert_before(void *parser, void *parent, void *child, 
		void *ref_child, void **result)
{
	dom_hubbub_parser *dom_parser = (dom_hubbub_parser *) parser;
	dom_exception err;

	err = dom_node_insert_before((struct dom_node *) parent,
			(struct dom_node *) child, 
			(struct dom_node *) ref_child,
			(struct dom_node **) result);
	if (err != DOM_NO_ERR) {
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Can't insert node '%p' before node '%p'",
				child, ref_child);
		return HUBBUB_UNKNOWN;
	}

	return HUBBUB_OK;
}

static hubbub_error remove_child(void *parser, void *parent, void *child,
		void **result)
{
	dom_hubbub_parser *dom_parser = (dom_hubbub_parser *) parser;
	dom_exception err;

	err = dom_node_remove_child((struct dom_node *) parent,
			(struct dom_node *) child, 
			(struct dom_node **) result);
	if (err != DOM_NO_ERR) {
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Can't remove child '%p'", child);
		return HUBBUB_UNKNOWN;
	}

	return HUBBUB_OK;
}

static hubbub_error clone_node(void *parser, void *node, bool deep,
		void **result)
{
	dom_hubbub_parser *dom_parser = (dom_hubbub_parser *) parser;
	dom_exception err;

	err = dom_node_clone_node((struct dom_node *) node, deep,
			(struct dom_node **) result);
	if (err != DOM_NO_ERR) {
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Can't clone node '%p'", node);
		return HUBBUB_UNKNOWN;
	}

	return HUBBUB_OK;
}

static hubbub_error reparent_children(void *parser, void *node,
		void *new_parent)
{
	dom_hubbub_parser *dom_parser = (dom_hubbub_parser *) parser;
	dom_exception err;
	struct dom_node *child, *result;

	while(true) {
		err = dom_node_get_first_child((struct dom_node *) node,
				&child);
		if (err != DOM_NO_ERR) {
			dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
					"Error in dom_note_get_first_child");
			return HUBBUB_UNKNOWN;
		}
		if (child == NULL)
			break;

		err = dom_node_remove_child(node, (struct dom_node *) child,
				&result);
		if (err != DOM_NO_ERR) {
			dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
					"Error in dom_node_remove_child");
			goto fail;
		}
		dom_node_unref(result);

		err = dom_node_append_child((struct dom_node *) new_parent, 
				(struct dom_node *) child, &result);
		if (err != DOM_NO_ERR) {
			dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
					"Error in dom_node_append_child");
			goto fail;
		}
		dom_node_unref(result);
		dom_node_unref(child);
	}
	return HUBBUB_OK;

fail:
	dom_node_unref(child);
	return HUBBUB_UNKNOWN;
}

static hubbub_error get_parent(void *parser, void *node, bool element_only, 
		void **result)
{
	dom_hubbub_parser *dom_parser = (dom_hubbub_parser *) parser;
	dom_exception err;
	struct dom_node *parent;
	dom_node_type type = DOM_NODE_TYPE_COUNT;

	err = dom_node_get_parent_node((struct dom_node *) node,
			&parent);
	if (err != DOM_NO_ERR) {
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Error in dom_node_get_parent");
		return HUBBUB_UNKNOWN;
	}
	if (element_only == false) {
		*result = parent;
		return HUBBUB_OK;
	}

	err = dom_node_get_node_type(parent, &type);
	if (err != DOM_NO_ERR) {
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Error in dom_node_get_type");
		goto fail;
	}
	if (type == DOM_ELEMENT_NODE) {
		*result = parent;
		return HUBBUB_OK;
	} else {
		*result = NULL;
		dom_node_unref(parent);
		return HUBBUB_OK;
	}

	return HUBBUB_OK;
fail:
	dom_node_unref(parent);
	return HUBBUB_UNKNOWN;
}

static hubbub_error has_children(void *parser, void *node, bool *result)
{
	dom_hubbub_parser *dom_parser = (dom_hubbub_parser *) parser;
	dom_exception err;

	UNUSED(parser);

	err = dom_node_has_child_nodes((struct dom_node *) node, result);
	if (err != DOM_NO_ERR) {
		dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
				"Error in dom_node_has_child_nodes");
		return HUBBUB_UNKNOWN;
	}
	return HUBBUB_OK;
}

static hubbub_error form_associate(void *parser, void *form, void *node)
{
	UNUSED(parser);
	UNUSED(form);
	UNUSED(node);

	return HUBBUB_OK;
}

static hubbub_error add_attributes(void *parser, void *node,
		const hubbub_attribute *attributes, uint32_t n_attributes)
{
	dom_hubbub_parser *dom_parser = (dom_hubbub_parser *) parser;
	dom_exception err;
	uint32_t i;

	for (i = 0; i < n_attributes; i++) {
		struct dom_string *name, *value;
		err = dom_string_create(dom_parser->alloc, dom_parser->pw,
				attributes[i].name.ptr,
				attributes[i].name.len, &name);
		if (err != DOM_NO_ERR) {
			dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
					"Can't create attribute name");
			goto fail;
		}

		err = dom_string_create(dom_parser->alloc, dom_parser->pw,
				attributes[i].value.ptr,
				attributes[i].value.len, &value);
		if (err != DOM_NO_ERR) {
			dom_parser->msg(DOM_MSG_CRITICAL, dom_parser->mctx,
					"Can't create attribute value");
			dom_string_unref(name);
			goto fail;
		}

		if (attributes[i].ns == HUBBUB_NS_NULL) {
			err = dom_element_set_attribute(
					(struct dom_element *) node, name,
					value);
			dom_string_unref(name);
			dom_string_unref(value);
			if (err != DOM_NO_ERR) {
				dom_parser->msg(DOM_MSG_CRITICAL, 
						dom_parser->mctx,
						"Can't add attribute");
				goto fail;
			}
		} else {
			err = dom_element_set_attribute_ns(
					(struct dom_element *) node, 
					dom_namespaces[attributes[i].ns], name,
					value);
			dom_string_unref(name);
			dom_string_unref(value);
			if (err != DOM_NO_ERR) {
				dom_parser->msg(DOM_MSG_CRITICAL, 
						dom_parser->mctx,
						"Can't add attribute ns");
				goto fail;
			}
		}
	}

	return HUBBUB_OK;

fail:
	return HUBBUB_UNKNOWN;
}

static hubbub_error set_quirks_mode(void *parser, hubbub_quirks_mode mode)
{
	UNUSED(parser);
	UNUSED(mode);

	return HUBBUB_OK;
}

static hubbub_error change_encoding(void *parser, const char *charset)
{
	dom_hubbub_parser *dom_parser = (dom_hubbub_parser *) parser;
	uint32_t source;
	const char *name;

	/* If we have an encoding here, it means we are *certain* */
	if (dom_parser->encoding != NULL) {
		return HUBBUB_OK;
	}

	/* Find the confidence otherwise (can only be from a BOM) */
	name = hubbub_parser_read_charset(dom_parser->parser, &source);

	if (source == HUBBUB_CHARSET_CONFIDENT) {
		dom_parser->encoding_source = ENCODING_SOURCE_DETECTED;
		dom_parser->encoding = (char *) charset;
		return HUBBUB_OK;
	}

	/* So here we have something of confidence tentative... */
	/* http://www.whatwg.org/specs/web-apps/current-work/#change */

	/* 2. "If the new encoding is identical or equivalent to the encoding
	 * that is already being used to interpret the input stream, then set
	 * the confidence to confident and abort these steps." */

	/* Whatever happens, the encoding should be set here; either for
	 * reprocessing with a different charset, or for confirming that the
	 * charset is in fact correct */
	dom_parser->encoding = charset;
	dom_parser->encoding_source = ENCODING_SOURCE_META;

	/* Equal encodings will have the same string pointers */
	return (charset == name) ? HUBBUB_OK : HUBBUB_ENCODINGCHANGE;
}

