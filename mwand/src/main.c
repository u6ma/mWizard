/*
 * Copyright (C) 2018-2026 alx@fastestcode.org
 *
 * Modified 2026 for mWizard's mWand. See NOTICE for details.
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
 * Startup, the top level window, and its layout.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/PushBG.h>
#include <Xm/CascadeBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/Frame.h>
#include <Xm/LabelG.h>
#include <Xm/SelectioB.h>
#include <Xm/TextF.h>
#include <Xm/MessageB.h>
#include <Xm/MwmUtil.h>
#include <X11/cursorfont.h>
#include "rcparse.h"
#include "common.h"
#include "wswitch.h"
#include "mwand.h"

/* Forward declarations */
static void set_icon(Widget);
static Boolean setup_hotkeys(void);
static int xgrabkey_err_handler(Display*, XErrorEvent*);
static void handle_root_event(XEvent*);
static void set_ws_presence(Widget);
static void create_utility_widgets(Widget);
static void xt_sigusr1_handler(XtPointer, XtSignalId*);
static void sigchld_handler(int);
static void sigusr_handler(int);

struct tb_resources app_res;

XtAppContext app_context;
Widget wshell = None;
Widget wmain = None;
String rc_file_path = NULL;
unsigned int hotkey_mods = 0;

static Widget wgadsep = None;
static Widget wgadrc = None;
static XtSignalId xt_sigusr1;
static KeyCode hotkey_code = 0;

static XrmOptionDescRec xrdb_options[]={
	{"-title","title",XrmoptionSepArg,(caddr_t)NULL},
	{"-rcfile","rcFile",XrmoptionSepArg,(caddr_t)NULL},
	{"-hotkey","hotkey",XrmoptionSepArg,(caddr_t)NULL},
	{"-horizontal", "horizontal", XrmoptionNoArg, (caddr_t)"True"},
	{"+horizontal", "horizontal", XrmoptionNoArg, (caddr_t)"False"}
};

static String fallback_res[]={
	"MWand.x: 8",
	"MWand.y: 28",
	"MWand.mwmDecorations: 58",
	"*mainFrame.shadowThickness: 1",
	NULL
};

int main(int argc, char **argv)
{
	Display *dpy;
	Window root_window;
	Widget wframe;
	int root_event_mask = PropertyChangeMask;
	int retries;
	
	rsignal(SIGUSR1, sigusr_handler);
	rsignal(SIGUSR2, sigusr_handler);
	rsignal(SIGCHLD, sigchld_handler);

	XtSetLanguageProc(NULL,NULL,NULL);
	XtToolkitInitialize();
	
	wshell = XtVaAppInitialize(&app_context, APP_CLASS,
		xrdb_options, XtNumber(xrdb_options), &argc,argv, fallback_res,
		XmNiconName, APP_TITLE, XmNallowShellResize, True,
		XmNmwmFunctions, MWM_FUNC_MOVE|MWM_FUNC_MINIMIZE,
		XmNmappedWhenManaged, False, NULL);
	
	dpy = XtDisplay(wshell);
	root_window = RootWindowOfScreen(XtScreen(wshell));

	if(argc > 1) {
		int i;
		
		for(i = 1; i < argc; i++) {
			if(!strcmp("-version", argv[i])) {
				print_version(APP_NAME);
				XtDestroyApplicationContext(app_context);
				return 0;
			}
		}
	}
	
	/*
	 * The rc file has to be located and its Settings block merged into the
	 * resource database before anything else is fetched from it. rcFile is
	 * looked up on its own first, because it names the file the settings
	 * live in and so cannot itself come from there; everything else can.
	 */
	{
		static XtResource rc_res[] = {
			{ "rcFile","RcFile",XmRString,sizeof(String),
			  XtOffsetOf(struct tb_resources, rc_file),
			  XmRImmediate,(XtPointer)NULL }
		};

		XtGetApplicationResources(wshell, &app_res, rc_res,
			XtNumber(rc_res), NULL, 0);

		rc_file_path = app_res.rc_file ? app_res.rc_file : FindRcFile();

		if(rc_file_path) LoadRcSettings(dpy, rc_file_path);
	}

	XtGetApplicationResources(wshell, &app_res, xrdb_resources,
		num_xrdb_resources, NULL, 0);

	/* EWMH virtual desktop atoms.
	 * Wait for the WM to avoid reconfiguring */
	retries = ATOM_WAIT_RETRIES;
	while( ((xa_ndesks = XInternAtom(dpy,
		_NET_NUMBER_OF_DESKTOPS, retries ? True : False)) == None)
		&& (retries--) ) {
		
			usleep(UWAIT_FOR_ATOMS);
	}
	xa_cdesk = XInternAtom(dpy, _NET_CURRENT_DESKTOP, False);

	if(!app_res.title){
		char *title;
		char *login;
		char host[256]="localhost";

		if( (login = get_login()) ) {
			gethostname(host,255);

			title = malloc(strlen(login)+strlen(host)+2);
			if(!title){
				perror("malloc");
				return EXIT_FAILURE;
			}
			sprintf(title, "%s@%s", login,host);
			XtVaSetValues(wshell, XmNtitle, title, NULL);
			free(title);
		}
	}

	wframe = XmVaCreateManagedFrame(wshell, "mainFrame",
		XmNshadowType, XmSHADOW_OUT, NULL);
	
	wmain = XmVaCreateManagedRowColumn(wframe, "main",
		XmNmarginWidth, 0,
		XmNmarginHeight, 0,
		XmNspacing, 0,
		XmNorientation, (app_res.horizontal ? XmHORIZONTAL:XmVERTICAL),
		NULL);

	if(rc_file_path){
		if(access(rc_file_path, R_OK) == -1){
			MessageDialog(False, "Cannot access RC file. Exiting!");
			perror(rc_file_path);
			return EXIT_FAILURE;
		}
		if(!ConstructMenu()) return EXIT_FAILURE;
	}else{
		MessageDialog(False, "RC file not found, nor specified. Exiting!");
		fprintf(stderr,"%s not found, nor specified.\n",RC_NAME);
		return EXIT_FAILURE;
	}

	create_utility_widgets(wmain);
	
	XtRealizeWidget(wshell);
	if(app_res.occupy_all) set_ws_presence(wshell);
	set_icon(wshell);

	if(setup_hotkeys())
		root_event_mask |= KeyPressMask;

	XtMapWidget(wshell);

	if(XtIsManaged(wswitch))
		XmProcessTraversal(wswitch, XmTRAVERSE_CURRENT);

	XSelectInput(XtDisplay(wshell), root_window, root_event_mask);
	
	xt_sigusr1 = XtAppAddSignal(app_context, xt_sigusr1_handler, NULL);
	
	for(;;) {
		XEvent evt;
		XtAppNextEvent(app_context, &evt);
		if(evt.xany.window == root_window)
			handle_root_event(&evt);
		else
			XtDispatchEvent(&evt);
	}
	
	return 0;
}

static void set_icon(Widget wshell)
{
	Pixmap image;
	Pixmap mask;
	Window root;
	Display *dpy = XtDisplay(wshell);
	int depth, screen;
	Screen *pscreen;
	Colormap cmap;
	XColor bg_color;
	XColor fg_color;
	XColor tmp;
	
	#include "xbm/toolbox.xbm"
	#include "xbm/toolbox_m.xbm"
	
	pscreen = XDefaultScreenOfDisplay(dpy);
	screen = XScreenNumberOfScreen(pscreen);
	root = RootWindowOfScreen(pscreen);
	depth = DefaultDepth(dpy, screen);
	cmap = DefaultColormap(dpy, screen);
	
	fg_color.pixel = BlackPixel(dpy, screen);
	bg_color.pixel = WhitePixel(dpy, screen);
	
	XAllocNamedColor(dpy, cmap, "LightBlue", &bg_color, &tmp);
	
	image = XCreatePixmapFromBitmapData(dpy, root,
		(char*)toolbox_xbm_bits,
		toolbox_xbm_width, toolbox_xbm_height,
		fg_color.pixel, bg_color.pixel, depth);

	mask = XCreatePixmapFromBitmapData(dpy, root,
		(char*)toolbox_m_xbm_bits,
		toolbox_m_xbm_width,
		toolbox_m_xbm_height, 1, 0, 1);
	
	XtVaSetValues(wshell, XmNiconPixmap, image, XmNiconMask, mask, NULL);
}

static void set_ws_presence(Widget wshell)
{
	Display *dpy = XtDisplay(wshell);
	Atom wps_atom = XInternAtom(dpy, _XA_MWM_WORKSPACE_PRESENCE, False);
	Atom all_atom = XInternAtom(dpy, _XA_MWM_WORKSPACE_ALL, False);
	
	XChangeProperty(dpy, XtWindow(wshell), wps_atom,
		wps_atom, 32, PropModeReplace, (unsigned char*)&all_atom, 1);
}

static Boolean setup_hotkeys(void)
{
	Window root_window;
	char *buf;
	char *token;
	KeySym key_sym = NoSymbol;
	static int (*def_x_err_handler)(Display*, XErrorEvent*) = NULL;
	
	if(!app_res.hotkey || !strcasecmp(app_res.hotkey, "none")) return False;

	hotkey_code = 0;
	hotkey_mods = 0;

	buf=strdup(app_res.hotkey);	
	token=strtok(buf," \t+");
	if(token){
		while(token){
			if(!strcasecmp(token, "alt")){
				hotkey_mods |= Mod1Mask;
			}else if(!strcasecmp(token, "ctrl") ||
				!strcasecmp(token, "control")){
				hotkey_mods |= ControlMask;
			}else if(!strcasecmp(token, "shift")){
				hotkey_mods |= ShiftMask;
			}else{
				key_sym = XStringToKeysym(token);
				break;
			}
			token = strtok(NULL," \t+");
		}
	}else{
		key_sym = XStringToKeysym(buf);
	}
	free(buf);

	if(key_sym == NoSymbol){
		fputs("Invalid hotkey specification\n", stderr);
		return False;
	}
	hotkey_code = XKeysymToKeycode(XtDisplay(wshell), key_sym);
	
	root_window = RootWindowOfScreen(XtScreen(wshell));
	
	XSync(XtDisplay(wshell), False);
	def_x_err_handler = XSetErrorHandler(xgrabkey_err_handler);
	
	XGrabKey(XtDisplay(wshell), hotkey_code, hotkey_mods,
		root_window, False, GrabModeAsync, GrabModeAsync);
	XGrabKey(XtDisplay(wshell), hotkey_code, hotkey_mods | Mod2Mask,
		root_window, False, GrabModeAsync, GrabModeAsync);
	XGrabKey(XtDisplay(wshell), hotkey_code, hotkey_mods | LockMask,
		root_window, False, GrabModeAsync, GrabModeAsync);
	XGrabKey(XtDisplay(wshell), hotkey_code, hotkey_mods | LockMask | Mod2Mask,
		root_window, False, GrabModeAsync, GrabModeAsync);	
	
	XSync(XtDisplay(wshell), False);
	XSetErrorHandler(def_x_err_handler);
	
	return True;		
}

static int xgrabkey_err_handler(Display *dpy, XErrorEvent *evt)
{
	if(evt->error_code == BadAccess){
		fputs("Cannot setup hotkey. "
			"Specified key code is used by another application.\n",stderr);
		return 0;
	}
	exit(EXIT_FAILURE); /* shouldn't normally happen */
}

static void handle_root_event(XEvent *evt)
{
	
	if(evt->type == KeyRelease) {
		XKeyEvent *e = (XKeyEvent*)evt;
	
		if(e->keycode == hotkey_code &&
			((e->state & hotkey_mods) || !hotkey_mods))
				raise_and_focus(wshell);

	} else if(evt->type == PropertyNotify) {
		XPropertyEvent *e = (XPropertyEvent*)evt;

		if((e->atom == xa_cdesk || e->atom == xa_ndesks) && app_res.switcher) {
			unsigned short nws, iws;
			if(GetWorkspaceInfo(&nws, &iws)) {
				
				if(e->atom == xa_ndesks) {
					Arg args[4];
					int n = 0;

					XtSetArg(args[n], NnumberOfWorkspaces, nws); n++;
					XtSetArg(args[n], NactiveWorkspace, iws); n++;
					if(app_res.horizontal) {
						XtSetArg(args[n], XmNcolumns, nws);
						n++;
					}

					if(nws > 1) {
						XtManageChild(wswitch);
						XtManageChild(wgadrc);
						if(app_res.separators) XtManageChild(wgadsep);
						XmProcessTraversal(wswitch, XmTRAVERSE_CURRENT);
					} else {
						XtUnmanageChild(wswitch);
						if(!app_res.show_date_time) {
							XtUnmanageChild(wgadrc);
							XtUnmanageChild(wgadsep);
						}
					}
					XtSetValues(wswitch, args, n);
				} else if(e->atom == xa_cdesk) {
					SwitcherSetActiveWorkspace(wswitch, iws);
				}
			} else {
				fputs("Failed to retrieve workspace information.\n", stderr);
				XtUnmanageChild(wswitch);
				if(!app_res.show_date_time) {
					XtUnmanageChild(wgadrc);
					XtUnmanageChild(wgadsep);
				}
			}
		}
	}
}

void raise_and_focus(Widget w)
{
	static Atom XaNET_ACTIVE_WINDOW = None;
	static Atom XaWM_STATE = None;
	static Atom XaWM_CHANGE_STATE = None;
	Atom ret_type;
	int ret_fmt;
	unsigned long ret_items;
	unsigned long ret_bytes;
	uint32_t *state = NULL;
	XClientMessageEvent evt;
	Display *dpy = XtDisplay(wshell);

	if(XaWM_STATE == None){
		XaWM_STATE = XInternAtom(dpy, "WM_STATE", True);
		XaWM_CHANGE_STATE = XInternAtom(dpy, "WM_CHANGE_STATE", True);
		XaNET_ACTIVE_WINDOW = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", True);
	}

	if(XaWM_STATE == None) return;

	if(XGetWindowProperty(dpy, XtWindow(w), XaWM_STATE, 0, 1,
		False, XaWM_STATE, &ret_type, &ret_fmt, &ret_items,
		&ret_bytes, (unsigned char**)&state) != Success) return;
	if(ret_type == XaWM_STATE && ret_fmt && *state == IconicState){
		evt.type = ClientMessage;
		evt.send_event = True;
		evt.message_type = XaWM_CHANGE_STATE;
		evt.display = dpy;
		evt.window = XtWindow(w);
		evt.format = 32;
		evt.data.l[0] = NormalState;
		XSendEvent(dpy, RootWindowOfScreen(XtScreen(wshell)), True,
			SubstructureNotifyMask | SubstructureRedirectMask, (XEvent*)&evt);
	}else{
		if(XaNET_ACTIVE_WINDOW){
			evt.type = ClientMessage,
			evt.send_event = True;
			evt.serial = 0;
			evt.display = dpy;
			evt.window = XtWindow(w);
			evt.message_type = XaNET_ACTIVE_WINDOW;
			evt.format = 32;

			XSendEvent(dpy, RootWindowOfScreen(XtScreen(wshell)), False,
				SubstructureNotifyMask|SubstructureRedirectMask, (XEvent*)&evt);
		}else{
			XRaiseWindow(dpy, XtWindow(w));
			XSync(dpy, False);
			XSetInputFocus(dpy, XtWindow(w), RevertToParent, CurrentTime);
		}
	}
	XFree((char*)state);
}

/*
 * Builds everything below the launcher menus: the command menu, the workspace
 * switcher and the clock. Each of those lives in its own file; this only
 * decides what appears and in what order.
 */
static void create_utility_widgets(Widget wparent)
{
	Widget w;
	Arg args[10];
	int n;
	Boolean have_switcher;

	XtSetArg(args[0], XmNorientation,
		(app_res.horizontal ? XmVERTICAL:XmHORIZONTAL));
	w = XmCreateSeparatorGadget(wparent, "separator", args, 1);
	if(app_res.separators) XtManageChild(w);

	/* Execute..., and the session actions when they are enabled */
	CreateCommandMenu(wparent);

	XtSetArg(args[0], XmNorientation,
		(app_res.horizontal ? XmVERTICAL:XmHORIZONTAL));
	wgadsep = XmCreateSeparatorGadget(wparent, "separator", args, 1);

	/* Switcher and clock share a row/column */
	n = 0;
	XtSetArg(args[n], XmNmarginWidth, 3); n++;
	XtSetArg(args[n], XmNmarginHeight, 3); n++;
	XtSetArg(args[n], XmNspacing, 3); n++;
	XtSetArg(args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg(args[n], XmNorientation,
		(app_res.horizontal ? XmHORIZONTAL:XmVERTICAL)); n++;

	wgadrc = XmCreateRowColumn(wparent, "gadgets", args, n);

	CreateSwitcherWidget(wgadrc);
	CreateClockWidget(wgadrc);

	have_switcher = XtIsManaged(wswitch);

	if((have_switcher || app_res.show_date_time) && app_res.separators)
		XtManageChild(wgadsep);

	if(have_switcher || app_res.show_date_time)
		XtManageChild(wgadrc);
}

char* FindRcFile(void)
{
	size_t len;
	char *home=NULL;
	char *lang=NULL;
	char *path;
	int i;
	char *sys_paths[32]={
		RCDIR,
		"/etc/X11",
		"/usr/lib/X11",
		"/usr/local/lib/X11",
		NULL
	};
	
	home = getenv("HOME");
	lang = getenv("LANG");
	
	if(!home){
		fprintf(stderr,"HOME is not set!\n");
		return NULL;
	}

	len = 36 + strlen(home) + strlen(RC_NAME);
	if(lang) len += strlen(lang);

	path = malloc(len);
	if(!path){
		perror("malloc");
		return NULL;
	}

	snprintf(path, len, "%s/.%s", home, RC_NAME);
	if(!access(path,R_OK)) return path;

	for(i = 0; sys_paths[i] != NULL; i++){
		if(lang){
			snprintf(path, len, "%s/%s/%s", sys_paths[i], lang, RC_NAME);
			if(!access(path,R_OK)) return path;
		}
		snprintf(path, len, "%s/%s", sys_paths[i], RC_NAME);
		if(!access(path,R_OK)) return path;
	}

	free(path);
	return NULL;
}

static void xt_sigusr1_handler(XtPointer client_data, XtSignalId *id)
{
	/* on parse error, the previous configuration remains active
	 * and the user is informed about */
	ConstructMenu();
}

static void sigchld_handler(int sig)
{
	int status;
	waitpid(-1, &status, WNOHANG);
}

static void sigusr_handler(int sig)
{
	if(sig == SIGUSR1) XtNoticeSignal(xt_sigusr1);
}
