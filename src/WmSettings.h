/*
 * Copyright (C) 2026 845 <vinci845@icloud.com>
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
 * Window manager behaviour settings, read from the rc file.
 *
 * EMWM split its configuration in two: bindings and menus lived in the rc
 * file, while every behaviour knob lived in the X resource database. mWizard
 * reads behaviour from the rc file too, leaving the resource database to hold
 * only what it is actually good at -- colors, shadows and fonts.
 *
 *     Settings
 *     {
 *         keyboardFocusPolicy   explicit
 *         moveOpaque            true
 *         workspaceCount        4
 *         lockCommand           "i3lock -c 000000"
 *     }
 *
 *     Client XTerm
 *     {
 *         clientDecoration      -resizeh
 *         focusAutoRaise        false
 *     }
 *
 * Each entry is written into the screen resource database under the window
 * manager's instance name, so the existing XtResource tables, type
 * converters and defaults in WmResource.c keep doing the actual work. An
 * instance-name entry outranks the class-name form that .Xdefaults files
 * use, and an identical key is replaced outright, so a setting here always
 * beats a leftover X resource.
 *
 * Setting names are validated against those same resource tables, so a
 * misspelled name is reported rather than silently ignored.
 */

#ifndef _WmSettings_h
#define _WmSettings_h

#include <X11/Intrinsic.h>
#include "WmGlobal.h"

/*
 * Reads the Settings and Client blocks out of the rc file and merges them
 * into the resource database of every screen on the display. Must be called
 * after the top level shell exists and before any resources are fetched.
 */
void LoadRcSettings(void);

/*
 * True if a top-level rc keyword names a block that LoadRcSettings has
 * already consumed, so that the main rc parse knows to skip over it.
 */
Boolean IsSettingsKeyword(const char *keyword);

#endif /* _WmSettings_h */
