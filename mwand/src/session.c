/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

/*
 * The command menu, and the optional session actions in it.
 *
 * xmtoolbox drove these through a private IPC with the xmsm session manager,
 * and hid entries according to flags xmsm published on the root window. mWand
 * has no session manager to talk to: each action simply runs a command the
 * user configured, exactly as mWizard's own f.shutdown and friends do. That
 * makes mWand usable under any window manager, and makes the whole session
 * section switchable off with one setting.
 *
 * "Execute..." lives here rather than in launcher.c because it belongs to
 * this menu; it stays available whether or not the session actions do.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/PushBG.h>
#include <Xm/CascadeBG.h>
#include <Xm/SeparatoG.h>
#include "common.h"
#include "mwand.h"

static void exec_item_cb(Widget, XtPointer, XtPointer);
static void command_item_cb(Widget, XtPointer, XtPointer);
static Widget AddItem(Widget wpulldown, const char *name, const char *label,
	KeySym mnemonic, XtCallbackProc cb, XtPointer data);

static void exec_item_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	ExecuteCommandDialog();
}

/*
 * Runs the command a session item was configured with. Items whose command is
 * empty are never created, so client_data is always a usable string here.
 */
static void command_item_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	const char *cmd = (const char*)client_data;
	char msg[512];

	if(!cmd || !(*cmd)) return;

	if(RunCommand(cmd) != 0) {
		snprintf(msg, sizeof(msg), "Could not run:\n%s", cmd);
		MessageDialog(False, msg);
	}
}

/*
 * Same for logout, reboot and shutdown, which are worth a confirmation.
 */
static void confirm_command_item_cb(Widget w, XtPointer client_data,
	XtPointer call_data)
{
	const char *cmd = (const char*)client_data;
	char msg[512];

	if(!cmd || !(*cmd)) return;

	snprintf(msg, sizeof(msg), "Are you sure?\n%s", cmd);
	if(!MessageDialog(True, msg)) return;

	if(RunCommand(cmd) != 0) {
		snprintf(msg, sizeof(msg), "Could not run:\n%s", cmd);
		MessageDialog(False, msg);
	}
}

static Widget AddItem(Widget wpulldown, const char *name, const char *label,
	KeySym mnemonic, XtCallbackProc cb, XtPointer data)
{
	XtCallbackRec cbr[2] = { { NULL, NULL }, { NULL, NULL } };
	XmString title;
	Widget w;
	Arg args[6];
	int n = 0;

	cbr[0].callback = cb;
	cbr[0].closure = data;

	title = XmStringCreateLocalized((char*)label);
	XtSetArg(args[n], XmNlabelString, title); n++;
	XtSetArg(args[n], XmNmnemonic, mnemonic); n++;
	XtSetArg(args[n], XmNactivateCallback, cbr); n++;
	w = XmCreatePushButtonGadget(wpulldown, (char*)name, args, n);
	XmStringFree(title);
	XtManageChild(w);

	return w;
}

/*
 * Builds the menu bar and its single cascade.
 *
 * The cascade is called "Session" when it carries session actions and
 * "Commands" when it does not, so that a menu holding nothing but
 * "Execute..." is not misleadingly labelled.
 */
Widget CreateCommandMenu(Widget wparent)
{
	Widget wmenu;
	Widget wpulldown;
	Widget wcascade;
	Widget w;
	XmString title;
	Arg args[10];
	int n;
	Boolean any_session = False;

	n = 0;
	XtSetArg(args[n], XmNshadowThickness, 0); n++;
	XtSetArg(args[n], XmNspacing, 1); n++;
	XtSetArg(args[n], XmNmarginWidth, 0); n++;
	XtSetArg(args[n], XmNorientation,
		(app_res.horizontal ? XmHORIZONTAL:XmVERTICAL)); n++;
	XtSetArg(args[n], XmNrowColumnType, XmMENU_BAR); n++;
	wmenu = XmCreateRowColumn(wparent, "menu", args, n);

	wpulldown = XmCreatePulldownMenu(wmenu, "commandPulldown", NULL, 0);

	if(app_res.session_menu) {
		any_session =
			(app_res.lock_command && *app_res.lock_command) ||
			(app_res.logout_command && *app_res.logout_command) ||
			(app_res.suspend_command && *app_res.suspend_command) ||
			(app_res.reboot_command && *app_res.reboot_command) ||
			(app_res.shutdown_command && *app_res.shutdown_command);
	}

	title = XmStringCreateLocalized(any_session ? "Session" : "Commands");
	n = 0;
	XtSetArg(args[n], XmNlabelString, title); n++;
	XtSetArg(args[n], XmNmnemonic, (KeySym)(any_session ? 'S' : 'C')); n++;
	XtSetArg(args[n], XmNsubMenuId, wpulldown); n++;
	wcascade = XmCreateCascadeButtonGadget(wmenu, "commands", args, n);
	XmStringFree(title);
	XtManageChild(wcascade);

	AddItem(wpulldown, "execute", "Execute...", (KeySym)'E',
		(XtCallbackProc)exec_item_cb, NULL);

	if(any_session) {
		w = XmCreateSeparatorGadget(wpulldown, "separator", NULL, 0);
		XtManageChild(w);

		/*
		 * An entry only exists when its command is set. There is no point
		 * showing "Lock" when nothing has been configured to lock with.
		 */
		if(app_res.lock_command && *app_res.lock_command)
			AddItem(wpulldown, "lock", "Lock", (KeySym)'L',
				(XtCallbackProc)command_item_cb, app_res.lock_command);

		if(app_res.suspend_command && *app_res.suspend_command)
			AddItem(wpulldown, "suspend", "Suspend", (KeySym)'u',
				(XtCallbackProc)command_item_cb, app_res.suspend_command);

		if(app_res.logout_command && *app_res.logout_command)
			AddItem(wpulldown, "logout", "Log Out...", (KeySym)'o',
				(XtCallbackProc)confirm_command_item_cb,
				app_res.logout_command);

		if(app_res.reboot_command && *app_res.reboot_command)
			AddItem(wpulldown, "reboot", "Reboot...", (KeySym)'R',
				(XtCallbackProc)confirm_command_item_cb,
				app_res.reboot_command);

		if(app_res.shutdown_command && *app_res.shutdown_command)
			AddItem(wpulldown, "shutdown", "Shut Down...", (KeySym)'D',
				(XtCallbackProc)confirm_command_item_cb,
				app_res.shutdown_command);
	}

	XtManageChild(wmenu);

	return wmenu;
}
