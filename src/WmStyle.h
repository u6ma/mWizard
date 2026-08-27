/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

#ifndef WMSTYLE_H
#define WMSTYLE_H

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

/*
 * Font roles.
 *
 * WmStyleFont is the base: it is what everything uses unless the style file
 * names a more specific role. The rest are the places that are allowed to
 * differ from it. Both programs share the vocabulary, so a role one of them
 * does not use is still a valid name in the file -- mWizard never draws a
 * panel, mWand has no title bars, and neither should complain about the
 * other's entry.
 */
#define WmStyleFont		"font"
#define WmStyleTitleFont	"titleFont"
#define WmStyleIconFont		"iconFont"
#define WmStyleMenuFont		"menuFont"
#define WmStyleMenuTitleFont	"menuTitleFont"
#define WmStyleFeedbackFont	"feedbackFont"
#define WmStyleDialogFont	"dialogFont"
#define WmStylePanelFont	"panelFont"

/*
 * Reads the style file and merges what it says into every screen's resource
 * database. Must run before any appearance resource is fetched, and after
 * the display is open -- see the call in InitWmGlobal().
 */
void LoadStyleFile(void);

/*
 * The render table for one role, or the base font's if that role was not
 * named, or NULL if no font could be made at all. Built on first use and
 * kept; do not free the result.
 *
 * For widgets whose font the resource database cannot be trusted to reach --
 * menu panes, whose gadgets Motif fonts from the menu shell rather than from
 * a loose binding. Everything else picks its font up from the database.
 */
XmRenderTable StyleFont(const char *role);

/* The style file that was read, or NULL if none was found. */
const char *StyleFileName(void);

#endif /* WMSTYLE_H */
