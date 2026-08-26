/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

/*
 * Internal session handling for mWizard.
 *
 * This replaces EMWM's XSMP support (WmXSMP.c), which did nothing unless an
 * external X session manager was running. Instead of speaking a protocol to
 * some other program, mWizard simply runs a command the user configured.
 * Every command defaults to a systemd invocation but is a plain string
 * resource, so pointing it at loginctl, doas, a script or nothing at all is
 * a configuration change rather than a rebuild.
 */

#ifndef _WmSession_h
#define _WmSession_h

#include <X11/Intrinsic.h>
#include "WmGlobal.h"

/*
 * Terminates the window manager. Kept with the signature WmXSMP.c used, so
 * that the ~40 error-path callers scattered through initialization and
 * resource parsing are unaffected. Safe to call before the display or the
 * screen data have been set up.
 */
_X_NORETURN void ExitWM(int exitCode);

/*
 * Runs a shell command in a detached child process. This is the fork/exec
 * path that F_Exec has always used, factored out so that the session
 * functions and the idle lock timer share it rather than duplicating it.
 * Returns False if the command is NULL or empty, or if the fork failed.
 */
Boolean SpawnCommand(const char *command);

/*
 * The same, for a command mWizard is expected to start again every time it
 * comes up -- the Startup block and the system tray. The child is remembered
 * so that TerminateManagedChildren() can end it.
 */
Boolean SpawnManagedCommand(const char *command);

/*
 * Ends every child started through SpawnManagedCommand() and waits for them
 * to go, so that RestartWm() can re-exec into an instance that starts them
 * again. Called on the way to a restart and nowhere else: on the way out of
 * the session there is nothing to hand them over to.
 */
void TerminateManagedChildren(void);

/*
 * Starts (or, when the settings change, restarts) the idle lock timer.
 * Does nothing unless lockTimeout is greater than zero and lockCommand is
 * a non-empty string. Compiled out entirely without IDLE_LOCK.
 */
void InitIdleLock(void);

/*
 * Starts the configured system tray, unless one already owns the
 * _NET_SYSTEM_TRAY selection on a managed screen.
 */
void InitSystemTray(void);

#endif /* _WmSession_h */
