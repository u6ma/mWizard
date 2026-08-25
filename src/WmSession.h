/*
 * Copyright (C) 2026 845 <vinci845@icloud.com>
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
 * Starts (or, when the settings change, restarts) the idle lock timer.
 * Does nothing unless lockTimeout is greater than zero and lockCommand is
 * a non-empty string. Compiled out entirely without IDLE_LOCK.
 */
void InitIdleLock(void);

#endif /* _WmSession_h */
