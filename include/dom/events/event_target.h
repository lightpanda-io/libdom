/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_events_event_target_h_
#define dom_events_event_target_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>

struct dom_event_listener;
struct dom_event;

struct listener_entry;

/* Event target is a mixin interface, thus has no concrete implementation. 
 * Subclasses must provide implementations of the event target methods. */
typedef struct dom_event_target {
	void *vtable;
} dom_event_target;

typedef struct dom_event_target_vtable {
	dom_exception (*add_event_listener)(
			dom_event_target *et, dom_string *type,
			struct dom_event_listener *listener,
			bool capture);
	dom_exception (*remove_event_listener)(
			dom_event_target *et, dom_string *type,
			struct dom_event_listener *listener,
			bool capture);
	dom_exception (*dispatch_event)(
			dom_event_target *et,
			struct dom_event *evt, bool *success);
	dom_exception (*add_event_listener_ns)(
			dom_event_target *et, 
			dom_string *namespace, dom_string *type,
			struct dom_event_listener *listener,
			bool capture);
	dom_exception (*remove_event_listener_ns)(
			dom_event_target *et,
			dom_string *namespace, dom_string *type,
			struct dom_event_listener *listener,
			bool capture);
	dom_exception (*iter_event_listener)(
			dom_event_target *et,
			dom_string *type, bool capture,
			struct listener_entry *cur,
			struct listener_entry **next,
			struct dom_event_listener **listener);
	dom_exception (*internal_type)(
			dom_event_target *et,
			unsigned int *internal_type);
} dom_event_target_vtable;

static inline dom_exception dom_event_target_add_event_listener(
		dom_event_target *et, dom_string *type, 
		struct dom_event_listener *listener, bool capture)
{
	return ((dom_event_target_vtable *) et->vtable)->add_event_listener(
			et, type, listener, capture);
}
#define dom_event_target_add_event_listener(et, t, l, c) \
		dom_event_target_add_event_listener((dom_event_target *) (et),\
		(dom_string *) (t), (struct dom_event_listener *) (l), \
		(bool) (c))

static inline dom_exception dom_event_target_remove_event_listener(
		dom_event_target *et, dom_string *type, 
		struct dom_event_listener *listener, bool capture)
{
	return ((dom_event_target_vtable *) et->vtable)->remove_event_listener(
			et, type, listener, capture);
}
#define dom_event_target_remove_event_listener(et, t, l, c) \
		dom_event_target_remove_event_listener(\
		(dom_event_target *) (et), (dom_string *) (t),\
		(struct dom_event_listener *) (l), (bool) (c))

static inline dom_exception dom_event_target_dispatch_event(
		dom_event_target *et, struct dom_event *evt, bool *success)
{
	return ((dom_event_target_vtable *) et->vtable)->dispatch_event(
			et, evt, success);
}
#define dom_event_target_dispatch_event(et, e, s) \
		dom_event_target_dispatch_event((dom_event_target *) (et),\
		(struct dom_event *) (e), (bool *) (s))

static inline dom_exception dom_event_target_add_event_listener_ns(
		dom_event_target *et,
		dom_string *namespace, dom_string *type, 
		struct dom_event_listener *listener, bool capture)
{
	return ((dom_event_target_vtable *) et->vtable)->add_event_listener_ns(
			et, namespace, type, listener, capture);
}
#define dom_event_target_add_event_listener_ns(et, n, t, l, c) \
		dom_event_target_add_event_listener_ns(\
		(dom_event_target *) (et), (dom_string *) (n),\
		(dom_string *) (t), (struct dom_event_listener *) (l),\
		(bool) (c))

static inline dom_exception dom_event_target_remove_event_listener_ns(
		dom_event_target *et,
		dom_string *namespace, dom_string *type, 
		struct dom_event_listener *listener, bool capture)
{
	return ((dom_event_target_vtable *) et->vtable)->remove_event_listener_ns(
			et, namespace, type, listener, capture);
}
#define dom_event_target_remove_event_listener_ns(et, n, t, l, c) \
		dom_event_target_remove_event_listener_ns(\
		(dom_event_target *) (et), (dom_string *) (n),\
		(dom_string *) (t), (struct dom_event_listener *) (l),\
		(bool) (c))

static inline dom_exception dom_event_iter_event_listener(
		dom_event_target *et,
		dom_string *type, bool capture,
		struct listener_entry *cur, struct listener_entry **next,
		struct dom_event_listener **listener)
{
	return ((dom_event_target_vtable *)et->vtable)
		->iter_event_listener(
			et, type, capture, cur, next, listener);
}
#define dom_event_target_iter_event_listener(et, t, ct, c, n, l)	\
		dom_event_target_iter_event_listener(\
		(dom_event_target *) (et),\
		(dom_string *) (t), (bool) (ct),\
		(struct listener_entry *) (c), (struct listener_entry **) (n),\
		(struct dom_event_listener **) (l))

#endif

