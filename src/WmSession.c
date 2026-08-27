/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
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
#include "WmWinInfo.h"

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
 * Runs a shell command in a detached child process, and reports which one.
 *
 * This is the body F_Exec has always used; it lives here so that the session
 * functions and the idle lock timer can share it. As in F_Exec, DISPLAY is
 * set for the active screen before forking and restored afterwards, so that
 * the child lands on the screen the user acted on.
 *
 * Most callers want SpawnCommand() and its yes-or-no answer. The pid is for
 * SpawnManagedCommand() below, which has to be able to find the child again.
 */
static pid_t SpawnCommandPid(const char *command)
{
    pid_t pid;
    char *shell;
    char *shellname;

    if (!command || !(*command)) return ((pid_t) -1);

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
	 * Exec the command using the execShell resource if set, then
	 * $WMSHELL, then $SHELL, then sh.
	 *
	 * execShell comes first because it is the one an rc file can state:
	 * $SHELL is whatever the login shell happens to be, which on many
	 * systems is an interactive shell that sources a profile on every
	 * f.exec, and the rc file should be able to say "use /bin/sh" without
	 * the user having to change their login shell.
	 */
	if (((shell = wmGD.execShell) != NULL && *shell) ||
	    ((shell = getenv ("WMSHELL")) != NULL) ||
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

    return (pid);
}

Boolean SpawnCommand(const char *command)
{
    return (SpawnCommandPid (command) != (pid_t) -1);
}

/*
 * The children mWizard started itself and starts again every time it comes
 * up: the Startup block and the system tray.
 *
 * They are remembered so that f.restart can end them on its way out, which is
 * what makes a restart a refresh of the whole session rather than of the
 * window manager alone -- the new instance re-reads the rc file and starts
 * them again from it. Nothing else is recorded here: an f.exec from a menu is
 * the user's own window and is no more the window manager's to close than any
 * other client is.
 */
static pid_t *managedPids = NULL;
static int    numManagedPids = 0;

Boolean SpawnManagedCommand(const char *command)
{
    pid_t pid;

    if ((pid = SpawnCommandPid (command)) == (pid_t) -1) return (False);

    managedPids = (pid_t *) XtRealloc ((char *)managedPids,
				       (numManagedPids + 1) * sizeof(pid_t));
    managedPids[numManagedPids++] = pid;

    return (True);
}

/*
 * Ends those children, and does not come back until they are gone.
 *
 * Waiting matters: the caller re-execs immediately afterwards, and the new
 * instance decides whether to start a tray by asking who owns the tray
 * selection. Leave before the old tray has dropped it and the session comes
 * back with none.
 *
 * The wait cannot be a waitpid(): SetupWmSignalHandlers() sets SIGCHLD to
 * SIG_IGN, so a child is reaped by the kernel the moment it dies and there is
 * nothing left to wait for. kill(pid, 0) answers the same question -- it
 * fails with ESRCH once the pid is gone -- and it is the reaping that makes
 * that answer trustworthy, since an unreaped zombie would still accept it.
 *
 * The signal goes to the process group rather than the process. SpawnCommand
 * puts every child in a session of its own through setsid(), so the group is
 * exactly this command and whatever it started -- which is how a Startup
 * entry written as a pipeline, or one that backgrounds its real work and lets
 * the shell exit, is ended along with the shell that ran it.
 *
 * That same setsid() is what the getpgid() check leans on. A pid is only
 * meaningful while its process lives, and these have been reaped, so in a
 * long-lived session one could in principle have been handed out again by the
 * time this runs. A pid that is no longer its own group leader is certainly
 * not one of ours, and is left alone.
 */
void TerminateManagedChildren(void)
{
    struct timespec slice;
    int  i, tries;
    Boolean anyLeft = False;

    for (i = 0; i < numManagedPids; i++)
    {
	if (getpgid (managedPids[i]) != managedPids[i] ||
	    kill (-managedPids[i], SIGTERM) != 0)
	{
	    managedPids[i] = 0;
	    continue;
	}
	anyLeft = True;
    }

    slice.tv_sec  = 0;
    slice.tv_nsec = 20 * 1000 * 1000;		/* 20ms */

    /* Two seconds of asking, then insist. */
    for (tries = 0; anyLeft && tries < 100; tries++)
    {
	nanosleep (&slice, NULL);

	anyLeft = False;
	for (i = 0; i < numManagedPids; i++)
	{
	    if (managedPids[i] == 0) continue;

	    if (kill (managedPids[i], 0) != 0)
		managedPids[i] = 0;
	    else
		anyLeft = True;
	}
    }

    for (i = 0; i < numManagedPids; i++)
	if (managedPids[i] != 0) kill (-managedPids[i], SIGKILL);

    XtFree ((char *)managedPids);
    managedPids = NULL;
    numManagedPids = 0;
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


/*
 * Starts the system tray named by trayCommand, if one is not running already.
 *
 * mWizard has no tray of its own; this runs whatever the user configured,
 * typically stalonetray. The selection check is what keeps a tray the window
 * manager did not start from being duplicated -- one the session file started
 * before mWizard, say, or one left behind by a restart that could not end it.
 * The tray mWizard does start is recorded, and f.restart ends it before
 * re-execing, so this finds the selection free and starts it afresh.
 */
void InitSystemTray(void)
{
	char sel_name[32];
	Atom sel_atom;
	int scr;

	if(!wmGD.trayCommand || !(*(wmGD.trayCommand))) return;

	for(scr = 0; scr < wmGD.numScreens; scr++) {
		if(!wmGD.Screens[scr].managed) continue;

		snprintf(sel_name, sizeof(sel_name), "_NET_SYSTEM_TRAY_S%d",
			wmGD.Screens[scr].screen);

		sel_atom = XInternAtom(DISPLAY, sel_name, False);

		if(XGetSelectionOwner(DISPLAY, sel_atom) == None) {
			SpawnManagedCommand(wmGD.trayCommand);
			return;
		}
	}
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

/*
 * ---------------------------------------------------------------------------
 * Restart state.
 *
 * f.restart execs the window manager again -- execvp() on the saved argv, so
 * an mWizard that has been rebuilt and installed underneath the running one
 * is picked up on the spot. What has to survive that is the desktop: the
 * clients are handed back to the root window rather than killed, and the next
 * instance adopts them in AdoptInitialClients().
 *
 * Most of what it needs to know is already on the windows themselves.
 * WM_STATE says whether a client was iconified, _MWM_WORKSPACE_PRESENCE says
 * which workspaces it lived in, and X itself remembers where each window is
 * and how big. Two things are not written down anywhere, and were lost every
 * restart until they were:
 *
 *   - whether a window was maximized. WM_STATE has no such state; ICCCM does
 *     not model one. A maximized window came back at its maximized size but
 *     with the window manager believing that was its ordinary size, so its
 *     real one was gone and Restore did nothing.
 *
 *   - which workspace was in front. The next instance started on the first
 *     one, or on whatever initialWorkspace named, and a session spread over
 *     several workspaces came back looking rearranged.
 *
 * Both are recorded here as properties, which is the same way everything else
 * that survives a restart does it: the state lives on the window it describes,
 * the window outlives the process, and no file is involved. The properties are
 * read with delete set, so nothing is left behind to confuse a later cold
 * start.
 * ---------------------------------------------------------------------------
 */

#define _XA_MWIZARD_RESTART		"_MWIZARD_RESTART"
#define _XA_MWIZARD_RESTART_WORKSPACE	"_MWIZARD_RESTART_WORKSPACE"

/* Property layout: flags, then the client's normal geometry. */
#define RESTART_PROP_LEN	5
#define RESTART_MAXIMIZED	(1L << 0)

static Atom RestartAtom(void)
{
    static Atom atom = None;

    if (atom == None)
	atom = XInternAtom (DISPLAY, _XA_MWIZARD_RESTART, False);

    return (atom);
}

static Atom RestartWorkspaceAtom(void)
{
    static Atom atom = None;

    if (atom == None)
	atom = XInternAtom (DISPLAY, _XA_MWIZARD_RESTART_WORKSPACE, False);

    return (atom);
}

/*
 * Records what the next instance cannot work out for itself.
 *
 * The geometry written is the client's own, before the window gravity
 * offset -- which is what DeFrameClient() hands back to the root window for
 * an ordinary client, and what InitClientPlacement() adds the offset back
 * onto. Storing it any other way would move maximized windows a title bar's
 * worth every restart.
 *
 * Only maximized clients need the geometry at all: for everything else the
 * window on the server still is the truth, and reading it back is both
 * simpler and correct if the client resized itself on the way out.
 */
void SaveRestartState(void)
{
    int scr;

    for (scr = 0; scr < wmGD.numScreens; scr++)
    {
	WmScreenData *pSD = &(wmGD.Screens[scr]);
	ClientListEntry *pEntry;

	if (!pSD->managed) continue;

	if (pSD->pActiveWS && pSD->pActiveWS->name)
	{
	    XChangeProperty (DISPLAY, pSD->rootWindow,
		RestartWorkspaceAtom(), XA_STRING, 8, PropModeReplace,
		(unsigned char *) pSD->pActiveWS->name,
		(int) strlen (pSD->pActiveWS->name));
	}

	for (pEntry = pSD->lastClient; pEntry; pEntry = pEntry->prevSibling)
	{
	    ClientData *pCD = pEntry->pCD;
	    long data[RESTART_PROP_LEN];
	    int xoff, yoff;

	    if (pEntry->type != NORMAL_STATE || !pCD || !pCD->client) continue;

	    CalculateGravityOffset (pCD, &xoff, &yoff);

	    data[0] = pCD->maxConfig ? RESTART_MAXIMIZED : 0;
	    data[1] = pCD->clientX - xoff;
	    data[2] = pCD->clientY - yoff;
	    data[3] = pCD->clientWidth;
	    data[4] = pCD->clientHeight;

	    XChangeProperty (DISPLAY, pCD->client, RestartAtom(),
		RestartAtom(), 32, PropModeReplace,
		(unsigned char *) data, RESTART_PROP_LEN);
	}
    }
}

/*
 * Puts a maximized client back the way it was.
 *
 * Called from ManageWindow() while the client is still being set up, in the
 * same place the restart's iconic clients get theirs. Nothing is maximized
 * here: the state is only asked for, and the SetClientState() at the end of
 * ManageWindow() carries it out through the path a user's Maximize would
 * take. That is the whole reason it is done this way rather than by filling
 * in maxConfig and the maximized geometry by hand.
 */
void RestoreClientRestartState(ClientData *pCD)
{
    Atom actualType;
    int actualFormat;
    unsigned long nitems, leftover;
    long *data = NULL;

    if (!pCD || !pCD->client) return;

    if (XGetWindowProperty (DISPLAY, pCD->client, RestartAtom(), 0L,
	    (long) RESTART_PROP_LEN, True, RestartAtom(),
	    &actualType, &actualFormat, &nitems, &leftover,
	    (unsigned char **) &data) != Success)
    {
	return;
    }

    if (!data) return;

    if ((actualType == RestartAtom()) && (actualFormat == 32) &&
	(nitems == RESTART_PROP_LEN) && (data[0] & RESTART_MAXIMIZED))
    {
	pCD->clientX      = (int) data[1];
	pCD->clientY      = (int) data[2];
	pCD->clientWidth  = (int) data[3];
	pCD->clientHeight = (int) data[4];

	pCD->clientState = MAXIMIZED_STATE;
    }

    XFree ((char *) data);
}

/*
 * The workspace that was in front, or NULL if this is not a restart or
 * nothing was recorded. The caller owns the string.
 *
 * Handed to pSD->initialWorkspace, which already exists for exactly this and
 * has since the days when a session manager filled it in. An initialWorkspace
 * set in the rc file wins: that one is a deliberate choice about how every
 * session starts, and a restart is not a new session.
 */
char *GetRestartWorkspace(WmScreenData *pSD)
{
    Atom actualType;
    int actualFormat;
    unsigned long nitems, leftover;
    unsigned char *data = NULL;
    char *name;

    if (!wmGD.wmRestarted) return (NULL);

    if (XGetWindowProperty (DISPLAY, pSD->rootWindow,
	    RestartWorkspaceAtom(), 0L, (long) MAXWMPATH, True, XA_STRING,
	    &actualType, &actualFormat, &nitems, &leftover,
	    &data) != Success)
    {
	return (NULL);
    }

    if (!data) return (NULL);

    if ((actualType != XA_STRING) || (actualFormat != 8) || (nitems == 0))
    {
	XFree ((char *) data);
	return (NULL);
    }

    name = XtMalloc (nitems + 1);
    memcpy (name, data, nitems);
    name[nitems] = '\0';

    XFree ((char *) data);

    return (name);
}
