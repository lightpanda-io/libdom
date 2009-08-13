/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include "events/ui_event.h"
#include "core/document.h"

static void _virtual_dom_ui_event_destroy(struct dom_event *evt);

static struct dom_event_private_vtable _event_vtable = {
	_virtual_dom_ui_event_destroy
};

/* Constructor */
dom_exception _dom_ui_event_create(struct dom_document *doc, 
		struct dom_ui_event **evt)
{
	*evt = _dom_document_alloc(doc, NULL, sizeof(dom_ui_event));
	if (*evt == NULL) 
		return DOM_NO_MEM_ERR;
	
	((struct dom_event *) *evt)->vtable = &_event_vtable;

	return _dom_ui_event_initialise(doc, *evt);
}

/* Destructor */
void _dom_ui_event_destroy(struct dom_document *doc, 
		struct dom_ui_event *evt)
{
	_dom_ui_event_finalise(doc, evt);

	_dom_document_alloc(doc, evt, 0); 
}

/* Initialise function */
dom_exception _dom_ui_event_initialise(struct dom_document *doc, 
		struct dom_ui_event *evt)
{
	evt->view = NULL;
	return _dom_event_initialise(doc, &evt->base);
}

/* Finalise function */
void _dom_ui_event_finalise(struct dom_document *doc, 
		struct dom_ui_event *evt)
{
	evt->view = NULL;
	_dom_event_finalise(doc, &evt->base);
}

/* The virtual destroy function */
void _virtual_dom_ui_event_destroy(struct dom_event *evt)
{
	_dom_ui_event_destroy(evt->doc, (dom_ui_event *) evt);
}

/*----------------------------------------------------------------------*/
/* The public API */

/**
 * Get the AbstractView inside this event
 *
 * \param evt   The Event object
 * \param view  The returned AbstractView
 * \return DOM_NO_ERR.
 */
dom_exception _dom_ui_event_get_view(dom_ui_event *evt, 
		struct dom_abstract_view **view)
{
	*view = evt->view;

	return DOM_NO_ERR;
}

/**
 * Get the detail param of this event
 *
 * \param evt     The Event object
 * \param detail  The detail object
 * \return DOM_NO_ERR.
 */
dom_exception _dom_ui_event_get_detail(dom_ui_event *evt,
		long *detail)
{
	*detail = evt->detail;

	return DOM_NO_ERR;
}

/**
 * Initialise the UIEvent
 *
 * \param evt         The Event object
 * \param type        The type of this UIEvent
 * \param bubble      Whether this event can bubble
 * \param cancelable  Whether this event is cancelable
 * \param view        The AbstractView of this UIEvent
 * \param detail      The detail object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_ui_event_init(dom_ui_event *evt, struct dom_string *type, 
		bool bubble, bool cancelable, struct dom_abstract_view *view,
		long detail)
{
	evt->view = view;
	evt->detail = detail;

	return _dom_event_init(&evt->base, type, bubble, cancelable);
}

/**
 * Initialise the UIEvent with namespace
 *
 * \param evt         The Event object
 * \param namespace   The namespace of this Event
 * \param type        The type of this UIEvent
 * \param bubble      Whether this event can bubble
 * \param cancelable  Whether this event is cancelable
 * \param view        The AbstractView of this UIEvent
 * \param detail      The detail object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_ui_event_init_ns(dom_ui_event *evt, 
		struct dom_string *namespace, struct dom_string *type,
		bool bubble, bool cancelable, struct dom_abstract_view *view,
		long detail)
{
	evt->view = view;
	evt->detail = detail;

	return _dom_event_init_ns(&evt->base, namespace, type, bubble,
			cancelable);
}

