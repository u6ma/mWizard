/*
 * Copyright (C) 2018-2026 alx@fastestcode.org
 *
 * Modified 2026 for mWizard's mWand. See NOTICE for details.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Workspace switcher plumbing.
 *
 * The switcher widget itself is in wswitch.c. This drives it from the
 * EWMH desktop properties, which is all that is needed for indexed
 * switching and works with any EWMH window manager.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/PushBG.h>
#include <Xm/CascadeBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/Frame.h>
#include <Xm/LabelG.h>
#include <Xm/SelectioB.h>
#include <Xm/TextF.h>
#include <Xm/MessageB.h>
#include <Xm/MwmUtil.h>
#include <X11/cursorfont.h>
#include "rcparse.h"
#include "common.h"
#include "wswitch.h"
#include "mwand.h"

Widget wswitch = None;
Atom xa_ndesks = None;
Atom xa_cdesk = None;

static void ws_change_cb(Widget, XtPointer, XtPointer);

Boolean GetWorkspaceInfo(unsigned short *ws_count, unsigned short *iactive)
{
	Boolean success = True;
	Display *dpy = XtDisplay(wshell);
	Window root = RootWindowOfScreen(XtScreen(wshell));

	Atom ret_type;
	int ret_format;
	unsigned long ret_items;
	unsigned long left_items;
	unsigned char *prop_data;
	
	if(xa_ndesks == None || xa_cdesk == None) return False;
	
	XGetWindowProperty(dpy, root, xa_ndesks, 0, sizeof(unsigned long),
			False, XA_CARDINAL, &ret_type, &ret_format, &ret_items,
			&left_items, &prop_data);
	if(ret_items) {
		*ws_count = (unsigned short)*prop_data;
		XFree(prop_data);
	} else {
		ws_count = 0;
		success = False;
	}

	XGetWindowProperty(dpy, root, xa_cdesk, 0, sizeof(unsigned long),
			False, XA_CARDINAL, &ret_type, &ret_format, &ret_items,
			&left_items, &prop_data);

	if(ret_items) {
		*iactive = (unsigned short)*prop_data;
		XFree(prop_data);
	} else {
		*iactive = 0;
		success = False;
	}

	return success;
}

static void ws_change_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	Display *dpy = XtDisplay(wshell);
	Window root_wnd = RootWindowOfScreen(XtScreen(wshell));
	short *index = (short*)call_data;

	XClientMessageEvent evt = {
		.type = ClientMessage,
		.display = dpy,
		.window = root_wnd,
		.message_type = xa_cdesk,
		.format = 32
	};

	evt.data.l[0] = (long)*index;
	evt.data.l[1] = CurrentTime;
	XSendEvent(dpy, root_wnd, False,
		SubstructureRedirectMask | SubstructureNotifyMask, (XEvent*)&evt);

}

/*
 * Creates the workspace switcher. As with the clock it is always created and
 * only managed when it is both wanted and useful -- a single workspace needs
 * no switcher.
 */
Widget CreateSwitcherWidget(Widget wparent)
{
	XtCallbackRec cbr[2] = { { NULL, NULL } };
	unsigned short nws = 0;
	unsigned short iws = 0;
	Arg args[8];
	int n = 0;

	if(GetWorkspaceInfo(&nws, &iws)) {
		XtSetArg(args[n], NnumberOfWorkspaces, nws); n++;
		XtSetArg(args[n], NactiveWorkspace, iws); n++;
	}
	cbr[0].callback = ws_change_cb;
	XtSetArg(args[n], XmNvalueChangedCallback, &cbr); n++;

	wswitch = CreateSwitcher(wparent, "workspaceSwitcher", args, n);

	if(app_res.switcher && (nws > 1)) XtManageChild(wswitch);

	return wswitch;
}

/*
 * Re-reads the desktop properties and updates the switcher to match.
 */
void UpdateSwitcher(void)
{
	unsigned short nws, iws;

	if(wswitch && GetWorkspaceInfo(&nws, &iws))
		SwitcherSetActiveWorkspace(wswitch, iws);
}
