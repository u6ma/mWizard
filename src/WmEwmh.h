/*
 * Copyright (C) 2018-2026 alx@fastestcode.org
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


#ifndef WmEwmh_h
#define WmEwmh_h

void SetupWmEwmh(void);
void ProcessEwmh(ClientData*);
void ProcessEwmhWindowType(ClientData*);
void SetEwmhActiveWindow(ClientData*);
void HandleEwmhCPropertyNotify(ClientData*, XPropertyEvent*);
void HandleEwmhClientMessage(ClientData*, XClientMessageEvent*);
Boolean HandleEwmhRootClientMessage(WmScreenData*, XClientMessageEvent*);
void ConfigureEwmhFullScreen(ClientData*, Boolean);
void UpdateEwmhClientList(WmScreenData*);
void RecomputeStruts(WmScreenData*);
void GetEwmhWorkArea(WmScreenData*, int *x, int *y, int *width, int *height);
void UpdateEwmhClientState(ClientData*);
void UpdateEwmhWorkspaceProperties(WmScreenData*);
void UpdateEwmhActiveWorkspace(WmScreenData*, WorkspaceID);


/*
 * Signals mWizard accepts on the pid it publishes in _NET_WM_PID.
 *
 * Advertised as a CARDINAL bitmask in _MWIZARD_SIGNALS on the
 * _NET_SUPPORTING_WM_CHECK window. It exists because the alternative is
 * unsafe: a program that finds a pid in a property and sends SIGUSR1 or
 * SIGUSR2 to it on the strength of that alone will kill any window manager
 * that does not happen to handle those signals -- the default action for both
 * is to terminate the process -- and killing the window manager ends the X
 * session. That covers a different window manager entirely, and equally a
 * mWizard built before these handlers existed.
 *
 * A bit is set by the function that installs the matching handler, and only
 * there, so the property cannot claim a signal that nothing is listening for.
 *
 * mWand reads this before signalling; its copy of these definitions is in
 * mwand/src/mwand.h and the two must agree.
 */
#define MWIZARD_SIGNALS_PROPERTY	"_MWIZARD_SIGNALS"
#define MWIZARD_SIGNAL_EXEC		(1L << 0)	/* SIGUSR1: f.run */
#define MWIZARD_SIGNAL_ABOUT		(1L << 1)	/* SIGUSR2: f.about */

/*
 * Records that mWizard handles one of the signals above, and republishes the
 * property. Call from the function that installed the handler, after
 * SetupWmEwmh() has made the check window.
 */
void AdvertiseWmSignal(unsigned long bit);

/*
 * Asking mWizard to put up one of its own windows, without a signal.
 *
 * The signal mechanism above ran out. SIGUSR1 and SIGUSR2 are the only two
 * signals a process may define for itself, both are spoken for, and there is
 * no third -- so mWmonitor, the third such window, needed another way to be
 * asked for.
 *
 * A ClientMessage is that way, and is the better mechanism besides. It carries
 * a verb rather than being one, so a fourth window costs an enum value instead
 * of a scarce resource; it is routed by the X server to the window manager
 * that is actually running rather than to a pid read out of a property; and it
 * is inherently safe, where the whole apparatus of _MWIZARD_SIGNALS exists
 * only because sending SIGUSR1 to something that is not listening kills it and
 * takes the X session down.
 *
 * Sent to the root window with message_type _MWIZARD_COMMAND, format 32, and
 * data.l[0] naming the command. An unrecognised verb is ignored.
 *
 * _MWIZARD_COMMANDS on the check window advertises which verbs are understood,
 * so that mWand can fall back to the signals when it is talking to a mWizard
 * older than 1.3. mWand's copy of these definitions is in mwand/src/mwand.h
 * and the two must agree.
 */
#define MWIZARD_COMMAND_PROPERTY	"_MWIZARD_COMMAND"
#define MWIZARD_COMMANDS_PROPERTY	"_MWIZARD_COMMANDS"

#define MWIZARD_CMD_RUN			1	/* f.run, the mWrun prompt */
#define MWIZARD_CMD_ABOUT		2	/* f.about, mWinfo */
#define MWIZARD_CMD_MONITOR		3	/* f.monitors, mWmonitor */

/*
 * Records that mWizard understands one of the commands above and republishes
 * _MWIZARD_COMMANDS. Call from the same Init function that makes the command
 * work, so that the property cannot promise something nothing implements.
 */
void AdvertiseWmCommand(unsigned long command);

/*
 * True if this ClientMessage was a _MWIZARD_COMMAND and has been acted on.
 * Called from HandleEwmhRootClientMessage().
 */
Boolean HandleWmCommandMessage(XClientMessageEvent *evt);

#endif /* WmEwmh_h */
