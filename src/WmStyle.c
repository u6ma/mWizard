/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

/*
 * The style file: appearance for the whole project, in one place.
 *
 * mWizard reads behaviour from its rc file and appearance from the X
 * resource database. That second half was awkward in practice. Appearance
 * lived in an app-defaults file (MWizard, and MWand for the panel), in
 * resource syntax, in a directory found through XFILESEARCHPATH -- two files
 * in a different language from the one the user had just been editing, and
 * silently ineffective if the search path did not happen to include them.
 *
 * So appearance moves to a file of its own, in the syntax the rc files
 * already use:
 *
 *     Fonts
 *     {
 *         font       fixed
 *         titleFont  "Liberation Sans:10:bold"
 *     }
 *
 *     Colors
 *     {
 *         client.background        #8C8C8C
 *         client.activeBackground  #7399BA
 *     }
 *
 * One file, ~/.mstylesrc, read by mWizard and by mWand alike -- a window
 * manager and its panel that do not look like each other are worse than
 * either looking wrong. A block may be qualified with a program name when
 * the two really should differ:
 *
 *     Fonts mwand { panelFont "Liberation Sans:9" }
 *
 * What is written here still ends up in the resource database, because that
 * is what Motif reads and there is no reason to reimplement it. The style
 * file is the interface; the database is the mechanism. Entries go in under
 * the program's instance name, which outranks the class-name form a leftover
 * .Xdefaults would use, so the style file wins the same way the rc file does.
 *
 * mWand keeps its own copy of this, in mwand/src/style.c -- the same
 * arrangement WmSettings.c and mwand/src/config.c already have, and for the
 * same reason: the two programs share no build. The file format is the
 * contract between them, so a change to one belongs in the other.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <X11/Intrinsic.h>
#include <X11/Xresource.h>
#include <Xm/Xm.h>
#include <Xm/XmP.h>		/* for XmeGetHomeDirName */

#include "WmGlobal.h"
#include "WmStyle.h"
#include "WmResource.h"
#include "WmError.h"

#define STYLE_LINE_MAX		(MAXWMPATH + 1)

/* Where the file is looked for, in order. */
#define STYLE_ENV	"MSTYLESRC"
#define STYLE_HOME	"/.mstylesrc"
#define STYLE_SYSTEM	"/system.mstylesrc"

/* Block names. */
#define FONTS_KEYWORD		"fonts"
#define COLORS_KEYWORD		"colors"
#define RESOURCES_KEYWORD	"resources"

/* Which program a block applies to, when it names one. */
#define THIS_PROGRAM		"mwizard"
#define OTHER_PROGRAM		"mwand"

/*
 * Every font role, and where each one is written in the resource database.
 *
 * A role with no entry here is one only the other program draws; it is
 * still accepted, so that a shared Fonts block does not have to be split in
 * two to keep either program quiet.
 *
 * The base font is deliberately the loosest binding there is: one entry that
 * every widget in the process matches, so a style file naming nothing but
 * "font" changes every piece of text mWizard draws. The rest are more
 * specific bindings, and only exist in the database when the style file
 * actually asked for them -- which is what makes them overrides rather than
 * a second set of defaults to keep in step.
 */
typedef struct {
    const char *role;
    const char *bindings[3];	/* NULL-terminated; empty: not drawn here */
} FontRole;

static const FontRole fontRoles[] = {
    { WmStyleFont,		{ "*renderTable", NULL } },
    { WmStyleTitleFont,		{ "*client*title*renderTable", NULL } },
    { WmStyleIconFont,		{ "*icon*renderTable", NULL } },
    { WmStyleFeedbackFont,	{ "*feedback*renderTable", NULL } },
    { WmStyleMenuFont,		{ "*menu*renderTable", NULL } },
    { WmStyleMenuTitleFont,	{ "*menu*menuTitle*renderTable", NULL } },
    { WmStyleDialogFont,	{ "*execDialog*renderTable",
				  "*mwinfo*renderTable", NULL } },
    { WmStylePanelFont,		{ NULL } }
};

#define NUM_FONT_ROLES	((int)XtNumber (fontRoles))

/*
 * The components a Colors entry may be written against, and the binding each
 * one turns into. An entry with no component applies to everything.
 */
typedef struct {
    const char *name;
    const char *binding;	/* NULL: not drawn by this program */
} ColorComponent;

static const ColorComponent colorComponents[] = {
    { "client",		"*client*" },
    { "title",		"*client*title*" },
    { "icon",		"*icon*" },
    { "feedback",	"*feedback*" },
    { "menu",		"*menu*" },
    { "panel",		NULL }
};

#define NUM_COLOR_COMPONENTS	((int)XtNumber (colorComponents))

/* The font spec for each role, as the file wrote it. NULL if unset. */
static char *fontSpecs[NUM_FONT_ROLES];

/* Render tables built from those, on demand. See StyleFont(). */
static XmRenderTable fontTables[NUM_FONT_ROLES];
static Boolean fontTablesTried[NUM_FONT_ROLES];

static char styleFileName[MAXWMPATH + 1];
static Boolean haveStyleFile = False;

static void StyleWarning(const char *fmt, ...);
static int FontRoleIndex(const char *role);


/*
 * Warning() takes one already-formatted string and prefixes the program
 * name, so the message is built here first. Same shape as the helpers in
 * WmSettings.c.
 */
static void StyleWarning(const char *fmt, ...)
{
    va_list ap;
    char *msg;
    int len;

    va_start (ap, fmt);
    len = vsnprintf (NULL, 0, fmt, ap);
    va_end (ap);

    if (len < 0) return;

    msg = XtMalloc (len + 1);

    va_start (ap, fmt);
    vsnprintf (msg, len + 1, fmt, ap);
    va_end (ap);

    Warning (msg);
    XtFree (msg);
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
 * Splits "name value" into its two parts, the same way the rc files do: the
 * value is everything after the first run of whitespace, so an unquoted
 * multi-word value survives, and a fully quoted one has its quotes stripped.
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

static int FontRoleIndex(const char *role)
{
    int i;

    for (i = 0; i < NUM_FONT_ROLES; i++)
	if (!strcmp (role, fontRoles[i].role)) return (i);

    return (-1);
}

/* True if name is one of the component appearance resources. */
static Boolean IsAppearanceName(const char *name)
{
    Cardinal i;

    for (i = 0; i < wmNumAppearanceResources; i++)
	if (!strcmp (name, wmAppearanceResources[i].resource_name))
	    return (True);

    return (False);
}

/*
 * Writes one entry into every screen's resource database, under mWizard's
 * instance name. Identical to what PutSetting() in WmSettings.c does for the
 * rc file, and for the same reason: the instance form outranks the class
 * form that .Xdefaults carries, so the style file cannot be overruled by a
 * leftover resource.
 */
static void PutStyle(const char *binding, const char *name, const char *value)
{
    char key[STYLE_LINE_MAX];
    XrmDatabase db;
    int scr;

    snprintf (key, sizeof(key), "%s%s%s", WM_RESOURCE_NAME, binding, name);

    for (scr = 0; scr < ScreenCount (DISPLAY); scr++)
    {
	db = XtScreenDatabase (ScreenOfDisplay (DISPLAY, scr));
	if (db) XrmPutStringResource (&db, key, (char *)value);
    }
}


/*
 * ---------------------------------------------------------------------------
 * Font specs.
 *
 * A spec is either a core X font -- an XLFD, or an alias such as "fixed" --
 * or an Xft font written the way fontconfig writes one:
 *
 *     fixed                                     core, and the default
 *     -*-helvetica-bold-r-normal--12-*-*-*-*-*  core
 *     Liberation Sans:10                        Xft, 10 point
 *     Liberation Sans:10:bold                   Xft, 10 point bold
 *
 * The colon is what tells them apart, which is unambiguous because an XLFD
 * cannot contain one. A core font name is passed through untouched, so
 * anything xlsfonts(1) lists can be named; an Xft spec is split into the
 * family, size and style that Motif's rendition resources want.
 * ---------------------------------------------------------------------------
 */

/*
 * Splits an Xft spec. family and style point into buf, which must hold a
 * copy of the spec; style is NULL when the spec did not name one.
 */
static void SplitXftSpec(char *buf, char **family, int *size, char **style)
{
    char *p;

    *family = buf;
    *size = 0;
    *style = NULL;

    if ((p = strchr (buf, ':')) == NULL) return;

    *p++ = '\0';
    *family = Trim (*family);

    {
	char *q = strchr (p, ':');

	if (q)
	{
	    *q++ = '\0';
	    q = Trim (q);
	    if (*q) *style = q;
	}
    }

    p = Trim (p);
    if (*p) *size = atoi (p);
}

/*
 * Writes the rendition resources for one font spec under the given tag, so
 * that Motif's own string-to-render-table converter builds it. This is the
 * path every widget that takes its font from the database goes through.
 */
static void PutFontRendition(const char *tag, const char *spec)
{
    char binding[STYLE_LINE_MAX];
    char buf[STYLE_LINE_MAX];
    char num[32];
    char *family, *style;
    int size;

    snprintf (binding, sizeof(binding), "*renderTable.%s.", tag);

    if (strchr (spec, ':') == NULL)
    {
	PutStyle (binding, "fontType", "FONT_IS_FONT");
	PutStyle (binding, "fontName", spec);
	return;
    }

    snprintf (buf, sizeof(buf), "%s", spec);
    SplitXftSpec (buf, &family, &size, &style);

    PutStyle (binding, "fontType", "FONT_IS_XFT");
    PutStyle (binding, "fontName", family);

    snprintf (num, sizeof(num), "%d", size > 0 ? size : 10);
    PutStyle (binding, "fontSize", num);

    if (style) PutStyle (binding, "fontStyle", style);
}

/*
 * Builds a render table for one spec.
 *
 * Only the widgets whose font the database cannot be trusted to reach need
 * this -- see StyleFont(). Everything else is served by PutFontRendition()
 * and never gets here.
 *
 * The rendition is tagged XmFONTLIST_DEFAULT_TAG rather than with the role's
 * name, because a string made by XmStringCreateLocalized() carries that tag
 * and would otherwise have to be matched by Motif's fallback rather than
 * outright.
 */
static XmRenderTable MakeRenderTable(const char *spec)
{
    Arg args[6];
    Cardinal n = 0;
    XmRendition rendition;
    char buf[STYLE_LINE_MAX];
    char *family, *style;
    int size;

    if (!wmGD.topLevelW) return (NULL);

    XtSetArg (args[n], XmNfont, XmAS_IS); n++;

    if (strchr (spec, ':') == NULL)
    {
	XtSetArg (args[n], XmNfontType, XmFONT_IS_FONT); n++;
	XtSetArg (args[n], XmNfontName, spec); n++;
    }
    else
    {
	snprintf (buf, sizeof(buf), "%s", spec);
	SplitXftSpec (buf, &family, &size, &style);

	XtSetArg (args[n], XmNfontType, XmFONT_IS_XFT); n++;
	XtSetArg (args[n], XmNfontName, family); n++;
	XtSetArg (args[n], XmNfontSize, size > 0 ? size : 10); n++;
	if (style) { XtSetArg (args[n], XmNfontStyle, style); n++; }
    }

    rendition = XmRenditionCreate (wmGD.topLevelW,
				   XmFONTLIST_DEFAULT_TAG, args, n);
    if (!rendition) return (NULL);

    return (XmRenderTableAddRenditions (NULL, &rendition, 1, XmMERGE_NEW));
}

XmRenderTable StyleFont(const char *role)
{
    int i = FontRoleIndex (role);
    int base;

    if (i < 0) return (NULL);

    if (!fontSpecs[i])
    {
	/* Not named in the style file: this role is the base font. */
	base = FontRoleIndex (WmStyleFont);
	if (base < 0 || base == i || !fontSpecs[base]) return (NULL);
	i = base;
    }

    if (!fontTablesTried[i])
    {
	fontTablesTried[i] = True;
	fontTables[i] = MakeRenderTable (fontSpecs[i]);

	if (!fontTables[i])
	    StyleWarning ("style file: could not make the font \"%s\".",
			  fontSpecs[i]);
    }

    return (fontTables[i]);
}

const char *StyleFileName(void)
{
    return (haveStyleFile ? styleFileName : NULL);
}


/*
 * ---------------------------------------------------------------------------
 * The scanner. Same shape as LoadRcSettings() in WmSettings.c: brace depth is
 * tracked across the whole file so a block header is only recognized at the
 * outermost level, and braces inside a quoted string are literal text.
 * ---------------------------------------------------------------------------
 */

typedef enum {
    BLOCK_NONE,
    BLOCK_FONTS,
    BLOCK_COLORS,
    BLOCK_RESOURCES
} BlockKind;

typedef struct {
    int       depth;
    Boolean   inBlock;
    Boolean   ours;		/* the block names us, or names nobody */
    BlockKind kind;
    Boolean   pending;		/* a header was seen, awaiting its '{' */
    Boolean   pendingOurs;
    BlockKind pendingKind;
    int       lineNum;
} ScanState;

/*
 * One entry of a Fonts block. The spec is kept rather than acted on at once,
 * because the rendition resources are written per role and a later entry for
 * the same role must replace an earlier one rather than add to it.
 */
static void HandleFontEntry(ScanState *st, const char *name, const char *value)
{
    int i = FontRoleIndex (name);

    if (i < 0)
    {
	StyleWarning ("style file line %d: unknown font \"%s\".",
		      st->lineNum, name);
	return;
    }

    XtFree (fontSpecs[i]);
    fontSpecs[i] = XtNewString (value);
}

/*
 * One entry of a Colors block: an appearance resource, optionally written
 * against one component as "icon.activeBackground".
 */
static void HandleColorEntry(ScanState *st, char *name, const char *value)
{
    const char *binding = "*";
    char *dot;
    int i;

    if ((dot = strchr (name, '.')) != NULL)
    {
	*dot = '\0';

	for (i = 0; i < NUM_COLOR_COMPONENTS; i++)
	    if (!strcmp (name, colorComponents[i].name)) break;

	if (i == NUM_COLOR_COMPONENTS)
	{
	    StyleWarning ("style file line %d: unknown component \"%s\".",
			  st->lineNum, name);
	    return;
	}

	/* A component only the other program draws. Not an error. */
	if (!colorComponents[i].binding) return;

	binding = colorComponents[i].binding;
	name = dot + 1;
    }

    if (!IsAppearanceName (name))
    {
	StyleWarning ("style file line %d: unknown color \"%s\".",
		      st->lineNum, name);
	return;
    }

    PutStyle (binding, name, value);
}

/*
 * Handles one stretch of text between braces: at depth 0 a possible block
 * header, at depth 1 inside one of our blocks an entry, and anything else
 * not ours.
 */
static void HandleSegment(ScanState *st, char *seg)
{
    char  first[STYLE_LINE_MAX];
    char  who[STYLE_LINE_MAX];
    char *name, *value, *p, *rest;
    int   n;

    seg = Trim (seg);
    if (!*seg) return;

    if (st->depth == 0)
    {
	n = 0;
	p = seg;
	while (*p && !isspace ((unsigned char)*p) && n < STYLE_LINE_MAX-1)
	    first[n++] = tolower ((unsigned char)*p++);
	first[n] = '\0';

	if (!strcmp (first, FONTS_KEYWORD))
	    st->pendingKind = BLOCK_FONTS;
	else if (!strcmp (first, COLORS_KEYWORD))
	    st->pendingKind = BLOCK_COLORS;
	else if (!strcmp (first, RESOURCES_KEYWORD))
	    st->pendingKind = BLOCK_RESOURCES;
	else
	{
	    st->pending = False;
	    return;
	}

	/*
	 * An unqualified block is for both programs; one that names a
	 * program is for that one alone.
	 */
	rest = Trim (p);
	n = 0;
	while (*rest && !isspace ((unsigned char)*rest) && n < STYLE_LINE_MAX-1)
	    who[n++] = tolower ((unsigned char)*rest++);
	who[n] = '\0';

	if (!*who || !strcmp (who, THIS_PROGRAM))
	{
	    st->pendingOurs = True;
	}
	else if (!strcmp (who, OTHER_PROGRAM))
	{
	    st->pendingOurs = False;
	}
	else
	{
	    StyleWarning ("style file line %d: \"%s\" is not a program name.",
			  st->lineNum, who);
	    st->pendingOurs = False;
	}

	st->pending = True;
	return;
    }

    if (!st->inBlock || !st->ours || st->depth != 1) return;

    if (!SplitEntry (seg, &name, &value)) return;

    switch (st->kind)
    {
	case BLOCK_FONTS:
	    HandleFontEntry (st, name, value);
	    break;

	case BLOCK_COLORS:
	    HandleColorEntry (st, name, value);
	    break;

	case BLOCK_RESOURCES:
	    /*
	     * The escape hatch: written through untouched, so that a Motif
	     * resource with no place in the vocabulary above -- a margin, a
	     * shadow thickness -- can still be set from this file rather than
	     * sending the user back to .Xdefaults. Not validated, because
	     * there is no list to validate it against.
	     */
	    PutStyle ("*", name, value);
	    break;

	default:
	    break;
    }
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
	    st->ours = st->pendingOurs;
	    st->pending = False;
	}
    }
    else
    {
	if (st->depth > 0) st->depth--;
	if (st->depth == 0)
	{
	    st->inBlock = False;
	    st->kind = BLOCK_NONE;
	}
    }
}

/*
 * Finds the style file: the MSTYLESRC environment variable, then the user's
 * own, then the one installed system wide.
 *
 * An environment variable rather than an X resource, because both programs
 * have to agree on the answer and only one of them has a resource database
 * worth consulting this early. It is also what makes a second style usable
 * without editing anything: MSTYLESRC=~/dark.mstylesrc mwizard.
 */
static FILE *FopenStyleFile(void)
{
    const char *env;
    char *homeDir;
    FILE *fp;

    if ((env = getenv (STYLE_ENV)) != NULL && *env)
    {
	snprintf (styleFileName, sizeof(styleFileName), "%s", env);
	if ((fp = fopen (styleFileName, "r")) != NULL) return (fp);

	StyleWarning ("cannot open the style file named by $%s (\"%s\").",
		      STYLE_ENV, styleFileName);
	return (NULL);
    }

    if ((homeDir = XmeGetHomeDirName ()) != NULL)
    {
	snprintf (styleFileName, sizeof(styleFileName), "%s%s",
		  homeDir, STYLE_HOME);
	if ((fp = fopen (styleFileName, "r")) != NULL) return (fp);
    }

    snprintf (styleFileName, sizeof(styleFileName), "%s%s",
	      RCDIR, STYLE_SYSTEM);
    if ((fp = fopen (styleFileName, "r")) != NULL) return (fp);

    styleFileName[0] = '\0';
    return (NULL);
}

void LoadStyleFile(void)
{
    FILE *fp;
    char  buf[STYLE_LINE_MAX];
    char  seg[STYLE_LINE_MAX];
    ScanState st;
    char *p;
    int   i, n;
    Boolean inQuotes;

    /*
     * The base font is Motif's own, which is what mWizard looked like before
     * any of this existed and is on every X server there is. It is set here
     * rather than left to Motif so that "font" always has a value to fall
     * back to, and so that a style file naming only one other role still
     * gets a consistent look.
     */
    i = FontRoleIndex (WmStyleFont);
    fontSpecs[i] = XtNewString (XmDEFAULT_FONT);

    if ((fp = FopenStyleFile ()) != NULL)
    {
	haveStyleFile = True;

	memset (&st, 0, sizeof(st));

	while (fgets (buf, sizeof(buf), fp) != NULL)
	{
	    st.lineNum++;

	    if ((p = strchr (buf, '\n')) != NULL) *p = '\0';

	    p = buf;
	    while (*p && isspace ((unsigned char)*p)) p++;
	    if (*p == '!' || *p == '#') continue;

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

		if (n < STYLE_LINE_MAX - 1) seg[n++] = *p;
	    }

	    seg[n] = '\0';
	    HandleSegment (&st, seg);
	}

	fclose (fp);
    }

    /*
     * Now the fonts, once the whole file has been read: a role written twice
     * has to end up with the last value rather than both.
     */
    for (i = 0; i < NUM_FONT_ROLES; i++)
    {
	int b;

	if (!fontSpecs[i] || !fontRoles[i].bindings[0]) continue;

	PutFontRendition (fontRoles[i].role, fontSpecs[i]);

	for (b = 0; fontRoles[i].bindings[b]; b++)
	    PutStyle (fontRoles[i].bindings[b], "", fontRoles[i].role);
    }
}
