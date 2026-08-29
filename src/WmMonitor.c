/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

/*
 * The monitor layer. The rationale, and why there are four sources of monitor
 * data tried in order, is written out at the top of WmMonitor.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xinerama.h>

#include "WmGlobal.h"
#include "WmError.h"
#include "WmMonitor.h"

static void SortMonitors(WmMonitorData *mon, int n);
static Boolean MonitorsFromRandR15(WmScreenData *pSD,
	WmMonitorData **ppMon, int *pNum);
static Boolean MonitorsFromRandR12(WmScreenData *pSD,
	WmMonitorData **ppMon, int *pNum);
static Boolean MonitorsFromXinerama(WmScreenData *pSD,
	WmMonitorData **ppMon, int *pNum);
static void MonitorsFromRoot(WmScreenData *pSD,
	WmMonitorData **ppMon, int *pNum);
static void CarryStateForward(WmMonitorData *old, int numOld,
	WmMonitorData *new, int numNew);
static void FreeMonitorList(WmMonitorData *mon, int num);

/*
 * Left-to-right, then top-to-bottom.
 *
 * The order is what "next monitor" means to f.move_to_monitor and what the
 * arranger lists, so it has to be the one the user sees on the desk rather
 * than whatever order the server happened to report outputs in. It also has to
 * be stable across a rebuild, or a hotplug would silently renumber the heads
 * under primaryXineramaScreen.
 */
static void SortMonitors(WmMonitorData *mon, int n)
{
    int i, j;

    for (i = 1; i < n; i++)
    {
	WmMonitorData tmp = mon[i];

	for (j = i;
	     j > 0 && (mon[j - 1].x > tmp.x ||
		       (mon[j - 1].x == tmp.x && mon[j - 1].y > tmp.y));
	     j--)
	{
	    mon[j] = mon[j - 1];
	}
	mon[j] = tmp;
    }
}

/*
 * RandR 1.5. The best source: XRRGetMonitors() already merges the outputs of a
 * mirrored set into one monitor and marks the primary, which is exactly the
 * view a window manager wants and the one both older paths have to assemble by
 * hand.
 */
static Boolean MonitorsFromRandR15(WmScreenData *pSD,
	WmMonitorData **ppMon, int *pNum)
{
    XRRMonitorInfo *info;
    WmMonitorData *mon;
    int num = 0;
    int i;
    int major = 0, minor = 0;

    if (!wmGD.xrandr_present) return (False);
    if (!XRRQueryVersion (DISPLAY, &major, &minor)) return (False);
    if (major < 1 || (major == 1 && minor < 5)) return (False);

    /* True: active monitors only. An output with no CRTC is not a head. */
    info = XRRGetMonitors (DISPLAY, pSD->rootWindow, True, &num);
    if (!info || num <= 0)
    {
	if (info) XRRFreeMonitors (info);
	return (False);
    }

    mon = (WmMonitorData *) XtCalloc (num, sizeof (WmMonitorData));

    for (i = 0; i < num; i++)
    {
	char *atomName = (info[i].name != None) ?
		XGetAtomName (DISPLAY, info[i].name) : NULL;

	/*
	 * Copied out of the Xlib allocation straight away so that every name
	 * in the list frees the same way, whichever source produced it.
	 */
	mon[i].name = XtNewString (atomName ? atomName : "monitor");
	if (atomName) XFree (atomName);

	mon[i].output = (info[i].noutput > 0) ?
		(unsigned long) info[i].outputs[0] : 0;
	mon[i].crtc = 0;
	mon[i].x = info[i].x;
	mon[i].y = info[i].y;
	mon[i].width = info[i].width;
	mon[i].height = info[i].height;
	mon[i].primary = info[i].primary ? True : False;
    }

    XRRFreeMonitors (info);

    SortMonitors (mon, num);
    *ppMon = mon;
    *pNum = num;
    return (True);
}

/*
 * RandR 1.2. Every connected output that has a CRTC becomes a monitor.
 *
 * Outputs sharing a CRTC are mirrored and describe one head, so the first one
 * seen for a CRTC wins and the rest are skipped -- otherwise a mirrored pair
 * would appear as two heads stacked at the same coordinates, and every
 * "which monitor is this point on" answer would depend on list order.
 *
 * GetScreenResourcesCurrent, not GetScreenResources: the latter makes the
 * server re-probe every output, which is a visible stall on some drivers and
 * is not wanted on a path that runs at startup and on every screen change.
 */
static Boolean MonitorsFromRandR12(WmScreenData *pSD,
	WmMonitorData **ppMon, int *pNum)
{
    XRRScreenResources *res;
    WmMonitorData *mon;
    RROutput primary;
    int num = 0;
    int i, j;

    if (!wmGD.xrandr_present) return (False);

    res = XRRGetScreenResourcesCurrent (DISPLAY, pSD->rootWindow);
    if (!res) return (False);

    if (res->noutput <= 0)
    {
	XRRFreeScreenResources (res);
	return (False);
    }

    primary = XRRGetOutputPrimary (DISPLAY, pSD->rootWindow);
    mon = (WmMonitorData *) XtCalloc (res->noutput, sizeof (WmMonitorData));

    for (i = 0; i < res->noutput; i++)
    {
	XRROutputInfo *oi;
	XRRCrtcInfo *ci;
	Boolean duplicate = False;

	oi = XRRGetOutputInfo (DISPLAY, res, res->outputs[i]);
	if (!oi) continue;

	if (oi->connection != RR_Connected || oi->crtc == None)
	{
	    XRRFreeOutputInfo (oi);
	    continue;
	}

	for (j = 0; j < num; j++)
	{
	    if (mon[j].crtc == (unsigned long) oi->crtc) duplicate = True;
	}
	if (duplicate)
	{
	    XRRFreeOutputInfo (oi);
	    continue;
	}

	ci = XRRGetCrtcInfo (DISPLAY, res, oi->crtc);
	if (!ci)
	{
	    XRRFreeOutputInfo (oi);
	    continue;
	}

	mon[num].name = XtNewString (oi->name ? oi->name : "monitor");
	mon[num].output = (unsigned long) res->outputs[i];
	mon[num].crtc = (unsigned long) oi->crtc;
	mon[num].x = ci->x;
	mon[num].y = ci->y;
	mon[num].width = (int) ci->width;
	mon[num].height = (int) ci->height;
	mon[num].primary = (res->outputs[i] == primary) ? True : False;
	num++;

	XRRFreeCrtcInfo (ci);
	XRRFreeOutputInfo (oi);
    }

    XRRFreeScreenResources (res);

    if (num == 0)
    {
	XtFree ((char *) mon);
	return (False);
    }

    SortMonitors (mon, num);
    *ppMon = mon;
    *pNum = num;
    return (True);
}

/*
 * Xinerama. No output names and no primary flag, so the heads are numbered.
 * This is what a server without RandR gives, and what the pre-1.3 code used
 * for everything.
 */
static Boolean MonitorsFromXinerama(WmScreenData *pSD,
	WmMonitorData **ppMon, int *pNum)
{
    XineramaScreenInfo *xsi;
    WmMonitorData *mon;
    int num = 0;
    int i;

    if (!XineramaIsActive (DISPLAY)) return (False);

    xsi = XineramaQueryScreens (DISPLAY, &num);
    if (!xsi || num <= 0)
    {
	if (xsi) XFree (xsi);
	return (False);
    }

    mon = (WmMonitorData *) XtCalloc (num, sizeof (WmMonitorData));

    for (i = 0; i < num; i++)
    {
	char buf[32];

	snprintf (buf, sizeof (buf), "screen%d", i);
	mon[i].name = XtNewString (buf);
	mon[i].output = 0;
	mon[i].crtc = 0;
	mon[i].x = xsi[i].x_org;
	mon[i].y = xsi[i].y_org;
	mon[i].width = xsi[i].width;
	mon[i].height = xsi[i].height;
	mon[i].primary = (i == 0) ? True : False;
    }

    XFree (xsi);

    SortMonitors (mon, num);
    *ppMon = mon;
    *pNum = num;
    return (True);
}

/*
 * The last resort, and the reason no caller has to handle an empty list: one
 * monitor covering the whole root window.
 */
static void MonitorsFromRoot(WmScreenData *pSD,
	WmMonitorData **ppMon, int *pNum)
{
    WmMonitorData *mon = (WmMonitorData *) XtCalloc (1, sizeof (WmMonitorData));

    mon[0].name = XtNewString ("screen0");
    mon[0].output = 0;
    mon[0].crtc = 0;
    mon[0].x = 0;
    mon[0].y = 0;
    mon[0].width = DisplayWidth (DISPLAY, pSD->screen);
    mon[0].height = DisplayHeight (DISPLAY, pSD->screen);
    mon[0].primary = True;

    *ppMon = mon;
    *pNum = 1;
}

/*
 * Carries per-monitor state that RandR knows nothing about across a rebuild.
 *
 * Matched by name rather than by index on purpose: a hotplug renumbers the
 * list, and carrying by index would hand a head the workspace another head was
 * showing. A monitor that has just appeared keeps its zeroed state and is
 * given a workspace by InitMonitorWorkspaces().
 */
static void CarryStateForward(WmMonitorData *old, int numOld,
	WmMonitorData *new, int numNew)
{
    int i, j;

    for (i = 0; i < numNew; i++)
    {
	for (j = 0; j < numOld; j++)
	{
	    if (strcmp (new[i].name, old[j].name)) continue;

	    new[i].pActiveWS = old[j].pActiveWS;
	    new[i].strutLeft = old[j].strutLeft;
	    new[i].strutRight = old[j].strutRight;
	    new[i].strutTop = old[j].strutTop;
	    new[i].strutBottom = old[j].strutBottom;
	    break;
	}
    }
}

static void FreeMonitorList(WmMonitorData *mon, int num)
{
    int i;

    if (!mon) return;

    for (i = 0; i < num; i++)
    {
	if (mon[i].name) XtFree (mon[i].name);
    }
    XtFree ((char *) mon);
}

void SetupMonitors(WmScreenData *pSD)
{
    WmMonitorData *mon = NULL;
    int num = 0;

    if (!MonitorsFromRandR15 (pSD, &mon, &num) &&
	!MonitorsFromRandR12 (pSD, &mon, &num) &&
	!MonitorsFromXinerama (pSD, &mon, &num))
    {
	MonitorsFromRoot (pSD, &mon, &num);
    }

    pSD->pMonitors = mon;
    pSD->numMonitors = num;
}

Boolean UpdateMonitors(WmScreenData *pSD)
{
    WmMonitorData *old = pSD->pMonitors;
    int numOld = pSD->numMonitors;
    WmMonitorData *mon = NULL;
    int num = 0;
    Boolean changed = False;
    int i;

    if (!MonitorsFromRandR15 (pSD, &mon, &num) &&
	!MonitorsFromRandR12 (pSD, &mon, &num) &&
	!MonitorsFromXinerama (pSD, &mon, &num))
    {
	MonitorsFromRoot (pSD, &mon, &num);
    }

    if (num != numOld)
    {
	changed = True;
    }
    else
    {
	for (i = 0; i < num; i++)
	{
	    if (mon[i].x != old[i].x || mon[i].y != old[i].y ||
		mon[i].width != old[i].width ||
		mon[i].height != old[i].height ||
		strcmp (mon[i].name, old[i].name))
	    {
		changed = True;
		break;
	    }
	}
    }

    CarryStateForward (old, numOld, mon, num);
    FreeMonitorList (old, numOld);

    pSD->pMonitors = mon;
    pSD->numMonitors = num;

    return (changed);
}

void DestroyMonitors(WmScreenData *pSD)
{
    FreeMonitorList (pSD->pMonitors, pSD->numMonitors);
    pSD->pMonitors = NULL;
    pSD->numMonitors = 0;

    if (pSD->connectedOutputs)
    {
	XtFree (pSD->connectedOutputs);
	pSD->connectedOutputs = NULL;
    }
}

int MonitorContaining(WmScreenData *pSD, int x, int y)
{
    int i;

    if (!pSD) return (-1);

    for (i = 0; i < pSD->numMonitors; i++)
    {
	WmMonitorData *m = &pSD->pMonitors[i];

	if (x >= m->x && x < (m->x + m->width) &&
	    y >= m->y && y < (m->y + m->height))
	{
	    return (i);
	}
    }

    return (-1);
}

int MonitorFromLocation(WmScreenData *pSD, int x, int y)
{
    int i;
    int best = 0;
    long bestDist = -1;

    if (!pSD || pSD->numMonitors < 1) return (0);

    for (i = 0; i < pSD->numMonitors; i++)
    {
	WmMonitorData *m = &pSD->pMonitors[i];

	if (x >= m->x && x < (m->x + m->width) &&
	    y >= m->y && y < (m->y + m->height))
	{
	    return (i);
	}
    }

    /*
     * No monitor covers the point. That is not an error case: two heads of
     * different heights sitting side by side leave the root bounding box with
     * corners no monitor occupies, and a window dragged into one has to land
     * somewhere sensible. Nearest by squared distance to the rectangle.
     */
    for (i = 0; i < pSD->numMonitors; i++)
    {
	WmMonitorData *m = &pSD->pMonitors[i];
	long dx = 0, dy = 0, dist;

	if (x < m->x) dx = m->x - x;
	else if (x >= (m->x + m->width)) dx = x - (m->x + m->width - 1);

	if (y < m->y) dy = m->y - y;
	else if (y >= (m->y + m->height)) dy = y - (m->y + m->height - 1);

	dist = dx * dx + dy * dy;

	if (bestDist < 0 || dist < bestDist)
	{
	    bestDist = dist;
	    best = i;
	}
    }

    return (best);
}

int MonitorOfClient(ClientData *pCD)
{
    if (!pCD || !pCD->pSD) return (0);

    /*
     * A window pinned to one head answers with that head whatever its
     * coordinates say, which is what makes the pin survive a stray move.
     */
    if (pCD->monitorPresence >= 0 &&
	pCD->monitorPresence < pCD->pSD->numMonitors)
    {
	return (pCD->monitorPresence);
    }

    return (MonitorFromLocation (pCD->pSD, pCD->clientX, pCD->clientY));
}

int MonitorFromPointer(WmScreenData *pSD)
{
    Window wroot, wchild;
    int rootX, rootY, childX, childY;
    unsigned int mask;

    if (!XQueryPointer (DISPLAY, pSD->rootWindow, &wroot, &wchild,
	    &rootX, &rootY, &childX, &childY, &mask))
    {
	return (PrimaryMonitor (pSD));
    }

    return (MonitorFromLocation (pSD, rootX, rootY));
}

int ActiveMonitor(WmScreenData *pSD)
{
    if (!pSD || pSD->numMonitors < 2) return (0);

    switch (wmGD.xineramaScreenFocus)
    {
	case XRS_FOCUS_PRIMARY:
	    return (PrimaryMonitor (pSD));

	case XRS_FOCUS_KEYBOARD:
	    /*
	     * Falls through to the pointer when nothing has the focus, which
	     * is the ordinary state right after a workspace switch empties a
	     * head -- answering "monitor 0" there would move the user's work
	     * to a head they are not looking at.
	     */
	    if (wmGD.keyboardFocus && wmGD.keyboardFocus->pSD == pSD)
		return (MonitorOfClient (wmGD.keyboardFocus));
	    break;

	default:
	    break;
    }

    return (MonitorFromPointer (pSD));
}

int MonitorByName(WmScreenData *pSD, const char *name)
{
    int i;

    if (!name || !pSD) return (-1);

    for (i = 0; i < pSD->numMonitors; i++)
    {
	if (!strcmp (pSD->pMonitors[i].name, name)) return (i);
    }

    return (-1);
}

int PrimaryMonitor(WmScreenData *pSD)
{
    int i;

    if (!pSD || pSD->numMonitors < 1) return (0);

    /*
     * primaryMonitor names an output, so it is the one preference that
     * survives a hotplug reordering the list. It wins over the index-based
     * resource for exactly that reason.
     */
    if (wmGD.primaryMonitor)
    {
	i = MonitorByName (pSD, wmGD.primaryMonitor);
	if (i >= 0) return (i);
    }

    /*
     * primaryXineramaScreen kept working from before 1.3. Out of range is
     * silently ignored here rather than warned about on every call; the
     * warning belongs where the resource is read.
     */
    if (wmGD.primaryXineramaScreen > 0 &&
	wmGD.primaryXineramaScreen < pSD->numMonitors)
    {
	return (wmGD.primaryXineramaScreen);
    }

    for (i = 0; i < pSD->numMonitors; i++)
    {
	if (pSD->pMonitors[i].primary) return (i);
    }

    return (0);
}

void MonitorWorkArea(WmScreenData *pSD, int monitor,
	int *x, int *y, int *width, int *height)
{
    WmMonitorData *m;
    int wx, wy, ww, wh;

    if (!pSD || pSD->numMonitors < 1)
    {
	if (x) *x = 0;
	if (y) *y = 0;
	if (width) *width = DisplayWidth (DISPLAY, pSD ? pSD->screen : 0);
	if (height) *height = DisplayHeight (DISPLAY, pSD ? pSD->screen : 0);
	return;
    }

    if (monitor < 0 || monitor >= pSD->numMonitors) monitor = 0;
    m = &pSD->pMonitors[monitor];

    wx = m->x + (int) m->strutLeft;
    wy = m->y + (int) m->strutTop;
    ww = m->width - (int) (m->strutLeft + m->strutRight);
    wh = m->height - (int) (m->strutTop + m->strutBottom);

    /*
     * Struts that leave nothing usable are ignored rather than honoured, the
     * same rule RecomputeStruts() applies screen wide. A misbehaving dock
     * should cost the user some space, not all of it.
     */
    if (ww <= 0 || wh <= 0)
    {
	wx = m->x;
	wy = m->y;
	ww = m->width;
	wh = m->height;
    }

    if (x) *x = wx;
    if (y) *y = wy;
    if (width) *width = ww;
    if (height) *height = wh;
}

void MapPointToMonitor(WmScreenData *pSD, int fromMon, int toMon,
	int *x, int *y, int width, int height)
{
    WmMonitorData *from, *to;
    int nx, ny;

    if (!pSD || fromMon < 0 || toMon < 0 ||
	fromMon >= pSD->numMonitors || toMon >= pSD->numMonitors) return;

    from = &pSD->pMonitors[fromMon];
    to = &pSD->pMonitors[toMon];

    /*
     * Proportional rather than a plain offset: heads are rarely the same size,
     * and a window three quarters of the way across a wide monitor should stay
     * three quarters of the way across a narrow one instead of landing off it.
     */
    nx = to->x + (int) (((long) (*x - from->x) * to->width) /
		(from->width ? from->width : 1));
    ny = to->y + (int) (((long) (*y - from->y) * to->height) /
		(from->height ? from->height : 1));

    if (nx + width > to->x + to->width) nx = to->x + to->width - width;
    if (ny + height > to->y + to->height) ny = to->y + to->height - height;
    if (nx < to->x) nx = to->x;
    if (ny < to->y) ny = to->y;

    *x = nx;
    *y = ny;
}

int MonitorFromSpec(WmScreenData *pSD, const char *spec, int fromMon)
{
    int i;

    if (!pSD || pSD->numMonitors < 1) return (-1);

    if (!spec || !*spec) return (-1);

    if (!strcmp (spec, "next"))
	return ((fromMon + 1) % pSD->numMonitors);

    if (!strcmp (spec, "prev"))
	return ((fromMon + pSD->numMonitors - 1) % pSD->numMonitors);

    if (!strcmp (spec, "primary"))
	return (PrimaryMonitor (pSD));

    i = MonitorByName (pSD, spec);
    if (i < 0)
    {
	char msg[128];

	snprintf (msg, sizeof (msg), "No monitor named \"%s\"", spec);
	Warning (msg);
    }

    return (i);
}

/*
 * ---------------------------------------------------------------------------
 *
 * Changing the configuration: mWmonitor's Apply, and ~/.mmonitors.
 *
 * Everything above reads the layout. Everything below writes it, and is used
 * by the arranger in WmMonitorDlg.c and by the hotplug path in WmCEvent.c. It
 * is kept out of the dialog so that restoring a saved layout does not need the
 * dialog to have been built -- which at startup and on a dock it has not.
 */

/* Whole Hz from a mode's timings, the way xrandr(1) computes it. */
static int ModeRefresh(XRRModeInfo *mi)
{
    double total;

    if (!mi || !mi->hTotal || !mi->vTotal) return (0);

    total = (double) mi->hTotal * (double) mi->vTotal;

    /*
     * Doublescan and interlace change how many of those total pixels make one
     * frame. Ignoring them reports half or twice the real rate on the modes
     * where it matters, which is exactly the modes a user picks by refresh.
     */
    if (mi->modeFlags & RR_DoubleScan) total *= 2;
    if (mi->modeFlags & RR_Interlace) total /= 2;

    return ((int) ((mi->dotClock / total) + 0.5));
}

static XRRModeInfo *FindModeInfo(XRRScreenResources *res, RRMode id)
{
    int i;

    for (i = 0; i < res->nmode; i++)
	if (res->modes[i].id == id) return (&res->modes[i]);

    return (NULL);
}

/*
 * The mode id on this output that best matches a width, height and refresh.
 *
 * Refresh is matched by nearest rather than exactly: a saved layout carries a
 * rounded whole number, and the mode it names may come back from the server
 * one Hz off after a driver update. Refusing it then would silently leave the
 * output disabled.
 */
static RRMode FindMode(XRRScreenResources *res, XRROutputInfo *oi,
	int width, int height, int refresh)
{
    RRMode best = None;
    int bestDelta = 0;
    int i;

    for (i = 0; i < oi->nmode; i++)
    {
	XRRModeInfo *mi = FindModeInfo (res, oi->modes[i]);
	int delta;

	if (!mi) continue;
	if ((int) mi->width != width || (int) mi->height != height) continue;

	if (refresh <= 0) return (mi->id);

	delta = ModeRefresh (mi) - refresh;
	if (delta < 0) delta = -delta;

	if (best == None || delta < bestDelta)
	{
	    best = mi->id;
	    bestDelta = delta;
	}
    }

    return (best);
}

WmMonitorMode *GetOutputModes(WmScreenData *pSD, const char *name, int *pNum)
{
    XRRScreenResources *res;
    WmMonitorMode *modes = NULL;
    int num = 0;
    int i;

    *pNum = 0;
    if (!wmGD.xrandr_present || !name) return (NULL);

    res = XRRGetScreenResourcesCurrent (DISPLAY, pSD->rootWindow);
    if (!res) return (NULL);

    for (i = 0; i < res->noutput; i++)
    {
	XRROutputInfo *oi = XRRGetOutputInfo (DISPLAY, res, res->outputs[i]);
	int j;

	if (!oi) continue;
	if (!oi->name || strcmp (oi->name, name))
	{
	    XRRFreeOutputInfo (oi);
	    continue;
	}

	modes = (WmMonitorMode *) XtCalloc (oi->nmode ? oi->nmode : 1,
		sizeof (WmMonitorMode));

	for (j = 0; j < oi->nmode; j++)
	{
	    XRRModeInfo *mi = FindModeInfo (res, oi->modes[j]);
	    int k;
	    Boolean dup = False;

	    if (!mi) continue;

	    /*
	     * An output commonly lists the same width/height/refresh more than
	     * once -- once from the EDID and once from a driver's own timing
	     * table. Two identical lines in a menu are a bug to the person
	     * reading it, whatever the server meant by them.
	     */
	    for (k = 0; k < num; k++)
	    {
		if (modes[k].width == (int) mi->width &&
		    modes[k].height == (int) mi->height &&
		    modes[k].refresh == ModeRefresh (mi)) dup = True;
	    }
	    if (dup) continue;

	    modes[num].width = mi->width;
	    modes[num].height = mi->height;
	    modes[num].refresh = ModeRefresh (mi);
	    modes[num].preferred = (j < oi->npreferred) ? True : False;
	    num++;
	}

	XRRFreeOutputInfo (oi);
	break;
    }

    XRRFreeScreenResources (res);

    *pNum = num;
    return (modes);
}

WmMonitorConfig *GetMonitorConfig(WmScreenData *pSD, int *pNum)
{
    XRRScreenResources *res;
    WmMonitorConfig *cfg;
    RROutput primary;
    int num = 0;
    int i;

    *pNum = 0;
    if (!wmGD.xrandr_present) return (NULL);

    res = XRRGetScreenResourcesCurrent (DISPLAY, pSD->rootWindow);
    if (!res) return (NULL);

    primary = XRRGetOutputPrimary (DISPLAY, pSD->rootWindow);
    cfg = (WmMonitorConfig *) XtCalloc (res->noutput ? res->noutput : 1,
	    sizeof (WmMonitorConfig));

    for (i = 0; i < res->noutput; i++)
    {
	XRROutputInfo *oi = XRRGetOutputInfo (DISPLAY, res, res->outputs[i]);

	if (!oi) continue;

	/*
	 * Disconnected outputs are left out entirely. A socket with nothing in
	 * it is not something to arrange, and listing it would put an empty
	 * box on the canvas for every unused port on the graphics card.
	 */
	if (oi->connection != RR_Connected)
	{
	    XRRFreeOutputInfo (oi);
	    continue;
	}

	cfg[num].name = XtNewString (oi->name ? oi->name : "output");
	cfg[num].primary = (res->outputs[i] == primary) ? True : False;

	if (oi->crtc != None)
	{
	    XRRCrtcInfo *ci = XRRGetCrtcInfo (DISPLAY, res, oi->crtc);

	    if (ci)
	    {
		XRRModeInfo *mi = FindModeInfo (res, ci->mode);

		cfg[num].enabled = True;
		cfg[num].x = ci->x;
		cfg[num].y = ci->y;
		cfg[num].width = ci->width;
		cfg[num].height = ci->height;
		cfg[num].refresh = mi ? ModeRefresh (mi) : 0;

		XRRFreeCrtcInfo (ci);
	    }
	}

	/*
	 * A connected but disabled output still needs a size to show in the
	 * arranger, or its box has nothing to be drawn at. Its preferred mode
	 * is what enabling it would pick anyway.
	 */
	if (!cfg[num].enabled && oi->npreferred > 0 && oi->nmode > 0)
	{
	    XRRModeInfo *mi = FindModeInfo (res, oi->modes[0]);

	    if (mi)
	    {
		cfg[num].width = mi->width;
		cfg[num].height = mi->height;
		cfg[num].refresh = ModeRefresh (mi);
	    }
	}

	num++;
	XRRFreeOutputInfo (oi);
    }

    XRRFreeScreenResources (res);

    *pNum = num;
    return (cfg);
}

void FreeMonitorConfig(WmMonitorConfig *cfg, int num)
{
    int i;

    if (!cfg) return;

    for (i = 0; i < num; i++)
	if (cfg[i].name) XtFree (cfg[i].name);

    XtFree ((char *) cfg);
}

Boolean ApplyMonitorLayout(WmScreenData *pSD, WmMonitorConfig *cfg, int num)
{
    XRRScreenResources *res;
    int i, j;
    int maxX = 0, maxY = 0;
    int mmWidth, mmHeight;
    Boolean ok = True;

    if (!wmGD.xrandr_present || !cfg || num < 1) return (False);

    res = XRRGetScreenResourcesCurrent (DISPLAY, pSD->rootWindow);
    if (!res) return (False);

    /* The bounding box the enabled outputs will need. */
    for (i = 0; i < num; i++)
    {
	if (!cfg[i].enabled) continue;
	if (cfg[i].x + cfg[i].width > maxX) maxX = cfg[i].x + cfg[i].width;
	if (cfg[i].y + cfg[i].height > maxY) maxY = cfg[i].y + cfg[i].height;
    }

    if (maxX < 1 || maxY < 1)
    {
	Warning ("Refusing a monitor layout with nothing enabled");
	XRRFreeScreenResources (res);
	return (False);
    }

    /*
     * Every CRTC off first.
     *
     * XRRSetScreenSize() fails outright if any active CRTC would fall outside
     * the new size, and working out which ones those are -- then shrinking
     * them individually in an order that never leaves an overlap -- is the
     * bulk of what makes this operation fiddly. Turning them all off costs a
     * moment of black and makes the rest of this function a straight line.
     */
    for (i = 0; i < res->ncrtc; i++)
    {
	XRRCrtcInfo *ci = XRRGetCrtcInfo (DISPLAY, res, res->crtcs[i]);

	if (!ci) continue;

	if (ci->mode != None)
	{
	    XRRSetCrtcConfig (DISPLAY, res, res->crtcs[i], CurrentTime,
		0, 0, None, RR_Rotate_0, NULL, 0);
	}

	XRRFreeCrtcInfo (ci);
    }

    /*
     * Physical size. Reported to clients as the basis for DPI, so it has to
     * track the pixel size rather than stay at whatever the old layout was --
     * otherwise every toolkit that scales by DPI gets it wrong after a resize.
     * Derived at 96dpi, which is what the X server itself assumes when it has
     * nothing better, and what a mixed-DPI desk has no single right answer for.
     */
    mmWidth = (int) ((maxX * 25.4) / 96.0 + 0.5);
    mmHeight = (int) ((maxY * 25.4) / 96.0 + 0.5);

    XRRSetScreenSize (DISPLAY, pSD->rootWindow, maxX, maxY,
	mmWidth, mmHeight);

    for (i = 0; i < num; i++)
    {
	XRROutputInfo *oi = NULL;
	RROutput output = None;
	RRCrtc crtc = None;
	RRMode mode;

	if (!cfg[i].enabled) continue;

	for (j = 0; j < res->noutput; j++)
	{
	    oi = XRRGetOutputInfo (DISPLAY, res, res->outputs[j]);
	    if (oi && oi->name && !strcmp (oi->name, cfg[i].name))
	    {
		output = res->outputs[j];
		break;
	    }
	    if (oi) XRRFreeOutputInfo (oi);
	    oi = NULL;
	}

	if (!oi)
	{
	    /*
	     * Named an output the server does not have. Not fatal: a saved
	     * layout matched on a set of names can still be worth applying for
	     * the outputs that are present.
	     */
	    continue;
	}

	mode = FindMode (res, oi, cfg[i].width, cfg[i].height, cfg[i].refresh);

	if (mode == None && oi->nmode > 0) mode = oi->modes[0];

	/*
	 * Any CRTC this output can drive that nothing has claimed yet. Its
	 * own comes first when it still has one, so an unchanged output keeps
	 * the CRTC it had and the server has less to reprogram.
	 */
	for (j = 0; j < oi->ncrtc && crtc == None; j++)
	{
	    XRRCrtcInfo *ci = XRRGetCrtcInfo (DISPLAY, res, oi->crtcs[j]);

	    if (!ci) continue;
	    if (ci->noutput == 0) crtc = oi->crtcs[j];
	    XRRFreeCrtcInfo (ci);
	}

	if (mode == None || crtc == None)
	{
	    char msg[128];

	    snprintf (msg, sizeof (msg),
		"No usable mode or CRTC for monitor \"%s\"", cfg[i].name);
	    Warning (msg);
	    ok = False;
	    XRRFreeOutputInfo (oi);
	    continue;
	}

	if (XRRSetCrtcConfig (DISPLAY, res, crtc, CurrentTime,
		cfg[i].x, cfg[i].y, mode, RR_Rotate_0, &output, 1) != Success)
	{
	    char msg[128];

	    snprintf (msg, sizeof (msg),
		"Could not configure monitor \"%s\"", cfg[i].name);
	    Warning (msg);
	    ok = False;
	}

	if (cfg[i].primary)
	    XRRSetOutputPrimary (DISPLAY, pSD->rootWindow, output);

	XRRFreeOutputInfo (oi);
    }

    XRRFreeScreenResources (res);

    /*
     * The server will send RRScreenChangeNotify for all of this and the
     * handler in WmCEvent.c will rebuild the monitor list from it. Nothing is
     * rebuilt here, so that the layout arrives through exactly the same path
     * as one set from the xrandr command line.
     */
    XSync (DISPLAY, False);

    return (ok);
}

/*
 * ---------------------------------------------------------------------------
 *
 * ~/.mmonitors -- the saved layouts.
 *
 * RandR keeps nothing across an X restart, so an arranger that only changed
 * the live configuration would have to be used again after every login. The
 * file is what makes mWmonitor worth having rather than a nicer way to type
 * one xrandr command.
 *
 * Entries are keyed on the sorted, comma-separated set of connected output
 * names, so a laptop docked and the same laptop on its own are two entries and
 * neither is applied in the other's situation. The format is the brace-block
 * shape the rc and style files already use, for the same reason they use it:
 * it is the one this program's users already know.
 *
 *     eDP-1,HDMI-1 {
 *         eDP-1   2560x1600  0x0        60  primary
 *         HDMI-1  1920x1080  2560x0     60
 *         DP-2    off
 *     }
 */

#define MONITOR_LAYOUT_FILE ".mmonitors"

static char *MonitorLayoutPath(void)
{
    static char path[MAXWMPATH + 1];
    char *home;

    if (wmGD.monitorLayoutFile && *wmGD.monitorLayoutFile)
    {
	if (wmGD.monitorLayoutFile[0] != '~')
	{
	    snprintf (path, sizeof (path), "%s", wmGD.monitorLayoutFile);
	    return (path);
	}
	/* "~/..." expanded the same way configFile is */
	home = getenv ("HOME");
	if (!home) return (NULL);
	snprintf (path, sizeof (path), "%s%s", home,
	    wmGD.monitorLayoutFile + 1);
	return (path);
    }

    home = getenv ("HOME");
    if (!home) return (NULL);

    snprintf (path, sizeof (path), "%s/%s", home, MONITOR_LAYOUT_FILE);
    return (path);
}

/*
 * The set of connected outputs, sorted and comma separated.
 *
 * Sorted so that the key does not depend on the order the server happens to
 * report outputs in, which is not guaranteed to be stable and is certainly not
 * guaranteed to match between two boots.
 */
static char *ConnectedOutputKey(WmScreenData *pSD)
{
    XRRScreenResources *res;
    char **names;
    char *key;
    int num = 0;
    int len = 1;
    int i, j;

    if (!wmGD.xrandr_present) return (NULL);

    res = XRRGetScreenResourcesCurrent (DISPLAY, pSD->rootWindow);
    if (!res) return (NULL);

    names = (char **) XtCalloc (res->noutput ? res->noutput : 1,
	    sizeof (char *));

    for (i = 0; i < res->noutput; i++)
    {
	XRROutputInfo *oi = XRRGetOutputInfo (DISPLAY, res, res->outputs[i]);

	if (!oi) continue;

	if (oi->connection == RR_Connected && oi->name)
	{
	    names[num++] = XtNewString (oi->name);
	    len += strlen (oi->name) + 1;
	}

	XRRFreeOutputInfo (oi);
    }

    XRRFreeScreenResources (res);

    for (i = 1; i < num; i++)
    {
	char *tmp = names[i];

	for (j = i; j > 0 && strcmp (names[j - 1], tmp) > 0; j--)
	    names[j] = names[j - 1];
	names[j] = tmp;
    }

    key = XtMalloc (len);
    key[0] = '\0';

    for (i = 0; i < num; i++)
    {
	if (i) strcat (key, ",");
	strcat (key, names[i]);
	XtFree (names[i]);
    }

    XtFree ((char *) names);

    return (key);
}

Boolean NoteConnectedOutputs(WmScreenData *pSD)
{
    char *key = ConnectedOutputKey (pSD);
    Boolean changed;

    if (!key) return (False);

    changed = (!pSD->connectedOutputs ||
	strcmp (pSD->connectedOutputs, key)) ? True : False;

    if (pSD->connectedOutputs) XtFree (pSD->connectedOutputs);
    pSD->connectedOutputs = key;

    return (changed);
}

Boolean SaveMonitorLayout(WmScreenData *pSD, WmMonitorConfig *cfg, int num)
{
    char *path = MonitorLayoutPath ();
    char *key = ConnectedOutputKey (pSD);
    char tmpPath[MAXWMPATH + 1];
    FILE *in = NULL;
    FILE *out;
    char line[512];
    Boolean skipping = False;
    int depth = 0;
    int i;

    if (!path || !key)
    {
	if (key) XtFree (key);
	return (False);
    }

    /*
     * Rewritten through a temporary file rather than edited in place: a write
     * interrupted halfway would otherwise leave the user with a file that
     * parses into a layout nobody chose, and this one is applied at login.
     */
    snprintf (tmpPath, sizeof (tmpPath), "%s.new", path);

    if (!(out = fopen (tmpPath, "w")))
    {
	Warning ("Could not write the monitor layout file");
	XtFree (key);
	return (False);
    }

    /*
     * Copy every entry but the one being replaced. Anything unrecognised is
     * copied through untouched, so a hand-written comment survives a save.
     */
    if ((in = fopen (path, "r")))
    {
	while (fgets (line, sizeof (line), in))
	{
	    if (!depth && !skipping)
	    {
		char name[256];

		if (sscanf (line, " %255[^ \t{] ", name) == 1 &&
		    strchr (line, '{') && !strcmp (name, key))
		{
		    skipping = True;
		    depth = 1;
		    continue;
		}
	    }

	    if (skipping)
	    {
		if (strchr (line, '{')) depth++;
		if (strchr (line, '}')) depth--;
		if (depth <= 0) { skipping = False; depth = 0; }
		continue;
	    }

	    fputs (line, out);
	}
	fclose (in);
    }

    fprintf (out, "%s {\n", key);

    for (i = 0; i < num; i++)
    {
	if (!cfg[i].enabled)
	{
	    fprintf (out, "\t%s\toff\n", cfg[i].name);
	    continue;
	}

	fprintf (out, "\t%s\t%dx%d\t%dx%d\t%d%s\n",
	    cfg[i].name, cfg[i].width, cfg[i].height,
	    cfg[i].x, cfg[i].y, cfg[i].refresh,
	    cfg[i].primary ? "\tprimary" : "");
    }

    fprintf (out, "}\n");
    fclose (out);

    if (rename (tmpPath, path))
    {
	Warning ("Could not replace the monitor layout file");
	XtFree (key);
	return (False);
    }

    XtFree (key);
    return (True);
}

Boolean ApplySavedMonitorLayout(WmScreenData *pSD)
{
    char *path = MonitorLayoutPath ();
    char *key = ConnectedOutputKey (pSD);
    FILE *f;
    char line[512];
    WmMonitorConfig *cfg = NULL;
    int num = 0;
    int size = 0;
    Boolean inBlock = False;
    Boolean applied = False;

    if (!path || !key)
    {
	if (key) XtFree (key);
	return (False);
    }

    if (!(f = fopen (path, "r")))
    {
	XtFree (key);
	return (False);
    }

    while (fgets (line, sizeof (line), f))
    {
	char name[256], mode[64], pos[64], flag[32];
	int refresh = 0;
	int n;

	if (!inBlock)
	{
	    if (sscanf (line, " %255[^ \t{] ", name) == 1 &&
		strchr (line, '{') && !strcmp (name, key))
	    {
		inBlock = True;
	    }
	    continue;
	}

	if (strchr (line, '}')) break;

	flag[0] = '\0';
	n = sscanf (line, " %255s %63s %63s %d %31s",
		name, mode, pos, &refresh, flag);
	if (n < 2) continue;

	if (num == size)
	{
	    size = size ? size * 2 : 4;
	    cfg = (WmMonitorConfig *) XtRealloc ((char *) cfg,
		    size * sizeof (WmMonitorConfig));
	}

	memset (&cfg[num], 0, sizeof (WmMonitorConfig));
	cfg[num].name = XtNewString (name);

	if (!strcmp (mode, "off")) { num++; continue; }

	if (n < 3) continue;

	if (sscanf (mode, "%dx%d", &cfg[num].width, &cfg[num].height) != 2)
	    continue;
	if (sscanf (pos, "%dx%d", &cfg[num].x, &cfg[num].y) != 2)
	    continue;

	cfg[num].enabled = True;
	cfg[num].refresh = (n >= 4) ? refresh : 0;
	cfg[num].primary = (n >= 5 && !strcmp (flag, "primary")) ? True : False;
	num++;
    }

    fclose (f);
    XtFree (key);

    if (num > 0) applied = ApplyMonitorLayout (pSD, cfg, num);

    FreeMonitorConfig (cfg, num);

    return (applied);
}
