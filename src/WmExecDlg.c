/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

/*
 * The Execute dialog: a prompt for a command to run.
 *
 * This lives in the window manager rather than in the panel. mWand had it
 * first, but a run prompt is not a panel feature -- it is wanted with or
 * without one, and the window manager is the process that is always there.
 * mWand now asks for this dialog instead of carrying its own.
 *
 * Reachable two ways: the f.run rc function (bound to a key or a menu
 * item), and SIGUSR1, which is what mWand sends.
 *
 * ---------------------------------------------------------------------------
 *
 * A window manager cannot put up a Motif dialog the way an ordinary
 * application does, and the difference is what this file is mostly about.
 *
 * The prompt started life in mWand as XmCreatePromptDialog() parented on the
 * application shell. Moved here unchanged, that is wrong in three ways:
 *
 *  - It waited for itself. Any top level shell asks the window manager for
 *    geometry and then blocks until the answer comes back; XtNwaitForWm is
 *    True by default on every WMShell, Motif's and Xt's alike. Since the
 *    window manager is this process, the answer cannot come, and the wait
 *    runs to the full XtNwmTimeout with mWizard's own dispatcher shut out of
 *    the nested event loop that Xt spins meanwhile. See the note on
 *    XtNwaitForWm in MakeExecDialog(), which is where this is turned off.
 *    XmCreatePromptDialog() also builds an XmDialogShell, a Motif
 *    VendorShell, which layers its own synchronous handshake on top of that
 *    one; every other window mWizard puts on the screen is a plain Xt shell,
 *    so this one is too.
 *
 *  - It was parented on wmGD.topLevelW1, the global application shell, which
 *    is 10x10, parked at x=10000 and never mapped. A dialog centred on that
 *    lands off screen. The per-screen shells are what the rest of the window
 *    manager hangs its windows off.
 *
 *  - It inherited whatever visual, depth and colormap Xt guessed for the
 *    default screen of the second connection rather than the ones for the
 *    screen being managed. Every other shell here states them explicitly.
 *
 * On top of that, a window mWizard owns must not be closable through
 * f.kill: with no WM_DELETE_WINDOW protocol on it, F_Kill falls through to
 * XKillClient(), and killing the client that owns the second display
 * connection means killing mWizard -- which takes the X session with it.
 * The _MOTIF_WM_HINTS set below withhold MWM_FUNC_CLOSE for that reason,
 * the same way PRESENCE_BOX_FUNCTIONS does for the presence dialog.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <Xm/Xm.h>
#include <Xm/SelectioB.h>
#include <Xm/TextF.h>

#include "WmGlobal.h"
#include "WmExecDlg.h"
#include "WmEwmh.h"
#include "WmSession.h"
#include "WmError.h"
#include "WmXinerama.h"

static Boolean MakeExecDialog(WmScreenData *pSD);
static void PlaceExecDialog(WmScreenData *pSD);
static void UnpostExecDialog(void);
static void ExecOkCB(Widget, XtPointer, XtPointer);
static void ExecCancelCB(Widget, XtPointer, XtPointer);
static void ExecSignalProc(XtPointer, XtSignalId*);
static void ExecSignalHandler(int);

static Widget execShellW = NULL;
static Widget execBoxW = NULL;
static Widget execTextW = NULL;
static WmScreenData *execPSD = NULL;
static Boolean execOnScreen = False;
static XtSignalId execSignalId;

/*
 * Builds the dialog for one screen. Returns False and leaves nothing behind
 * if any part of it could not be made.
 */
static Boolean MakeExecDialog(WmScreenData *pSD)
{
    Arg args[16];
    int n;
    XmString xm_prompt;
    XtTranslations alt_tt;
    PropMwmHints hints;
    Widget helpW;

    /* Reset the text field's Home/End translations to the defaults, since
     * the selection box overrides them to drive the list above, which is
     * unexpected here and not very useful either */
    static char alt_tt_src[] =
	":s <Key>osfEndLine: end-of-line(extend)\n"
	":s <Key>osfBeginLine: beginning-of-line(extend)\n"
	":<Key>osfEndLine: end-of-line()\n"
	":<Key>osfBeginLine: beginning-of-line()\n";

    if (!pSD->screenTopLevelW1) return (False);

    /*
     * A plain Xt TransientShell, not an XmDialogShell: see the note at the
     * top of the file. Depth, screen and colormap are stated for the screen
     * being managed rather than left to the second connection's default.
     */
    n = 0;
    /*
     * XtNwaitForWm must be off, and this is the one that matters.
     *
     * It is a WMShell resource, so it is on every top level shell and not
     * just Motif's. It defaults to True, which makes Xt's root geometry
     * manager send its ConfigureRequest and then sit in a nested
     * XtAppProcessEvent() loop until the window manager answers, or until
     * XtNwmTimeout (5 seconds) runs out.
     *
     * The window manager here is this process. That nested loop dispatches
     * through XtDispatchEvent() only; main()'s WmDispatchWsEvent() and
     * WmDispatchClientEvent() -- which are what actually answer a
     * ConfigureRequest -- never run inside it. So the reply cannot arrive,
     * mWizard blocks against itself for the full timeout, and every event
     * that turns up meanwhile is eaten by the nested loop without the window
     * manager half of the dispatch ever seeing it. That is what left the
     * root menu wedged afterwards.
     *
     * wspSetPosition() turns it off on the presence dialog for this reason,
     * and XmNuseAsyncGeometry in WmInitWs.c is the VendorShell spelling of
     * the same thing for topLevelW/topLevelW1.
     */
    XtSetArg (args[n], XtNwaitForWm, (XtArgVal) False);			n++;
    XtSetArg (args[n], XtNallowShellResize, (XtArgVal) True);		n++;
    XtSetArg (args[n], XtNtitle, (XtArgVal) MWM_NAME);			n++;
    XtSetArg (args[n], XtNdepth,
	(XtArgVal) DefaultDepth (DISPLAY1, pSD->screen));		n++;
    XtSetArg (args[n], XtNscreen,
	(XtArgVal) ScreenOfDisplay (DISPLAY1, pSD->screen));		n++;
    XtSetArg (args[n], XtNcolormap,
	(XtArgVal) DefaultColormap (DISPLAY1, pSD->screen));		n++;

    execShellW = XtCreatePopupShell ("execDialog", transientShellWidgetClass,
				     pSD->screenTopLevelW1, args, n);
    if (!execShellW) return (False);

    /*
     * XmNautoUnmanage would unmanage the selection box and leave an empty
     * shell mapped behind it, since there is no XmDialogShell here to notice
     * and pop down. The callbacks below pop the shell down themselves.
     */
    n = 0;
    xm_prompt = XmStringCreateLocalized ("Specify a command");
    XtSetArg (args[n], XmNdialogType, (XtArgVal) XmDIALOG_PROMPT);	n++;
    XtSetArg (args[n], XmNselectionLabelString, (XtArgVal) xm_prompt);	n++;
    XtSetArg (args[n], XmNautoUnmanage, (XtArgVal) False);		n++;
    XtSetArg (args[n], XmNtraversalOn, (XtArgVal) True);			n++;

    execBoxW = XtCreateManagedWidget ("execBox", xmSelectionBoxWidgetClass,
				      execShellW, args, n);
    XmStringFree (xm_prompt);

    if (!execBoxW)
    {
	XtDestroyWidget (execShellW);
	execShellW = NULL;
	return (False);
    }

    XtAddCallback (execBoxW, XmNokCallback,
	(XtCallbackProc) ExecOkCB, (XtPointer) NULL);
    XtAddCallback (execBoxW, XmNcancelCallback,
	(XtCallbackProc) ExecCancelCB, (XtPointer) NULL);

    if ((helpW = XmSelectionBoxGetChild (execBoxW, XmDIALOG_HELP_BUTTON)))
    {
	XtUnmanageChild (helpW);
    }

    execTextW = XmSelectionBoxGetChild (execBoxW, XmDIALOG_TEXT);
    if (execTextW && (alt_tt = XtParseTranslationTable (alt_tt_src)))
    {
	XtOverrideTranslations (execTextW, alt_tt);
    }

    /*
     * Positioned before realizing. On an unrealized shell this only writes
     * core.x/core.y; once it is realized the same call becomes a request to
     * the window manager, and there is no reason to make one here.
     */
    PlaceExecDialog (pSD);

    XtRealizeWidget (execShellW);

    /*
     * Withhold MWM_FUNC_CLOSE. F_Kill() calls XKillClient() on a client that
     * offers no WM_DELETE_WINDOW, and this client is mWizard's own second
     * display connection.
     */
    hints.flags = MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
    hints.functions = MWM_FUNC_MOVE;
    hints.decorations = MWM_DECOR_BORDER | MWM_DECOR_TITLE;
    hints.inputMode = 0;
    hints.status = 0;

    XChangeProperty (DISPLAY1, XtWindow (execShellW),
	wmGD.xa_MWM_HINTS, wmGD.xa_MWM_HINTS, 32, PropModeReplace,
	(unsigned char *) &hints, PROP_MWM_HINTS_ELEMENTS);

    execPSD = pSD;

    return (True);
}

/*
 * Centres the dialog, on the user's preferred Xinerama head when there is
 * one. Same rule ConfirmAction() uses.
 */
static void PlaceExecDialog(WmScreenData *pSD)
{
    Arg args[4];
    int n;
    Dimension width = 0, height = 0;
    Position x, y;
    XineramaScreenInfo xsi;

    n = 0;
    XtSetArg (args[n], XmNwidth, &width);	n++;
    XtSetArg (args[n], XmNheight, &height);	n++;
    XtGetValues (execShellW, args, n);

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
    XtSetValues (execShellW, args, n);
}

/*
 * Posts the dialog, creating it the first time.
 *
 * It is kept rather than destroyed so that the text field retains the last
 * command: raising the prompt again and pressing Return repeats it, which is
 * the common case. The text is selected on re-post so typing replaces it.
 */
void PostExecDialog(void)
{
    WmScreenData *pSD = ACTIVE_PSD;

    if (!pSD) return;

    /*
     * A system modal window is up and has the input; posting over it would
     * put up a prompt that cannot be typed into. Same guard ConfirmAction()
     * uses.
     */
    if (wmGD.systemModalActive) return;

    /*
     * The shell belongs to one screen and cannot be moved to another, so on
     * a genuinely multi-screen display it is rebuilt when the active screen
     * changes. This costs nothing in the ordinary single-screen case.
     */
    if (execShellW && execPSD != pSD)
    {
	UnpostExecDialog ();
	XtDestroyWidget (execShellW);
	execShellW = NULL;
	execBoxW = NULL;
	execTextW = NULL;
	execOnScreen = False;
    }

    if (!execShellW)
    {
	if (!MakeExecDialog (pSD)) return;
    }

    if (execTextW)
    {
	char *text = XmTextFieldGetString (execTextW);
	size_t len;

	if (text)
	{
	    if ((len = strlen (text)) != 0)
	    {
		XmTextFieldSetSelection (execTextW, 0, len,
		    XtLastTimestampProcessed (XtDisplay (execTextW)));
	    }
	    XtFree (text);
	}
    }

    if (!execOnScreen)
    {
	PlaceExecDialog (pSD);
	XtPopup (execShellW, XtGrabNone);
	execOnScreen = True;
    }

    if (execTextW) XmProcessTraversal (execTextW, XmTRAVERSE_CURRENT);
}

/*
 * Takes the dialog off the screen. Popping the shell down unmaps it, which
 * is what tells the window manager side to unmanage the frame; the widgets
 * stay so that the next post still has the last command in the text field.
 */
static void UnpostExecDialog(void)
{
    if (execShellW && execOnScreen)
    {
	XtPopdown (execShellW);

	/*
	 * XtPopdown does nothing if the window is already unmapped -- which
	 * it is whenever the window manager side has iconified the dialog or
	 * put it on a workspace that is not showing -- and then the frame is
	 * never unmanaged. Withdraw it explicitly, over the first connection
	 * so that the window manager sees this in order with its own events.
	 * HidePresenceBox() does the same for the same reason.
	 */
	if (XtWindow (execShellW))
	{
	    XWithdrawWindow (DISPLAY, XtWindow (execShellW), execPSD->screen);
	    XSync (DISPLAY, False);
	}

	execOnScreen = False;
    }
}

static void ExecOkCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    XmSelectionBoxCallbackStruct *cbs =
	(XmSelectionBoxCallbackStruct *) call_data;
    char *command;

    UnpostExecDialog ();

    command = (char *) XmStringUnparse (cbs->value, NULL, XmCHARSET_TEXT,
			   XmCHARSET_TEXT, NULL, 0, XmOUTPUT_ALL);
    if (!command) return;

    /*
     * SpawnCommand() runs it through a shell, so what is typed here behaves
     * the way the same string would in an f.exec binding -- variables from
     * the Variables block, pipelines and "&" all work.
     */
    if (*command) SpawnCommand (command);

    XtFree (command);
}

static void ExecCancelCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    UnpostExecDialog ();
}

/*
 * SIGUSR1 posts the dialog, which is how mWand reaches it.
 *
 * The handler only notes the signal; Xt calls ExecSignalProc from the event
 * loop, so the dialog is built from a safe context rather than from inside
 * a signal handler.
 */
static void ExecSignalHandler(int sig)
{
    XtNoticeSignal (execSignalId);
}

static void ExecSignalProc(XtPointer client_data, XtSignalId *id)
{
    PostExecDialog ();
}

void InitExecDialog(void)
{
    struct sigaction sa;

    execSignalId = XtAppAddSignal (wmGD.mwmAppContext, ExecSignalProc, NULL);

    (void) sigemptyset (&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = ExecSignalHandler;
    (void) sigaction (SIGUSR1, &sa, (struct sigaction *) 0);

    /*
     * Only now that the handler is installed: the property is what
     * tells mWand this signal is safe to send. See WmEwmh.h.
     */
    AdvertiseWmSignal (MWIZARD_SIGNAL_EXEC);
}
