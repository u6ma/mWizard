/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

#ifndef _WmExecDlg_h
#define _WmExecDlg_h

/*
 * Posts the Execute dialog, creating it on first use. Bound to f.run, and
 * reached from mWand through SIGUSR1.
 */
void PostExecDialog(void);

/*
 * Installs the SIGUSR1 handler that posts the dialog. Call once at startup,
 * after the application context exists.
 */
void InitExecDialog(void);

#endif /* _WmExecDlg_h */
