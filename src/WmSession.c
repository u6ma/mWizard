/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Intrinsic.h>
#include <X11/Xatom.h>

#ifdef IDLE_LOCK
#include <X11/extensions/scrnsaver.h>
#endif

#include "WmGlobal.h"
#include "WmSession.h"
#include "WmError.h"
#include "WmFeedback.h"
#include "WmFunction.h"
#include "WmSignal.h"

/*
 * Terminates the window manager.
 *
 * EMWM's version of this lived in WmXSMP.c and resigned from the session
 * manager on the way out. There is no session manager to resign from now, so
 * all that is left is closing the display, and even that has to be guarded:
 * several callers are error paths that run before InitWmGlobal has opened a
 * display.
 */
_X_NORETURN void ExitWM(int exitCode)
{
    if (DISPLAY)
    {
	XSync (DISPLAY, False);
	XCloseDisplay (DISPLAY);
    }

    exit (exitCode);
}

/*
 * Runs a shell command in a detached child process.
 *
 * This is the body F_Exec has always used; it lives here so that the session
 * functions and the idle lock timer can share it. As in F_Exec, DISPLAY is
 * set for the active screen before forking and restored afterwards, so that
 * the child lands on the screen the user acted on.
 */
Boolean SpawnCommand(const char *command)
{
    int   pid;
    char *shell;
    char *shellname;

    if (!command || !(*command)) return (False);

    if (wmGD.pActiveSD && wmGD.pActiveSD->displayString)
    {
	putenv (wmGD.pActiveSD->displayString);
    }

    if ((pid = vfork ()) == 0)
    {
	setsid();

	/*
	 * Fix up signal handling.
	 */
	RestoreDefaultSignalHandlers ();

	/*
	 * Exec the command using $WMSHELL if set, or $SHELL, or sh.
	 */
	if (((shell = getenv ("WMSHELL")) != NULL) ||
	    ((shell = getenv ("SHELL")) != NULL))
	{
	    shellname = strrchr (shell, '/');
	    if (shellname == NULL)
	    {
		/*
		 * If the shell pathname obtained from SHELL or WMSHELL does
		 * not have a "/" in the path and if the user expects this
		 * shell to be obtained using the PATH variable rather than
		 * the current directory, then we must call execlp and not
		 * execl.
		 */
		shellname = shell;
		execlp (shell, shellname, "-c", command, NULL);
	    }
	    else
	    {
		shellname++;
		execl (shell, shellname, "-c", command, NULL);
	    }
	}

	/*
	 * There is no SHELL environment variable or the first execl failed.
	 */
	execl ("/bin/sh", "sh", "-c", command, NULL);

	_exit (127);
    }

    /*
     * Restore the original DISPLAY environment variable value so that a
     * restart will start on the same screen.
     */
    if (wmGD.pActiveSD && wmGD.pActiveSD->displayString && wmGD.displayString)
    {
	putenv (wmGD.displayString);
    }

    return (pid != -1);
}

/*
 * Shared by the three power functions below. Warns rather than doing nothing
 * silently when the command has been left empty, so that a misconfiguration
 * is visible instead of looking like a broken menu entry.
 */
static void RunSessionCommand(const char *command, const char *what)
{
    char *msg;
    const char fmt[] = "No command configured for %s. "
		       "Set %sCommand in your Settings block.";
    size_t len;

    if (command && *command)
    {
	if (!SpawnCommand (command))
	{
	    len = snprintf (NULL, 0, "Could not run the %s command.", what);
	    msg = XtMalloc (len + 1);
	    sprintf (msg, "Could not run the %s command.", what);
	    Warning (msg);
	    XtFree (msg);
	}
	return;
    }

    len = snprintf (NULL, 0, fmt, what, what);
    msg = XtMalloc (len + 1);
    sprintf (msg, fmt, what, what);
    Warning (msg);
    XtFree (msg);
}

/*
 * Confirm dialog callbacks. ConfirmAction() calls these through
 * confirm_func[] in WmFeedback.c, which types them as void (*)(Boolean);
 * the argument is unused here.
 */
void Do_Reboot (Boolean dummy)
{
    RunSessionCommand (wmGD.rebootCommand, "reboot");
}

void Do_Shutdown (Boolean dummy)
{
    RunSessionCommand (wmGD.shutdownCommand, "shutdown");
}

void Do_Suspend (Boolean dummy)
{
    RunSessionCommand (wmGD.suspendCommand, "suspend");
}

/*
 * f.logout -- end the session. Identical in effect to f.quit; it exists so
 * that a root menu can say "Log Out" without the binding reading as though
 * it only quits the window manager.
 */
Boolean F_Logout (String args, ClientData *pCD, XEvent *event)
{
    if (wmGD.showFeedback & WM_SHOW_FB_QUIT)
    {
	ConfirmAction (ACTIVE_PSD, QUIT_MWM_ACTION);
    }
    else
    {
	Do_Quit_Mwm (False);
    }

    return (False);
}

Boolean F_Reboot (String args, ClientData *pCD, XEvent *event)
{
    if (wmGD.showFeedback & WM_SHOW_FB_QUIT)
    {
	ConfirmAction (ACTIVE_PSD, REBOOT_ACTION);
    }
    else
    {
	Do_Reboot (False);
    }

    return (False);
}

Boolean F_Shutdown (String args, ClientData *pCD, XEvent *event)
{
    if (wmGD.showFeedback & WM_SHOW_FB_QUIT)
    {
	ConfirmAction (ACTIVE_PSD, SHUTDOWN_ACTION);
    }
    else
    {
	Do_Shutdown (False);
    }

    return (False);
}

/*
 * Suspending is cheap to undo, so it is not put behind a confirm dialog the
 * way reboot and shutdown are.
 */
Boolean F_Suspend (String args, ClientData *pCD, XEvent *event)
{
    Do_Suspend (False);

    return (False);
}


#ifdef IDLE_LOCK

/*
 * Idle lock timer.
 *
 * mWizard does not implement locking itself -- lockCommand runs whatever the
 * user prefers (xscreensaver, i3lock, slock). All this does is notice that
 * the display has been idle for lockTimeout minutes and run that command
 * once, then wait for activity before arming again.
 *
 * The idle counter comes from the MIT-SCREEN-SAVER extension, which is why
 * this needs -lXss. Everything else in mWizard builds without it; see the
 * IDLE_LOCK comments in common.mf to drop this feature.
 */

#define IDLE_POLL_MS 15000

static XtIntervalId idleTimerId = (XtIntervalId)0;
static Boolean	    idleLockAvailable = False;
static Boolean	    idleLockFired = False;

static void IdleTimerProc(XtPointer closure, XtIntervalId *id)
{
    static XScreenSaverInfo *info = NULL;
    unsigned long idleMs;
    unsigned long thresholdMs;

    idleTimerId = (XtIntervalId)0;

    if (!info) info = XScreenSaverAllocInfo();

    if (info && wmGD.lockTimeout > 0 &&
	wmGD.lockCommand && *(wmGD.lockCommand))
    {
	thresholdMs = (unsigned long)wmGD.lockTimeout * 60UL * 1000UL;

	if (XScreenSaverQueryInfo (DISPLAY,
		RootWindow (DISPLAY, DefaultScreen (DISPLAY)), info))
	{
	    idleMs = info->idle;

	    if (idleMs >= thresholdMs)
	    {
		/*
		 * Fire once per idle period. Without this the locker would
		 * be re-launched every poll for as long as the machine stays
		 * idle, which stacks up processes behind the lock screen.
		 */
		if (!idleLockFired)
		{
		    idleLockFired = True;
		    SpawnCommand (wmGD.lockCommand);
		}
	    }
	    else
	    {
		idleLockFired = False;
	    }
	}
    }

    idleTimerId = XtAppAddTimeOut (wmGD.mwmAppContext, IDLE_POLL_MS,
				   IdleTimerProc, (XtPointer)NULL);
}

void InitIdleLock(void)
{
    int eventBase, errorBase;

    if (idleTimerId)
    {
	XtRemoveTimeOut (idleTimerId);
	idleTimerId = (XtIntervalId)0;
    }

    if (wmGD.lockTimeout <= 0 ||
	!wmGD.lockCommand || !(*(wmGD.lockCommand))) return;

    if (!idleLockAvailable)
    {
	if (!XScreenSaverQueryExtension (DISPLAY, &eventBase, &errorBase))
	{
	    Warning ("The X server has no MIT-SCREEN-SAVER extension; "
		     "lockTimeout will be ignored.");
	    return;
	}
	idleLockAvailable = True;
    }

    idleLockFired = False;

    idleTimerId = XtAppAddTimeOut (wmGD.mwmAppContext, IDLE_POLL_MS,
				   IdleTimerProc, (XtPointer)NULL);
}

#else /* IDLE_LOCK */

void InitIdleLock(void)
{
    if (wmGD.lockTimeout > 0)
    {
	Warning ("lockTimeout is set, but this build has the idle lock "
		 "timer compiled out (IDLE_LOCK).");
    }
}

#endif /* IDLE_LOCK */
