/*
 * This file is distributed under the terms of the MIT license.
 * See the included COPYING.MIT file for further information.
 */

/*
 * mWand settings, read from the rc file.
 *
 * xmtoolbox split its configuration in two: menus in toolboxrc, and every
 * behaviour knob in the X resource database as XmToolbox*name. mWand reads
 * behaviour from the rc file too, in a Settings block, leaving the resource
 * database to carry fonts and colors. This mirrors what mWizard does with
 * its own rc file, and for the same reason.
 *
 *     Settings
 *     {
 *         title            Toolbox
 *         horizontal       False
 *         dateTimeFormat   "%m/%d %l:%M %p"
 *         sessionMenu      True
 *         shutdownCommand  "systemctl poweroff"
 *     }
 *
 * Each entry is written into the screen resource database under mWand's
 * instance name, so the resource table below, its converters and its defaults
 * keep doing the work. The instance-name form outranks the class-name form
 * that .Xdefaults files use, and an identical key is replaced, so a setting
 * here always beats a leftover X resource.
 *
 * Names are validated against that same table: a misspelled setting is
 * reported rather than silently ignored.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <X11/Intrinsic.h>
#include <X11/Xresource.h>
#include <Xm/Xm.h>
#include "common.h"
#include "mwand.h"

#define RES_FIELD(f) XtOffsetOf(struct tb_resources,f)
#define RC_LINE_MAX 1024

/*
 * Everything the user can set. The first four are appearance-adjacent or
 * bootstrap settings; the rest are behaviour. rcFile is the one that cannot
 * move into the rc file, since it names the file the settings live in.
 */
XtResource xrdb_resources[]={
	{ "title","Title",XmRString,sizeof(String),
		RES_FIELD(title),XmRImmediate,(XtPointer)NULL
	},
	{ "dateTimeDisplay","DateTimeDisplay",XmRBoolean,sizeof(Boolean),
		RES_FIELD(show_date_time),XmRImmediate,(XtPointer)True
	},
	{ "dateTimeFormat","DateTimeFormat",XmRString,sizeof(String),
		RES_FIELD(date_time_fmt),XmRImmediate,(XtPointer)"%m/%d %l:%M %p"
	},
	{ "rcFile","RcFile",XmRString,sizeof(String),
		RES_FIELD(rc_file),XmRImmediate,(XtPointer)NULL
	},
	{ "hotkey","Hotkey",XmRString,sizeof(String),
		RES_FIELD(hotkey),XmRImmediate,(XtPointer)NULL
	},
	{ "horizontal","Horizontal",XmRBoolean,sizeof(Boolean),
		RES_FIELD(horizontal),XmRImmediate,(XtPointer)False
	},
	{ "separators","Separators",XmRBoolean,sizeof(Boolean),
		RES_FIELD(separators),XmRImmediate,(XtPointer)True
	},
	{ "workspaceSwitcher","WorkspaceSwitcher",XmRBoolean,sizeof(Boolean),
		RES_FIELD(switcher),XmRImmediate,(XtPointer)True
	},
	{ "occupyAllWorkspaces","OccupyAllWorkspaces",XmRBoolean,sizeof(Boolean),
		RES_FIELD(occupy_all),XmRImmediate,(XtPointer)True
	},
	{ "sessionMenu","SessionMenu",XmRBoolean,sizeof(Boolean),
		RES_FIELD(session_menu),XmRImmediate,(XtPointer)True
	},
	{ "lockCommand","LockCommand",XmRString,sizeof(String),
		RES_FIELD(lock_command),XmRImmediate,(XtPointer)NULL
	},
	{ "logoutCommand","LogoutCommand",XmRString,sizeof(String),
		RES_FIELD(logout_command),XmRImmediate,(XtPointer)NULL
	},
	{ "suspendCommand","SuspendCommand",XmRString,sizeof(String),
		RES_FIELD(suspend_command),XmRImmediate,
		(XtPointer)"systemctl suspend"
	},
	{ "rebootCommand","RebootCommand",XmRString,sizeof(String),
		RES_FIELD(reboot_command),XmRImmediate,
		(XtPointer)"systemctl reboot"
	},
	{ "shutdownCommand","ShutdownCommand",XmRString,sizeof(String),
		RES_FIELD(shutdown_command),XmRImmediate,
		(XtPointer)"systemctl poweroff"
	}
};

Cardinal num_xrdb_resources = XtNumber(xrdb_resources);

static Boolean IsKnownName(const char *name)
{
	Cardinal i;

	for(i = 0; i < num_xrdb_resources; i++)
		if(!strcmp(name, xrdb_resources[i].resource_name)) return True;

	return False;
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
 * Splits "name value". The value is everything after the first run of
 * whitespace, so an unquoted multi-word value survives; a fully quoted value
 * has its quotes stripped.
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

/*
 * Scanner state. Brace depth is tracked across the whole file so that a block
 * header is only ever recognized at the outermost level -- a menu titled
 * "Settings" must not be mistaken for one.
 */
enum block_kind {
	BLOCK_SETTINGS,	/* behaviour, merged into the resource database */
	BLOCK_VARIABLES	/* command variables, exported to the environment */
};

struct scan_state {
	XrmDatabase db;
	const char *rc_file;
	int depth;
	int line_num;
	Boolean in_block;
	enum block_kind kind;
	Boolean pending;
	enum block_kind pending_kind;
};

/*
 * Exports one entry of the Variables block to the environment.
 *
 * Menu commands are run through expand_env_vars() before exec, so a variable
 * put here is what "$TERMINAL" in a menu entry expands to -- the same
 * spelling mWizard's rc file uses, since there it is the shell that expands
 * it. Nothing needs to happen at parse time in either program.
 */
static void PutVariable(struct scan_state *st, const char *name,
	const char *value)
{
	const char *p;

	if(!isalpha((unsigned char)name[0]) && name[0] != '_') {
		fprintf(stderr, "%s: %s line %d: \"%s\" is not a usable "
			"variable name.\n", APP_NAME, st->rc_file, st->line_num, name);
		return;
	}

	for(p = name; *p; p++) {
		if(!isalnum((unsigned char)*p) && *p != '_') {
			fprintf(stderr, "%s: %s line %d: \"%s\" is not a usable "
				"variable name.\n", APP_NAME, st->rc_file,
				st->line_num, name);
			return;
		}
	}

	setenv(name, value, 1);
}

/*
 * Handles one stretch of text between braces: at depth 0 a possible block
 * header, at depth 1 inside our block a setting, and anything else -- the
 * body of a menu -- not ours.
 */
static void HandleSegment(struct scan_state *st, char *seg)
{
	char key[RC_LINE_MAX];
	char first[RC_LINE_MAX];
	char *name, *value, *p;
	int k = 0;

	seg = Trim(seg);
	if(!*seg) return;

	if(st->depth == 0) {
		p = seg;
		while(*p && !isspace((unsigned char)*p) && k < RC_LINE_MAX-1)
			first[k++] = tolower((unsigned char)*p++);
		first[k] = '\0';

		if(!strcmp(first, "settings")) {
			st->pending = True;
			st->pending_kind = BLOCK_SETTINGS;
		} else if(!strcmp(first, "variables")) {
			st->pending = True;
			st->pending_kind = BLOCK_VARIABLES;
		} else {
			st->pending = False;
		}
		return;
	}

	if(!st->in_block || st->depth != 1) return;

	if(!SplitEntry(seg, &name, &value)) return;

	if(st->kind == BLOCK_VARIABLES) {
		PutVariable(st, name, value);
		return;
	}

	if(!IsKnownName(name)) {
		fprintf(stderr, "%s: %s line %d: unknown setting \"%s\".\n",
			APP_NAME, st->rc_file, st->line_num, name);
		return;
	}

	snprintf(key, sizeof(key), "%s*%s", APP_NAME, name);
	XrmPutStringResource(&st->db, key, value);
}

static void HandleBrace(struct scan_state *st, char c)
{
	if(c == '{') {
		st->depth++;
		if(st->depth == 1 && st->pending) {
			st->in_block = True;
			st->kind = st->pending_kind;
			st->pending = False;
		}
	} else {
		if(st->depth > 0) st->depth--;
		if(st->depth == 0) st->in_block = False;
	}
}

/*
 * Reads the Settings block out of the rc file and merges it into the screen
 * resource database. Braces inside a quoted string are literal text, and a
 * short block may open and close on one line.
 */
void LoadRcSettings(Display *dpy, const char *rc_file)
{
	FILE *fp;
	char buf[RC_LINE_MAX];
	char seg[RC_LINE_MAX];
	struct scan_state st;
	char *p;
	int n;
	Boolean in_quotes;

	if(!rc_file || !(fp = fopen(rc_file, "r"))) return;

	memset(&st, 0, sizeof(st));
	st.db = XtScreenDatabase(DefaultScreenOfDisplay(dpy));
	st.rc_file = rc_file;

	while(fgets(buf, sizeof(buf), fp) != NULL) {
		st.line_num++;

		if((p = strchr(buf, '\n')) != NULL) *p = '\0';

		p = buf;
		while(*p && isspace((unsigned char)*p)) p++;
		if(*p == '#' || *p == '!') continue;

		n = 0;
		in_quotes = False;

		for(; *p; p++) {
			if(*p == '"') in_quotes = in_quotes ? False : True;

			if(!in_quotes && (*p == '{' || *p == '}')) {
				seg[n] = '\0';
				HandleSegment(&st, seg);
				n = 0;
				HandleBrace(&st, *p);
				continue;
			}

			if(n < RC_LINE_MAX - 1) seg[n++] = *p;
		}

		seg[n] = '\0';
		HandleSegment(&st, seg);
	}

	fclose(fp);
}
