/* The lwlib interface to "xlwmenu" menus.
   Copyright (C) 1992, 1994 Lucid, Inc.

This file is part of the Lucid Widget Library.

The Lucid Widget Library is free software; you can redistribute it and/or 
modify it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

The Lucid Widget Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of 
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Emacs; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdlib.h> /* for abort () */
#include <limits.h>

#include "lwlib-Xlw.h"
#include <X11/StringDefs.h>
#include <X11/IntrinsicP.h>
#include <X11/ObjectP.h>
#include <X11/CompositeP.h>
#include <X11/Shell.h>
#ifdef MENUBARS_LUCID
#include "xlwmenu.h"
#endif
#ifdef SCROLLBARS_LUCID
#include "xlwscrollbar.h"
#endif



#ifdef MENUBARS_LUCID

/* Menu callbacks */

static void
pre_hook (Widget w, XtPointer client_data, XtPointer call_data)
{
  widget_instance* instance = (widget_instance*)client_data;
  widget_value* val;
  
  if (w->core.being_destroyed)
    return;

  val = lw_get_widget_value_for_widget (instance, w);
#if 0
  /* #### - this code used to (for some random back_asswards reason) pass
  the expression below in the call_data slot.  For incremental menu
  construction, this needs to go.  I can't even figure out why it was done
  this way in the first place...it's just a historical wierdism. --Stig */
  call_data = (val ? val->call_data : NULL);
#endif 
  if (val && val->call_data)
    abort();			/* #### - the call_data for the top_level
				   "menubar" widget_value used to be passed
				   back to the pre_hook. */

  if (instance->info->pre_activate_cb)
    instance->info->pre_activate_cb (w, instance->info->id, call_data);
}

static void
pick_hook (Widget w, XtPointer client_data, XtPointer call_data)
{
  widget_instance* instance = (widget_instance*)client_data;
  widget_value* contents_val = (widget_value*)call_data;
  widget_value* widget_val;
  XtPointer widget_arg;
  LWLIB_ID id;
  lw_callback post_activate_cb;

  if (w->core.being_destroyed)
    return;

  /* Grab these values before running any functions, in case running
     the selection_cb causes the widget to be destroyed. */
  id = instance->info->id;
  post_activate_cb = instance->info->post_activate_cb;

  widget_val = lw_get_widget_value_for_widget (instance, w);
  widget_arg = widget_val ? widget_val->call_data : NULL;

  if (instance->info->selection_cb &&
      contents_val &&
      contents_val->enabled &&
      !contents_val->contents)
    instance->info->selection_cb (w, id, contents_val->call_data);

  if (post_activate_cb)
    post_activate_cb (w, id, widget_arg);
}



/* creation functions */
static Widget
xlw_create_menubar (widget_instance* instance)
{
  Widget widget =
    XtVaCreateWidget (instance->info->name, xlwMenuWidgetClass,
		      instance->parent,
		      XtNmenu, instance->info->val,
		      0);
  XtAddCallback (widget, XtNopen, pre_hook, (XtPointer)instance);
  XtAddCallback (widget, XtNselect, pick_hook, (XtPointer)instance);
  return widget;
}

static Widget
xlw_create_popup_menu (widget_instance* instance)
{
  Widget popup_shell =
    XtCreatePopupShell (instance->info->name, overrideShellWidgetClass,
			instance->parent, NULL, 0);
  
  Widget widget = 
    XtVaCreateManagedWidget ("popup", xlwMenuWidgetClass,
			     popup_shell,
			     XtNmenu, instance->info->val,
			     XtNhorizontal, False,
			     0);

  XtAddCallback (widget, XtNselect, pick_hook, (XtPointer)instance);

  return popup_shell;
}
#endif /* MENUBARS_LUCID */

#ifdef SCROLLBARS_LUCID
static void
xlw_scrollbar_callback (Widget widget, XtPointer closure, XtPointer call_data)
{
  widget_instance *instance = (widget_instance *) closure;
  LWLIB_ID id;
  XlwScrollBarCallbackStruct *data =
    (XlwScrollBarCallbackStruct *) call_data;
  scroll_event event_data;
  scrollbar_values *val =
    (scrollbar_values *) instance->info->val->scrollbar_data;
  double percent;

  if (!instance || widget->core.being_destroyed)
    return;

  id = instance->info->id;

  percent = (double) (data->value - 1) / (double) (INT_MAX - 1);
  event_data.slider_value =
    (int) (percent * (double) (val->maximum - val->minimum)) + val->minimum;

  if (event_data.slider_value > (val->maximum - val->slider_size))
    event_data.slider_value = val->maximum - val->slider_size;
  else if (event_data.slider_value < val->minimum)
    event_data.slider_value = val->minimum;

  if (data->event)
    {
      switch (data->event->xany.type)
	{
	case KeyPress:
	case KeyRelease:
	  event_data.time = data->event->xkey.time;
	  break;
	case ButtonPress:
	case ButtonRelease:
	  event_data.time = data->event->xbutton.time;
	  break;
	case MotionNotify:
	  event_data.time = data->event->xmotion.time;
	  break;
	case EnterNotify:
	case LeaveNotify:
	  event_data.time = data->event->xcrossing.time;
	  break;
	default:
	  event_data.time = 0;
	  break;
	}
    }
  else
    event_data.time = 0;

  switch (data->reason)
    {
    case XmCR_DECREMENT:
      event_data.action = SCROLLBAR_LINE_UP;
      break;
    case XmCR_INCREMENT:
      event_data.action = SCROLLBAR_LINE_DOWN;
      break;
    case XmCR_PAGE_DECREMENT:
      event_data.action = SCROLLBAR_PAGE_UP;
      break;
    case XmCR_PAGE_INCREMENT:
      event_data.action = SCROLLBAR_PAGE_DOWN;
      break;
    case XmCR_TO_TOP:
      event_data.action = SCROLLBAR_TOP;
      break;
    case XmCR_TO_BOTTOM:
      event_data.action = SCROLLBAR_BOTTOM;
      break;
    case XmCR_DRAG:
      event_data.action = SCROLLBAR_DRAG;
      break;
    case XmCR_VALUE_CHANGED:
      event_data.action = SCROLLBAR_CHANGE;
      break;
    default:
      event_data.action = SCROLLBAR_CHANGE;
      break;
    }

  if (instance->info->pre_activate_cb)
    instance->info->pre_activate_cb (widget, id, (XtPointer) &event_data);
}

/* #### Does not yet support horizontal scrollbars. */
static Widget
xlw_create_scrollbar (widget_instance *instance, int vertical)
{
  Arg al[20];
  int ac = 0;
  Widget scrollbar;

  XtSetArg (al[ac], XmNminimum, 1); ac++;
  XtSetArg (al[ac], XmNmaximum, INT_MAX); ac++;
  XtSetArg (al[ac], XmNincrement, 1); ac++;
  XtSetArg (al[ac], XmNpageIncrement, 1); ac++;
  if (vertical)
    {
      XtSetArg (al[ac], XmNorientation, XmVERTICAL); ac++;
    }
  else
    {
      XtSetArg (al[ac], XmNorientation, XmHORIZONTAL); ac++;
    }

  scrollbar =
    XtCreateWidget (instance->info->name, xlwScrollBarWidgetClass, instance->parent, al, ac);

  XtAddCallback(scrollbar, XmNdecrementCallback, xlw_scrollbar_callback,
		(XtPointer) instance);
  XtAddCallback(scrollbar, XmNdragCallback, xlw_scrollbar_callback,
		(XtPointer) instance);
  XtAddCallback(scrollbar, XmNincrementCallback, xlw_scrollbar_callback,
		(XtPointer) instance);
  XtAddCallback(scrollbar, XmNpageDecrementCallback, xlw_scrollbar_callback,
		(XtPointer) instance);
  XtAddCallback(scrollbar, XmNpageIncrementCallback, xlw_scrollbar_callback,
		(XtPointer) instance);
  XtAddCallback(scrollbar, XmNtoBottomCallback, xlw_scrollbar_callback,
		(XtPointer) instance);
  XtAddCallback(scrollbar, XmNtoTopCallback, xlw_scrollbar_callback,
		(XtPointer) instance);
  XtAddCallback(scrollbar, XmNvalueChangedCallback, xlw_scrollbar_callback,
		(XtPointer) instance);

  return scrollbar;
}

static Widget
xlw_create_vertical_scrollbar (widget_instance *instance)
{
  return xlw_create_scrollbar (instance, 1);
}

static Widget
xlw_create_horizontal_scrollbar (widget_instance *instance)
{
  return xlw_create_scrollbar (instance, 0);
}

static void
xlw_update_scrollbar (widget_instance *instance, Widget widget,
		      widget_value *val)
{
  if (val->scrollbar_data)
    {
      scrollbar_values *data = val->scrollbar_data;
      int widget_sliderSize, widget_val;
      int new_sliderSize, new_value;
      double percent;

      /*
       * First size and position the scrollbar widget.
       */
      XtVaSetValues (widget,
		     XtNx, data->scrollbar_x,
		     XtNy, data->scrollbar_y,
		     XtNwidth, data->scrollbar_width,
		     XtNheight, data->scrollbar_height,
		     0);

      /*
       * Now the size the scrollbar's slider.
       */

      XtVaGetValues (widget,
		     XmNsliderSize, &widget_sliderSize,
		     XmNvalue, &widget_val,
		     0);

      percent = (double) data->slider_size /
	(double) (data->maximum - data->minimum);
      percent = (percent > 1.0 ? 1.0 : percent);
      new_sliderSize = (int) ((double) (INT_MAX - 1) * percent);

      percent = (double) (data->slider_position - data->minimum) /
	(double) (data->maximum - data->minimum);
      percent = (percent > 1.0 ? 1.0 : percent);
      new_value = (int) ((double) (INT_MAX - 1) * percent);

      if (new_sliderSize > (INT_MAX - 1))
	new_sliderSize = INT_MAX - 1;
      if (new_sliderSize < 1)
	new_sliderSize = 1;

      if (new_value > (INT_MAX - new_sliderSize))
	new_value = INT_MAX - new_sliderSize;
      else if (new_value < 1)
	new_value = 1;

      if (new_sliderSize != widget_sliderSize || new_value != widget_val)
	XlwScrollBarSetValues (widget, new_value, new_sliderSize, 1, 1, False);
    }
}

#endif /* SCROLLBARS_LUCID */

widget_creation_entry 
xlw_creation_table [] =
{
#ifdef MENUBARS_LUCID
  {"menubar", xlw_create_menubar},
  {"popup", xlw_create_popup_menu},
#endif
#ifdef SCROLLBARS_LUCID
  {"vertical-scrollbar",	xlw_create_vertical_scrollbar},
  {"horizontal-scrollbar",	xlw_create_horizontal_scrollbar},
#endif
  {NULL, NULL}
};

Boolean
lw_lucid_widget_p (Widget widget)
{
  WidgetClass the_class = XtClass (widget);
#ifdef MENUBARS_LUCID
  if (the_class == xlwMenuWidgetClass)
    return True;
#endif
#ifdef SCROLLBARS_LUCID
  if (the_class == xlwScrollBarWidgetClass)
    return True;
#endif
#ifdef MENUBARS_LUCID
  if (the_class == overrideShellWidgetClass)
    return
      XtClass (((CompositeWidget)widget)->composite.children [0])
	== xlwMenuWidgetClass;
#endif
  return False;
}

void
xlw_update_one_widget (widget_instance* instance, Widget widget,
		       widget_value* val, Boolean deep_p)
{
  WidgetClass class;

  class = XtClass (widget);

  if (0)
    ;
#ifdef MENUBARS_LUCID
  else if (class == xlwMenuWidgetClass)
    {
      XlwMenuWidget mw;
      if (XtIsShell (widget))
	mw = (XlwMenuWidget)((CompositeWidget)widget)->composite.children [0];
      else
	mw = (XlwMenuWidget)widget;
      XtVaSetValues (widget, XtNmenu, val, 0);
    }
#endif
#ifdef SCROLLBARS_LUCID
  else if (class == xlwScrollBarWidgetClass)
    {
      xlw_update_scrollbar (instance, widget, val);
    }
#endif
}

void
xlw_update_one_value (widget_instance* instance, Widget widget,
		      widget_value* val)
{
  return;
}

void
xlw_pop_instance (widget_instance* instance, Boolean up)
{
}

#ifdef MENUBARS_LUCID
void
xlw_popup_menu (Widget widget, XEvent *event)
{
  XlwMenuWidget mw;

  if (!XtIsShell (widget))
    return;

  if (event->type == ButtonPress || event->type == ButtonRelease)
    {
      mw = (XlwMenuWidget)((CompositeWidget)widget)->composite.children [0];
      xlw_pop_up_menu (mw, (XButtonPressedEvent *)event);
    }
  else
    abort ();
}
#endif

/* Destruction of instances */
void
xlw_destroy_instance (widget_instance* instance)
{
  if (instance->widget)
    XtDestroyWidget (instance->widget);
}

