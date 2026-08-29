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

/*
 * The Xinerama-shaped view of the monitor list.
 *
 * Until 1.3 this file owned the monitor data: it called XineramaQueryScreens()
 * and kept the array in two file statics. It no longer does. WmMonitor.c owns
 * the list -- backed by RandR, so the heads have names -- and this is the
 * compatibility face over it.
 *
 * The file is kept rather than folded into WmMonitor.c because about twenty
 * layout sites across WmWinConf.c, WmWinInfo.c, WmIPlace.c, WmMenu.c,
 * WmCDecor.c, WmFeedback.c and the dialogs already speak this API, and every
 * one of them gets named monitors, per-monitor struts and the nearest-monitor
 * rule by having this reimplemented underneath them instead of being rewritten.
 * XineramaScreenInfo carries exactly the four numbers they want.
 *
 * The callers' fallbacks still matter. Every one of these used to return False
 * when Xinerama was inactive, and each caller has a DisplayWidth/DisplayHeight
 * path for that. 1.3 first made them always succeed -- there is always at least
 * one monitor now -- and that was a mistake worth recording: it made
 * GetXineramaScreenFromLocation() answer with the nearest monitor for a point
 * on no monitor at all, so a caller clamping to that rectangle moved the thing
 * it was clamping onto a different screen instead of leaving it alone. The
 * location query is strict again and the fallbacks are live.
 */

#include <string.h>

#include "WmGlobal.h"
#include "WmError.h"
#include "WmMonitor.h"
#include "WmXinerama.h"

static Bool FillFromMonitor(WmScreenData *pSD, int i, XineramaScreenInfo *xsi);

/*
 * Copies one monitor into the shape the old callers expect.
 */
static Bool FillFromMonitor(WmScreenData *pSD, int i, XineramaScreenInfo *xsi)
{
	if(!pSD || i < 0 || i >= pSD->numMonitors) return False;

	xsi->screen_number = i;
	xsi->x_org = pSD->pMonitors[i].x;
	xsi->y_org = pSD->pMonitors[i].y;
	xsi->width = pSD->pMonitors[i].width;
	xsi->height = pSD->pMonitors[i].height;

	return True;
}

/*
 * Kept so that WmInitWs.c's early call still has something to call. The
 * monitor list cannot be built here -- it is per screen, and no WmScreenData
 * exists this early -- so SetupMonitors() is called from InitWmScreen()
 * instead and this does nothing.
 */
void SetupXinerama(void)
{
}

/*
 * Called on xrandr screen change events.
 *
 * The rebuild belongs to WmMonitor.c now; HandleRRScreenChangeNotify() calls
 * UpdateMonitors() directly and this remains only for callers that have not
 * been changed.
 */
void UpdateXineramaInfo(void)
{
	if(ACTIVE_PSD) UpdateMonitors(ACTIVE_PSD);
}

/*
 * Retrieves the count of monitors available.
 */
Bool GetXineramaScreenCount(int *i)
{
	WmScreenData *pSD = ACTIVE_PSD;

	if(!pSD || pSD->numMonitors < 1) {
		*i = 0;
		return False;
	}
	*i = pSD->numMonitors;
	return True;
}

/*
 * Retrieves monitor info from given coordinates, or False if the point is on
 * no monitor at all.
 */
Bool GetXineramaScreenFromLocation(int x, int y, XineramaScreenInfo *xsi)
{
	WmScreenData *pSD = ACTIVE_PSD;
	int mon;

	if(!pSD) return False;

	/*
	 * Strictly the monitor containing the point, and False when none does.
	 *
	 * This keeps the contract the callers were written against, and 1.3
	 * briefly broke it: answering with the *nearest* monitor made this
	 * always succeed, which turned every caller's whole-root fallback into
	 * dead code. That is fine while the monitor list is right and actively
	 * harmful when it is not -- a menu posted at a point the list does not
	 * cover stopped being left alone and started being clamped onto
	 * whichever head came nearest, which on a two-head desk means the menu
	 * jumps to the other monitor.
	 *
	 * Callers that want the nearest monitor for a point in the dead space
	 * of an L-shaped arrangement -- the clamping sites in WmWinConf.c --
	 * ask MonitorFromLocation() directly and get it.
	 */
	mon = MonitorContaining(pSD, x, y);
	if(mon < 0) return False;

	return FillFromMonitor(pSD, mon, xsi);
}

/*
 * Retrieves info for the monitor that contains the mouse pointer.
 */
Bool GetXineramaScreenFromPointer(XineramaScreenInfo *xsi)
{
	WmScreenData *pSD = ACTIVE_PSD;

	if(!pSD) return False;

	return FillFromMonitor(pSD, MonitorFromPointer(pSD), xsi);
}

/*
 * Retrieves info for the monitor that contains a client window with keyboard
 * focus or the mouse pointer (in that order).
 */
Bool GetActiveXineramaScreen(XineramaScreenInfo *xsi)
{
	if(wmGD.keyboardFocus){
		return GetXineramaScreenFromLocation(
			wmGD.keyboardFocus->clientX,
			wmGD.keyboardFocus->clientY,xsi);
	}else{
		return GetXineramaScreenFromPointer(xsi);
	}
}

/*
 * Retrieves the user's preferred monitor.
 */
Bool GetPrimaryXineramaScreen(XineramaScreenInfo *xsi)
{
	WmScreenData *pSD = ACTIVE_PSD;

	if(!pSD) return False;

	return FillFromMonitor(pSD, PrimaryMonitor(pSD), xsi);
}

/*
 * Retrieves monitor info by index.
 */
Bool GetXineramaScreenInfo(int index, XineramaScreenInfo *xsi)
{
	WmScreenData *pSD = ACTIVE_PSD;

	if(!pSD) return False;

	return FillFromMonitor(pSD, index, xsi);
}
