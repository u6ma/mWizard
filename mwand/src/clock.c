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
 * The date and time display.
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

Widget wdtframe = None;
Widget wdtlabel = None;

static void time_update_cb(XtPointer, XtIntervalId*);

static void time_update_cb(XtPointer client_data, XtIntervalId *id)
{
	Arg args[2];
	char time_str[256];
	time_t secs;
	struct tm *the_time;
	XmString xm_str;
	static Boolean init = True;
	
	time(&secs);
	the_time = localtime(&secs);
	strftime(time_str, 255, app_res.date_time_fmt, the_time);
	xm_str = XmStringCreateLocalized(time_str);

	XtSetArg(args[0], XmNlabelString, xm_str);
	XtSetValues(wdtlabel, args, 1);

	if(init) {
		XtSetArg(args[0], XmNrecomputeSize, False);
		XtSetValues(wdtlabel, args, 1);
		init = False;
	}

	XmStringFree(xm_str);
	
	XtAppAddTimeOut(app_context,
		60000-(the_time->tm_sec * 1000), time_update_cb, NULL);
}

/*
 * Public entry point: refresh the label now.
 */
void UpdateClock(void)
{
	if(wdtlabel) time_update_cb(NULL, NULL);
}

/*
 * Creates the date and time display. It is created either way so that the
 * layout code can ask whether it is managed; it is only managed, and only
 * starts ticking, when dateTimeDisplay is set.
 */
Widget CreateClockWidget(Widget wparent)
{
	Arg args[8];
	int n = 0;

	XtSetArg(args[n], XmNshadowType, XmSHADOW_IN); n++;
	XtSetArg(args[n], XmNshadowThickness, 1); n++;
	XtSetArg(args[n], XmNmarginWidth, 2); n++;
	XtSetArg(args[n], XmNmarginHeight, 2); n++;
	wdtframe = XmCreateFrame(wparent, "dateTimeFrame", args, n);

	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	wdtlabel = XmCreateLabelGadget(wdtframe, "dateTime", args, n);

	if(app_res.show_date_time) {
		XtManageChild(wdtlabel);
		XtManageChild(wdtframe);
		time_update_cb(NULL, NULL);
	}

	return wdtframe;
}
