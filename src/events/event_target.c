/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <assert.h>
#include <stdlib.h>

#include "events/event.h"
#include "events/event_listener.h"
#include "events/event_target.h"

#include "core/document.h"
#include "core/node.h"
#include "core/string.h"

#include "utils/utils.h"
#include "utils/validate.h"

static void event_target_destroy_listener(struct listener_entry *e)
{
	list_del(&e->list);
	dom_event_listener_unref(e->listener);
	dom_string_unref(e->type);
	free(e);
}
static void event_target_destroy_listeners(struct listener_entry *list)
{
	struct listener_entry *next;

	while (list != (struct listener_entry *) list->list.next) {
		next = (struct listener_entry *) list->list.next;
		event_target_destroy_listener(list);
		list = next;
	}

	event_target_destroy_listener(list);
}

/* Initialise this EventTarget */
dom_exception _dom_event_target_internal_initialise(
		dom_event_target_internal *eti)
{
	eti->listeners = NULL;

	return DOM_NO_ERR;
}

/* Finalise this EventTarget */
void _dom_event_target_internal_finalise(dom_event_target_internal *eti)
{
	if (eti->listeners != NULL) {
		event_target_destroy_listeners(eti->listeners);
		eti->listeners = NULL;
	}
}

/*-------------------------------------------------------------------------*/
/* The public API */

/**
 * Add an EventListener to the EventTarget
 *
 * \param et        The EventTarget object
 * \param type      The event type which this event listener listens for
 * \param listener  The event listener object
 * \param capture   Whether add this listener in the capturing phase
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_event_target_add_event_listener(
		dom_event_target_internal *eti,
		dom_string *type, struct dom_event_listener *listener, 
		bool capture)
{
	struct listener_entry *le = NULL;

	le = malloc(sizeof(struct listener_entry));
	if (le == NULL)
		return DOM_NO_MEM_ERR;
	
	/* Initialise the listener_entry */
	list_init(&le->list);
	le->type = dom_string_ref(type);
	le->listener = listener;
	dom_event_listener_ref(listener);
	le->capture = capture;

	if (eti->listeners == NULL) {
		eti->listeners = le;
	} else {
		list_append(&eti->listeners->list, &le->list);
	}

	return DOM_NO_ERR;
}

/**
 * Remove an EventListener from the EventTarget
 *
 * (LibDOM extension: If type is NULL, remove all listener registrations
 * regardless of type and cature)
 *
 * \param et        The EventTarget object
 * \param type      The event type this listener is registered for 
 * \param listener  The listener object
 * \param capture   Whether the listener is registered at the capturing phase
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_event_target_remove_event_listener(
		dom_event_target_internal *eti,
		dom_string *type, struct dom_event_listener *listener, 
		bool capture)
{
	if (eti->listeners != NULL) {
		struct listener_entry *le = eti->listeners;

		do {
			bool matches;
			if (type == NULL) {
				matches = (le->listener == listener);
			} else {
				matches = dom_string_isequal(le->type, type) &&
					(le->listener == listener) &&
					(le->capture == capture);
			}
			if (matches) {
				if (le->list.next == &le->list) {
					eti->listeners = NULL;
				} else {
					eti->listeners =
						(struct listener_entry *)
						le->list.next;
				}
				list_del(&le->list);
				dom_event_listener_unref(le->listener);
				dom_string_unref(le->type);
				free(le);
				break;
			}

			le = (struct listener_entry *) le->list.next;
		} while (eti->listeners != NULL && le != eti->listeners);
	}

	return DOM_NO_ERR;
}

/**
 * Add an EventListener
 *
 * \param et         The EventTarget object
 * \param namespace  The namespace of this listener
 * \param type       The event type which this event listener listens for
 * \param listener   The event listener object
 * \param capture    Whether add this listener in the capturing phase
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 *
 * We don't support this API now, so it always return DOM_NOT_SUPPORTED_ERR.
 */
dom_exception _dom_event_target_add_event_listener_ns(
		dom_event_target_internal *eti,
		dom_string *namespace, dom_string *type, 
		struct dom_event_listener *listener, bool capture)
{
	UNUSED(eti);
	UNUSED(namespace);
	UNUSED(type);
	UNUSED(listener);
	UNUSED(capture);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Remove an EventListener
 *
 * \param et         The EventTarget object
 * \param namespace  The namespace of this listener
 * \param type       The event type which this event listener listens for
 * \param listener   The event listener object
 * \param capture    Whether add this listener in the capturing phase
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 *
 * We don't support this API now, so it always return DOM_NOT_SUPPORTED_ERR.
 */
dom_exception _dom_event_target_remove_event_listener_ns(
		dom_event_target_internal *eti,
		dom_string *namespace, dom_string *type, 
		struct dom_event_listener *listener, bool capture)
{
	UNUSED(eti);
	UNUSED(namespace);
	UNUSED(type);
	UNUSED(listener);
	UNUSED(capture);

	return DOM_NOT_SUPPORTED_ERR;
}

/*-------------------------------------------------------------------------*/

/**
 * Dispatch an event on certain EventTarget
 *
 * \param et       The EventTarget object
 * \param eti      Internal EventTarget object
 * \param evt      The event object
 * \param success  Indicates whether any of the listeners which handled the 
 *                 event called Event.preventDefault(). If 
 *                 Event.preventDefault() was called the returned value is 
 *                 false, else it is true.
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_event_target_dispatch(dom_event_target *et,
		dom_event_target_internal *eti, 
		struct dom_event *evt, dom_event_flow_phase phase,
		bool *success)
{
	if (eti->listeners != NULL) {
		struct listener_entry *le = eti->listeners;

		evt->current = et;

		do {
			if (dom_string_isequal(le->type, evt->type)) {
				assert(le->listener->handler != NULL);

				if ((le->capture &&
				     phase == DOM_CAPTURING_PHASE) ||
				    (le->capture == false &&
				     evt->bubble &&
				     phase == DOM_BUBBLING_PHASE) ||
				    (evt->target == evt->current &&
				     phase == DOM_AT_TARGET)) {
					le->listener->handler(evt, 
							le->listener->pw);
					/* If the handler called
					 * stopImmediatePropagation, we should
					 * break */
					if (evt->stop_now == true)
						break;

					// A listener can modify the event target listeners list
					// during the loop. At this stage, `le` can be removed from
					// the list and its memory freed.
					// First we need to detect if `le` still exists.
					if (!list_has((struct list_entry *)eti->listeners, (struct list_entry *)le)) {
						// `le` has been deleted.
						break;
					}
				}
			}
			le = (struct listener_entry *) le->list.next;
		} while (le != eti->listeners);
	}

	if (evt->prevent_default == true)
		*success = false;

	return DOM_NO_ERR;
}

/**
 * Iterate on the EventTarget's listeners
 *
 * \param eti      Internal EventTarget object
 * \param type     The event type this listener is registered for
 * \param capture  Whether the listener is registered at the capturing phase
 * \param cur      The current listener entry of the iteration
 * \param next     The next listener entry of the iteration
 * \param listener The returned EventListener,
 *                 if the event type and capture matches or are NULL
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 *
 * (If type is NULL, all listener will be returned
 * regardless of type and capture)
 *
 * The returned listener will have its reference count increased. It is
 * the responsibility of the caller to unref the listener once it has
 * finished with it.
 */
dom_exception _dom_event_target_iter_event_listener(dom_event_target_internal eti,
		dom_string *type, bool capture,
                struct listener_entry *cur, struct listener_entry **next,
                dom_event_listener **listener)
{
	if (cur == NULL) {

		if (eti.listeners == NULL) {
			/* EventTarget has no listeners */
			*next = NULL;
			*listener = NULL;
			return DOM_NO_ERR;
		}

		/* If the current entry is not provided,
		 * set it to the beginning of the EventTarget's listeners list */
		cur = eti.listeners;

	} else if (cur == eti.listeners) {

		/* if the current entry is the beginning of the EventTarget's listeners,
		 * stop iteration */
		*next = NULL;
		*listener = NULL;
		return DOM_NO_ERR;
	}

	/* Set the next entry */
	struct listener_entry *le = (struct listener_entry *) cur->list.next;
	*next = le;

	/* Check the event type */
	if (type == NULL || (dom_string_isequal(le->type, type) &&
	    capture == le->capture)
	    ) {
		*listener = le->listener;
		dom_event_listener_ref(le->listener);
	} else {
		*listener = NULL;
	}
	return DOM_NO_ERR;
}
