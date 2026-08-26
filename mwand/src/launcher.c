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
 * Building the launcher menus from the rc file, and running the
 * commands they name.
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

static void menu_command_cb(Widget, XtPointer, XtPointer);
static void report_exec_error(const char*, const char*, int);

/*
 * The menu bar built from the rc file. It outlives ConstructMenu(), which
 * destroys and rebuilds it whenever SIGUSR1 asks for a reload.
 */
static Widget wmenu = None;

Boolean ConstructMenu(void)
{
	Arg args[10];
	int n = 0;
	Widget *wlevel;
	unsigned int nlevels=1;
	struct tb_entry *entries, *cur;
	int err;

	if((err=tb_parse_config(rc_file_path, &entries))){
		ReportRcFileError(rc_file_path,
			tb_parser_error_string() ?
			tb_parser_error_string() : strerror(err));
		return False;
	}

	if(!entries){
		ReportRcFileError(rc_file_path,
			"File doesn't seem to contain any entries.");
		return False;
	}
	
	cur = entries;
	
	while(cur){
		nlevels = (cur->level > nlevels) ? cur->level : nlevels;
		cur = cur->next;
	}
	
	if(wmenu){
		XtUnmanageChild(wmenu);
		XtDestroyWidget(wmenu);
	}
	
	n = 0;
	
	XtSetArg(args[n], XmNshadowThickness, 0); n++;
	XtSetArg(args[n], XmNspacing, 1); n++;
	XtSetArg(args[n], XmNmarginWidth, 0); n++;
	XtSetArg(args[n], XmNorientation,
		(app_res.horizontal ? XmHORIZONTAL:XmVERTICAL)); n++;
	XtSetArg(args[n], XmNpacking,
		(app_res.horizontal ? XmPACK_TIGHT:XmPACK_COLUMN)); n++;
	XtSetArg(args[n], XmNrowColumnType, XmMENU_BAR); n++;
	XtSetArg(args[n], XmNpositionIndex, 0); n++;

	wmenu = XmCreateRowColumn(wmain, "menu", args, n);

	#ifdef DEBUG_MENU
	printf("Max %d cascade levels\n",nlevels);
	#endif

	wlevel = calloc(nlevels + 1, sizeof(Widget));
	if(!wlevel){
		perror("malloc");
		return False;
	}
	
	cur = entries;
	wlevel[0] = wmenu;
	
	while(cur){
		Widget w;
		XmString title;

		if(cur->type == TBE_CASCADE && cur->next){
			Widget new_pulldown, new_cascade;
			
			#ifdef DEBUG_MENU
			printf("Adding Cascade: %s; Level: %d\n",cur->title,cur->level);
			#endif
			new_pulldown=XmCreatePulldownMenu(
				wlevel[cur->level],"commandPulldown",NULL,0);
			
			title=XmStringCreateLocalized(cur->title);
			
			n = 0;
			XtSetArg(args[n], XmNlabelString, title); n++;
			XtSetArg(args[n], XmNmnemonic, (KeySym)cur->mnemonic); n++;
			XtSetArg(args[n], XmNsubMenuId, new_pulldown); n++;
			new_cascade = XmCreateCascadeButtonGadget(
				wlevel[cur->level],"cascadeButton",args,n);
			
			XmStringFree(title);
		
			wlevel[cur->next->level] = new_pulldown;
						
			XtManageChild(new_cascade);
		
		}else if(cur->type == TBE_COMMAND){
			XtCallbackRec push_callback[]={
				{ (XtCallbackProc)menu_command_cb, (XtPointer)cur->command},
				{ (XtCallbackProc)NULL, (XtPointer)NULL}
			};
			#ifdef DEBUG_MENU
			printf("Adding Command: %s; Level: %d\n",cur->title,cur->level);
			#endif
			
			title=XmStringCreateLocalized(cur->title);

			n = 0;
			XtSetArg(args[n], XmNlabelString, title); n++;
			if(cur->mnemonic){
				XtSetArg(args[n], XmNmnemonic, (KeySym)cur->mnemonic);
				n++;
			}
			XtSetArg(args[n], XmNactivateCallback, push_callback); n++;
			w = XmCreatePushButtonGadget(
				wlevel[cur->level], "menuButton",args,n);

			XmStringFree(title);
			XtManageChild(w);

		}else if(cur->type == TBE_SEPARATOR){
			w = XmCreateSeparatorGadget(
				wlevel[cur->level], "separator", NULL, 0);

			XtManageChild(w);
		}
		cur = cur->next;
	}

	free(wlevel);
	
	XtManageChild(wmenu);
	
	return True;
}

/*
 * Asks the window manager to put up one of its own windows.
 *
 * Both the Execute prompt and mWinfo belong to the window manager: they are
 * wanted with or without a panel, and mWizard is the process that is always
 * running. mWand asks for them rather than carrying second copies.
 *
 * The window manager is found the standard EWMH way -- the check window
 * named by _NET_SUPPORTING_WM_CHECK carries _NET_WM_PID -- and signalled
 * directly, so this needs no helper program and no private protocol. Under a
 * window manager that publishes neither, the item simply reports that there
 * is nothing to ask.
 *
 * what names the thing being asked for, and appears in the message when the
 * window manager cannot provide it.
 */
static void AskWindowManager(int sig, const char *what)
{
	Display *dpy = XtDisplay(wshell);
	Window root = RootWindowOfScreen(XtScreen(wshell));
	Atom xa_check, xa_pid;
	Atom ret_type;
	int ret_fmt;
	unsigned long ret_items, ret_after;
	unsigned char *data = NULL;
	Window wm_window;
	long wm_pid;
	char msg[256];

	snprintf(msg, sizeof(msg),
		"The window manager does not provide %s.", what);

	xa_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", True);
	xa_pid = XInternAtom(dpy, "_NET_WM_PID", True);

	if(xa_check == None || xa_pid == None) {
		MessageDialog(False, msg);
		return;
	}

	if(XGetWindowProperty(dpy, root, xa_check, 0, 1, False, XA_WINDOW,
		&ret_type, &ret_fmt, &ret_items, &ret_after, &data) != Success
		|| !data || !ret_items) {
		if(data) XFree(data);
		MessageDialog(False, "No EWMH window manager is running.");
		return;
	}
	wm_window = *((Window*)data);
	XFree(data);
	data = NULL;

	if(XGetWindowProperty(dpy, wm_window, xa_pid, 0, 1, False, XA_CARDINAL,
		&ret_type, &ret_fmt, &ret_items, &ret_after, &data) != Success
		|| !data || !ret_items) {
		if(data) XFree(data);
		MessageDialog(False, msg);
		return;
	}
	wm_pid = *((long*)data);
	XFree(data);

	if(kill((pid_t)wm_pid, sig) == -1)
		MessageDialog(False, "Could not reach the window manager.");
}

/*
 * SIGUSR1 is the Execute prompt, SIGUSR2 is mWinfo. See WmExecDlg.c and
 * WmWinfo.c on the window manager side.
 */
void ExecuteCommandDialog(void)
{
	AskWindowManager(SIGUSR1, "a command prompt");
}

void AboutWindowManagerDialog(void)
{
	AskWindowManager(SIGUSR2, "an About window");
}

/*
 * Runs a command through the shell named by the "shell" setting.
 *
 * Unset by default, in which case RunCommand() splits the command itself and
 * execs it directly -- which is what mWand has always done, and keeps a
 * stray metacharacter in a menu entry from being interpreted. Setting it
 * hands the string to a shell instead, so the Execute dialog accepts
 * pipelines, redirection, "&" and $VAR the same way mWizard's f.exec does.
 */
static int RunCommandInShell(const char *shell, const char *cmd_spec)
{
	pid_t pid;
	const char *shellname;
	volatile int errval = 0;

	shellname = strrchr(shell, '/');
	shellname = shellname ? (shellname + 1) : shell;

	pid = vfork();
	if(pid == 0) {
		setsid();

		/*
		 * execlp rather than execl: a shell named without a path -- an
		 * rc file saying "shell bash" -- has to be found on PATH.
		 */
		if(execlp(shell, shellname, "-c", cmd_spec, (char*)NULL) == (-1))
			errval = errno;

		_exit(127);
	} else if(pid == -1) {
		errval = errno;
	}

	return errval;
}

int RunCommand(const char *cmd_spec)
{
	pid_t pid;
	char *str;
	char *p, *t;
	char pc = 0;
	int done = 0;
	char **argv = NULL;
	size_t argv_size = 0;
	unsigned int argc = 0;
	volatile int errval = 0;

	if(app_res.shell && *app_res.shell)
		return RunCommandInShell(app_res.shell, cmd_spec);

	str = strdup(cmd_spec);

	p = str;
	t = NULL;
	
	/* split the command string into separate arguments */
	while(!done){
		if(!t){
			while(*p && isblank((int)*p)) p++;
			if(*p == '\0') break;
			t = p;
		}
		
		if(*p == '\"' || *p == '\''){
			if(pc == '\\'){
				/* literal " or ' */
				memmove(p - 1, p, strlen(p) + 1);
			}else{
				/* quotation marks, remove them ignoring blanks within */
				memmove(p, p + 1, strlen(p));
				while(*p != '\"' && *p != '\''){
					if(*p == '\0'){
						if(argv) free(argv);
						return EINVAL;
					}
					p++;
				}
				memmove(p, p + 1, strlen(p));
			}
		}
		if(isblank((int)*p) || *p == '\0'){
			if(*p == '\0') done = 1;
			if(argv_size < argc+1){
				char **new_ptr;
				new_ptr = realloc(argv, (argv_size += 64) * sizeof(char*));
				if(!new_ptr){
					free(str);
					if(argv) free(argv);
					return ENOMEM;
				}
				argv=new_ptr;
			}
			*p = '\0';
			argv[argc] = t;
			
			#ifdef DEBUG_EXEC
			printf("argv[%d]: %s\n",argc,argv[argc]);
			#endif
			
			t = NULL;
			argc++;
		}
		pc = *p;
		p++;
	}
	
	if(!argc) return EINVAL;
	argv[argc] = NULL;
	
	pid = vfork();
	if(pid == 0){
		setsid();
		
		if(execvp(argv[0],argv) == (-1))
			errval = errno;

		_exit(0);
	}else if(pid == -1){
		errval = errno;
	}
	
	free(str);
	free(argv);
	return errval;
}

static void report_exec_error(const char *err_msg,
	const char *command, int errno_value)
{
	char *errno_str=strerror(errno_value);
	char *buffer;

	buffer=malloc(strlen(err_msg)+strlen(command)+strlen(errno_str)+10);
	if(!err_msg){
		perror("malloc");
		return;
	}		
	sprintf(buffer,"%s \'%s\'.\n%s.",err_msg,command,errno_str);
	MessageDialog(False,buffer);
	free(buffer);
}

static void menu_command_cb(Widget w,
	XtPointer client_data, XtPointer call_data)
{
	int errval;
	char *cmd = (char*) client_data;
	char *exp_cmd;
	
	errval = expand_env_vars(cmd, &exp_cmd);
	if(errval) {
		report_exec_error("Failed to parse command string", cmd, errval);
		return;
	}

	if((errval = RunCommand(exp_cmd)))
		report_exec_error("Error executing command", exp_cmd, errval);
	
	free(exp_cmd);
}
