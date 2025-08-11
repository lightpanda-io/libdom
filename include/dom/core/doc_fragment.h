/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#ifndef dom_core_documentfragment_h_
#define dom_core_documentfragment_h_

typedef struct dom_document_fragment dom_document_fragment;
void _dom_document_fragment_get_host(dom_document_fragment *frag, dom_node_internal **host);
void _dom_document_fragment_set_host(dom_document_fragment *frag, dom_node_internal *host);

#endif
