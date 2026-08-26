/* 
 * Motif
 *
 * Copyright (c) 1987-2012, The Open Group. All rights reserved.
 * Copyright (c) 2018-2026, alx@fastestcode.org
 *
 * Modified 2026 for mWizard, a fork of EMWM. See NOTICE for details.
 *
 * These libraries and programs are free software; you can
 * redistribute them and/or modify them under the terms of the GNU
 * Lesser General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * These libraries and programs are distributed in the hope that
 * they will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with these librararies and programs; if not, write
 * to the Free Software Foundation, Inc., 51 Franklin Street, Fifth
 * Floor, Boston, MA 02110-1301 USA
*/ 

/*
 * Included Files:
 */

#include "WmGlobal.h"
#include <stdio.h>
#include <stdlib.h>
#include "WmSession.h"

/*
 * Function Declarations:
 */
#include "WmError.h"

#ifdef DEBUG

#define E_MAJOR_CODE		0
#define E_MINOR_CODE		1
#define E_RESOURCE_ID		2
#define E_ERROR_SERIAL		3
#define E_CURRENT_SERIAL	4

#define NUM_E_STRINGS		5

static char *pchErrorFormatNames [NUM_E_STRINGS] = {
    "MajorCode", 
    "MinorCode", 
    "ResourceID", 
    "ErrorSerial", 
    "CurrentSerial" 
};

static char *pchDefaultErrorFormat [NUM_E_STRINGS] = {
    " %d ",
    " %d ",
    " %ld ",
    " %ld ",
    " %ld "
};

static char *pchErrorFormat [NUM_E_STRINGS];

#endif /* DEBUG */

static int WmXErrorHandler (Display *display, XErrorEvent *errorEvent);
static int WmXIOErrorHandler (Display *display);
static _X_NORETURN void WmXtErrorHandler (char *message);
static void WmXtWarningHandler (char *message);



/*************************************<->*************************************
 *
 *  WmInitErrorHandler (display)
 *
 *
 *  Description:
 *  -----------
 *  This function initializes the window manager error handler.
 *
 *
 *  Inputs:
 *  ------
 *  display = display we're talking about
 *  -------
 *
 *************************************<->***********************************/

void
WmInitErrorHandler (Display *display)
{
#ifdef DEBUG
    char buffer[BUFSIZ];
    int i;

    /*
     * Fetch the X error format strings from XErrorDB
     */
    for (i = 0; i< NUM_E_STRINGS; i++)
    {
	XGetErrorDatabaseText (display, "XlibMessage", 
			       pchErrorFormatNames[i], 
			       pchDefaultErrorFormat[i], buffer, BUFSIZ);

	if ((pchErrorFormat[i] = (char *) XtMalloc (1+strlen(buffer))) == NULL)
	{
	    Warning ("Insufficient memory for error message initialization.");
	    ExitWM (1);
	}

	strcpy(pchErrorFormat[i], buffer);
    }

#endif /* DEBUG */

    XSetErrorHandler (WmXErrorHandler);
    XSetIOErrorHandler (WmXIOErrorHandler);

    XtSetWarningHandler (WmXtWarningHandler);
    XtSetErrorHandler (WmXtErrorHandler);

} /* END OF FUNCTION WmInitErrorHandler */


/*************************************<->*************************************
 *
 *  WmXErrorHandler (display, errorEvent)
 *
 *
 *  Description:
 *  -----------
 *  This function is the X error handler that is registered with X to
 *  handle X errors resulting from window management activities.
 *
 *
 *  Inputs:
 *  ------
 *  display = display on which X error occurred
 *
 *  errorEvent = pointer to a block of information describing the error
 *
 * 
 *  Outputs:
 *  -------
 *  wmGD.errorFlag = set to True
 *
 *  Return = 0
 *
 *************************************<->***********************************/

static int
WmXErrorHandler (Display *display, XErrorEvent *errorEvent)
{
    ClientData *pCD;

#ifdef DEBUG
    char buffer[BUFSIZ];
    char message[BUFSIZ];

    XGetErrorText (display, errorEvent->error_code, buffer, BUFSIZ);
    Warning ("X error occurred during window management operation");
    fprintf (stderr, "Description = '%s'\n  ", buffer);

    fprintf (stderr, pchErrorFormat[E_MAJOR_CODE], errorEvent->request_code);
    sprintf(message, "%d", errorEvent->request_code);
    XGetErrorDatabaseText (display, "XRequest", message, 
	" ", buffer, BUFSIZ);
    fprintf (stderr, " (%s)\n  ", buffer);
    fprintf (stderr, pchErrorFormat[E_MINOR_CODE], errorEvent->minor_code);
    fprintf (stderr, "\n  ");
    fprintf (stderr, pchErrorFormat[E_RESOURCE_ID], errorEvent->resourceid);
    fprintf (stderr, "\n  ");
    fprintf (stderr, pchErrorFormat[E_ERROR_SERIAL], errorEvent->serial);
    fprintf (stderr, "\n  ");
    fprintf (stderr, pchErrorFormat[E_CURRENT_SERIAL],
			LastKnownRequestProcessed(display));
    fprintf (stderr, "\n");
#else /* !DEBUG */

    /*
     * Report the error rather than swallowing it.
     *
     * This used to be visible only in a DEBUG build, which meant that a
     * protocol error in a release build produced no output at all: the
     * window manager either carried on with something half done or fell over
     * later somewhere unrelated, with nothing to say which request had
     * failed. One line naming the error, the request and the resource is
     * enough to place it.
     *
     * BadWindow and BadDrawable are left silent. Those are the ordinary race
     * with a client that has just exited -- the window manager still has the
     * window id and the server no longer does -- and they are handled just
     * below; printing them would bury everything else.
     */
    if ((errorEvent->error_code != BadWindow) &&
	(errorEvent->error_code != BadDrawable))
    {
	char errText[BUFSIZ];
	char reqText[BUFSIZ];
	char reqNum[32];

	XGetErrorText (display, errorEvent->error_code, errText, BUFSIZ);

	sprintf (reqNum, "%d", errorEvent->request_code);
	XGetErrorDatabaseText (display, "XRequest", reqNum, "unknown",
			       reqText, BUFSIZ);

	fprintf (stderr,
	    "%s: X error on %s: %s\n"
	    "    request %d (%s), minor %d, resource 0x%lx, serial %lu\n",
	    MWM_NAME, XDisplayString (display), errText,
	    (int) errorEvent->request_code, reqText,
	    (int) errorEvent->minor_code,
	    (unsigned long) errorEvent->resourceid,
	    (unsigned long) errorEvent->serial);
	fflush (stderr);
    }
#endif /* DEBUG */

    /*
     * Check for a BadWindow error for a managed window.  If this error
     * is detected indicate in the client data that the window no longer
     * exists.
     */

    if ((errorEvent->error_code == BadWindow) &&
	!XFindContext (DISPLAY, errorEvent->resourceid, wmGD.windowContextType,
	     (XPointer*)&pCD))
    {
	if (errorEvent->resourceid == pCD->client)
	{
	    pCD->clientFlags |= CLIENT_DESTROYED;
	}
    }

    wmGD.errorFlag = True;
    wmGD.errorResource = errorEvent->resourceid;
    wmGD.errorRequestCode = errorEvent->request_code;

    return (0);

} /* END OF FUNCTION WmXErrorHandler */



/*************************************<->*************************************
 *
 *  WmXIOErrorHandler (display)
 *
 *
 *  Description:
 *  -----------
 *  This function is the X IO error handler that is registered with X to
 *  handle X IO errors.  This function exits the window manager.
 *
 *
 *  Inputs:
 *  ------
 *  display = X display on which the X IO error occurred
 * 
 *************************************<->***********************************/

static int
WmXIOErrorHandler (Display *display)
{
  char  err[100];
 
  sprintf (err, "%s: %s\n", "I/O error on display:", XDisplayString(display));
  Warning(err);

  ExitWM (WM_ERROR_EXIT_VALUE);

  /*NOTREACHED*/
  return 1;
} /* END OF FUNCTIONS WmXIOErrorHandler */



/*************************************<->*************************************
 *
 *  WmXtErrorHandler (message)
 *
 *
 *  Description:
 *  -----------
 *  This function is registered as the X Toolkit fatal error handler.
 *
 *
 *  Inputs:
 *  ------
 *  message = pointer to an error message
 *
 *************************************<->***********************************/

static _X_NORETURN void
WmXtErrorHandler (char *message)
{

    Warning (message);
    ExitWM (WM_ERROR_EXIT_VALUE);

} /* END OF FUNCTION WmXtErrorHandler */



/*************************************<->*************************************
 *
 *  WmXtWarningHandler (message)
 *
 *
 *  Description:
 *  -----------
 *  This function is registered as an X Toolkit warning handler.
 *
 *
 *  Inputs:
 *  ------
 *  message = pointer to a warning message
 * 
 *************************************<->***********************************/

static void
WmXtWarningHandler (char *message)
{

#ifdef DEBUG
    Warning (message);
#endif /* DEBUG */

} /* END OF FUNCTIONS WmXtWarningHandler */


/*************************************<->*************************************
 *
 *  Warning (message)
 *
 *
 *  Description:
 *  -----------
 *  This function lists a message to stderr.
 *
 *
 *  Inputs:
 *  ------
 *  message = pointer to a message string
 * 
 *************************************<->***********************************/

void
Warning (char *message)
{
    fprintf (stderr, "%s: %s\n", wmGD.mwmName, message);
    fflush (stderr);

} /* END OF FUNCTION Warning */

#ifdef DEBUGGER

/******************************<->*************************************
 *
 *  PrintFormatted (format, message, message, ...)
 *
 *
 *  Description:
 *  -----------
 *  This function lists several messages to stderr using fprinf()
 *  formatting capabilities.
 *
 *  Inputs:
 *  ------
 *  s0-s9 = pointers to message strings
 * 
 *  Comments:
 *  ------
 *  Caller must provide his/her own argv[0] to this function.
 ******************************<->***********************************/

/*VARARGS1*/
void
PrintFormatted(char *f, char *s0, char *s1, char *s2, char *s3, char *s4, char *s5, char *s6, char *s7, char *s8, char *s9)
/* limit of ten args */
{
    fprintf( stderr, f, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9);
    fflush (stderr);
} /* END OF FUNCTION PrintFormatted */

/************************    eof   **************************/
#endif /* DEBUGGER */
