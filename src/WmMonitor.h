/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

/*
 * The monitor layer: what mWizard knows about the physical heads that make up
 * one X screen.
 *
 * mWizard had half of this already. WmXinerama.c wrapped XineramaQueryScreens()
 * and about twenty layout sites asked it which head a point was on. What it
 * could not answer is everything that needs a monitor to have an *identity*:
 * which head the user called primary after a hotplug reordered the list, which
 * head a dock reserved space on, and -- new in 1.3 -- which workspace a head is
 * showing.
 *
 * So the source of truth is now RandR, which names its outputs, and Xinerama is
 * the fallback for a server that has no RandR at all. Four sources are tried in
 * order, best first:
 *
 *   1. XRRGetMonitors()          RandR 1.5. Name, primary flag and geometry in
 *                                one round trip, and it already merges mirrored
 *                                outputs into a single monitor, which is what
 *                                a window manager wants.
 *   2. XRRGetScreenResourcesCurrent() + XRRGetOutputInfo()
 *                                RandR 1.2. Same information assembled by hand.
 *                                "Current" rather than plain GetScreenResources
 *                                on purpose: the latter forces the server to
 *                                re-probe every output, which on some drivers
 *                                takes long enough to be visible as a stall.
 *   3. XineramaQueryScreens()    No names, no primary. Index order only.
 *   4. The root window as one unnamed monitor.
 *
 * The list is a small array built once at startup and rebuilt only from
 * UpdateMonitors(), which is called from the RRScreenChangeNotify handler. It
 * is never queried per layout decision -- the whole point of keeping it is that
 * MonitorFromLocation() is a scan over a handful of rectangles rather than a
 * round trip to the server.
 *
 * WmXinerama.c still exists and still has its old API; it is reimplemented on
 * top of this so that its existing callers pick up named monitors, per-monitor
 * struts and the nearest-monitor rule without being touched.
 */

#ifndef _WM_MONITOR_H
#define _WM_MONITOR_H

/*
 * One physical head.
 *
 * output/crtc are RandR XIDs, held as unsigned long rather than RROutput so
 * that this header -- which WmGlobal.h pulls in through WmScreenData -- does
 * not drag <X11/extensions/Xrandr.h> into every source file in the tree. Both
 * are 0 (None) when the list came from Xinerama or from the root window.
 */
typedef struct _WmMonitorData
{
    char		*name;		/* RandR output name, "DP-1"; never NULL */
    unsigned long	output;		/* RROutput, or 0 */
    unsigned long	crtc;		/* RRCrtc, or 0 */

    int			x;
    int			y;
    int			width;
    int			height;

    Boolean		primary;

    /*
     * Space reserved on this monitor by docked clients. Split out of the
     * screen-wide pSD->strut* fields so that a panel on one head stops
     * shrinking maximize on the others; see RecomputeStruts().
     */
    unsigned long	strutLeft;
    unsigned long	strutRight;
    unsigned long	strutTop;
    unsigned long	strutBottom;

    /*
     * The workspace this head is showing. Points into pSD->pWS -- monitors
     * share one workspace list rather than each having their own, so this is a
     * pointer and not a copy. NULL until InitMonitorWorkspaces() runs, which
     * cannot happen until the workspace list exists.
     */
    struct _WmWorkspaceData *pActiveWS;

} WmMonitorData;

/* Values for ClientData.monitorPresence */
#define MONITOR_FOLLOW	(-1)	/* whichever monitor the window is on */
#define MONITOR_ALL	(-2)	/* visible whatever any monitor is showing */

/*
 * Builds the monitor list for a screen. Always leaves at least one monitor
 * behind: a screen with no readable RandR or Xinerama data gets the root
 * window as a single unnamed head, so no caller has to handle an empty list.
 */
void SetupMonitors(WmScreenData *pSD);

/*
 * Rebuilds the list after a configuration change, preserving each monitor's
 * active workspace and struts by name where the name is still present.
 * Returns True if the set of monitors or their geometry actually changed.
 */
Boolean UpdateMonitors(WmScreenData *pSD);

/* Frees the list. */
void DestroyMonitors(WmScreenData *pSD);

/*
 * Index of the monitor containing a root-relative point.
 *
 * Never fails. A point no monitor covers -- which is not a corner case but the
 * ordinary result of an L-shaped arrangement, where the root bounding box is
 * larger than the union of the heads -- returns the nearest monitor by squared
 * edge distance. That is what lets the clamping sites in WmWinConf.c use this
 * without a fallback path of their own.
 */
int MonitorFromLocation(WmScreenData *pSD, int x, int y);

/* Index of the monitor a client's frame origin is on. */
int MonitorOfClient(ClientData *pCD);

/* Index of the monitor under the pointer, or the primary if the query fails. */
int MonitorFromPointer(WmScreenData *pSD);

/*
 * The monitor the user is working on, and therefore the one f.goto_workspace
 * and friends act on. Follows the xineramaScreenFocus resource, which already
 * spells out whether "active" means the pointer, the focused window or simply
 * the primary head.
 */
int ActiveMonitor(WmScreenData *pSD);

/* Index of a monitor by RandR output name, or -1 if there is no such name. */
int MonitorByName(WmScreenData *pSD, const char *name);

/*
 * Index of the user's preferred monitor: the primaryMonitor resource if it
 * names one that is present, else primaryXineramaScreen if it is in range,
 * else whichever head RandR marks primary, else 0.
 */
int PrimaryMonitor(WmScreenData *pSD);

/*
 * The monitor's rectangle less the space docked clients reserved on it.
 * Callers wanting the raw rectangle read pMonitors[i] directly.
 */
void MonitorWorkArea(WmScreenData *pSD, int monitor,
	int *x, int *y, int *width, int *height);

/*
 * Moves a point to the equivalent position on another monitor, preserving the
 * fraction of the way across the head it sat at. Used by f.move_to_monitor and
 * by the re-homing that follows a RandR change.
 */
void MapPointToMonitor(WmScreenData *pSD, int fromMon, int toMon,
	int *x, int *y, int width, int height);

/*
 * Resolves the argument shared by f.move_to_monitor and f.goto_monitor:
 * "next", "prev", "primary", or an output name. Returns -1 if it names
 * nothing, having already warned.
 */
int MonitorFromSpec(WmScreenData *pSD, const char *spec, int fromMon);

/*
 * One output's worth of a layout: what mWmonitor edits and what ~/.mmonitors
 * stores. Deliberately named rather than indexed, because the whole point of
 * the file is to survive being reloaded onto a differently ordered output list.
 */
typedef struct _WmMonitorConfig
{
    char	*name;		/* output name; owned by the config */
    Boolean	enabled;
    Boolean	primary;
    int		x, y;
    int		width, height;	/* mode dimensions */
    int		refresh;	/* whole Hz, 0 for "any mode this size" */
} WmMonitorConfig;

/*
 * Reads the current configuration of every connected output, enabled or not,
 * as a layout mWmonitor can edit. The caller frees it with FreeMonitorConfig().
 */
WmMonitorConfig *GetMonitorConfig(WmScreenData *pSD, int *pNum);
void FreeMonitorConfig(WmMonitorConfig *cfg, int num);

/*
 * Applies a layout through RandR. Returns False and warns on failure, having
 * changed as little as it could.
 */
Boolean ApplyMonitorLayout(WmScreenData *pSD, WmMonitorConfig *cfg, int num);

/*
 * The layout file, ~/.mmonitors. Entries are keyed on the set of connected
 * output names, so a docked and an undocked laptop each keep their own.
 */
Boolean SaveMonitorLayout(WmScreenData *pSD, WmMonitorConfig *cfg, int num);
Boolean ApplySavedMonitorLayout(WmScreenData *pSD);

/*
 * Notes the current set of connected outputs and reports whether it differs
 * from the set recorded last time. False means the RROutputChangeNotify that
 * prompted the call was a mode change or a blank rather than a plug.
 */
Boolean NoteConnectedOutputs(WmScreenData *pSD);

/*
 * The modes an output can drive, newest-first as RandR reports them, for
 * mWmonitor's mode menu. Returns a count and fills an array the caller frees
 * with XtFree().
 */
typedef struct _WmMonitorMode
{
    int		width, height;
    int		refresh;	/* whole Hz */
    Boolean	preferred;
} WmMonitorMode;

WmMonitorMode *GetOutputModes(WmScreenData *pSD, const char *name, int *pNum);

#endif /* _WM_MONITOR_H */
