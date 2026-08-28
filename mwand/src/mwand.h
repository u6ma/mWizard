/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

/*
 * Shared internal declarations for mWand.
 *
 * mWand is a launcher, workspace switcher and clock for mWizard, derived from
 * xmtoolbox in emwm-utils. What was one 1400 line file is split here into the
 * pieces it was already made of: main.c holds startup and layout, launcher.c
 * the menus and command execution, switcher.c the workspace plumbing, clock.c
 * the date and time display, session.c the optional session menu, dialogs.c
 * the message boxes, and config.c the rc file settings.
 *
 * Unlike xmtoolbox, mWand talks to no session manager. It runs commands the
 * user configured, so it works under any window manager.
 */

#ifndef MWAND_H
#define MWAND_H

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#define APP_TITLE "mWand"

/*
 * The mWinfo menu item, named in one place because two menus offer it:
 * the built-in Commands menu (session.c) and the MWINFO rc keyword
 * (launcher.c).
 */
#define MWINFO_ITEM_LABEL "About mWizard..."
#define MWINFO_ITEM_MNEMONIC 'A'

/* Likewise for mWmonitor, the arranger; the MONITORS rc keyword. */
#define MONITORS_ITEM_LABEL "Monitors..."
#define MONITORS_ITEM_MNEMONIC 'M'

/*
 * The name spelled out. APP_TITLE is the short form and is what goes in the
 * title bar, which on a panel this narrow has no room for anything longer;
 * this is for the places that introduce the program.
 */
#define APP_FULL_TITLE "motifWand"
#define APP_NAME "mwand"
#define APP_CLASS "MWand"
#define RC_NAME "mwandrc"

/* The per-user file is ~/.mwandrc; the one installed system wide is named
 * system.mwandrc, the same way mWizard names system.mwizardrc. */
#define SYS_RC_NAME "system." RC_NAME

/* EWMH virtual desktop properties. mWizard has a more elaborate interface for
 * dealing with workspaces, but nothing here needs more than indexed
 * switching, and using EWMH means any conforming window manager will do. */
#define _NET_NUMBER_OF_DESKTOPS "_NET_NUMBER_OF_DESKTOPS"
#define _NET_CURRENT_DESKTOP "_NET_CURRENT_DESKTOP"

/* MWM workspace presence atom, used to put mWand on all workspaces */
/*
 * Signals mWizard accepts on the pid in _NET_WM_PID, advertised as a CARDINAL
 * bitmask in _MWIZARD_SIGNALS on the _NET_SUPPORTING_WM_CHECK window.
 *
 * Checked before signalling. Without it mWand would send SIGUSR1 or SIGUSR2
 * to whatever pid it found, and the default action for both is to terminate:
 * a window manager that is not mWizard, or one built before these handlers
 * existed, dies on the spot and takes the X session with it.
 *
 * mWizard's copy of these definitions is in src/WmEwmh.h; the two must agree.
 */
#define _XA_MWIZARD_SIGNALS "_MWIZARD_SIGNALS"
#define MWIZARD_SIGNAL_EXEC  (1L << 0)	/* SIGUSR1: the Execute dialog */
#define MWIZARD_SIGNAL_ABOUT (1L << 1)	/* SIGUSR2: mWinfo */

/*
 * Asking for one of those windows without a signal, new in mWizard 1.3.
 *
 * The signals ran out: SIGUSR1 and SIGUSR2 are the only two a process may
 * define for itself, both were taken, and mWmonitor is a third window. A
 * ClientMessage carries a verb rather than being one, so it does not run out,
 * and it needs none of the apparatus above -- there is no pid to find and
 * nothing to kill by getting it wrong.
 *
 * Sent to the root window, format 32, data.l[0] naming the command.
 * _MWIZARD_COMMANDS on the check window says which verbs are understood; when
 * it is absent the window manager predates 1.3 and the signals are used
 * instead. mWizard's copy of these definitions is in src/WmEwmh.h and the two
 * must agree.
 */
#define _XA_MWIZARD_COMMAND  "_MWIZARD_COMMAND"
#define _XA_MWIZARD_COMMANDS "_MWIZARD_COMMANDS"

#define MWIZARD_CMD_RUN     1	/* mWrun, the Execute prompt */
#define MWIZARD_CMD_ABOUT   2	/* mWinfo */
#define MWIZARD_CMD_MONITOR 3	/* mWmonitor */

#define _XA_MWM_WORKSPACE_PRESENCE "_MWM_WORKSPACE_PRESENCE"
#define _XA_MWM_WORKSPACE_ALL "all"

/*
 * Which monitor a window belongs to; mWizard 1.3 and later. A STRING naming
 * an output, or "all", "primary" or "current". mWizard's copy of this is in
 * src/WmGlobal.h.
 */
#define _XA_MWM_MONITOR_PRESENCE "_MWM_MONITOR_PRESENCE"

/*
 * How many X protocol errors are reported before the handler goes quiet. An
 * error raised from a redraw returns on every expose; see x_err_handler().
 */
#define X_ERROR_REPORT_LIMIT 20

/* Microseconds to wait for the window manager's atoms at startup */
#define UWAIT_FOR_ATOMS 250000
#define ATOM_WAIT_RETRIES 12

/*
 * Everything the user can configure. Read from the Settings block of the rc
 * file; see config.c.
 */
struct tb_resources {
	char *title;
	Boolean show_date_time;
	char *date_time_fmt;
	char *rc_file;
	char *hotkey;
	Boolean horizontal;
	Boolean separators;
	Boolean switcher;
	Boolean show_user_host;
	Boolean occupy_all;
	Boolean occupy_all_monitors;
	Boolean session_menu;
	char *lock_command;
	char *logout_command;
	char *suspend_command;
	char *reboot_command;
	char *shutdown_command;
	char *shell;
};

extern struct tb_resources app_res;

/* main.c */
extern XtAppContext app_context;
extern Widget wshell;
extern Widget wmain;
extern String rc_file_path;
extern unsigned int hotkey_mods;
void raise_and_focus(Widget w);

/* config.c */
extern XtResource xrdb_resources[];
extern Cardinal num_xrdb_resources;
void LoadRcSettings(Display *dpy, const char *rc_file);

/* main.c, but rc-file related */
char* FindRcFile(void);

/* launcher.c */
Boolean ConstructMenu(void);
void ExecuteCommandDialog(void);
void AboutWindowManagerDialog(void);
void MonitorDialog(void);
int RunCommand(const char *cmd_spec);

/* switcher.c */
extern Widget wswitch;
extern Atom xa_ndesks;
extern Atom xa_cdesk;
Widget CreateSwitcherWidget(Widget wparent);
Boolean GetWorkspaceInfo(unsigned short *ws_count, unsigned short *iactive);
void UpdateSwitcher(void);

/* clock.c */
extern Widget wdtframe;
extern Widget wdtlabel;
Widget CreateClockWidget(Widget wparent);
void UpdateClock(void);

/* session.c */
Widget CreateCommandMenu(Widget wparent);

/* dialogs.c */
Boolean MessageDialog(Boolean confirm, const char *message_str);
void ReportRcFileError(const char *rc_file, const char *err_desc);

#endif /* MWAND_H */
