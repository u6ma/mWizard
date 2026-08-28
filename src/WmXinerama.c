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
 * One behavioural change comes with that, and it is the point of the exercise:
 * these functions used to return False whenever Xinerama was inactive, and
 * every caller had a DisplayWidth/DisplayHeight fallback for that case. There
 * is always at least one monitor now (WmMonitor.h says why), so they return
 * True and the fallbacks have become dead code that stays for safety. On a
 * single head the monitor covers the whole root and the answer is identical to
 * what the fallback would have produced.
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
 * Retrieves monitor info from given coordinates.
 *
 * A point outside every monitor now answers with the nearest one rather than
 * failing; see MonitorFromLocation(). The negative-coordinate clamp the old
 * implementation needed is gone with it -- a negative coordinate is simply a
 * point off the left or top edge, and nearest handles it.
 */
Bool GetXineramaScreenFromLocation(int x, int y, XineramaScreenInfo *xsi)
{
	WmScreenData *pSD = ACTIVE_PSD;

	if(!pSD) return False;

	return FillFromMonitor(pSD, MonitorFromLocation(pSD, x, y), xsi);
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
