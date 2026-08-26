/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

#ifndef _WmWinfo_h
#define _WmWinfo_h

/*
 * Posts the mWinfo window, creating it on first use. Bound to f.about (and
 * its alias f.mwinfo), and reached from mWand through SIGUSR2.
 */
void PostWinfoDialog(void);

/*
 * Installs the SIGUSR2 handler that posts the window. Call once at startup,
 * after the application context exists.
 */
void InitWinfoDialog(void);

#endif /* _WmWinfo_h */
