/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

/*
 * mWinfo: the About window.
 *
 * What the name is, what version is running, who holds the copyrights and
 * under what terms, and where the source lives. The same job winver does on
 * Windows and the Info panel does on Window Maker.
 *
 * Reachable three ways: the f.about rc function (bound to a key or a menu
 * item), SIGUSR2, which is what mWand's "About mWizard..." item sends, and
 * therefore any binding the user cares to add.
 *
 * ---------------------------------------------------------------------------
 *
 * Built the way every other window mWizard puts on the screen is built, for
 * reasons that are written out at the top of WmExecDlg.c and not repeated
 * here: a plain Xt TransientShell on the second display connection's
 * per-screen shell, with XtNwaitForWm off, explicit depth/screen/colormap,
 * and positioned before it is realized.
 *
 * Where this one differs is that it is meant to behave like an ordinary
 * window -- movable, resizable, and closable from its frame -- so unlike the
 * Execute dialog it does not withhold MWM_FUNC_CLOSE. That makes
 * WM_DELETE_WINDOW mandatory rather than optional: F_Kill() falls through to
 * XKillClient() on a client that offers no such protocol, and this client is
 * mWizard's own second display connection, so being killed that way would
 * take the X session down. The protocol is registered below and the
 * ClientMessage handled in WinfoProtocolHandler().
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/SeparatoG.h>
#include <Xm/Text.h>
#include <Xm/ScrolledW.h>

#include "WmGlobal.h"
#include "WmWinfo.h"
#include "WmEwmh.h"
#include "WmSession.h"
#include "WmFunction.h"
#include "WmError.h"
#include "WmXinerama.h"

static Boolean MakeWinfoDialog(WmScreenData *pSD);
static void PlaceWinfoDialog(WmScreenData *pSD);
static void UnpostWinfoDialog(void);
static void WinfoCloseCB(Widget, XtPointer, XtPointer);
static void WinfoUrlCB(Widget, XtPointer, XtPointer);
static void WinfoProtocolHandler(Widget, XtPointer, XEvent*, Boolean*);
static void WinfoSignalProc(XtPointer, XtSignalId*);
static void WinfoSignalHandler(int);

static Widget winfoShellW = NULL;
static Widget winfoFormW = NULL;
static WmScreenData *winfoPSD = NULL;
static Boolean winfoOnScreen = False;
static XtSignalId winfoSignalId;

/*
 * The notice, kept short enough to read.
 *
 * Written as unwrapped paragraphs: the text widget wraps to whatever width the
 * window has, so a newline in the middle of a sentence would only get wrapped
 * again and come out ragged. The line breaks that are here are the ones that
 * are meant -- the blank lines between paragraphs, and the two copyright
 * lines.
 *
 * This has to stay true to the NOTICE file at the top of the tree: naming the
 * licenses is the whole point of the window, and getting them wrong here
 * would be worse than not having it. Anything that changes there should
 * change here too.
 */
/*
 * What it is, ahead of the legal part.
 */
static const char winfoAboutText[] =
MWM_FULL_NAME " is a window manager for the X Window System: a lighter, "
"opinionated build of EMWM, which derives in turn from the Motif Window "
"Manager published by The Open Group.\n"
"\n"
"It keeps the Motif look and the mwm resource model, and configures its "
"behaviour from a single rc file rather than from the X resource database, "
"which is left holding appearance alone.";

static const char winfoLicenseText[] =
MWM_FULL_NAME " is a fork of EMWM, which derives from the Motif Window Manager "
"originally published by The Open Group.\n"
"\n"
"Copyright (c) 1987-2012 The Open Group\n"
"Copyright (c) 2018-2026 alx@fastestcode.org\n"
"\n"
"The project as a whole is distributed under the GNU Lesser General Public "
"License, version 2.1 or (at your option) any later version; see the file "
"COPYING. Files written for " MWM_NAME ", and a few inherited from EMWM, are "
"under the MIT license; see COPYING.MIT. Individual files keep their own "
"copyright and license headers.\n"
"\n"
"Not affiliated with, sponsored by, or endorsed by The Open Group. "
"\"Motif\" is a trademark of The Open Group.";

/*
 * Builds the window for one screen. Returns False and leaves nothing behind
 * if any part of it could not be made.
 */
static Boolean MakeWinfoDialog(WmScreenData *pSD)
{
    Arg args[24];
    int n;
    char buf[256];
    char body[4096];
    XmString xms;
    Widget wname, wversion, wsep1, wtext;
    Widget wsep2, wbuttons, wclose, wproject;
    Atom deleteAtom;

    if (!pSD->screenTopLevelW1) return (False);

    /*
     * See the note at the top of the file, and the longer one in
     * WmExecDlg.c, for why this is a plain Xt shell with XtNwaitForWm off.
     */
    n = 0;
    XtSetArg (args[n], XtNwaitForWm, (XtArgVal) False);			n++;
    XtSetArg (args[n], XtNallowShellResize, (XtArgVal) True);		n++;
    XtSetArg (args[n], XtNtitle, (XtArgVal) "About " MWM_NAME);		n++;
    XtSetArg (args[n], XtNiconName, (XtArgVal) "mWinfo");		n++;
    XtSetArg (args[n], XtNdepth,
	(XtArgVal) DefaultDepth (DISPLAY1, pSD->screen));		n++;
    XtSetArg (args[n], XtNscreen,
	(XtArgVal) ScreenOfDisplay (DISPLAY1, pSD->screen));		n++;
    XtSetArg (args[n], XtNcolormap,
	(XtArgVal) DefaultColormap (DISPLAY1, pSD->screen));		n++;

    winfoShellW = XtCreatePopupShell ("mwinfo", transientShellWidgetClass,
				      pSD->screenTopLevelW1, args, n);
    if (!winfoShellW) return (False);

    n = 0;
    XtSetArg (args[n], XmNmarginWidth, (XtArgVal) 10);			n++;
    XtSetArg (args[n], XmNmarginHeight, (XtArgVal) 10);			n++;
    XtSetArg (args[n], XmNhorizontalSpacing, (XtArgVal) 8);		n++;
    XtSetArg (args[n], XmNverticalSpacing, (XtArgVal) 8);		n++;
    winfoFormW = XtCreateManagedWidget ("winfoForm", xmFormWidgetClass,
					winfoShellW, args, n);
    if (!winfoFormW)
    {
	XtDestroyWidget (winfoShellW);
	winfoShellW = NULL;
	return (False);
    }

    /*
     * The buttons are attached to the bottom and the notice to what is left
     * between, so that making the window bigger gives the space to the text
     * rather than stranding the buttons in the middle of it.
     */
    /*
     * Both buttons sit together in the middle: each keeps its natural width
     * and one edge is attached to the halfway position, so they meet there.
     *
     * Attaching both edges of a button by position instead would stretch it
     * to that share of the form, and the form's preferred width would then
     * come out as buttonWidth divided by the share -- making the buttons,
     * rather than the text, decide how wide the window has to be.
     */
    n = 0;
    XtSetArg (args[n], XmNfractionBase, (XtArgVal) 100);		n++;
    XtSetArg (args[n], XmNleftAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNrightAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNbottomAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    wbuttons = XtCreateManagedWidget ("buttons", xmFormWidgetClass,
				      winfoFormW, args, n);

    xms = XmStringCreateLocalized ("Project Page");
    n = 0;
    XtSetArg (args[n], XmNlabelString, (XtArgVal) xms);			n++;
    XtSetArg (args[n], XmNrightAttachment,
	(XtArgVal) XmATTACH_POSITION);					n++;
    XtSetArg (args[n], XmNrightPosition, (XtArgVal) 50);			n++;
    XtSetArg (args[n], XmNrightOffset, (XtArgVal) 4);			n++;
    XtSetArg (args[n], XmNtopAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNbottomAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    wproject = XtCreateManagedWidget ("projectButton", xmPushButtonWidgetClass,
				      wbuttons, args, n);
    XmStringFree (xms);
    XtAddCallback (wproject, XmNactivateCallback,
	(XtCallbackProc) WinfoUrlCB, (XtPointer) NULL);

    xms = XmStringCreateLocalized ("Close");
    n = 0;
    XtSetArg (args[n], XmNlabelString, (XtArgVal) xms);			n++;
    XtSetArg (args[n], XmNleftAttachment,
	(XtArgVal) XmATTACH_POSITION);					n++;
    XtSetArg (args[n], XmNleftPosition, (XtArgVal) 50);			n++;
    XtSetArg (args[n], XmNleftOffset, (XtArgVal) 4);			n++;
    XtSetArg (args[n], XmNtopAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNbottomAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNshowAsDefault, (XtArgVal) 1);			n++;
    wclose = XtCreateManagedWidget ("closeButton", xmPushButtonWidgetClass,
				    wbuttons, args, n);
    XmStringFree (xms);
    XtAddCallback (wclose, XmNactivateCallback,
	(XtCallbackProc) WinfoCloseCB, (XtPointer) NULL);

    n = 0;
    XtSetArg (args[n], XmNdefaultButton, (XtArgVal) wclose);		n++;
    XtSetValues (winfoFormW, args, n);

    n = 0;
    XtSetArg (args[n], XmNleftAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNrightAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNbottomAttachment, (XtArgVal) XmATTACH_WIDGET);n++;
    XtSetArg (args[n], XmNbottomWidget, (XtArgVal) wbuttons);		n++;
    wsep2 = XtCreateManagedWidget ("separator", xmSeparatorGadgetClass,
				   winfoFormW, args, n);

    /* Top down: the name, the version, a rule, then the notice. */
    xms = XmStringCreateLocalized (MWM_FULL_NAME);
    n = 0;
    XtSetArg (args[n], XmNlabelString, (XtArgVal) xms);			n++;
    XtSetArg (args[n], XmNalignment, (XtArgVal) XmALIGNMENT_CENTER);	n++;
    XtSetArg (args[n], XmNtopAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNleftAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNrightAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    wname = XtCreateManagedWidget ("productName", xmLabelWidgetClass,
				   winfoFormW, args, n);
    XmStringFree (xms);

    snprintf (buf, sizeof (buf),
	      "%s %d.%d.%d \"%s\"\nMotif %d.%d.%d",
	      MWM_NAME, MWM_VERSION, MWM_REVISION, MWM_PATCHLEVEL,
	      MWM_CODENAME, XmVERSION, XmREVISION, XmUPDATE_LEVEL);

    xms = XmStringCreateLocalized (buf);
    n = 0;
    XtSetArg (args[n], XmNlabelString, (XtArgVal) xms);			n++;
    XtSetArg (args[n], XmNalignment, (XtArgVal) XmALIGNMENT_CENTER);	n++;
    XtSetArg (args[n], XmNtopAttachment, (XtArgVal) XmATTACH_WIDGET);	n++;
    XtSetArg (args[n], XmNtopWidget, (XtArgVal) wname);			n++;
    XtSetArg (args[n], XmNleftAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNrightAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    wversion = XtCreateManagedWidget ("version", xmLabelWidgetClass,
				      winfoFormW, args, n);
    XmStringFree (xms);

    n = 0;
    XtSetArg (args[n], XmNtopAttachment, (XtArgVal) XmATTACH_WIDGET);	n++;
    XtSetArg (args[n], XmNtopWidget, (XtArgVal) wversion);		n++;
    XtSetArg (args[n], XmNleftAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNrightAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    wsep1 = XtCreateManagedWidget ("separator", xmSeparatorGadgetClass,
				   winfoFormW, args, n);

    /*
     * The notice is a scrolled read-only text rather than a label.
     *
     * A label is exactly as tall as its string and cannot give any of it
     * back: shrink the window and the bottom lines are simply cut off with
     * no way to reach them. This wraps to the width it is given and puts a
     * scrollbar on whatever does not fit, so the text stays readable at any
     * size the window is dragged to. Read-only, but still selectable, so it
     * can be copied.
     */
    /*
     * Assembled here rather than held as one string, because the middle of it
     * is what this particular session is actually running -- the sort of
     * thing that otherwise means xdpyinfo and a guess.
     */
    {
	int xsiCount = 0;
	Boolean haveXinerama = (GetXineramaScreenCount (&xsiCount) &&
				xsiCount > 1);
	const char *extensions;

	if (haveXinerama && wmGD.xrandr_present)
	    extensions = "Xinerama, XRandR";
	else if (haveXinerama)
	    extensions = "Xinerama";
	else if (wmGD.xrandr_present)
	    extensions = "XRandR";
	else
	    extensions = "none detected";

	snprintf (body, sizeof (body),
		  "%s\n"
		  "\n"
		  "This session\n"
		  "Display: %s\n"
		  "Screens: %d\n"
		  "Workspaces: %d\n"
		  "Server: %s\n"
		  "Release: %d\n"
		  "Extensions: %s\n"
		  "Toolkit: Motif %d.%d.%d\n"
		  "\n"
		  "%s",
		  winfoAboutText,
		  DisplayString (DISPLAY),
		  wmGD.numScreens,
		  pSD->numWorkspaces,
		  ServerVendor (DISPLAY),
		  (int) VendorRelease (DISPLAY),
		  extensions,
		  XmVERSION, XmREVISION, XmUPDATE_LEVEL,
		  winfoLicenseText);
    }

    n = 0;
    XtSetArg (args[n], XmNvalue, (XtArgVal) body);			n++;
    XtSetArg (args[n], XmNeditMode, (XtArgVal) XmMULTI_LINE_EDIT);	n++;
    XtSetArg (args[n], XmNeditable, (XtArgVal) False);			n++;
    XtSetArg (args[n], XmNcursorPositionVisible, (XtArgVal) False);	n++;
    XtSetArg (args[n], XmNwordWrap, (XtArgVal) True);			n++;
    XtSetArg (args[n], XmNscrollHorizontal, (XtArgVal) False);		n++;
    XtSetArg (args[n], XmNrows, (XtArgVal) 18);				n++;
    XtSetArg (args[n], XmNcolumns, (XtArgVal) 24);			n++;
    wtext = XmCreateScrolledText (winfoFormW, "license", args, n);
    XtManageChild (wtext);

    /*
     * XmCreateScrolledText hands back the text, but the form's child is the
     * scrolled window wrapped around it, so that is what carries the
     * attachments.
     */
    n = 0;
    XtSetArg (args[n], XmNtopAttachment, (XtArgVal) XmATTACH_WIDGET);	n++;
    XtSetArg (args[n], XmNtopWidget, (XtArgVal) wsep1);			n++;
    XtSetArg (args[n], XmNleftAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNrightAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNbottomAttachment, (XtArgVal) XmATTACH_WIDGET);n++;
    XtSetArg (args[n], XmNbottomWidget, (XtArgVal) wsep2);		n++;
    XtSetValues (XtParent (wtext), args, n);

    PlaceWinfoDialog (pSD);

    XtRealizeWidget (winfoShellW);

    /*
     * No _MOTIF_WM_HINTS: this window is meant to behave like any other, so
     * it keeps the full frame. That makes the delete protocol mandatory --
     * see the note at the top of the file.
     */
    deleteAtom = wmGD.xa_WM_DELETE_WINDOW;
    XSetWMProtocols (DISPLAY1, XtWindow (winfoShellW), &deleteAtom, 1);

    XtAddEventHandler (winfoShellW, NoEventMask, True,
	(XtEventHandler) WinfoProtocolHandler, (XtPointer) NULL);

    winfoPSD = pSD;

    return (True);
}

/*
 * Centres the window, on the user's preferred Xinerama head when there is
 * one. Same rule ConfirmAction() and the Execute dialog use.
 */
static void PlaceWinfoDialog(WmScreenData *pSD)
{
    Arg args[4];
    int n;
    Dimension width = 0, height = 0;
    Position x, y;
    XineramaScreenInfo xsi;

    n = 0;
    XtSetArg (args[n], XmNwidth, &width);	n++;
    XtSetArg (args[n], XmNheight, &height);	n++;
    XtGetValues (winfoShellW, args, n);

    if (GetPrimaryXineramaScreen (&xsi))
    {
	x = xsi.x_org + (xsi.width - (int) width) / 2;
	y = xsi.y_org + (xsi.height - (int) height) / 2;
    }
    else
    {
	x = (DisplayWidth (DISPLAY, pSD->screen) - (int) width) / 2;
	y = (DisplayHeight (DISPLAY, pSD->screen) - (int) height) / 2;
    }

    if (x < 0) x = 0;
    if (y < 0) y = 0;

    n = 0;
    XtSetArg (args[n], XmNx, (XtArgVal) x);		n++;
    XtSetArg (args[n], XmNy, (XtArgVal) y);		n++;
    XtSetArg (args[n], XtNwaitForWm, (XtArgVal) False);	n++;
    XtSetValues (winfoShellW, args, n);
}

/*
 * Posts the window, creating it the first time. Kept rather than destroyed,
 * so posting it again is cheap and it comes back the size it was left.
 */
void PostWinfoDialog(void)
{
    WmScreenData *pSD = ACTIVE_PSD;

    if (!pSD) return;

    /* A system modal window has the input; nothing else can be used. */
    if (wmGD.systemModalActive) return;

    /*
     * The shell belongs to one screen and cannot be moved to another, so on
     * a genuinely multi-screen display it is rebuilt when the active screen
     * changes. This costs nothing in the ordinary single-screen case.
     */
    if (winfoShellW && winfoPSD != pSD)
    {
	UnpostWinfoDialog ();
	XtDestroyWidget (winfoShellW);
	winfoShellW = NULL;
	winfoFormW = NULL;
	winfoOnScreen = False;
    }

    if (!winfoShellW)
    {
	if (!MakeWinfoDialog (pSD)) return;
    }

    if (!winfoOnScreen)
    {
	XtPopup (winfoShellW, XtGrabNone);
	winfoOnScreen = True;
	return;
    }

    /*
     * Already up. Raise it rather than doing nothing: it may well be behind
     * whatever the user was looking at, and a key binding that appears to do
     * nothing is worse than no key binding.
     *
     * Through F_Raise so that mWizard's own stacking list is updated;
     * XRaiseWindow on the frame would restack the server behind its back.
     */
    {
	ClientData *pCD = NULL;

	if (XtWindow (winfoShellW) &&
	    !XFindContext (DISPLAY, XtWindow (winfoShellW),
			   wmGD.windowContextType, (XPointer *) &pCD) && pCD)
	{
	    F_Raise ((String) NULL, pCD, (XEvent *) NULL);
	}
    }
}

/*
 * Takes the window off the screen. The widgets stay, so the next post is
 * just a map.
 */
static void UnpostWinfoDialog(void)
{
    if (winfoShellW && winfoOnScreen)
    {
	XtPopdown (winfoShellW);

	/*
	 * XtPopdown does nothing if the window is already unmapped -- which
	 * it is whenever the window manager side has iconified it or put it
	 * on a workspace that is not showing -- and then the frame is never
	 * unmanaged. Withdraw it explicitly, over the first connection so
	 * that the window manager sees this in order with its own events.
	 */
	if (XtWindow (winfoShellW))
	{
	    XWithdrawWindow (DISPLAY, XtWindow (winfoShellW),
			     winfoPSD->screen);
	    XSync (DISPLAY, False);
	}

	winfoOnScreen = False;
    }
}

static void WinfoCloseCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    UnpostWinfoDialog ();
}

/*
 * Hands the project page to xdg-open through SpawnCommand(), the same way any
 * other command mWizard runs is handled. If there is no xdg-open the command
 * simply fails; the address itself is in MWM_PROJECT_URL and in the manual.
 */
static void WinfoUrlCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    SpawnCommand ("xdg-open '" MWM_PROJECT_URL "' &");
}

/*
 * WM_DELETE_WINDOW, which is what the frame's Close sends us.
 *
 * This reaches Xt because HandleEventsOnClientWindow() leaves
 * doXtDispatchEvent set for ClientMessage; the window manager looks at the
 * message first and then passes it on.
 */
static void WinfoProtocolHandler(Widget w, XtPointer client_data,
				 XEvent *event, Boolean *cont)
{
    if (event->type != ClientMessage) return;
    if (event->xclient.message_type != wmGD.xa_WM_PROTOCOLS) return;

    if ((Atom) event->xclient.data.l[0] == wmGD.xa_WM_DELETE_WINDOW)
    {
	UnpostWinfoDialog ();
    }
}

/*
 * SIGUSR2 posts the window, which is how mWand reaches it. SIGUSR1 is the
 * Execute dialog; see WmExecDlg.c.
 *
 * The handler only notes the signal; Xt calls WinfoSignalProc from the event
 * loop, so the window is built from a safe context rather than from inside a
 * signal handler.
 */
static void WinfoSignalHandler(int sig)
{
    XtNoticeSignal (winfoSignalId);
}

static void WinfoSignalProc(XtPointer client_data, XtSignalId *id)
{
    PostWinfoDialog ();
}

void InitWinfoDialog(void)
{
    struct sigaction sa;

    winfoSignalId = XtAppAddSignal (wmGD.mwmAppContext, WinfoSignalProc, NULL);

    (void) sigemptyset (&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = WinfoSignalHandler;
    (void) sigaction (SIGUSR2, &sa, (struct sigaction *) 0);

    /*
     * Only now that the handler is installed: the property is what
     * tells mWand this signal is safe to send. See WmEwmh.h.
     */
    AdvertiseWmSignal (MWIZARD_SIGNAL_ABOUT);
}
