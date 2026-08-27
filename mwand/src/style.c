/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

/*
 * The style file: appearance for the whole project, in one place.
 *
 * mWand used to keep its fonts in an app-defaults file of its own (MWand),
 * separately from the window manager's (MWizard) -- two files in resource
 * syntax, found through XFILESEARCHPATH, describing one desktop that is
 * meant to look like a single thing. A panel whose menus are not the window
 * manager's menus is a panel that looks bolted on.
 *
 * So both programs read ~/.mstylesrc instead, in the syntax the rc files
 * already use:
 *
 *     Fonts
 *     {
 *         font       fixed
 *         panelFont  "Liberation Sans:9"
 *     }
 *
 * A block with no program name is for both; one written "Fonts mwand" is for
 * this program alone.
 *
 * What is read here still ends up in the resource database, under mWand's
 * instance name, so the resource tables and converters keep doing the work
 * and the style file outranks anything left in .Xdefaults -- the same
 * arrangement config.c has for the rc file.
 *
 * mWizard has the other copy of this, in src/WmStyle.c. The two programs
 * share no build, so the file format is the contract between them: a change
 * to one belongs in the other. config.c and src/WmSettings.c stand in the
 * same relation.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <X11/Intrinsic.h>
#include <X11/Xresource.h>
#include <Xm/Xm.h>
#include "common.h"
#include "mwand.h"
#include "style.h"

#define STYLE_LINE_MAX 1024
#define STYLE_PATH_MAX 1024

/* Where the file is looked for, in order. */
#define STYLE_ENV	"MSTYLESRC"
#define STYLE_NAME	"mstylesrc"
#define STYLE_HOME	"/." STYLE_NAME
#define STYLE_SYSTEM	"/system." STYLE_NAME

/*
 * What to fall back to when a core font named in the style file is not on
 * the server. The first is a fully wildcarded XLFD, which matches whatever
 * core font the server does have; the second is for a server that has none
 * at all, which is no longer unusual -- a modern Xorg install without the
 * legacy bitmap font packages has exactly zero, "fixed" included.
 */
#define STYLE_ANY_CORE_FONT	"-*-*-*-*-*-*-*-*-*-*-*-*-*-*"
#define STYLE_LAST_RESORT_FONT	"Sans:10"

#define FONTS_KEYWORD		"fonts"
#define COLORS_KEYWORD		"colors"
#define RESOURCES_KEYWORD	"resources"

#define THIS_PROGRAM	APP_NAME
#define OTHER_PROGRAM	"mwizard"

/*
 * Every font role, and where each one is written in the resource database.
 *
 * A role with no binding is one only mWizard draws; naming it here is not an
 * error, it simply does nothing. The base font is the loosest binding there
 * is, so a style file that sets only "font" changes every piece of text
 * mWand draws.
 *
 * Menu items and menu titles are addressed by widget class rather than by
 * name because mWand builds three kinds of menu -- the launcher's, the
 * session menu's, and whatever the rc file nests inside them -- and they
 * agree on the class of their entries, not on names.
 */
struct font_role {
	const char *role;
	const char *bindings[3];	/* NULL-terminated */
};

static const struct font_role font_roles[] = {
	{ StyleFontBase,	{ "*renderTable", NULL } },
	{ StyleFontMenu,	{ "*XmPushButtonGadget.renderTable", NULL } },
	{ StyleFontMenuTitle,	{ "*XmCascadeButtonGadget.renderTable", NULL } },
	{ StyleFontPanel,	{ "*dateTime.renderTable",
				  "*userHost.renderTable", NULL } },
	{ StyleFontDialog,	{ "*messageDialog*renderTable", NULL } },
	{ StyleFontTitle,	{ NULL } },
	{ StyleFontIcon,	{ NULL } },
	{ StyleFontFeedback,	{ NULL } }
};

#define NUM_FONT_ROLES ((int)XtNumber(font_roles))

/*
 * The components a Colors entry may name. mWand is one panel, so everything
 * it draws is "panel"; the rest are mWizard's and are skipped rather than
 * reported, since the block may well be shared.
 */
struct color_component {
	const char *name;
	const char *binding;	/* NULL: not drawn by this program */
};

static const struct color_component color_components[] = {
	{ "panel",	"*" },
	{ "client",	NULL },
	{ "title",	NULL },
	{ "icon",	NULL },
	{ "feedback",	NULL },
	{ "menu",	"*menu*" }
};

#define NUM_COLOR_COMPONENTS ((int)XtNumber(color_components))

/*
 * The appearance resources a Colors entry may set. mWizard validates against
 * its own component appearance table; mWand has no such table, so this is
 * the list, and it is the same one.
 */
static const char *color_names[] = {
	"background", "foreground",
	"topShadowColor", "bottomShadowColor",
	"backgroundPixmap", "topShadowPixmap", "bottomShadowPixmap",
	"activeBackground", "activeForeground",
	"activeTopShadowColor", "activeBottomShadowColor",
	"activeBackgroundPixmap", "activeTopShadowPixmap",
	"activeBottomShadowPixmap"
};

/*
 * The rendition tag each role ended up bound to: the role's own name
 * normally, a fallback tag when the font it asked for was unusable, NULL
 * when nothing usable could be made. See LoadStyleFile().
 */
static const char *font_tags[NUM_FONT_ROLES];

static char *font_specs[NUM_FONT_ROLES];
static XmRenderTable font_tables[NUM_FONT_ROLES];
static Boolean font_tables_tried[NUM_FONT_ROLES];

static char style_file[STYLE_PATH_MAX];
static XrmDatabase style_db;
static Display *style_dpy;
static Widget style_widget;

/*
 * Motif's own, and the same one mWizard declares in src/WmXmP.h: it reports
 * zero height for a render table whose font never loaded, which is the only
 * check here that does not depend on guessing why one failed.
 */
void XmRenderTableGetDefaultFontExtents(XmRenderTable,
	int *height, int *ascent, int *descent);

static int FontRoleIndex(const char *role);

/* Same shape as the messages config.c prints, including the file and line. */
static void StyleWarning(int line_num, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: %s line %d: ", APP_NAME, style_file, line_num);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fputc('\n', stderr);
}

static char *Trim(char *s)
{
	char *end;

	while(*s && isspace((unsigned char)*s)) s++;
	if(!*s) return s;

	end = s + strlen(s) - 1;
	while(end > s && isspace((unsigned char)*end)) *end-- = '\0';

	return s;
}

/*
 * Splits "name value", the same way the rc file does: the value is
 * everything after the first run of whitespace, and a fully quoted value has
 * its quotes stripped.
 */
static Boolean SplitEntry(char *line, char **namep, char **valuep)
{
	char *p, *value;
	size_t len;

	p = line;
	while(*p && !isspace((unsigned char)*p)) p++;
	if(!*p) return False;

	*p++ = '\0';
	value = Trim(p);
	if(!*value) return False;

	len = strlen(value);
	if(len >= 2 && value[0] == '"' && value[len-1] == '"') {
		value[len-1] = '\0';
		value++;
	}

	*namep = line;
	*valuep = value;

	return True;
}

static int FontRoleIndex(const char *role)
{
	int i;

	for(i = 0; i < NUM_FONT_ROLES; i++)
		if(!strcmp(role, font_roles[i].role)) return i;

	return -1;
}

static Boolean IsColorName(const char *name)
{
	size_t i;

	for(i = 0; i < XtNumber(color_names); i++)
		if(!strcmp(name, color_names[i])) return True;

	return False;
}

/* Writes one entry into the screen database, under mWand's instance name. */
static void PutStyle(const char *binding, const char *name, const char *value)
{
	char key[STYLE_LINE_MAX];

	if(!style_db) return;

	snprintf(key, sizeof(key), "%s%s%s", APP_NAME, binding, name);
	XrmPutStringResource(&style_db, key, (char*)value);
}

/*
 * ---------------------------------------------------------------------------
 * Font specs. A spec is either a core X font -- an XLFD, or an alias such as
 * "fixed" -- or an Xft font written family:size or family:size:style. The
 * colon is what tells them apart, which is unambiguous because an XLFD
 * cannot contain one.
 * ---------------------------------------------------------------------------
 */

/*
 * Splits an Xft spec. family and style point into buf, which must hold a
 * copy of the spec; style is NULL when the spec did not name one.
 */
static void SplitXftSpec(char *buf, char **family, int *size, char **style)
{
	char *p, *q;

	*family = buf;
	*size = 0;
	*style = NULL;

	if(!(p = strchr(buf, ':'))) return;

	*p++ = '\0';
	*family = Trim(*family);

	if((q = strchr(p, ':'))) {
		*q++ = '\0';
		q = Trim(q);
		if(*q) *style = q;
	}

	p = Trim(p);
	if(*p) *size = atoi(p);
}

/*
 * Answers with a font spec that will actually load.
 *
 * Motif does not check. Name a core font the server does not have and it
 * carries the failure all the way to the first XSetFont, where it surfaces
 * as a BadFont from the server and not as anything mentioning a font name.
 *
 * That matters for the default: "fixed" is Motif's own font and was on every
 * X server for twenty years, but an Xorg install without the legacy bitmap
 * font packages has no core fonts at all, and fontconfig will not answer for
 * it either.
 *
 * Xft specs are left alone -- fontconfig always answers with something.
 */
static const char *UsableFontSpec(const char *spec)
{
	XFontStruct *fs;

	if(strchr(spec, ':')) return spec;

	if((fs = XLoadQueryFont(style_dpy, spec))) {
		XFreeFont(style_dpy, fs);
		return spec;
	}

	if((fs = XLoadQueryFont(style_dpy, STYLE_ANY_CORE_FONT))) {
		XFreeFont(style_dpy, fs);
		fprintf(stderr, "%s: no core font matches \"%s\"; using another "
			"the server does have. Try an Xft font instead, written "
			"family:size.\n", APP_NAME, spec);
		return STYLE_ANY_CORE_FONT;
	}

	fprintf(stderr, "%s: no core font matches \"%s\", and this server has "
		"no core fonts at all; using \"%s\". Name Xft fonts in the "
		"style file, written family:size.\n",
		APP_NAME, spec, STYLE_LAST_RESORT_FONT);

	return STYLE_LAST_RESORT_FONT;
}

/*
 * Writes the rendition resources for one spec under the given tag, so that
 * Motif's own converter builds the render table. This is the path every
 * widget that takes its font from the database goes through.
 */
static void PutFontRendition(const char *tag, const char *spec)
{
	char binding[STYLE_LINE_MAX];
	char buf[STYLE_LINE_MAX];
	char num[32];
	char *family, *style;
	int size;

	snprintf(binding, sizeof(binding), "*renderTable.%s.", tag);

	if(!strchr(spec, ':')) {
		PutStyle(binding, "fontType", "FONT_IS_FONT");
		PutStyle(binding, "fontName", spec);
		return;
	}

	snprintf(buf, sizeof(buf), "%s", spec);
	SplitXftSpec(buf, &family, &size, &style);

	PutStyle(binding, "fontType", "FONT_IS_XFT");
	PutStyle(binding, "fontName", family);

	snprintf(num, sizeof(num), "%d", size > 0 ? size : 10);
	PutStyle(binding, "fontSize", num);

	if(style) PutStyle(binding, "fontStyle", style);
}

/*
 * The render table Motif itself would build for one rendition tag.
 *
 * Not built here by hand. An earlier version of this did assemble the
 * rendition with XmRenditionCreate(), passing XmNfont as XmAS_IS the way
 * mWizard's own fallback does -- and XmAS_IS is the integer 255, which Motif
 * kept and which is a perfectly well-formed Font id naming no font. The
 * first string drawn with it died on a BadFont from the server, which is
 * what stopped mWand starting at all in 1.2 before this.
 *
 * Going through the string-to-render-table converter instead means the
 * widgets that need a render table in hand and the widgets that get theirs
 * from the resource database are served by one code path -- the one that was
 * already working for every app-defaults file that ever set a Motif font.
 * Xt caches the conversion, which is also why nothing here caches.
 */
static XmRenderTable RenderTableForTag(Widget w, const char *tag)
{
	XrmValue from, to;
	XmRenderTable rt = NULL;

	from.addr = (XPointer)tag;
	from.size = strlen(tag) + 1;
	to.addr = (XPointer)&rt;
	to.size = sizeof(rt);

	if(!XtConvertAndStore(w, XmRString, &from, XmRRenderTable, &to))
		return NULL;

	return rt;
}

/*
 * Whether Motif can build a render table with a font in it from this tag.
 *
 * A rendition whose font never loaded keeps XmNfont at its default, XmAS_IS,
 * which is the integer 255 -- and Motif hands that to XChangeGC as a Font id,
 * which is a BadFont naming nothing and the end of the process. Zero font
 * height is what such a table reports.
 */
static Boolean TagIsUsable(const char *tag)
{
	XmRenderTable rt;
	int height = 0;

	if(!style_widget) return True;	/* nothing to check with; let it be */

	rt = RenderTableForTag(style_widget, tag);
	if(rt) XmRenderTableGetDefaultFontExtents(rt, &height, NULL, NULL);

	return (height > 0) ? True : False;
}

XmRenderTable StyleFont(Widget w, const char *role)
{
	int i = FontRoleIndex(role);
	int base;

	if(i < 0 || !w) return NULL;

	if(!font_tags[i]) {
		/* Not named in the style file: this role is the base font. */
		base = FontRoleIndex(StyleFontBase);
		if(base < 0 || base == i) return NULL;
		i = base;
	}

	if(!font_tags[i]) return NULL;

	if(!font_tables_tried[i]) {
		font_tables_tried[i] = True;
		font_tables[i] = RenderTableForTag(w, font_tags[i]);
	}

	return font_tables[i];
}

/*
 * ---------------------------------------------------------------------------
 * The scanner. Same shape as LoadRcSettings() in config.c: brace depth is
 * tracked across the whole file so a block header is only recognized at the
 * outermost level, and braces inside a quoted string are literal text.
 * ---------------------------------------------------------------------------
 */

enum block_kind {
	BLOCK_NONE,
	BLOCK_FONTS,
	BLOCK_COLORS,
	BLOCK_RESOURCES
};

struct scan_state {
	int depth;
	int line_num;
	Boolean in_block;
	Boolean ours;		/* the block names us, or names nobody */
	enum block_kind kind;
	Boolean pending;
	Boolean pending_ours;
	enum block_kind pending_kind;
};

static void HandleFontEntry(struct scan_state *st, const char *name,
	const char *value)
{
	int i = FontRoleIndex(name);

	if(i < 0) {
		StyleWarning(st->line_num, "unknown font \"%s\".", name);
		return;
	}

	free(font_specs[i]);
	font_specs[i] = strdup(value);
}

static void HandleColorEntry(struct scan_state *st, char *name,
	const char *value)
{
	const char *binding = "*";
	char *dot;
	int i;

	if((dot = strchr(name, '.'))) {
		*dot = '\0';

		for(i = 0; i < NUM_COLOR_COMPONENTS; i++)
			if(!strcmp(name, color_components[i].name)) break;

		if(i == NUM_COLOR_COMPONENTS) {
			StyleWarning(st->line_num,
				"unknown component \"%s\".", name);
			return;
		}

		/* A component only mWizard draws. Not an error. */
		if(!color_components[i].binding) return;

		binding = color_components[i].binding;
		name = dot + 1;
	}

	if(!IsColorName(name)) {
		StyleWarning(st->line_num, "unknown color \"%s\".", name);
		return;
	}

	PutStyle(binding, name, value);
}

static void HandleSegment(struct scan_state *st, char *seg)
{
	char first[STYLE_LINE_MAX];
	char who[STYLE_LINE_MAX];
	char *name, *value, *p, *rest;
	int n;

	seg = Trim(seg);
	if(!*seg) return;

	if(st->depth == 0) {
		n = 0;
		p = seg;
		while(*p && !isspace((unsigned char)*p) && n < STYLE_LINE_MAX-1)
			first[n++] = tolower((unsigned char)*p++);
		first[n] = '\0';

		if(!strcmp(first, FONTS_KEYWORD))
			st->pending_kind = BLOCK_FONTS;
		else if(!strcmp(first, COLORS_KEYWORD))
			st->pending_kind = BLOCK_COLORS;
		else if(!strcmp(first, RESOURCES_KEYWORD))
			st->pending_kind = BLOCK_RESOURCES;
		else {
			st->pending = False;
			return;
		}

		rest = Trim(p);
		n = 0;
		while(*rest && !isspace((unsigned char)*rest) &&
			n < STYLE_LINE_MAX-1)
			who[n++] = tolower((unsigned char)*rest++);
		who[n] = '\0';

		if(!*who || !strcmp(who, THIS_PROGRAM)) {
			st->pending_ours = True;
		} else if(!strcmp(who, OTHER_PROGRAM)) {
			st->pending_ours = False;
		} else {
			StyleWarning(st->line_num,
				"\"%s\" is not a program name.", who);
			st->pending_ours = False;
		}

		st->pending = True;
		return;
	}

	if(!st->in_block || !st->ours || st->depth != 1) return;

	if(!SplitEntry(seg, &name, &value)) return;

	switch(st->kind) {
		case BLOCK_FONTS:
		HandleFontEntry(st, name, value);
		break;

		case BLOCK_COLORS:
		HandleColorEntry(st, name, value);
		break;

		case BLOCK_RESOURCES:
		/*
		 * The escape hatch: written through untouched, for a Motif
		 * resource that is appearance but is neither a font nor a
		 * color. Not validated, because there is no list to validate
		 * it against.
		 */
		PutStyle("*", name, value);
		break;

		default:
		break;
	}
}

static void HandleBrace(struct scan_state *st, char c)
{
	if(c == '{') {
		st->depth++;
		if(st->depth == 1 && st->pending) {
			st->in_block = True;
			st->kind = st->pending_kind;
			st->ours = st->pending_ours;
			st->pending = False;
		}
	} else {
		if(st->depth > 0) st->depth--;
		if(st->depth == 0) {
			st->in_block = False;
			st->kind = BLOCK_NONE;
		}
	}
}

/*
 * Finds the style file: the MSTYLESRC environment variable, then the user's
 * own, then the one installed system wide.
 *
 * An environment variable rather than a resource or an rc setting, because
 * mWizard has to arrive at the same answer and the two do not share a
 * configuration. It also makes a second style usable without editing
 * anything: MSTYLESRC=~/dark.mstylesrc mwand.
 */
static FILE* FopenStyleFile(void)
{
	const char *env;
	const char *home;
	FILE *fp;

	if((env = getenv(STYLE_ENV)) && *env) {
		snprintf(style_file, sizeof(style_file), "%s", env);
		if((fp = fopen(style_file, "r"))) return fp;

		fprintf(stderr, "%s: cannot open the style file named by "
			"$%s (\"%s\").\n", APP_NAME, STYLE_ENV, style_file);
		return NULL;
	}

	if((home = getenv("HOME"))) {
		snprintf(style_file, sizeof(style_file), "%s%s",
			home, STYLE_HOME);
		if((fp = fopen(style_file, "r"))) return fp;
	}

	snprintf(style_file, sizeof(style_file), "%s%s", RCDIR, STYLE_SYSTEM);
	if((fp = fopen(style_file, "r"))) return fp;

	style_file[0] = '\0';
	return NULL;
}

void LoadStyleFile(Widget w)
{
	Display *dpy = XtDisplay(w);
	FILE *fp;
	char buf[STYLE_LINE_MAX];
	char seg[STYLE_LINE_MAX];
	struct scan_state st;
	char *p;
	int i, n;
	Boolean in_quotes;

	style_dpy = dpy;
	style_widget = w;
	style_db = XtScreenDatabase(DefaultScreenOfDisplay(dpy));

	/*
	 * The base font is Motif's own, which is what both programs drew
	 * their menus in before any of this existed and is on every X server
	 * there is. Set here rather than left to Motif so that "font" always
	 * has a value for the other roles to fall back to.
	 */
	i = FontRoleIndex(StyleFontBase);
	font_specs[i] = strdup(XmDEFAULT_FONT);

	if((fp = FopenStyleFile())) {
		memset(&st, 0, sizeof(st));

		while(fgets(buf, sizeof(buf), fp) != NULL) {
			st.line_num++;

			if((p = strchr(buf, '\n')) != NULL) *p = '\0';

			p = buf;
			while(*p && isspace((unsigned char)*p)) p++;
			if(*p == '!' || *p == '#') continue;

			n = 0;
			in_quotes = False;

			for(; *p; p++) {
				if(*p == '"')
					in_quotes = in_quotes ? False : True;

				if(!in_quotes && (*p == '{' || *p == '}')) {
					seg[n] = '\0';
					HandleSegment(&st, seg);
					n = 0;
					HandleBrace(&st, *p);
					continue;
				}

				if(n < STYLE_LINE_MAX - 1) seg[n++] = *p;
			}

			seg[n] = '\0';
			HandleSegment(&st, seg);
		}

		fclose(fp);
	}

	/*
	 * Now the fonts, once the whole file has been read: a role written
	 * twice has to end up with the last value rather than both.
	 */
	for(i = 0; i < NUM_FONT_ROLES; i++) {
		char alt_tag[64];
		const char *usable;
		const char *tag;
		int b;

		if(!font_specs[i] || !font_roles[i].bindings[0]) continue;

		usable = UsableFontSpec(font_specs[i]);
		if(usable != font_specs[i]) {
			free(font_specs[i]);
			font_specs[i] = strdup(usable);
		}

		tag = font_roles[i].role;
		PutFontRendition(tag, font_specs[i]);

		/*
		 * Ask Motif to build it before anything is bound to it. A font
		 * that cannot be made is not a cosmetic problem: the first
		 * string drawn with it ends the process on a BadFont, and the
		 * X error names no font. Better one nobody asked for.
		 */
		if(!TagIsUsable(tag)) {
			/*
			 * Under a second tag rather than by rewriting the
			 * first, because Xt may hand back the conversion it
			 * has already made for a tag it has seen.
			 */
			snprintf(alt_tag, sizeof(alt_tag), "%sFallback",
				font_roles[i].role);
			PutFontRendition(alt_tag, STYLE_LAST_RESORT_FONT);

			if(!TagIsUsable(alt_tag)) {
				fprintf(stderr, "%s: neither \"%s\" nor \"%s\" "
					"could be made into a usable font; "
					"leaving %s to Motif.\n", APP_NAME,
					font_specs[i], STYLE_LAST_RESORT_FONT,
					font_roles[i].role);
				continue;	/* bind nothing */
			}

			fprintf(stderr, "%s: \"%s\" could not be made into a "
				"usable font; using \"%s\" for %s.\n", APP_NAME,
				font_specs[i], STYLE_LAST_RESORT_FONT,
				font_roles[i].role);

			/* alt_tag is this iteration's stack; font_tags keeps it */
			tag = strdup(alt_tag);
		}

		font_tags[i] = tag;

		for(b = 0; font_roles[i].bindings[b]; b++)
			PutStyle(font_roles[i].bindings[b], "", tag);
	}
}
