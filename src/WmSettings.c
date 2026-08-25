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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <X11/Intrinsic.h>
#include <X11/Xresource.h>

#include "WmGlobal.h"
#include "WmSettings.h"
#include "WmResource.h"
#include "WmResParse.h"
#include "WmError.h"

#define SETTINGS_KEYWORD	"settings"
#define CLIENT_KEYWORD		"client"
#define WORKSPACE_KEYWORD	"workspace"

/* Which resource tables a block's names are validated against. */
typedef enum {
    BLOCK_SETTINGS,	/* global and per-screen behaviour */
    BLOCK_CLIENT,	/* per-client behaviour */
    BLOCK_WORKSPACE	/* per-workspace */
} BlockKind;

#define RC_LINE_MAX		(MAXWMPATH + 1)

/*
 * A setting name is accepted if it appears in one of the resource tables
 * that describe window manager behaviour. Appearance resources are
 * deliberately absent: fonts, colors and shadows stay in the X resource
 * database, and naming one here should be reported as a mistake.
 */
static Boolean IsKnownName(const char *name, BlockKind kind)
{
    Cardinal i;

    if (kind == BLOCK_CLIENT)
    {
	for (i = 0; i < wmNumClientResources; i++)
	    if (!strcmp (name, wmClientResources[i].resource_name))
		return (True);
	return (False);
    }

    if (kind == BLOCK_WORKSPACE)
    {
	for (i = 0; i < wmNumWorkspaceResources; i++)
	    if (!strcmp (name, wmWorkspaceResources[i].resource_name))
		return (True);
	return (False);
    }

    for (i = 0; i < wmNumGlobalResources; i++)
	if (!strcmp (name, wmGlobalResources[i].resource_name))
	    return (True);

    for (i = 0; i < wmNumGlobalScreenResources; i++)
	if (!strcmp (name, wmGlobalScreenResources[i].resource_name))
	    return (True);

    for (i = 0; i < wmNumScreenResources; i++)
	if (!strcmp (name, wmScreenResources[i].resource_name))
	    return (True);

    for (i = 0; i < wmNumWorkspaceResources; i++)
	if (!strcmp (name, wmWorkspaceResources[i].resource_name))
	    return (True);

    return (False);
}

/*
 * Writes one setting into every screen's resource database.
 *
 * The key is built from the window manager's instance name rather than its
 * class, which is what gives an rc setting precedence over the class-form
 * entries (MWizard*foo) that .Xdefaults files carry. An identical key is
 * replaced, so the instance form loses to nothing.
 */
static void PutSetting(const char *prefix, const char *name, const char *value)
{
    char key[RC_LINE_MAX];
    XrmDatabase db;
    int scr;

    snprintf (key, sizeof(key), "%s%s%s",
	      WM_RESOURCE_NAME, prefix, name);

    for (scr = 0; scr < ScreenCount (DISPLAY); scr++)
    {
	db = XtScreenDatabase (ScreenOfDisplay (DISPLAY, scr));
	if (db) XrmPutStringResource (&db, key, (char *)value);
    }
}

/* Trims leading and trailing whitespace in place; returns the new start. */
static char *Trim(char *s)
{
    char *end;

    while (*s && isspace ((unsigned char)*s)) s++;
    if (!*s) return (s);

    end = s + strlen(s) - 1;
    while (end > s && isspace ((unsigned char)*end)) *end-- = '\0';

    return (s);
}

/*
 * Splits "name value" into its two parts. The value is everything after the
 * first run of whitespace, so unquoted multi-word values such as
 * "left bottom" survive; a fully quoted value has its quotes stripped.
 */
static Boolean SplitEntry(char *line, char **namep, char **valuep)
{
    char *p, *value;
    size_t len;

    p = line;
    while (*p && !isspace ((unsigned char)*p)) p++;
    if (!*p) return (False);

    *p++ = '\0';
    value = Trim (p);
    if (!*value) return (False);

    len = strlen (value);
    if (len >= 2 && value[0] == '"' && value[len-1] == '"')
    {
	value[len-1] = '\0';
	value++;
    }

    *namep  = line;
    *valuep = value;

    return (True);
}

Boolean IsSettingsKeyword(const char *keyword)
{
    return (!strcmp (keyword, SETTINGS_KEYWORD) ||
	    !strcmp (keyword, CLIENT_KEYWORD) ||
	    !strcmp (keyword, WORKSPACE_KEYWORD));
}

void LoadRcSettings(void)
{
    FILE *fp;
    char  buf[RC_LINE_MAX];
    char  prefix[RC_LINE_MAX];
    char *line, *name, *value, *p;
    int   depth = 0;
    Boolean inBlock = False;
    BlockKind kind = BLOCK_SETTINGS;
    Boolean pendingBrace = False;
    int   lineNum = 0;
    char *msg;
    size_t len;
    const char fmt[] = "%s, line %d: unknown setting \"%s\".";

    if ((fp = FopenConfigFile ()) == NULL) return;

    prefix[0] = '\0';

    while (fgets (buf, sizeof(buf), fp) != NULL)
    {
	lineNum++;

	if ((p = strchr (buf, '\n')) != NULL) *p = '\0';

	line = Trim (buf);
	if (!*line || *line == '!') continue;

	/*
	 * A header may be followed by its brace on the same line or the
	 * next one, which is why the brace is tracked separately.
	 */
	if (pendingBrace)
	{
	    if (*line == '{')
	    {
		pendingBrace = False;
		inBlock = True;
		depth = 1;
		line = Trim (line + 1);
		if (!*line) continue;
	    }
	    else
	    {
		/*
		 * Not a block after all -- most likely a menu item whose
		 * label happened to be "Settings". Forget it and carry on.
		 */
		pendingBrace = False;
	    }
	}

	if (!inBlock)
	{
	    /*
	     * Only recognize a block header at the outermost level, so that
	     * a menu item or binding named "Settings" cannot be mistaken
	     * for one.
	     */
	    char first[RC_LINE_MAX];
	    char rest[RC_LINE_MAX];
	    int  n = 0;

	    p = line;
	    while (*p && !isspace ((unsigned char)*p) && n < RC_LINE_MAX-1)
		first[n++] = tolower ((unsigned char)*p++);
	    first[n] = '\0';

	    strncpy (rest, Trim (p), sizeof(rest) - 1);
	    rest[sizeof(rest)-1] = '\0';

	    if (!strcmp (first, SETTINGS_KEYWORD))
	    {
		kind = BLOCK_SETTINGS;
		snprintf (prefix, sizeof(prefix), "*");
		pendingBrace = True;
		if (*rest == '{')
		{
		    pendingBrace = False;
		    inBlock = True;
		    depth = 1;
		}
		continue;
	    }
	    else if ((!strcmp (first, CLIENT_KEYWORD) ||
		      !strcmp (first, WORKSPACE_KEYWORD)) && *rest)
	    {
		char spec[RC_LINE_MAX];
		n = 0;
		p = rest;
		while (*p && !isspace ((unsigned char)*p) && n < RC_LINE_MAX-1)
		    spec[n++] = *p++;
		spec[n] = '\0';

		kind = (!strcmp (first, CLIENT_KEYWORD)) ?
			    BLOCK_CLIENT : BLOCK_WORKSPACE;
		snprintf (prefix, sizeof(prefix), "*%s*", spec);
		pendingBrace = True;
		p = Trim (p);
		if (*p == '{')
		{
		    pendingBrace = False;
		    inBlock = True;
		    depth = 1;
		}
		continue;
	    }

	    /* Some other top-level construct; not ours. */
	    continue;
	}

	/* Inside a Settings or Client block. */
	if (*line == '}')
	{
	    depth--;
	    if (depth <= 0) inBlock = False;
	    continue;
	}

	if (!SplitEntry (line, &name, &value)) continue;

	if (!IsKnownName (name, kind))
	{
	    len = snprintf (NULL, 0, fmt, WM_RESOURCE_NAME, lineNum, name);
	    msg = XtMalloc (len + 1);
	    sprintf (msg, fmt, WM_RESOURCE_NAME, lineNum, name);
	    Warning (msg);
	    XtFree (msg);
	    continue;
	}

	PutSetting (prefix, name, value);
    }

    fclose (fp);
}
