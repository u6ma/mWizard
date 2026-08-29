/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

/*
 * mWmonitor: the monitor arranger.
 *
 * Where the monitors are, how big they are, which one is primary and which are
 * on at all. Everything the xrandr command line does that a person actually
 * does by hand, done by dragging boxes instead of counting pixel offsets.
 *
 * Reachable four ways: the f.monitors rc function, mWand's Commands menu over
 * _MWIZARD_COMMAND, a hotplug when monitorDialogOnHotplug is set, and
 * therefore any binding the user cares to add.
 *
 * ---------------------------------------------------------------------------
 *
 * Built the way every other window mWizard puts on the screen is built, for
 * reasons that are written out in full at the top of WmExecDlg.c and not
 * repeated here: a plain Xt TransientShell on the second display connection's
 * per-screen shell, with XtNwaitForWm off, explicit depth/screen/colormap, and
 * positioned before it is realized.
 *
 * Like mWinfo and unlike the Execute prompt it is meant to behave as an
 * ordinary window -- movable, resizable, closable from its frame -- so it does
 * not withhold MWM_FUNC_CLOSE, and that makes WM_DELETE_WINDOW mandatory
 * rather than optional. F_Kill() falls through to XKillClient() on a client
 * offering no such protocol, and this client owns mWizard's second display
 * connection, so being killed that way would end the X session.
 *
 * ---------------------------------------------------------------------------
 *
 * Nothing RandR-sized stays resident. The dialog holds a WmMonitorConfig array
 * -- a handful of ints and one name per output -- and reads XRRScreenResources
 * only inside the calls that need it, which free it before they return. The
 * mode list for the selected output is fetched when the selection changes and
 * freed when it changes again. The window itself is built on first post and
 * then kept, the same bargain the other two dialogs make: a few kilobytes of
 * widgets against rebuilding them every time.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/DrawingA.h>
#include <Xm/List.h>
#include <Xm/ScrolledW.h>
#include <Xm/RowColumn.h>
#include <Xm/SeparatoG.h>

#include "WmGlobal.h"
#include "WmMonitorDlg.h"
#include "WmMonitor.h"
#include "WmEwmh.h"
#include "WmFunction.h"
#include "WmStyle.h"
#include "WmError.h"

static Boolean MakeMonitorDialog(WmScreenData *pSD);
static void MakeCanvasGCs(void);
static void PlaceMonitorDialog(WmScreenData *pSD);
static void UnpostMonitorDialog(void);
static void LoadConfig(void);
static void FreeConfig(void);
static void RebuildModeList(void);
static void SyncControls(void);
static void CanvasScale(int *ox, int *oy, double *scale);
static int  CanvasHit(int cx, int cy);
static void SnapSelected(void);
static void ResolveOverlaps(void);
static Boolean MonitorsOverlap(WmMonitorConfig *a, WmMonitorConfig *b);

static void CanvasExposeCB(Widget, XtPointer, XtPointer);
static void CanvasInputCB(Widget, XtPointer, XtPointer);
static void CanvasMotionEH(Widget, XtPointer, XEvent*, Boolean*);
static void HandleCanvasEvent(XEvent *ev);
static void ModeCB(Widget, XtPointer, XtPointer);
static void PostMonitorNow(void);
static void PostMonitorTimeout(XtPointer, XtIntervalId*);
static void PrimaryCB(Widget, XtPointer, XtPointer);
static void EnabledCB(Widget, XtPointer, XtPointer);
static void RotateCB(Widget, XtPointer, XtPointer);
static void ApplyCB(Widget, XtPointer, XtPointer);
static void SaveCB(Widget, XtPointer, XtPointer);
static void RevertCB(Widget, XtPointer, XtPointer);
static void CloseCB(Widget, XtPointer, XtPointer);
static void MonitorProtocolHandler(Widget, XtPointer, XEvent*, Boolean*);

static Widget monShellW = NULL;
static Widget monCanvasW = NULL;
static Widget monNameW = NULL;
static Widget monModeListW = NULL;
static Widget monPrimaryW = NULL;
static Widget monEnabledW = NULL;
static Widget monRotateW = NULL;
static WmScreenData *monPSD = NULL;
static Boolean monOnScreen = False;

/* The layout being edited. Live only while the dialog exists. */
static WmMonitorConfig *monCfg = NULL;
static int monNumCfg = 0;
static int monSelected = 0;

/* Modes of the selected output, refreshed when the selection changes. */
static WmMonitorMode *monModes = NULL;
static int monNumModes = 0;

/*
 * Posting is deferred by one turn of the event loop; see PostMonitorDialog().
 */
static XtIntervalId monPostTimer = (XtIntervalId) 0;

/*
 * The canvas GCs.
 *
 * Made here rather than borrowed from pSD->feedbackAppearance, which is the
 * obvious shortcut and the wrong one: those GCs belong to mWizard's *first*
 * display connection, and this window lives on the second. Passing an XID
 * across connections happens to be legal X, but it ties the drawing to
 * resources another connection owns and the depth and screen have to keep
 * agreeing for it to stay legal. Three GCs on the connection that owns the
 * window is a few lines and no such coupling.
 *
 * The colors come from the widget, so the canvas follows ~/.mstylesrc the same
 * way the rest of the window does.
 */
static GC monFillGC = NULL;	/* lit face of an enabled monitor */
static GC monLineGC = NULL;	/* outlines */
static GC monTextGC = NULL;	/* names, and the selection marker */

/* Drag state on the canvas. */
static Boolean monDragging = False;
static int monDragX = 0, monDragY = 0;

#define CANVAS_WIDTH	420
#define CANVAS_HEIGHT	240
#define CANVAS_MARGIN	8

/*
 * How close two edges have to be, in real pixels, before a drag snaps them
 * together. Scaled by the canvas, so this is a distance on the desk rather
 * than on the screen -- which is what makes the snap feel the same whether the
 * arrangement is two 1080p panels or one 5K and a laptop.
 */
#define SNAP_DISTANCE	80

static void LoadConfig(void)
{
    FreeConfig ();

    monCfg = GetMonitorConfig (monPSD, &monNumCfg);
    if (monSelected >= monNumCfg) monSelected = 0;
}

static void FreeConfig(void)
{
    if (monCfg) FreeMonitorConfig (monCfg, monNumCfg);
    monCfg = NULL;
    monNumCfg = 0;

    if (monModes) XtFree ((char *) monModes);
    monModes = NULL;
    monNumModes = 0;

}

/*
 * The scale and offset that fit the whole arrangement into the canvas.
 *
 * Recomputed on every draw rather than stored, because a drag changes the
 * bounding box continuously and a stored scale would make the boxes appear to
 * shrink as one is pulled away from the others.
 */
static void CanvasScale(int *ox, int *oy, double *scale)
{
    int minX = 0, minY = 0, maxX = 1, maxY = 1;
    double sx, sy;
    Dimension cw = CANVAS_WIDTH, ch = CANVAS_HEIGHT;
    int i;
    Arg args[2];

    if (monCanvasW)
    {
	XtSetArg (args[0], XmNwidth, &cw);
	XtSetArg (args[1], XmNheight, &ch);
	XtGetValues (monCanvasW, args, 2);
    }

    for (i = 0; i < monNumCfg; i++)
    {
	if (monCfg[i].x < minX) minX = monCfg[i].x;
	if (monCfg[i].y < minY) minY = monCfg[i].y;
	if (monCfg[i].x + MonitorConfigWidth (&monCfg[i]) > maxX)
	    maxX = monCfg[i].x + MonitorConfigWidth (&monCfg[i]);
	if (monCfg[i].y + MonitorConfigHeight (&monCfg[i]) > maxY)
	    maxY = monCfg[i].y + MonitorConfigHeight (&monCfg[i]);
    }

    sx = (double) (cw - 2 * CANVAS_MARGIN) / (double) (maxX - minX);
    sy = (double) (ch - 2 * CANVAS_MARGIN) / (double) (maxY - minY);

    *scale = (sx < sy) ? sx : sy;
    if (*scale <= 0.0) *scale = 0.05;

    /* Centred, so the arrangement does not sit in a corner of the canvas. */
    *ox = CANVAS_MARGIN +
	(int) (((cw - 2 * CANVAS_MARGIN) - (maxX - minX) * *scale) / 2.0)
	- (int) (minX * *scale);
    *oy = CANVAS_MARGIN +
	(int) (((ch - 2 * CANVAS_MARGIN) - (maxY - minY) * *scale) / 2.0)
	- (int) (minY * *scale);
}

static void CanvasExposeCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    Window win = XtWindow (w);
    XmRenderTable rt = NULL;
    Arg args[1];
    double scale;
    int ox, oy;
    int i;

    if (!win || !monPSD || !monNumCfg) return;

    MakeCanvasGCs ();
    if (!monFillGC) return;

    /*
     * The render table off the canvas itself, so the names follow the
     * dialogFont role in the style file along with the rest of the window.
     */
    XtSetArg (args[0], XmNrenderTable, &rt);
    XtGetValues (monCanvasW, args, 1);

    CanvasScale (&ox, &oy, &scale);

    XClearWindow (DISPLAY1, win);

    for (i = 0; i < monNumCfg; i++)
    {
	int bx = ox + (int) (monCfg[i].x * scale);
	int by = oy + (int) (monCfg[i].y * scale);
	int bw = (int) (MonitorConfigWidth (&monCfg[i]) * scale);
	int bh = (int) (MonitorConfigHeight (&monCfg[i]) * scale);
	XmString label;
	char text[128];

	if (bw < 4) bw = 4;
	if (bh < 4) bh = 4;

	/*
	 * A disabled output is outlined but not filled, so that it is
	 * obviously there to be turned on rather than simply missing.
	 */
	if (monCfg[i].enabled)
	{
	    XFillRectangle (DISPLAY1, win, monFillGC, bx, by, bw, bh);
	}

	XDrawRectangle (DISPLAY1, win, monLineGC, bx, by, bw - 1, bh - 1);

	/* The selection gets a second, inset outline rather than a colour. */
	if (i == monSelected)
	{
	    XDrawRectangle (DISPLAY1, win, monTextGC,
		bx + 2, by + 2, bw - 5, bh - 5);
	    XDrawRectangle (DISPLAY1, win, monTextGC,
		bx + 3, by + 3, bw - 7, bh - 7);
	}

	snprintf (text, sizeof (text), "%s%s", monCfg[i].name,
	    monCfg[i].primary ? " *" : "");

	label = XmStringCreateLocalized (text);
	if (rt)
	{
	    XmStringDraw (DISPLAY1, win, rt, label, monTextGC,
		bx + 4, by + 4, bw - 8,
		XmALIGNMENT_CENTER, XmSTRING_DIRECTION_L_TO_R, NULL);
	}
	XmStringFree (label);
    }
}

static int CanvasHit(int cx, int cy)
{
    double scale;
    int ox, oy;
    int i;

    CanvasScale (&ox, &oy, &scale);

    /*
     * Backwards, so that the box drawn last -- the one on top where two
     * overlap mid-drag -- is the one the click lands on.
     */
    for (i = monNumCfg - 1; i >= 0; i--)
    {
	int bx = ox + (int) (monCfg[i].x * scale);
	int by = oy + (int) (monCfg[i].y * scale);
	int bw = (int) (MonitorConfigWidth (&monCfg[i]) * scale);
	int bh = (int) (MonitorConfigHeight (&monCfg[i]) * scale);

	if (cx >= bx && cx < bx + bw && cy >= by && cy < by + bh) return (i);
    }

    return (-1);
}

/*
 * Pulls the dragged monitor's edges onto its neighbours' when they are close,
 * then shifts the whole arrangement back to the origin.
 *
 * The second half matters as much as the first. RandR coordinates start at
 * 0,0, and a layout whose leftmost monitor sits at x=-1920 describes a screen
 * with 1920 unusable columns down its left side. Normalising here means the
 * user can drag a monitor to the left of the primary without having to think
 * about it.
 */
/* Two monitor rectangles sharing any area at all. */
static Boolean MonitorsOverlap(WmMonitorConfig *a, WmMonitorConfig *b)
{
    return ((a->x < b->x + MonitorConfigWidth (b)) &&
	    (b->x < a->x + MonitorConfigWidth (a)) &&
	    (a->y < b->y + MonitorConfigHeight (b)) &&
	    (b->y < a->y + MonitorConfigHeight (a)) ? True : False);
}

/*
 * Pushes the dragged monitor clear of anything it has landed on.
 *
 * RandR will happily accept overlapping CRTCs -- that is what mirroring looks
 * like at the protocol level -- but two boxes sitting on top of each other in
 * an arranger describe a desk nobody has, and the layout they save puts part
 * of one screen inside another. The drag is a picture of where the monitors
 * physically are, so the boxes are solid.
 *
 * Pushed along whichever axis needs the least movement, which is what makes it
 * feel like sliding a monitor against its neighbour rather than teleporting.
 * Repeated because clearing one neighbour can push it onto the next; bounded
 * by the number of monitors, so it always terminates.
 */
static void ResolveOverlaps(void)
{
    WmMonitorConfig *m = &monCfg[monSelected];
    int pass, i;

    if (!m->enabled) return;

    for (pass = 0; pass <= monNumCfg; pass++)
    {
	Boolean moved = False;

	for (i = 0; i < monNumCfg; i++)
	{
	    WmMonitorConfig *o = &monCfg[i];
	    int left, right, up, down, best;

	    if (i == monSelected || !o->enabled) continue;
	    if (!MonitorsOverlap (m, o)) continue;

	    left  = (m->x + MonitorConfigWidth (m)) - o->x;
	    right = (o->x + MonitorConfigWidth (o)) - m->x;
	    up    = (m->y + MonitorConfigHeight (m)) - o->y;
	    down  = (o->y + MonitorConfigHeight (o)) - m->y;

	    best = left;
	    if (right < best) best = right;
	    if (up < best) best = up;
	    if (down < best) best = down;

	    if (best == left)       m->x -= left;
	    else if (best == right) m->x += right;
	    else if (best == up)    m->y -= up;
	    else                    m->y += down;

	    moved = True;
	}

	if (!moved) break;
    }
}

static void SnapSelected(void)
{
    WmMonitorConfig *m = &monCfg[monSelected];
    int minX = m->x, minY = m->y;
    int i;

    for (i = 0; i < monNumCfg; i++)
    {
	WmMonitorConfig *o = &monCfg[i];
	int d;

	if (i == monSelected || !o->enabled) continue;

	/* Horizontal: right-to-left edge, left-to-right edge, and aligned. */
	d = (o->x + MonitorConfigWidth (o)) - m->x;
	if (d > -SNAP_DISTANCE && d < SNAP_DISTANCE)
	    m->x = o->x + MonitorConfigWidth (o);

	d = o->x - (m->x + MonitorConfigWidth (m));
	if (d > -SNAP_DISTANCE && d < SNAP_DISTANCE)
	    m->x = o->x - MonitorConfigWidth (m);

	d = o->x - m->x;
	if (d > -SNAP_DISTANCE && d < SNAP_DISTANCE) m->x = o->x;

	/* Vertical: the same three. */
	d = (o->y + MonitorConfigHeight (o)) - m->y;
	if (d > -SNAP_DISTANCE && d < SNAP_DISTANCE)
	    m->y = o->y + MonitorConfigHeight (o);

	d = o->y - (m->y + MonitorConfigHeight (m));
	if (d > -SNAP_DISTANCE && d < SNAP_DISTANCE)
	    m->y = o->y - MonitorConfigHeight (m);

	d = o->y - m->y;
	if (d > -SNAP_DISTANCE && d < SNAP_DISTANCE) m->y = o->y;
    }

    /*
     * Snapping can seat an edge flush against a neighbour and still leave the
     * two boxes overlapping, so this runs after it and before the layout is
     * normalised back to the origin.
     */
    ResolveOverlaps ();

    minX = m->x;
    minY = m->y;

    for (i = 0; i < monNumCfg; i++)
    {
	if (!monCfg[i].enabled) continue;
	if (monCfg[i].x < minX) minX = monCfg[i].x;
	if (monCfg[i].y < minY) minY = monCfg[i].y;
    }

    if (minX || minY)
    {
	for (i = 0; i < monNumCfg; i++)
	{
	    monCfg[i].x -= minX;
	    monCfg[i].y -= minY;
	}
    }
}

static void CanvasInputCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    XmDrawingAreaCallbackStruct *cbs =
	(XmDrawingAreaCallbackStruct *) call_data;

    if (cbs) HandleCanvasEvent (cbs->event);
}

/*
 * XmNinputCallback delivers button presses and releases but not motion, which
 * is the half a drag is actually made of -- so motion comes in through a plain
 * Xt event handler and both are funnelled into the same body.
 */
static void CanvasMotionEH(Widget w, XtPointer client_data, XEvent *ev,
	Boolean *cont)
{
    HandleCanvasEvent (ev);
}

static void HandleCanvasEvent(XEvent *ev)
{
    double scale;
    int ox, oy;
    int hit;

    if (!ev || !monNumCfg) return;

    CanvasScale (&ox, &oy, &scale);

    switch (ev->type)
    {
	case ButtonPress:
	    hit = CanvasHit (ev->xbutton.x, ev->xbutton.y);
	    if (hit < 0) return;

	    if (hit != monSelected)
	    {
		monSelected = hit;
		RebuildModeList ();
		SyncControls ();
	    }

	    monDragging = True;
	    monDragX = ev->xbutton.x;
	    monDragY = ev->xbutton.y;
	    CanvasExposeCB (monCanvasW, NULL, NULL);
	    break;

	case MotionNotify:
	    if (!monDragging) return;

	    /*
	     * Converted back through the same scale the drawing uses, so the
	     * box tracks the pointer exactly however far the arrangement is
	     * zoomed out.
	     */
	    monCfg[monSelected].x +=
		(int) ((ev->xmotion.x - monDragX) / scale);
	    monCfg[monSelected].y +=
		(int) ((ev->xmotion.y - monDragY) / scale);

	    monDragX = ev->xmotion.x;
	    monDragY = ev->xmotion.y;

	    CanvasExposeCB (monCanvasW, NULL, NULL);
	    break;

	case ButtonRelease:
	    if (!monDragging) return;
	    monDragging = False;
	    SnapSelected ();
	    CanvasExposeCB (monCanvasW, NULL, NULL);
	    break;

	default:
	    break;
    }
}

/*
 * Rebuilds the mode menu for the selected output.
 *
 * The pane's buttons are destroyed and remade rather than relabelled: outputs
 * do not offer the same number of modes, and a menu that kept the longest
 * output's worth of buttons would show modes the current one cannot drive.
 */
/*
 * Refills the mode list for the selected output.
 *
 * An XmList and not an option menu, and that is the whole point rather than a
 * matter of taste.
 *
 * mWmonitor is posted from ActivateCallback() in WmMenu.c, which invokes an rc
 * function *while the root menu is still posted and Motif still owns the
 * pointer and keyboard grab that menu took* -- wmGD.menuActive is not cleared
 * until the menu unmaps, later. Building a second Motif menu system from
 * inside that reenters Motif's per-display menu state machine while it is
 * mid-activation, and when that goes wrong the grab is never released: no
 * pointer, no keyboard, no way to reach a terminal, nothing left but the power
 * switch. f.run and f.about have always been safe from the same callback
 * because neither creates a menu.
 *
 * A list has no menu shell, takes no grab, and touches none of that state. It
 * also suits a list of thirty modes better than a pulldown does.
 */
static void RebuildModeList(void)
{
    XmStringTable items;
    int i;

    if (!monModeListW || !monNumCfg) return;

    if (monModes) XtFree ((char *) monModes);
    monModes = GetOutputModes (monPSD, monCfg[monSelected].name, &monNumModes);

    XmListDeleteAllItems (monModeListW);

    if (monNumModes < 1) return;

    items = (XmStringTable) XtCalloc (monNumModes, sizeof (XmString));

    for (i = 0; i < monNumModes; i++)
    {
	char label[64];

	snprintf (label, sizeof (label), "%dx%d  %d Hz%s",
	    monModes[i].width, monModes[i].height, monModes[i].refresh,
	    monModes[i].preferred ? "  *" : "");

	items[i] = XmStringCreateLocalized (label);
    }

    XmListAddItems (monModeListW, items, monNumModes, 1);

    for (i = 0; i < monNumModes; i++) XmStringFree (items[i]);
    XtFree ((char *) items);
}

/*
 * Points the controls at whatever is selected.
 */
static void SyncControls(void)
{
    XmString xms;
    Arg args[4];
    char text[160];
    int i;

    if (!monNumCfg || !monNameW) return;

    snprintf (text, sizeof (text), "%s   %dx%d",
	monCfg[monSelected].name,
	monCfg[monSelected].width, monCfg[monSelected].height);

    xms = XmStringCreateLocalized (text);
    XtSetArg (args[0], XmNlabelString, (XtArgVal) xms);
    XtSetValues (monNameW, args, 1);
    XmStringFree (xms);

    if (monRotateW)
    {
	snprintf (text, sizeof (text), "Rotation: %s",
	    MonitorRotationName (monCfg[monSelected].rotation));

	xms = XmStringCreateLocalized (text);
	XtSetArg (args[0], XmNlabelString, (XtArgVal) xms);
	XtSetValues (monRotateW, args, 1);
	XmStringFree (xms);
    }

    XmToggleButtonSetState (monPrimaryW, monCfg[monSelected].primary, False);
    XmToggleButtonSetState (monEnabledW, monCfg[monSelected].enabled, False);

    /* Point the list at the mode this output is actually running. */
    if (!monModeListW) return;

    XmListDeselectAllItems (monModeListW);

    for (i = 0; i < monNumModes; i++)
    {
	if (monModes[i].width == monCfg[monSelected].width &&
	    monModes[i].height == monCfg[monSelected].height &&
	    monModes[i].refresh == monCfg[monSelected].refresh)
	{
	    /* Positions are one-based, and False: selecting must not run the
	     * callback, or syncing the display would count as a user edit. */
	    XmListSelectPos (monModeListW, i + 1, False);
	    break;
	}
    }
}

static void ModeCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    XmListCallbackStruct *cbs = (XmListCallbackStruct *) call_data;
    int i;

    if (!cbs || !monNumCfg) return;

    i = cbs->item_position - 1;		/* the list counts from one */

    if (i < 0 || i >= monNumModes) return;

    monCfg[monSelected].width = monModes[i].width;
    monCfg[monSelected].height = monModes[i].height;
    monCfg[monSelected].refresh = monModes[i].refresh;

    SyncControls ();
    CanvasExposeCB (monCanvasW, NULL, NULL);
}

static void PrimaryCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    int i;

    if (!monNumCfg) return;

    /*
     * Exactly one primary. Turning the toggle off would leave none, which
     * RandR permits and nothing else in mWizard has an answer for, so the
     * toggle is put back rather than obeyed.
     */
    if (!XmToggleButtonGetState (w))
    {
	XmToggleButtonSetState (w, True, False);
	return;
    }

    for (i = 0; i < monNumCfg; i++) monCfg[i].primary = False;
    monCfg[monSelected].primary = True;

    CanvasExposeCB (monCanvasW, NULL, NULL);
}

static void EnabledCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    int i, enabled = 0;

    if (!monNumCfg) return;

    monCfg[monSelected].enabled = XmToggleButtonGetState (w) ? True : False;

    for (i = 0; i < monNumCfg; i++) if (monCfg[i].enabled) enabled++;

    /*
     * Turning off the last monitor would apply a layout with no screen in it,
     * which is a way to lose the session to a black display with no way back.
     * ApplyMonitorLayout() refuses it as well; this is the half the user can
     * see.
     */
    if (!enabled)
    {
	monCfg[monSelected].enabled = True;
	XmToggleButtonSetState (w, True, False);
	Warning ("At least one monitor has to stay enabled");
	return;
    }

    CanvasExposeCB (monCanvasW, NULL, NULL);
}

static void RotateCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    if (!monNumCfg) return;

    monCfg[monSelected].rotation =
	(monCfg[monSelected].rotation + 1) % MONITOR_ROTATE_COUNT;

    /*
     * Turning a monitor swaps its footprint, so it can now be sitting inside a
     * neighbour. SnapSelected() re-seats it and clears any overlap, the same
     * as it does at the end of a drag.
     */
    SnapSelected ();

    SyncControls ();
    CanvasExposeCB (monCanvasW, NULL, NULL);
}

static void ApplyCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    if (!monNumCfg) return;

    ApplyMonitorLayout (monPSD, monCfg, monNumCfg);

    /*
     * Reloaded from the server rather than trusted: the layout that came back
     * may not be the one asked for -- a mode can be rejected, an output can
     * fail to light -- and the canvas should show what happened, not what was
     * requested.
     */
    LoadConfig ();
    RebuildModeList ();
    SyncControls ();
    CanvasExposeCB (monCanvasW, NULL, NULL);
}

static void SaveCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    if (!monNumCfg) return;

    /*
     * Apply first. Saving a layout that was never applied would write a file
     * that gets restored at the next login without anyone having seen whether
     * it works, and a bad one is restored onto a screen with no dialog on it.
     */
    ApplyMonitorLayout (monPSD, monCfg, monNumCfg);
    SaveMonitorLayout (monPSD, monCfg, monNumCfg);

    LoadConfig ();
    RebuildModeList ();
    SyncControls ();
    CanvasExposeCB (monCanvasW, NULL, NULL);
}

static void RevertCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    LoadConfig ();
    RebuildModeList ();
    SyncControls ();
    CanvasExposeCB (monCanvasW, NULL, NULL);
}

static void CloseCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    UnpostMonitorDialog ();
}

/*
 * WM_DELETE_WINDOW, which is what the frame's Close sends us.
 *
 * This reaches Xt because HandleEventsOnClientWindow() leaves
 * doXtDispatchEvent set for ClientMessage; the window manager looks at the
 * message first and then passes it on. Same path mWinfo uses.
 */
static void MonitorProtocolHandler(Widget w, XtPointer client_data,
				   XEvent *event, Boolean *cont)
{
    if (event->type != ClientMessage) return;
    if (event->xclient.message_type != wmGD.xa_WM_PROTOCOLS) return;
    if ((Atom) event->xclient.data.l[0] == wmGD.xa_WM_DELETE_WINDOW)
	UnpostMonitorDialog ();
}

/*
 * Called once, after the canvas has a window to make GCs against.
 */
static void MakeCanvasGCs(void)
{
    XGCValues gcv;
    Pixel fg = 0, bg = 0, top = 0, bottom = 0;
    Arg args[4];
    int n;

    if (monFillGC || !monCanvasW || !XtWindow (monCanvasW)) return;

    n = 0;
    XtSetArg (args[n], XmNforeground, &fg);		n++;
    XtSetArg (args[n], XmNbackground, &bg);		n++;
    XtSetArg (args[n], XmNtopShadowColor, &top);	n++;
    XtSetArg (args[n], XmNbottomShadowColor, &bottom);	n++;
    XtGetValues (monCanvasW, args, n);

    gcv.foreground = top;
    monFillGC = XCreateGC (DISPLAY1, XtWindow (monCanvasW), GCForeground, &gcv);

    gcv.foreground = bottom;
    monLineGC = XCreateGC (DISPLAY1, XtWindow (monCanvasW), GCForeground, &gcv);

    gcv.foreground = fg;
    gcv.background = bg;
    monTextGC = XCreateGC (DISPLAY1, XtWindow (monCanvasW),
	GCForeground | GCBackground, &gcv);
}

static Boolean MakeMonitorDialog(WmScreenData *pSD)
{
    Arg args[20];
    int n;
    Widget formW, buttonsW, controlsW, frameW, sepW;
    Widget applyW, saveW, revertW, closeW;
    XmString xms;
    Atom deleteAtom;

    if (!pSD->screenTopLevelW1) return (False);

    n = 0;
    XtSetArg (args[n], XtNwaitForWm, (XtArgVal) False);			n++;
    XtSetArg (args[n], XtNallowShellResize, (XtArgVal) True);		n++;
    XtSetArg (args[n], XtNtitle, (XtArgVal) "mWmonitor");		n++;
    XtSetArg (args[n], XtNiconName, (XtArgVal) "mWmonitor");		n++;
    XtSetArg (args[n], XtNdepth,
	(XtArgVal) DefaultDepth (DISPLAY1, pSD->screen));		n++;
    XtSetArg (args[n], XtNscreen,
	(XtArgVal) ScreenOfDisplay (DISPLAY1, pSD->screen));		n++;
    XtSetArg (args[n], XtNcolormap,
	(XtArgVal) DefaultColormap (DISPLAY1, pSD->screen));		n++;

    monShellW = XtCreatePopupShell ("mwmonitor", transientShellWidgetClass,
				    pSD->screenTopLevelW1, args, n);
    if (!monShellW) return (False);

    monPSD = pSD;
    LoadConfig ();

    n = 0;
    XtSetArg (args[n], XmNhorizontalSpacing, (XtArgVal) 8);	n++;
    XtSetArg (args[n], XmNverticalSpacing, (XtArgVal) 8);	n++;
    formW = XtCreateManagedWidget ("form", xmFormWidgetClass,
				   monShellW, args, n);

    /*
     * Buttons first and attached to the bottom, so that the canvas above them
     * takes whatever height is left when the window is resized rather than the
     * buttons growing to fill it.
     */
    n = 0;
    XtSetArg (args[n], XmNleftAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNrightAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNbottomAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNfractionBase, (XtArgVal) 4);			n++;
    buttonsW = XtCreateManagedWidget ("buttons", xmFormWidgetClass,
				      formW, args, n);

/*
 * Each button gets exactly its quarter, both edges attached.
 *
 * Attached on the left alone -- which is what this did -- a button starts at
 * its quarter and then runs to whatever width its label needs. Narrow the
 * window and they grow into each other; Motif stacks the overlap and the
 * button on top takes the click, so the ones underneath simply stop working
 * with nothing to show for it. Pinning both edges costs a wider minimum
 * window and makes overlap impossible.
 */
#define BUTTON(var,label,pos,cb)					\
    xms = XmStringCreateLocalized (label);				\
    n = 0;								\
    XtSetArg (args[n], XmNlabelString, (XtArgVal) xms);		n++;	\
    XtSetArg (args[n], XmNleftAttachment,				\
	(XtArgVal) XmATTACH_POSITION);				n++;	\
    XtSetArg (args[n], XmNleftPosition, (XtArgVal) (pos));	n++;	\
    XtSetArg (args[n], XmNleftOffset, (XtArgVal) 4);		n++;	\
    XtSetArg (args[n], XmNrightAttachment,				\
	(XtArgVal) XmATTACH_POSITION);				n++;	\
    XtSetArg (args[n], XmNrightPosition, (XtArgVal) ((pos) + 1)); n++;	\
    XtSetArg (args[n], XmNrightOffset, (XtArgVal) 4);		n++;	\
    XtSetArg (args[n], XmNtopAttachment, (XtArgVal) XmATTACH_FORM); n++;	\
    XtSetArg (args[n], XmNbottomAttachment,				\
	(XtArgVal) XmATTACH_FORM);				n++;	\
    var = XtCreateManagedWidget (label, xmPushButtonWidgetClass,		\
				 buttonsW, args, n);			\
    XmStringFree (xms);							\
    XtAddCallback (var, XmNactivateCallback, (XtCallbackProc) cb, NULL);

    BUTTON (applyW,  "Apply",  0, ApplyCB)
    BUTTON (saveW,   "Save",   1, SaveCB)
    BUTTON (revertW, "Revert", 2, RevertCB)
    BUTTON (closeW,  "Close",  3, CloseCB)
#undef BUTTON

    n = 0;
    XtSetArg (args[n], XmNleftAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNrightAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNbottomAttachment, (XtArgVal) XmATTACH_WIDGET);n++;
    XtSetArg (args[n], XmNbottomWidget, (XtArgVal) buttonsW);		n++;
    sepW = XtCreateManagedWidget ("separator", xmSeparatorGadgetClass,
				  formW, args, n);

    /* The controls for whichever monitor is selected. */
    n = 0;
    XtSetArg (args[n], XmNorientation, (XtArgVal) XmHORIZONTAL);	n++;
    XtSetArg (args[n], XmNpacking, (XtArgVal) XmPACK_TIGHT);		n++;
    XtSetArg (args[n], XmNleftAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNrightAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNbottomAttachment, (XtArgVal) XmATTACH_WIDGET);n++;
    XtSetArg (args[n], XmNbottomWidget, (XtArgVal) sepW);		n++;
    controlsW = XtCreateManagedWidget ("controls", xmRowColumnWidgetClass,
				       formW, args, n);

    xms = XmStringCreateLocalized ("No monitors");
    n = 0;
    XtSetArg (args[n], XmNlabelString, (XtArgVal) xms);			n++;
    XtSetArg (args[n], XmNalignment, (XtArgVal) XmALIGNMENT_BEGINNING);	n++;
    monNameW = XtCreateManagedWidget ("monitorName", xmLabelWidgetClass,
				      controlsW, args, n);
    XmStringFree (xms);

    xms = XmStringCreateLocalized ("Primary");
    n = 0;
    XtSetArg (args[n], XmNlabelString, (XtArgVal) xms);		n++;
    monPrimaryW = XtCreateManagedWidget ("primary", xmToggleButtonWidgetClass,
					 controlsW, args, n);
    XmStringFree (xms);
    XtAddCallback (monPrimaryW, XmNvalueChangedCallback,
	(XtCallbackProc) PrimaryCB, NULL);

    xms = XmStringCreateLocalized ("Enabled");
    n = 0;
    XtSetArg (args[n], XmNlabelString, (XtArgVal) xms);		n++;
    monEnabledW = XtCreateManagedWidget ("enabled", xmToggleButtonWidgetClass,
					 controlsW, args, n);
    XmStringFree (xms);
    XtAddCallback (monEnabledW, XmNvalueChangedCallback,
	(XtCallbackProc) EnabledCB, NULL);

    /*
     * Rotation, as one button that cycles rather than four toggles or a menu.
     *
     * A menu is out of the question here -- see the note above
     * RebuildModeList() -- and four radio buttons would be the widest thing in
     * the window for a setting most people never touch. There are only four
     * values and they have an obvious order, so the button shows the current
     * one and advances.
     */
    xms = XmStringCreateLocalized ("Rotation: normal");
    n = 0;
    XtSetArg (args[n], XmNlabelString, (XtArgVal) xms);		n++;
    monRotateW = XtCreateManagedWidget ("rotate", xmPushButtonWidgetClass,
					controlsW, args, n);
    XmStringFree (xms);
    XtAddCallback (monRotateW, XmNactivateCallback,
	(XtCallbackProc) RotateCB, NULL);

    /*
     * The mode list, down the right hand side.
     *
     * Beside the canvas rather than in the row of controls below it: a list
     * tall enough to be useful would otherwise push the canvas out of the
     * window. XmCreateScrolledList hands back the list; its parent is the
     * scrolled window, and that is what gets attached.
     */
    n = 0;
    XtSetArg (args[n], XmNvisibleItemCount, (XtArgVal) 8);		n++;
    XtSetArg (args[n], XmNselectionPolicy, (XtArgVal) XmBROWSE_SELECT);	n++;
    monModeListW = XmCreateScrolledList (formW, "modeList", args, n);
    XtManageChild (monModeListW);

    XtAddCallback (monModeListW, XmNbrowseSelectionCallback,
	(XtCallbackProc) ModeCB, NULL);

    n = 0;
    XtSetArg (args[n], XmNtopAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNrightAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNbottomAttachment, (XtArgVal) XmATTACH_WIDGET);n++;
    XtSetArg (args[n], XmNbottomWidget, (XtArgVal) controlsW);		n++;
    XtSetValues (XtParent (monModeListW), args, n);

    /* The canvas takes everything above the controls, left of the list. */
    n = 0;
    XtSetArg (args[n], XmNshadowType, (XtArgVal) XmSHADOW_IN);		n++;
    XtSetArg (args[n], XmNtopAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNleftAttachment, (XtArgVal) XmATTACH_FORM);	n++;
    XtSetArg (args[n], XmNrightAttachment, (XtArgVal) XmATTACH_WIDGET);	n++;
    XtSetArg (args[n], XmNrightWidget,
	(XtArgVal) XtParent (monModeListW));				n++;
    XtSetArg (args[n], XmNbottomAttachment, (XtArgVal) XmATTACH_WIDGET);n++;
    XtSetArg (args[n], XmNbottomWidget, (XtArgVal) controlsW);		n++;
    frameW = XtCreateManagedWidget ("canvasFrame", xmFrameWidgetClass,
				    formW, args, n);

    n = 0;
    XtSetArg (args[n], XmNwidth, (XtArgVal) CANVAS_WIDTH);		n++;
    XtSetArg (args[n], XmNheight, (XtArgVal) CANVAS_HEIGHT);		n++;
    XtSetArg (args[n], XmNmarginWidth, (XtArgVal) 0);			n++;
    XtSetArg (args[n], XmNmarginHeight, (XtArgVal) 0);			n++;
    monCanvasW = XtCreateManagedWidget ("canvas", xmDrawingAreaWidgetClass,
					frameW, args, n);

    XtAddCallback (monCanvasW, XmNexposeCallback,
	(XtCallbackProc) CanvasExposeCB, NULL);
    XtAddCallback (monCanvasW, XmNinputCallback,
	(XtCallbackProc) CanvasInputCB, NULL);
    XtAddCallback (monCanvasW, XmNresizeCallback,
	(XtCallbackProc) CanvasExposeCB, NULL);

    /*
     * XmNinputCallback fires for button presses and releases but not for
     * motion, which is the one a drag is made of.
     */
    XtAddEventHandler (monCanvasW, ButtonMotionMask, False,
	(XtEventHandler) CanvasMotionEH, NULL);

    RebuildModeList ();
    SyncControls ();

    /*
     * Positioned before realizing. On an unrealized shell this only writes
     * core.x/core.y; once it is realized the same call becomes a request to
     * the window manager, and there is no reason to make one here.
     */
    PlaceMonitorDialog (pSD);

    XtRealizeWidget (monShellW);

    /*
     * allowShellResize goes off once the window exists, and this is not
     * cosmetic.
     *
     * With it on, anything that changes a child's preferred size after realize
     * -- refilling the mode list, relabelling the monitor name -- makes Xt ask
     * the window manager to resize the shell. The window manager is this
     * process, and the ask can arrive while it is already inside a callback.
     * The window is user-resizable and nothing here needs to drive its size,
     * so the safe answer is to stop asking.
     */
    n = 0;
    XtSetArg (args[n], XtNallowShellResize, (XtArgVal) False);	n++;
    XtSetArg (args[n], XtNwaitForWm, (XtArgVal) False);		n++;
    XtSetValues (monShellW, args, n);

    /*
     * No _MOTIF_WM_HINTS: this window is meant to behave like any other, so
     * it keeps the full frame. That makes the delete protocol mandatory --
     * see the note at the top of the file.
     */
    deleteAtom = wmGD.xa_WM_DELETE_WINDOW;
    XSetWMProtocols (DISPLAY1, XtWindow (monShellW), &deleteAtom, 1);

    XtAddEventHandler (monShellW, NoEventMask, True,
	(XtEventHandler) MonitorProtocolHandler, (XtPointer) NULL);

    return (True);
}

/*
 * Centres the window on the user's preferred monitor, the same rule the
 * Execute dialog and mWinfo use.
 */
static void PlaceMonitorDialog(WmScreenData *pSD)
{
    Arg args[4];
    int n;
    Dimension width = 0, height = 0;
    Position x, y;
    int mx, my, mw, mh;

    n = 0;
    XtSetArg (args[n], XmNwidth, &width);	n++;
    XtSetArg (args[n], XmNheight, &height);	n++;
    XtGetValues (monShellW, args, n);

    MonitorWorkArea (pSD, PrimaryMonitor (pSD), &mx, &my, &mw, &mh);

    x = mx + (mw - (int) width) / 2;
    y = my + (mh - (int) height) / 2;

    if (x < 0) x = 0;
    if (y < 0) y = 0;

    n = 0;
    XtSetArg (args[n], XmNx, (XtArgVal) x);		n++;
    XtSetArg (args[n], XmNy, (XtArgVal) y);		n++;
    XtSetArg (args[n], XtNwaitForWm, (XtArgVal) False);	n++;
    XtSetValues (monShellW, args, n);
}

/*
 * Posts the window, one turn of the event loop later.
 *
 * ActivateCallback() in WmMenu.c runs an rc function from inside Motif's menu
 * activation, with the menu still posted and its pointer and keyboard grab
 * still held -- wmGD.menuActive is cleared later, when the menu unmaps. Doing
 * anything substantial there runs inside Motif's menu state machine, and the
 * cost of getting that wrong is the grab never being released: no pointer, no
 * keyboard, nothing to do but cut the power.
 *
 * A zero-length timeout costs nothing and moves the whole of it out to the
 * event loop, where the menu is down, the grab is gone and this is an ordinary
 * callback like any other. The same shape the signal handlers use --
 * XtNoticeSignal notes it, and the work happens from the loop.
 *
 * Coalesced, so holding the key binding down cannot queue a hundred of them.
 */
static void PostMonitorTimeout(XtPointer client_data, XtIntervalId *id)
{
    monPostTimer = (XtIntervalId) 0;
    PostMonitorNow ();
}

void PostMonitorDialog(void)
{
    if (monPostTimer) return;

    monPostTimer = XtAppAddTimeOut (wmGD.mwmAppContext, 0,
	PostMonitorTimeout, (XtPointer) NULL);
}

static void PostMonitorNow(void)
{
    WmScreenData *pSD = ACTIVE_PSD;

    if (!pSD) return;

    /*
     * A system modal window is up and has the input; posting over it would
     * put up a window that cannot be used. Same guard ConfirmAction() uses.
     */
    if (wmGD.systemModalActive) return;

    if (!wmGD.xrandr_present)
    {
	Warning ("The X server has no RandR extension; "
		 "monitors cannot be configured");
	return;
    }

    /*
     * The shell belongs to one screen and cannot be moved to another, so on a
     * genuinely multi-screen display it is rebuilt when the active screen
     * changes. This costs nothing in the ordinary single-screen case.
     */
    if (monShellW && monPSD != pSD)
    {
	UnpostMonitorDialog ();
	XtDestroyWidget (monShellW);
	monShellW = NULL;
	monCanvasW = NULL;
	monModeListW = NULL;
	monOnScreen = False;

	/*
	 * The GCs went with the window they were made against, so they are
	 * dropped here and remade for the new screen's canvas.
	 */
	if (monFillGC) { XFreeGC (DISPLAY1, monFillGC); monFillGC = NULL; }
	if (monLineGC) { XFreeGC (DISPLAY1, monLineGC); monLineGC = NULL; }
	if (monTextGC) { XFreeGC (DISPLAY1, monTextGC); monTextGC = NULL; }

	FreeConfig ();
    }

    if (!monShellW)
    {
	if (!MakeMonitorDialog (pSD)) return;

	/*
	 * Nothing to arrange. Better to say so and put no window up than to
	 * leave an empty one with dead controls on the screen, which is what
	 * this did before.
	 */
	if (monNumCfg < 1)
	{
	    /*
	     * Naming the cause rather than the symptom. "Nothing to
	     * configure" with RandR missing and "nothing to configure" with
	     * RandR present but reporting no connected output are different
	     * faults, and telling them apart from the outside is otherwise
	     * guesswork. mWinfo carries the same information.
	     */
	    Warning (wmGD.xrandr_present ?
		"mWmonitor: RandR reports no connected outputs" :
		"mWmonitor: the X server has no RandR extension");
	    XtDestroyWidget (monShellW);
	    monShellW = NULL;
	    monCanvasW = NULL;
	    monModeListW = NULL;
	    return;
	}
    }
    else
    {
	/*
	 * Reloaded on every post. The layout may well have changed since the
	 * window was last up -- a hotplug is one of the things that posts it.
	 */
	LoadConfig ();
	RebuildModeList ();
	SyncControls ();
    }

    if (!monOnScreen)
    {
	PlaceMonitorDialog (pSD);
	XtPopup (monShellW, XtGrabNone);
	monOnScreen = True;
    }
    else
    {
	/*
	 * Already up: raise it through F_Raise rather than XRaiseWindow, so
	 * that mWizard's own stacking list stays correct. Same as mWinfo.
	 */
	ClientData *pCD = NULL;

	if (XtWindow (monShellW) &&
	    !XFindContext (DISPLAY, XtWindow (monShellW),
			   wmGD.windowContextType, (XPointer *) &pCD) && pCD)
	{
	    F_Raise ((String) NULL, pCD, (XEvent *) NULL);
	}
    }

    if (monCanvasW) CanvasExposeCB (monCanvasW, NULL, NULL);
}

/*
 * Takes the window off the screen. Popping the shell down unmaps it, which is
 * what tells the window manager side to unmanage the frame; the widgets stay
 * for the next post.
 */
static void UnpostMonitorDialog(void)
{
    if (monShellW && monOnScreen)
    {
	XtPopdown (monShellW);

	/*
	 * XtPopdown does nothing if the window is already unmapped -- which it
	 * is whenever the window manager side has iconified it or put it on a
	 * workspace that is not showing -- and then the frame is never
	 * unmanaged. Withdraw it explicitly, over the first connection so that
	 * the window manager sees this in order with its own events.
	 */
	if (XtWindow (monShellW))
	{
	    XWithdrawWindow (DISPLAY, XtWindow (monShellW), monPSD->screen);
	    XSync (DISPLAY, False);
	}

	monOnScreen = False;
    }
}

void InitMonitorDialog(void)
{
    int scr;

    /*
     * Nothing is built here. The window is made on first post like the other
     * two dialogs, and until then mWmonitor costs one advertised bit.
     */
    AdvertiseWmCommand (MWIZARD_CMD_MONITOR);

    if (!wmGD.xrandr_present) return;

    /*
     * Restore a saved layout for the monitors that are attached, and note the
     * output set so that the first hotplug after startup can be told from the
     * state the session began in.
     */
    for (scr = 0; scr < wmGD.numScreens; scr++)
    {
	WmScreenData *pSD = &wmGD.Screens[scr];

	if (!pSD->managed) continue;

	NoteConnectedOutputs (pSD);
	ApplySavedMonitorLayout (pSD);
    }
}
