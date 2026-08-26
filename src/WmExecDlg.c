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
 * Reachable three ways: the f.run rc function (bound to a key or a menu
 * item), and SIGUSR1, which is what mWand sends.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/SelectioB.h>
#include <Xm/TextF.h>

#include "WmGlobal.h"
#include "WmExecDlg.h"
#include "WmSession.h"
#include "WmError.h"

static void ExecDialogCB(Widget, XtPointer, XtPointer);
static void ExecSignalProc(XtPointer, XtSignalId*);
static void ExecSignalHandler(int);

static Widget execDialog = NULL;
static XtSignalId execSignalId;

/*
 * Posts the dialog, creating it the first time.
 *
 * It is kept rather than destroyed so that the text field retains the last
 * command: raising the prompt again and pressing Return repeats it, which is
 * the common case. The text is selected on re-post so typing replaces it.
 */
void PostExecDialog(void)
{
    static Widget wtext = NULL;
    Arg args[8];
    int n = 0;

    /*
     * Parented on the application shell of the second display connection,
     * not on a screen's popup shell.
     *
     * XmCreatePromptDialog expects an ordinary widget or an application
     * shell and creates its own XmDialogShell underneath; handing it the
     * popup VendorShell that screenTopLevelW1 is leaves the dialog with a
     * shell parent it cannot use, and the window never appears. mWand
     * parented this same dialog on its application shell, which is what
     * topLevelW1 is here.
     *
     * The second connection matters too: this window is an ordinary client
     * as far as the window manager is concerned, and it is the connection
     * mWizard uses for all of its own windows for exactly that reason.
     */
    if (!wmGD.topLevelW1) return;

    if (execDialog == NULL)
    {
	XmString xm_title;
	XmString xm_prompt;
	XtCallbackRec callback[] = {
	    {(XtCallbackProc) ExecDialogCB, (XtPointer) NULL},
	    {(XtCallbackProc) NULL, (XtPointer) NULL}
	};
	/* Reset the text field's Home/End translations to the defaults, since
	 * the selection box overrides them to drive the list above, which is
	 * unexpected here and not very useful either */
	char alt_tt_src[] =
	    ":s <Key>osfEndLine: end-of-line(extend)\n"
	    ":s <Key>osfBeginLine: beginning-of-line(extend)\n"
	    ":<Key>osfEndLine: end-of-line()\n"
	    ":<Key>osfBeginLine: beginning-of-line()\n";
	XtTranslations alt_tt = NULL;

	n = 0;
	xm_title = XmStringCreateLocalized (MWM_NAME);
	xm_prompt = XmStringCreateLocalized ("Specify a command");
	XtSetArg (args[n], XmNdialogTitle, xm_title); n++;
	XtSetArg (args[n], XmNokCallback, callback); n++;
	XtSetArg (args[n], XmNcancelCallback, callback); n++;
	XtSetArg (args[n], XmNselectionLabelString, xm_prompt); n++;

	execDialog = XmCreatePromptDialog (wmGD.topLevelW1,
					   "execDialog", args, n);
	XmStringFree (xm_title);
	XmStringFree (xm_prompt);

	wtext = XmSelectionBoxGetChild (execDialog, XmDIALOG_TEXT);
	alt_tt = XtParseTranslationTable (alt_tt_src);
	if (alt_tt) XtOverrideTranslations (wtext, alt_tt);

	XtUnmanageChild (
	    XmSelectionBoxGetChild (execDialog, XmDIALOG_HELP_BUTTON));
    }
    else
    {
	char *text;
	size_t len;

	text = XmTextFieldGetString (wtext);
	if (text)
	{
	    if ((len = strlen (text)) != 0)
	    {
		XmTextFieldSetSelection (wtext, 0, len,
		    XtLastTimestampProcessed (XtDisplay (wtext)));
	    }
	    XtFree (text);
	}
    }
    XtManageChild (execDialog);
}

static void ExecDialogCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    XmSelectionBoxCallbackStruct *cbs =
	(XmSelectionBoxCallbackStruct *) call_data;
    char *command;

    if (cbs->reason == XmCR_CANCEL) return;

    command = (char *) XmStringUnparse (cbs->value, NULL, 0,
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
}
