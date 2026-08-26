/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
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
#include "WmSession.h"

#define SETTINGS_KEYWORD	"settings"
#define CLIENT_KEYWORD		"client"
#define WORKSPACE_KEYWORD	"workspace"
#define VARIABLES_KEYWORD	"variables"
#define STARTUP_KEYWORD		"startup"

/* Which resource tables a block's names are validated against. */
typedef enum {
    BLOCK_SETTINGS,	/* global and per-screen behaviour */
    BLOCK_CLIENT,	/* per-client behaviour */
    BLOCK_WORKSPACE,	/* per-workspace */
    BLOCK_VARIABLES,	/* command variables, exported to the environment */
    BLOCK_STARTUP	/* commands to run once the window manager is up */
} BlockKind;

/*
 * Commands from the Startup block, collected here and run later by
 * RunStartupCommands(). They are not run as they are read: the rc file is
 * scanned before the screens are managed, and a client that maps in that
 * window would race the window manager.
 */
static char **startupCmds = NULL;
static int    numStartupCmds = 0;

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

    /*
     * Client resources are accepted here too. Written without a client
     * component the key matches every client, which is how EMWM's
     * "Emwm*clientDecoration" applied to all of them; a Client block then
     * overrides it for one application.
     */
    for (i = 0; i < wmNumClientResources; i++)
	if (!strcmp (name, wmClientResources[i].resource_name))
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

static void WarnBadVariable(const char *name, int lineNum)
{
    /* Warning() already prefixes the program name. */
    const char fmt[] = "rc file line %d: \"%s\" is not a usable variable name.";
    size_t len;
    char *msg;

    len = snprintf (NULL, 0, fmt, lineNum, name);
    msg = XtMalloc (len + 1);
    sprintf (msg, fmt, lineNum, name);
    Warning (msg);
    XtFree (msg);
}

/*
 * Exports one entry of the Variables block to the environment.
 *
 * Every command the window manager runs goes through SpawnCommand(), which
 * execs it with "sh -c", so the shell performs the substitution: a variable
 * put here is what "$TERMINAL" in an f.exec string expands to. Nothing has
 * to happen at parse time, which is why this needs no support in the rc
 * parser itself -- and it means "$TERMINAL -e mutt" and "${TERMINAL:-xterm}"
 * work exactly as they would in a shell.
 *
 * Children inherit the environment, so a program mWizard starts -- the tray
 * from trayCommand, anything from f.exec -- sees these too. Programs started
 * from the session file before mWizard does not; those keep their own.
 */
static void PutVariable(const char *name, const char *value, int lineNum)
{
    const char *p;

    /*
     * Refuse anything the shell would not treat as a variable name. Without
     * this a stray line would be exported as an unusable environment entry
     * and the reference in the command would silently expand to nothing.
     */
    if (!isalpha ((unsigned char)name[0]) && name[0] != '_')
    {
	WarnBadVariable (name, lineNum);
	return;
    }

    for (p = name; *p; p++)
    {
	if (!isalnum ((unsigned char)*p) && *p != '_')
	{
	    WarnBadVariable (name, lineNum);
	    return;
	}
    }

    setenv (name, value, 1);
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

/*
 * Records one entry of the Startup block.
 *
 * Unlike every other block here an entry is a whole command line rather than
 * a name and a value, so it is taken as it stands -- quoting it is optional
 * and only matters for leading or trailing spaces.
 */
static void AddStartupCommand(char *cmd)
{
    size_t len;

    len = strlen (cmd);
    if (len >= 2 && cmd[0] == '"' && cmd[len-1] == '"')
    {
	cmd[len-1] = '\0';
	cmd++;
    }
    if (!*cmd) return;

    startupCmds = (char **) XtRealloc ((char *)startupCmds,
				       (numStartupCmds + 1) * sizeof(char *));
    startupCmds[numStartupCmds++] = XtNewString (cmd);
}

/*
 * Runs the Startup block, once the window manager is ready to manage what
 * those commands map.
 *
 * Skipped on a restart. f.restart re-execs the window manager while its
 * clients keep running, so running these again would leave a second copy of
 * everything -- the same reason InitSystemTray() checks the tray selection
 * first. wmRestarted is set from the _MOTIF_WM_INFO property the previous
 * instance left on the root window.
 */
void RunStartupCommands(void)
{
    int i;

    if (numStartupCmds == 0) return;

    /*
     * Say so rather than doing nothing quietly. f.restart is how the rc file
     * gets reloaded, so it is also how someone will test a Startup entry
     * they just added -- and without this they would see no effect and no
     * reason for it.
     */
    if (wmGD.wmRestarted)
    {
	Warning ("Startup block skipped: the window manager was restarted "
		 "rather than started, and its clients are still running. "
		 "Log out and back in to run it.");
	return;
    }

    for (i = 0; i < numStartupCmds; i++)
    {
	if (!SpawnCommand (startupCmds[i]))
	{
	    const char fmt[] = "could not run startup command \"%s\".";
	    size_t len;
	    char *msg;

	    len = snprintf (NULL, 0, fmt, startupCmds[i]);
	    msg = XtMalloc (len + 1);
	    sprintf (msg, fmt, startupCmds[i]);
	    Warning (msg);
	    XtFree (msg);
	}
    }
}

Boolean IsSettingsKeyword(const char *keyword)
{
    return (!strcmp (keyword, SETTINGS_KEYWORD) ||
	    !strcmp (keyword, CLIENT_KEYWORD) ||
	    !strcmp (keyword, WORKSPACE_KEYWORD) ||
	    !strcmp (keyword, VARIABLES_KEYWORD) ||
	    !strcmp (keyword, STARTUP_KEYWORD));
}


/*
 * Scanner state. Brace depth is tracked across the whole file so that a
 * block header is only ever recognized at the outermost level -- otherwise
 * a menu item labelled "Settings" would be mistaken for one.
 */
typedef struct {
    int       depth;		/* current brace nesting depth */
    Boolean   inBlock;		/* inside a block we care about */
    BlockKind kind;		/* which kind, when inBlock */
    char      prefix[RC_LINE_MAX];	/* resource prefix for that block */
    Boolean   pending;		/* a header was seen, awaiting its '{' */
    BlockKind pendingKind;
    char      pendingPrefix[RC_LINE_MAX];
    int       lineNum;
} ScanState;

static void WarnUnknown(const char *name, int lineNum)
{
    /* Warning() already prefixes the program name. */
    const char fmt[] = "rc file line %d: unknown setting \"%s\".";
    size_t len;
    char *msg;

    len = snprintf (NULL, 0, fmt, lineNum, name);
    msg = XtMalloc (len + 1);
    sprintf (msg, fmt, lineNum, name);
    Warning (msg);
    XtFree (msg);
}

/*
 * Handles one stretch of text between braces.
 *
 * At depth 0 it is a possible block header; at depth 1 inside one of our
 * blocks it is a setting. Anything else -- the body of a Menu, Keys or
 * Buttons block -- is not ours and is passed over.
 */
static void HandleSegment(ScanState *st, char *seg)
{
    char *name, *value, *p;
    char  first[RC_LINE_MAX];
    char *rest;
    int   n;

    seg = Trim (seg);
    if (!*seg) return;

    if (st->depth == 0)
    {
	n = 0;
	p = seg;
	while (*p && !isspace ((unsigned char)*p) && n < RC_LINE_MAX-1)
	    first[n++] = tolower ((unsigned char)*p++);
	first[n] = '\0';

	rest = Trim (p);

	if (!strcmp (first, SETTINGS_KEYWORD))
	{
	    st->pending = True;
	    st->pendingKind = BLOCK_SETTINGS;
	    snprintf (st->pendingPrefix, sizeof(st->pendingPrefix), "*");
	}
	else if (!strcmp (first, VARIABLES_KEYWORD))
	{
	    st->pending = True;
	    st->pendingKind = BLOCK_VARIABLES;
	    st->pendingPrefix[0] = '\0';
	}
	else if (!strcmp (first, STARTUP_KEYWORD))
	{
	    st->pending = True;
	    st->pendingKind = BLOCK_STARTUP;
	    st->pendingPrefix[0] = '\0';
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

	    st->pending = True;
	    st->pendingKind = (!strcmp (first, CLIENT_KEYWORD)) ?
				  BLOCK_CLIENT : BLOCK_WORKSPACE;
	    snprintf (st->pendingPrefix, sizeof(st->pendingPrefix),
		      "*%s*", spec);
	}
	else
	{
	    /* Some other top-level construct; not ours. */
	    st->pending = False;
	}
	return;
    }

    if (!st->inBlock || st->depth != 1) return;

    if (st->kind == BLOCK_STARTUP)
    {
	AddStartupCommand (seg);
	return;
    }

    if (!SplitEntry (seg, &name, &value)) return;

    if (st->kind == BLOCK_VARIABLES)
    {
	PutVariable (name, value, st->lineNum);
	return;
    }

    if (!IsKnownName (name, st->kind))
    {
	WarnUnknown (name, st->lineNum);
	return;
    }

    PutSetting (st->prefix, name, value);
}

static void HandleBrace(ScanState *st, char c)
{
    if (c == '{')
    {
	st->depth++;
	if (st->depth == 1 && st->pending)
	{
	    st->inBlock = True;
	    st->kind = st->pendingKind;
	    strcpy (st->prefix, st->pendingPrefix);
	    st->pending = False;
	}
    }
    else
    {
	if (st->depth > 0) st->depth--;
	if (st->depth == 0) st->inBlock = False;
    }
}

void LoadRcSettings(void)
{
    FILE *fp;
    char  buf[RC_LINE_MAX];
    char  seg[RC_LINE_MAX];
    ScanState st;
    char *p;
    int   n;
    Boolean inQuotes;

    if ((fp = FopenConfigFile ()) == NULL) return;

    memset (&st, 0, sizeof(st));

    while (fgets (buf, sizeof(buf), fp) != NULL)
    {
	st.lineNum++;

	if ((p = strchr (buf, '\n')) != NULL) *p = '\0';

	p = buf;
	while (*p && isspace ((unsigned char)*p)) p++;
	if (*p == '!') continue;		/* whole-line comment */

	/*
	 * Split the line at braces, so that a block written as
	 * "Workspace ws0 { title Web }" is handled the same as one spread
	 * over several lines. Braces inside a quoted string -- a menu label,
	 * say -- are literal text and do not count.
	 */
	n = 0;
	inQuotes = False;
	for (; *p; p++)
	{
	    if (*p == '"') inQuotes = !inQuotes;

	    if (!inQuotes && (*p == '{' || *p == '}'))
	    {
		seg[n] = '\0';
		HandleSegment (&st, seg);
		n = 0;
		HandleBrace (&st, *p);
		continue;
	    }

	    if (n < RC_LINE_MAX - 1) seg[n++] = *p;
	}
	seg[n] = '\0';
	HandleSegment (&st, seg);
    }

    fclose (fp);
}
