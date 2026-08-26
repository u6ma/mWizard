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

#endif /* WmEwmh_h */
