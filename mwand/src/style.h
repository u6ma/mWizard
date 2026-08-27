/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

#ifndef STYLE_H
#define STYLE_H

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

/*
 * Font roles. The vocabulary is shared with mWizard -- see src/WmStyle.h --
 * so a role only the window manager draws is still a valid name here, and
 * one style file can serve both without either complaining about the other's
 * entries.
 */
#define StyleFontBase		"font"
#define StyleFontTitle		"titleFont"
#define StyleFontIcon		"iconFont"
#define StyleFontMenu		"menuFont"
#define StyleFontMenuTitle	"menuTitleFont"
#define StyleFontFeedback	"feedbackFont"
#define StyleFontDialog		"dialogFont"
#define StyleFontPanel		"panelFont"

/*
 * Reads the style file and merges what it says into the screen resource
 * database. Must run before any resource is fetched from it, and before the
 * widgets are made.
 */
void LoadStyleFile(Widget w);

/*
 * The render table for one role, or the base font's if that role was not
 * named, or NULL if no font could be made. Built on first use and kept; do
 * not free the result.
 *
 * For the menu gadgets, whose font Motif resolves from their menu shell
 * rather than from the database. Everything else takes its font from the
 * database and never needs this.
 */
XmRenderTable StyleFont(Widget w, const char *role);

#endif /* STYLE_H */
