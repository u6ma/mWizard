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
 * Message and confirmation dialogs.
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

static void message_dialog_cb(Widget, XtPointer, XtPointer);

Boolean MessageDialog(Boolean confirm, const char *message_str)
{
	Widget wdlg;
	XmString xm_message_str;
	Arg args[8];
	int n = 0;
	int result = (-1);
	XmString xm_title;
	XtCallbackRec callback[]={
		{(XtCallbackProc)message_dialog_cb,(XtPointer)&result},
		{(XtCallbackProc)NULL,(XtPointer)NULL}
	};

	/*
	 * The rc file is located before the main window is realized, so a
	 * startup failure reaches this with wshell still windowless. A dialog
	 * parented on it would carry a WM_TRANSIENT_FOR pointing at a window
	 * that does not exist, and the window manager cannot frame or place
	 * that -- which is how a missing rc file shows up as an undecorated
	 * rectangle instead of a message. Realizing is enough on its own:
	 * XmNmappedWhenManaged is False, so the empty main window stays hidden
	 * until main() maps it.
	 */
	if(!XtIsRealized(wshell)) XtRealizeWidget(wshell);

	xm_message_str=XmStringCreateLocalized((char*)message_str);
	xm_title=XmStringCreateLocalized(APP_TITLE);

	XtSetArg(args[n], XmNdialogTitle, xm_title); n++;
	XtSetArg(args[n], XmNokCallback, callback); n++;
	XtSetArg(args[n], XmNcancelCallback, callback); n++;
	XtSetArg(args[n], XmNmessageString,xm_message_str); n++;
	XtSetArg(args[n], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); n++;

	wdlg = XmCreateMessageDialog(wshell, "messageDialog", args, n);
	
	n = 0;
	XtSetArg(args[n], XmNdialogType,
		confirm ? XmDIALOG_QUESTION : XmDIALOG_INFORMATION); n++;
	XtSetArg(args[n], XmNdefaultButtonType,
		confirm ? XmDIALOG_CANCEL_BUTTON : XmDIALOG_OK_BUTTON); n++;
	
	XtSetValues(wdlg, args, n);

	XmStringFree(xm_title);
	XmStringFree(xm_message_str);

	if(!confirm) XtUnmanageChild(
		XmMessageBoxGetChild(wdlg, XmDIALOG_CANCEL_BUTTON));
	XtUnmanageChild(XmMessageBoxGetChild(wdlg, XmDIALOG_HELP_BUTTON));

	XtManageChild(wdlg);

	while(XtIsManaged(wdlg) && result == (-1))
		XtAppProcessEvent(app_context, XtIMXEvent);
	
	if(result == (-1)) result = 0;
	
	XtDestroyWidget(wdlg);
	XSync(XtDisplay(wdlg), False);
	XmUpdateDisplay(wshell);
	
	return (Boolean)result;
}

static void message_dialog_cb(Widget w, XtPointer client_data,
	XtPointer call_data)
{
	XmSelectionBoxCallbackStruct *cbs=
		(XmSelectionBoxCallbackStruct*)call_data;
	char *result=(Boolean*)client_data;

	if(cbs->reason==XmCR_OK)
		*result=1;
	else
		*result=0;
}

void ReportRcFileError(const char *rc_file, const char *err_desc)
{
	char *buffer;
	char err_msg[]="Error while parsing RC file:";
	size_t msg_len;

	msg_len=strlen(err_msg)+strlen(err_desc)+strlen(rc_file)+10;
	buffer=malloc(msg_len);
	if(!buffer){
		perror("malloc");
		return;
	}

	sprintf(buffer,"%s %s\n%s.",err_msg,rc_file,err_desc);

	/*
	 * To stderr as well as to a dialog, and the stderr half is the one
	 * that matters.
	 *
	 * This is called from ConstructMenu(), which runs before
	 * XtRealizeWidget() -- and the shell is created with
	 * XmNmappedWhenManaged False besides, so at this point there is no
	 * mapped window to parent a dialog on and nothing appears. mWand then
	 * returns EXIT_FAILURE and is gone, having said nothing at all: no
	 * window, no message, no clue. Run from a terminal it printed nothing
	 * either, which is a miserable way to debug a panel that will not
	 * start.
	 */
	fprintf(stderr, "%s: %s %s\n%s.\n", APP_NAME, err_msg, rc_file,
		err_desc);

	MessageDialog(False,buffer);
	free(buffer);
}
