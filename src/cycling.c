/* cycling.c- window cycling
 * 
 *  Window Maker window manager
 * 
 *  Copyright (c) 2000 Alfredo K. Kojima
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
 */

#include "wconfig.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>


#include "WindowMaker.h"
#include "GNUstep.h"
#include "screen.h"
#include "wcore.h"
#include "window.h"
#include "framewin.h"
#include "keybind.h"
#include "actions.h"
#include "stacking.h"
#include "funcs.h"

/* Globals */
extern WPreferences wPreferences;

extern WShortKey wKeyBindings[WKBD_LAST];








static WWindow*
nextToFocusAfter(WWindow *wwin)
{
    WWindow *tmp = wwin->prev;

    while (tmp) {
	if (wWindowCanReceiveFocus(tmp) && !WFLAGP(tmp, skip_window_list)) {
	    return tmp;
	}
	tmp = tmp->prev;
    }

    tmp = wwin;
    /* start over from the beginning of the list */
    while (tmp->next)
	tmp = tmp->next;

    while (tmp && tmp != wwin) {
	if (wWindowCanReceiveFocus(tmp) && !WFLAGP(tmp, skip_window_list)) {
	    return tmp;
	}
	tmp = tmp->prev;
    }

    return wwin;
}


static WWindow*
nextToFocusBefore(WWindow *wwin)
{
    WWindow *tmp = wwin->next;

    while (tmp) {
	if (wWindowCanReceiveFocus(tmp) && !WFLAGP(tmp, skip_window_list)) {
	    return tmp;
	}
	tmp = tmp->next;
    }

    /* start over from the beginning of the list */
    tmp = wwin;
    while (tmp->prev)
	tmp = tmp->prev;

    while (tmp && tmp != wwin) {
	if (wWindowCanReceiveFocus(tmp) && !WFLAGP(tmp, skip_window_list)) {

	    return tmp;
	}
	tmp = tmp->next;
    }

    return wwin;
}


void
StartWindozeCycle(WWindow *wwin, XEvent *event, Bool next)
{
    WScreen *scr = wScreenForRootWindow(event->xkey.root);
    Bool done = False;
    Bool openedSwitchMenu = False;
    WWindow *newFocused;
    WWindow *oldFocused;
    int modifiers;
    XModifierKeymap *keymap;
    Bool somethingElse = False;
    XEvent ev;

    if (!wwin)
	return;

    keymap = XGetModifierMapping(dpy);

    
    XGrabKeyboard(dpy, scr->root_win, False, GrabModeAsync, GrabModeAsync, 
		  CurrentTime);

    if (next) {
	newFocused = nextToFocusAfter(wwin);
    } else {
	newFocused = nextToFocusBefore(wwin); 
    }

    scr->flags.doing_alt_tab = 1;


    if (wPreferences.circ_raise)
	XRaiseWindow(dpy, newFocused->frame->core->window);
    wWindowFocus(newFocused, scr->focused_window);
    oldFocused = newFocused;

#if 0
    if (wPreferences.popup_switchmenu && 
	(!scr->switch_menu || !scr->switch_menu->flags.mapped)) {

	OpenSwitchMenu(scr, scr->scr_width/2, scr->scr_height/2, False);
	openedSwitchMenu = True;
    }
#endif	
    while (!done) {
	WMMaskEvent(dpy,KeyPressMask|KeyReleaseMask|ExposureMask, &ev);

	if (ev.type != KeyRelease && ev.type != KeyPress) {
	    WMHandleEvent(&ev);
	    continue;
	}
	/* ignore CapsLock */
	modifiers = ev.xkey.state & ValidModMask;

	if (ev.type == KeyPress) {
	    if (wKeyBindings[WKBD_FOCUSNEXT].keycode == ev.xkey.keycode
		&& wKeyBindings[WKBD_FOCUSNEXT].modifier == modifiers) {

		UpdateSwitchMenu(scr, newFocused, ACTION_CHANGE_STATE);
		newFocused = nextToFocusAfter(newFocused);
		wWindowFocus(newFocused, oldFocused);
		oldFocused = newFocused;
	    
		if (wPreferences.circ_raise) {
		    /* restore order */
		    CommitStacking(scr);
		    XRaiseWindow(dpy, newFocused->frame->core->window);
		}

		UpdateSwitchMenu(scr, newFocused, ACTION_CHANGE_STATE);

	    } else if (wKeyBindings[WKBD_FOCUSPREV].keycode == ev.xkey.keycode
		       && wKeyBindings[WKBD_FOCUSPREV].modifier == modifiers) {

		UpdateSwitchMenu(scr, newFocused, ACTION_CHANGE_STATE);
		newFocused = nextToFocusBefore(newFocused);
		wWindowFocus(newFocused, oldFocused);
		oldFocused = newFocused;

		if (wPreferences.circ_raise) {
		    /* restore order */
		    CommitStacking(scr);
		    XRaiseWindow(dpy, newFocused->frame->core->window);
		}
		UpdateSwitchMenu(scr, newFocused, ACTION_CHANGE_STATE);

	    } else {
		somethingElse = True;
		done = True;
	    }
	} else if (ev.type == KeyRelease) {
	    int i;

	    for (i = 0; i < 8 * keymap->max_keypermod; i++) {
		if (keymap->modifiermap[i] == ev.xkey.keycode &&
		    wKeyBindings[WKBD_FOCUSNEXT].modifier 
		    & 1<<(i/keymap->max_keypermod)) {
		    done = True;
		    break;
		}
	    }
	}
    }
    XFreeModifiermap(keymap);

    XUngrabKeyboard(dpy, CurrentTime);
    wSetFocusTo(scr, newFocused);

    if (wPreferences.circ_raise) {
	wRaiseFrame(newFocused->frame->core);
    	CommitStacking(scr);
    }

    scr->flags.doing_alt_tab = 0;
    if (openedSwitchMenu) 
	OpenSwitchMenu(scr, scr->scr_width/2, scr->scr_height/2, False);
    
    if (somethingElse) {
	WMHandleEvent(&ev);
    }
}






static WWindow*
nextFocusWindow(WScreen *scr)
{
    WWindow *tmp, *wwin, *closest, *min;
    Window d;

    if (!(wwin = scr->focused_window))
        return NULL;
    tmp = wwin->prev;
    closest = NULL;
    min = wwin;
    d = 0xffffffff;
    while (tmp) {
        if (wWindowCanReceiveFocus(tmp) 
            && (!WFLAGP(tmp, skip_window_list)|| tmp->flags.internal_window)) {
            if (min->client_win > tmp->client_win)
              min = tmp;
            if (tmp->client_win > wwin->client_win
                && (!closest
                    || (tmp->client_win - wwin->client_win) < d)) {
                closest = tmp;
                d = tmp->client_win - wwin->client_win;
            }
        }
        tmp = tmp->prev;
    }
    if (!closest||closest==wwin)
      return min;
    return closest;
}


static WWindow*
prevFocusWindow(WScreen *scr)
{
    WWindow *tmp, *wwin, *closest, *max;
    Window d;
    
    if (!(wwin = scr->focused_window))
        return NULL;
    tmp = wwin->prev;
    closest = NULL;
    max = wwin;
    d = 0xffffffff;
    while (tmp) {
        if (wWindowCanReceiveFocus(tmp) &&
            (!WFLAGP(tmp, skip_window_list) || tmp->flags.internal_window)) {
            if (max->client_win < tmp->client_win)
              max = tmp;
            if (tmp->client_win < wwin->client_win
                && (!closest
                    || (wwin->client_win - tmp->client_win) < d)) {
                closest = tmp;
                d = wwin->client_win - tmp->client_win;
            }
        }
        tmp = tmp->prev;
    }
    if (!closest||closest==wwin)
      return max;
    return closest;
}


void CycleWindow(WScreen *scr, Bool forward)
{
    WWindow *wwin;
    
    if (forward)
	wwin = nextFocusWindow(scr);
    else
	wwin = prevFocusWindow(scr);
    
    if (wwin != NULL)
	wSetFocusTo(scr, wwin);
}
