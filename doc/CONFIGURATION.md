# Configuring mWizard

motifWizard, abbreviated mWizard, is configured from **one file**:
`~/.mwizardrc`.

EMWM split configuration in two. Key bindings, mouse bindings and menus lived
in `~/.emwmrc`, while every behaviour knob — focus policy, window placement,
decoration, workspaces — lived in the X resource database, as
`Emwm*someResource:` lines in an app-defaults file or `.Xdefaults`. Changing
how the window manager behaved meant editing two files in two syntaxes.

In mWizard the rc file holds behaviour as well, in `Settings`, `Client` and
`Workspace` blocks. **Appearance** — colors, shadows, pixmaps and fonts — has
a file of its own, `~/.mstylesrc`, shared with mWand and written in this same
syntax; see [STYLE.md](STYLE.md). Neither one is the X resource database, and
neither one is a second syntax to learn.

---

## 1. File locations

mWizard looks for its rc file in this order:

1. the path given by the `configFile` X resource, if set
2. `$HOME/$LANG/.mwizardrc`, then `$HOME/.mwizardrc`
3. `$RCDIR/$LANG/system.mwizardrc`, then `$RCDIR/system.mwizardrc`
   (`$RCDIR` is set at build time, usually `/etc/X11`)

To start from the shipped defaults:

```sh
cp /etc/X11/system.mwizardrc ~/.mwizardrc
```

Appearance comes from the style file, `~/.mstylesrc`, which mWand reads too;
see [STYLE.md](STYLE.md). The app-defaults file mWizard used to install as
`/etc/X11/app-defaults/MWizard` is gone as of 1.2.

Monitor layouts saved from mWmonitor live in `~/.mmonitors`, which is written
by mWizard rather than by hand; see section 6c. `monitorLayoutFile` moves it.

> **`configFile` is the one setting that cannot live in the rc file.** It names
> the rc file, so it has to be read before the rc file can be opened. Set it as
> an X resource (`MWizard*configFile: /path/to/rc`) or on the command line
> (`mwizard -xrm 'MWizard*configFile: /path/to/rc'`).

---

## 2. Syntax

The rc file is line-oriented. A line whose first character is `!` is a comment.
Blank lines are ignored. Values containing spaces can be quoted with `"`.

```
Settings
{
    keyboardFocusPolicy   explicit
    moveOpaque            True
    iconPlacement         left bottom
    lockCommand           "i3lock -c 000000"
}
```

One setting per line: the **name** is the first word, the **value** is
everything after it. An unquoted multi-word value like `left bottom` is taken
whole, so quotes are only needed to preserve leading/trailing spaces or to
protect a `#`.

A block header may be followed by its `{` on the same line or the next, and a
short block may be written entirely on one line.

**Names are validated.** A name that is not a real setting — including a
misspelling, or an appearance resource that belongs in the X database — is
reported on stderr at startup rather than silently ignored:

```
mwizard: rc file line 12: unknown setting "moveOpqaue".
```

### Precedence

Settings from the rc file are merged into the X resource database under
mWizard's instance name (`mwizard*name`), which outranks the class-name form
(`MWizard*name`) that `.Xdefaults` files use, and replaces an identical key
outright.

**The rc file always wins.** A behaviour resource left over in `.Xdefaults`
from an EMWM setup has no effect. If a setting appears not to apply, the
problem is the rc file, not a stale X resource.

---

## 3. The `Settings` block

Every name below is the same name EMWM used as an X resource, minus the
`Emwm*` prefix, and takes the same values. `mwizard(1)` documents what each one
does in detail; this is the index.

### Input focus

| Setting | Type | Default |
|---|---|---|
| `keyboardFocusPolicy` | `explicit` \| `pointer` | `explicit` |
| `colormapFocusPolicy` | `explicit` \| `pointer` \| `keyboard` | `keyboard` |
| `autoKeyFocus` | boolean | `True` |
| `deiconifyKeyFocus` | boolean | `True` |
| `startupKeyFocus` | boolean | `True` |
| `enforceKeyFocus` | boolean | `True` |
| `raiseKeyFocus` | boolean | `False` |
| `autoRaiseDelay` | ms | `500` |

### Window placement and geometry

| Setting | Type | Default |
|---|---|---|
| `clientAutoPlace` | boolean | `True` |
| `interactivePlacement` | boolean | `False` |
| `positionOnScreen` | boolean | `True` |
| `maximumMaximumSize` | `WxH` | `0x0` (screen) |
| `limitResize` | boolean | `True` |
| `moveOpaque` | boolean | `False` |
| `useWindowOutline` | boolean | `True` |
| `outlineWidth` | pixels | `2` |
| `moveThreshold` | pixels | `4` |

### Frames and decoration

| Setting | Type | Default |
|---|---|---|
| `frameStyle` | `recessed` \| `slab` | `recessed` |
| `frameBorderWidth` | pixels | dynamic |
| `resizeBorderWidth` | pixels | dynamic |
| `frameExternalShadowWidth` | pixels | `2` |
| `resizeCursors` | boolean | `True` |
| `titleLeft` | boolean | `False` |
| `cleanText` | boolean | `True` |
| `transientDecoration` | decoration list | `system title resizeh` |
| `transientFunctions` | function list | all but maximize/minimize |
| `utilityDecoration` | decoration list | `system title resizeh` |
| `utilityFunctions` | function list | all but maximize |
| `dockDecoration` | decoration list | `minimize border` |
| `dockFunctions` | function list | `move minimize` |

`dockDecoration` and `dockFunctions` are new in 1.2 and are the only way to
reach a `_NET_WM_WINDOW_TYPE_DOCK` window — see section 8.

### Icons and the icon box

| Setting | Type | Default |
|---|---|---|
| `iconPlacement` | e.g. `left bottom` | `left bottom` |
| `iconPlacementMargin` | pixels | `-1` (auto) |
| `iconAutoPlace` | boolean | `True` |
| `iconClick` | boolean | `True` |
| `iconDecoration` | `label` \| `image` \| `activelabel` … | dynamic |
| `iconImageMaximum` | `WxH` | dynamic |
| `iconImageMinimum` | `WxH` | `16x16` |
| `iconExternalShadowWidth` | pixels | `2` |
| `fadeNormalIcon` | boolean | `False` |
| `lowerOnIconify` | boolean | `True` |
| `useIconBox` | boolean | `False` |
| `iconBoxName` | string | `iconbox` |
| `iconBoxTitle` | string | `Icons` |
| `iconBoxScheme` | int | `0` |
| `iconBoxSBDisplayPolicy` | `all` \| `vertical` \| `horizontal` | `all` |
| `bitmapDirectory` | path | build-time |

### Buttons, menus and feedback

| Setting | Type | Default |
|---|---|---|
| `passButtons` | boolean | `False` |
| `passSelectButton` | boolean | `True` |
| `rootButtonClick` | boolean | `False` |
| `wMenuButtonClick` | boolean | `True` |
| `wMenuButtonClick2` | boolean | `True` |
| `doubleClickTime` | ms | server default |
| `marqueeSelectGranularity` | int | `0` |
| `showFeedback` | flag list, e.g. `-quit` | all |
| `feedbackGeometry` | geometry | none |
| `keyBindings` | binding set name | `DefaultKeyBindings` |
| `buttonBindings` | binding set name | `DefaultButtonBindings` |

### Workspaces

| Setting | Type | Default |
|---|---|---|
| `workspaceCount` | int | `4` |
| `workspaceList` | names, e.g. `Web Mail Code` | none |
| `initialWorkspace` | workspace name | first |

### Multi-monitor and multi-screen

Two different things share this section, and it is worth being clear which is
which. A **monitor** is one physical head — an XRandR output such as `eDP-1` or
`DP-2`. An **X screen** is a separate root window with its own workspaces, which
almost nothing uses any more; `multiScreen` and `screens` are about those.

| Setting | Type | Default |
|---|---|---|
| `primaryMonitor` | output name | RandR's own primary |
| `perMonitorWorkspaces` | boolean | `True` |
| `monitorDialogOnHotplug` | boolean | `False` |
| `monitorLayoutFile` | path | `~/.mmonitors` |
| `primaryXineramaScreen` | index | `0` |
| `xineramaScreenFocus` | `pointer` \| `keyboard` \| `primary` | `pointer` |
| `xineramaIconifyToPrimary` | boolean | `False` |
| `multiScreen` | boolean | `True` |
| `screens` | screen names | none |

`primaryMonitor` names an output; `primaryXineramaScreen` numbers one. Prefer
the name. An index is a position in a list the X server orders, and plugging a
monitor in can reorder it — so the index that meant your laptop panel this
morning can mean the projector this afternoon. The old setting still works and
is still honoured when `primaryMonitor` is unset.

The primary monitor is where mWizard's own windows are centred, where mWand and
the tray sit by default, and what `f.goto_monitor "primary"` means.

### Workspaces per monitor — new in 1.3

With `perMonitorWorkspaces` on, each monitor shows its own workspace out of the
one shared list. Switching workspaces changes the monitor you are working on and
leaves the others alone:

```
workspaceList   Web Mail Code Term

  eDP-1   [Web]  Mail   Code   Term
  DP-1     Web   Mail  [Code]  Term
```

Which monitor "the one you are working on" means follows `xineramaScreenFocus`,
the same resource that already decided this for window placement — the pointer
by default, the focused window under `keyboard`, or always the primary under
`primary`.

**On a single monitor there is no difference whatsoever**, which is why this is
on by default. Turn it off to get the pre-1.3 behaviour, where every monitor
switches together.

A window dragged from one monitor to another moves into whatever workspace the
destination is showing, so it does not disappear on arrival. A window that
occupies all workspaces is unaffected, as is one pinned to all monitors.

`_NET_CURRENT_DESKTOP` carries one number for the whole screen — the EWMH spec
has no notion of monitors — so it reports the workspace most recently switched
to. A client that *sets* it, mWand's workspace switcher included, switches the
monitor you are working on, which is the useful half and the one that keeps the
switcher doing what it looks like it does.

### Which monitor a window belongs to

A `Client` block can say where a window lives:

```
Client stalonetray
{
    monitor         primary
}
```

| Value | Meaning |
|---|---|
| `current` | whichever monitor the window is on (the default) |
| `primary` | pinned to the primary monitor |
| `all` | stays on screen whatever any monitor is showing |
| an output name | pinned to that monitor, e.g. `DP-1` |

`all` is the one to reach for with a panel or a tray: it is the monitor
equivalent of occupying all workspaces, and without it a tray on the primary
monitor disappears when that monitor switches to a workspace the tray is not in.

The same thing can be set by the client itself through the
`_MWM_MONITOR_PRESENCE` property — a `STRING` holding one of the four values
above — which is how mWand places itself. The property wins over the `Client`
block, the same way `_MWM_WORKSPACE_PRESENCE` does.

It is also the third control in the **Occupy Workspace** dialog
(`f.workspace_presence`), next to the workspace list and the *All Workspaces*
toggle.

### Moving between monitors

| Function | Argument | Does |
|---|---|---|
| `f.monitors` | none | posts mWmonitor, the arranger |
| `f.move_to_monitor` | `next` \| `prev` \| `primary` \| output name | moves the window there |
| `f.goto_monitor` | same | moves you there |

`f.move_to_monitor` keeps the window's position within the monitor
proportional, so a window three quarters of the way across a wide screen stays
three quarters of the way across a narrow one instead of landing off it.

Monitors are ordered left to right, then top to bottom — the order they sit on
the desk — so `next` and `prev` mean what they look like they mean.

The shipped bindings are `Alt Shift` plus an arrow to move the window, and
`Alt Ctrl` plus an arrow to move yourself.

### Miscellaneous

| Setting | Type | Default |
|---|---|---|
| `enableWarp` | boolean | `True` |
| `refreshByClearing` | boolean | `True` |
| `quitTimeout` | ms | `1000` |

### Session and lock — new in mWizard

| Setting | Type | Default |
|---|---|---|
| `shutdownCommand` | command | `systemctl poweroff` |
| `rebootCommand` | command | `systemctl reboot` |
| `suspendCommand` | command | `systemctl suspend` |
| `lockCommand` | command | *(empty)* |
| `lockTimeout` | minutes | `0` (disabled) |
| `trayCommand` | command | *(empty)* |
| `execShell` | shell path | *(empty)* |

See sections 6, 7 and 8.

---

## 3a. The `Variables` block

Names a program once so the rest of the file can refer to it:

```
Variables
{
    TERMINAL   xterm
    BROWSER    chromium-bin
    EDITOR     nedit
}
```

Each entry is exported to the environment, and every command mWizard runs is
exec'd through `sh -c`, so the **shell** performs the substitution:

```
Alt Shift<Key>Return root|icon|window  f.exec "$TERMINAL &"
"New Terminal"                         f.exec "$TERMINAL &"
```

Because it is the shell doing the work, ordinary shell syntax applies —
`$TERMINAL -e mutt` passes arguments, and `${TERMINAL:-xterm}` falls back when
the variable is unset. Nothing happens at parse time, which is why variables
work only where a command is run: **`f.exec` strings and `trayCommand`, not
key names, menu titles or `Client` block names.** A `Client` block matches
`WM_CLASS`, a property of the program itself, so it must name the real class of
whichever terminal you chose.

Names must look like shell variable names — a letter or underscore followed by
letters, digits or underscores. Anything else is reported and ignored.

Programs mWizard starts inherit these; programs your session file started
before mWizard do not. mWand is normally in the second group, so it reads its
own `Variables` block from `~/.mwandrc` — see `doc/MWAND.md`. Export them from
`~/.xinitrc` instead and both pick them up, and you can drop both blocks.

---

## 3b. The `Startup` block

Programs to run when the session starts, so they need not go in `~/.xinitrc`:

```
Startup
{
    dunst
!   mwand
}
```

One command per line — a whole command line, not a name and a value, so
quoting is optional. They run through the same shell as `f.exec`, after the
window manager is ready to manage what they map, and with the `Variables`
block already in effect.

**Run again on `f.restart`.** A restart re-reads this file and starts the
block over from what it now says. Nothing is duplicated: on its way out, the
old instance ends the programs it started from this block and waits for them
to go, so what comes back is one copy of the current configuration rather than
a second copy of the old one. `trayCommand` is ended and started the same way.

That is what makes a restart a refresh of the session rather than of the
window manager alone, and it is how to try out a change to this block — edit
it, restart, and the notification daemon or panel you just changed is the one
running.

It ends what *it* started, and nothing else. Windows you opened yourself,
programs the session file started before mWizard, and anything run from
`f.exec` or the Execute dialog are ordinary clients and are left alone —
a restart has never touched those and still does not.

Nothing here is a substitute for the session file itself. A command that must
run *before* the window manager, such as a wallpaper setter you want painted
first, still belongs in `~/.xinitrc` — and so does anything that should run
once for the whole life of the X session rather than once per window manager.

---

## 4. `Client` blocks

Per-application overrides. The name is the client's resource name or class —
whatever `xprop WM_CLASS` reports:

```
Client XTerm
{
    clientTitle           Terminal
    clientDecoration      -resizeh
    focusAutoRaise        False
}

Client Firefox
{
    usePPosition          on
}
```

To apply a client setting to **every** application, put it in `Settings`
instead; a `Client` block then overrides it for one application:

```
! all clients
Settings
{
    clientDecoration      -resizeh
}

! except this one
Client XTerm
{
    clientDecoration      all
}
```

Braces may sit on the header line or on their own, and a short block can be
written on one line — `Workspace ws0 { title Web }` is fine. Braces inside a
quoted string, such as a menu label, are literal text.

`Client defaults` is a special case: it applies only to clients that set no
`WM_CLASS` at all, not to clients generally.

| Setting | Type | Default |
|---|---|---|
| `clientDecoration` | decoration list | all |
| `clientFunctions` | function list | all |
| `windowMenu` | menu name | `DefaultWindowMenu` |
| `focusAutoRaise` | boolean | policy-dependent |
| `secondariesOnTop` | boolean | dynamic |
| `usePPosition` | `on` \| `off` \| `nonzero` | `nonzero` |
| `overrideGeometry` | geometry | none |
| `maximumClientSize` | `WxH` \| `vertical` \| `horizontal` | `0x0` |
| `matteWidth` | pixels | `0` |
| `useClientIcon` | boolean | `False` |
| `absentMapBehavior` | `add` \| `switch` | `add` |
| `occupyWorkspaces` | workspace names \| `all` | current |

A `Client` block also accepts the per-client icon appearance resources
(`iconImage`, `iconImageForeground`, `iconImageBackground`,
`iconImageTopShadowColor`, `iconImageBottomShadowColor`, and the two
`…ShadowPixmap` variants). These are the one place appearance and behaviour
share a table, so they are accepted here as well as in the X database.

---

## 5. `Workspace` blocks

Workspaces are named `ws0`, `ws1` and so on. The *title* is what you see; the
*name* is how you refer to it here.

```
Workspace ws0
{
    title                 Web
}

Workspace ws1
{
    title                 Code
    iconBoxGeometry       200x100+0-0
}
```

Titles can also be set all at once with `workspaceList` in `Settings`, and
changed at runtime with `f.rename_workspace`.

> EMWM's per-workspace **backdrop** resources have no equivalent. mWizard does
> not draw on the root window at all — see section 9.

---

## 5a. The root menu

The menu on the right button of the desktop is a `Menu` block named by
`DefaultRootMenu`. Its full syntax — labels, mnemonics, accelerators, bitmap
labels — is in `mwizardrc(4)`; what follows is only how the shipped one is
laid out and why.

An entry whose function is `f.menu` is a **pull-right**: it opens the menu
named after it instead of doing something.

```
Menu DefaultRootMenu
{
    "Main Menu"          f.title
    "Programs"      _P   f.menu   ProgramsMenu
    ...
}

Menu ProgramsMenu
{
    "New Terminal"  _T   f.exec "$TERMINAL &"
    ...
}
```

The menu it names is an ordinary `Menu` block, defined anywhere in the file
and free to contain another `f.menu` in turn — so menus nest as deep as you
care to take them, the way mWand's launcher menus do (`doc/MWAND.md`). mWizard
notices a menu that reaches itself and refuses to post it rather than
recursing.

The shipped root menu is organized around that. Four pull-rights carry
everything that belongs to a category:

| Entry | Holds |
|---|---|
| `ProgramsMenu` | the programs from `Variables` — terminal, browser, editor |
| `WindowsMenu` | `f.circle_up`, `f.circle_down`, `f.pack_icons`, `f.refresh` |
| `WorkspacesMenu` | move between workspaces, add one, remove one |
| `SessionMenu` | lock, restart, log out, suspend, reboot, shut down |

and only `f.title`, those pull-rights and `Execute...` are on the first press.
Adding a program means adding a line to `ProgramsMenu`, so the first press
stays the same length however much you add — which is the point of arranging
it this way rather than listing everything at the top level.

Nothing about it is fixed. Move an entry up into `DefaultRootMenu` and it is
back on the first press; the earlier flat menu is a matter of doing that to
all of them. Which menu the desktop posts at all is the `f.menu` argument in
the `Buttons` blocks —

```
<Btn3Down>   root   f.menu   DefaultRootMenu
```

— so a menu of your own replaces it by being named there instead. All three
shipped `Buttons` blocks carry that line; change the one `buttonBindings`
selects, or all of them.

`WorkspacesMenu` carries `f.create_workspace` and `f.delete_workspace`.
Removing a workspace is not a way to lose windows — clients that live only
there are moved to the next workspace first, and the last remaining workspace
cannot be removed.

---

## 6. Session functions

These replace EMWM's XSMP session management, which only did anything when an
external session manager was running.

| Function | Runs | Confirms first |
|---|---|---|
| `f.logout` | ends the session directly | yes |
| `f.reboot` | `rebootCommand` | yes |
| `f.shutdown` | `shutdownCommand` | yes |
| `f.suspend` | `suspendCommand` | no |

Nothing here is hardwired to systemd. The defaults happen to be `systemctl`
invocations, but each is an ordinary string:

```
Settings
{
    shutdownCommand   "loginctl poweroff"
    rebootCommand     "doas /sbin/reboot"
    suspendCommand    "/usr/local/bin/my-suspend-script"
}
```

Setting a command to `""` disables its function: it prints a warning and does
nothing rather than failing in some confusing way.

The confirmation dialogs are governed by `showFeedback`; `showFeedback -quit`
turns them off for all of them, including `f.logout`.

In a menu — in the shipped configuration these are a pull-right of their own,
`SessionMenu`, rather than the tail of the root menu:

```
Menu SessionMenu
{
    "Lock Screen"   _L  f.exec "i3lock -c 000000"
     no-label           f.separator
    "Restart..."    _R  f.restart
    "Log Out..."    _O  f.logout
     no-label           f.separator
    "Suspend"       _S  f.suspend
    "Reboot..."     _b  f.reboot
    "Shut Down..."  _D  f.shutdown
}
```

### What `f.restart` keeps

`f.restart` execs the window manager again — `execvp()` on the arguments it
started with — so an mWizard that has been rebuilt and installed underneath
the running one is **picked up on the spot**. There is no need to end the X
session to try a new build.

The desktop survives it. Clients are handed back to the root window rather
than killed, and the next instance adopts them:

| Kept | How |
|---|---|
| every managed window | reparented to the root, re-adopted from `WM_STATE` |
| which were iconified | `WM_STATE` |
| which workspaces each lived in | `_MWM_WORKSPACE_PRESENCE` on the client |
| where each window is, and its size | the server never forgot |
| which were maximized | `_MWIZARD_RESTART`, written just before the exec |
| which workspace was in front | `_MWIZARD_RESTART_WORKSPACE` on the root |

The last two are new in 1.2. Neither is expressible in ICCCM — `WM_STATE`
knows about iconic and normal and nothing else — so before then a maximized
window came back at its maximized size with the window manager believing that
was its ordinary size, and Restore did nothing; and a session spread over
several workspaces came back on the first one.

An `initialWorkspace` set in the rc file still wins over the remembered one:
that setting says how every *session* should start, and a restart is not a new
session.

What does not survive is anything the window manager did not start and does
not manage — see the `Startup` block (section 3b), which is re-run on restart
precisely so that it does.

---

## 6a. mWrun, the Execute dialog

`f.run` posts a prompt for a command to run. The window is titled **mWrun** as
of 1.3, alongside mWinfo and mWmonitor; nothing else about it changed — the
function is still `f.run` and the shipped menu entry is still "Execute...".

```
Alt<Key>F1 root|icon|window  f.run
"Execute..."                 f.run
```

The command is run the same way an `f.exec` string is, so `Variables`,
pipelines and `&` all work. The dialog keeps the last command, so raising it
again and pressing Return repeats it.

Dismiss it with Cancel or Escape. It carries no Close item in its window menu:
the dialog belongs to the window manager itself, and a window manager that
closes its own window closes the X connection it is running on.

mWand's "Execute..." item posts this same dialog rather than one of its own —
see `doc/MWAND.md`. How it asks is described under
[Asking mWizard for its own windows](#6c-asking-mwizard-for-its-own-windows)
below.

---

## 6b. mWinfo, the About window

`f.about` posts **mWinfo**: the name of the window manager spelled out, the
version and release name it is running, what it is and where it came from,
what this particular session is running on — display, screens, workspaces, X
server and release, extensions in use, Motif version — and a short notice
naming the copyrights and licenses. `f.mwinfo` is an alias for the same
function.

It is bound out of the box:

```
Alt Shift Ctrl<Key>i root|icon|window  f.about
```

mWand's Commands menu carries it as "About mWizard...", also out of the box,
and the `MWINFO` rc keyword puts it in any of mWand's own menus — see
`doc/MWAND.md`. The shipped root menu has an entry for it commented out,
since two ways in are usually enough; uncomment it in `DefaultRootMenu` to get
a third:

```
"About mWizard..."      f.about
```

Unlike the Execute dialog, mWinfo is an ordinary window: move it, resize it,
and close it from its frame like anything else. The notice wraps to the width
it is given and scrolls, so nothing is cut off however small the window is
made. **Project Page** hands the address to `xdg-open`, and **Close** puts the
window away.

At the top is the **motifWizard wordmark**, drawn from a one-bit bitmap
(`src/xbm/mwizard_logo.xbm`) in the label's own foreground and background —
so it follows whatever colors the style file gives the window rather than
carrying its own, and comes out light on a dark scheme. If the pixmap cannot
be made the name is written out in words instead, which is what was there
before 1.2.

---

## 6c. mWmonitor, the monitor arranger

`f.monitors` posts **mWmonitor**: where the monitors are, what mode each is
running, which one is primary, and which are on at all. It does with a mouse
what would otherwise be an `xrandr` command line.

```
Alt Shift Ctrl<Key>m root|icon|window  f.monitors
"Monitors..."                          f.monitors
```

The canvas shows each connected output as a box, drawn to scale. Drag one to
move it; edges snap to their neighbours when they come close, and the whole
arrangement is shifted back to the origin afterwards, so you never have to
think about the fact that RandR coordinates start at 0,0. A disabled output is
outlined but not filled, so it is visibly there to be turned on rather than
simply missing. The primary is marked with an asterisk.

Click a box to select it, then use the controls below:

| Control | Does |
|---|---|
| **Mode** | resolution and refresh, from the modes that output reports |
| **Primary** | makes this the primary monitor |
| **Enabled** | turns the output on or off |

| Button | Does |
|---|---|
| **Apply** | applies the layout now |
| **Save** | applies it *and* writes it to `~/.mmonitors` |
| **Revert** | throws the edits away and re-reads the server |
| **Close** | puts the window away |

Apply re-reads the configuration afterwards rather than assuming it worked, so
what the canvas shows is what the server actually did — a mode can be refused
and an output can fail to light.

At least one monitor has to stay enabled. Turning the last one off is refused,
both by the toggle and by the code underneath it: applying a layout with no
screen in it is a way to lose the session to a black display with nothing on it
to fix the problem with.

Like mWinfo, mWmonitor is an ordinary window — move it, resize it, close it from
its frame.

### Saved layouts: `~/.mmonitors`

RandR keeps nothing across an X restart, so **Save** writes the layout to a file
and mWizard applies it again at startup and on hotplug.

Entries are keyed on the sorted, comma-separated set of *connected* output
names, so a docked laptop and the same laptop on its own are separate entries
and neither is applied in the other's situation:

```
eDP-1,HDMI-1 {
	eDP-1	2560x1600	0x0	60	primary
	HDMI-1	1920x1080	2560x0	60
	DP-2	off
}
```

Each line is an output name, then `WIDTHxHEIGHT`, then `XxY`, then the refresh
in whole Hz, then `primary` if it is the primary. An output that should be off
is written `off` and nothing else. Refresh is matched to the nearest mode the
output offers rather than exactly, so a layout still applies when a driver
update moves a mode by a hertz.

The file is written whole through a temporary file rather than edited in place,
and entries for other monitor sets — and any comments you added — are copied
through untouched. Point `monitorLayoutFile` elsewhere if you want it somewhere
other than `~/.mmonitors`.

### On hotplug

When an output is connected or disconnected, mWizard:

1. applies the saved layout for the set of monitors now attached, if there is
   one;
2. otherwise posts mWmonitor, if `monitorDialogOnHotplug` is `True`;
3. otherwise does nothing but re-read the layout the server gave it.

Windows are re-homed either way: a window left sitting on a monitor that has
just been unplugged is moved onto the nearest one that is left, rather than
stranded in a part of the root window nothing is displaying.

This needs the RandR extension. Without it `f.monitors` reports as much and
mWizard falls back to Xinerama, which can describe the monitors but not change
them.

---

## 6d. Asking mWizard for its own windows

mWrun, mWinfo and mWmonitor belong to the window manager, and mWand asks for
them rather than carrying copies. It asks by sending a `ClientMessage` to the
root window:

| Field | Value |
|---|---|
| `message_type` | `_MWIZARD_COMMAND` |
| `format` | 32 |
| `data.l[0]` | 1 = mWrun, 2 = mWinfo, 3 = mWmonitor |

`_MWIZARD_COMMANDS`, a `CARDINAL` bitmask on the `_NET_SUPPORTING_WM_CHECK`
window, says which of those this mWizard understands. A verb it does not know
is ignored.

Before 1.3 this was done with signals — `SIGUSR1` for the run prompt and
`SIGUSR2` for mWinfo, gated by a matching bit in `_MWIZARD_SIGNALS` for safety,
since an unhandled `SIGUSR1` terminates a process and a terminated window
manager ends the X session. Those still work and mWand still falls back to them
against an older mWizard. They did not survive as the primary mechanism for a
simple reason: a process has exactly two signals it may define for itself, both
were spoken for, and mWmonitor is a third window. A ClientMessage carries a verb
rather than being one, so it does not run out — and it cannot kill anything by
being misaddressed.

---

### Which shell runs commands

`execShell` names the shell used for `f.exec`, the mWrun prompt, the
`Startup` block, and the session and tray commands. Left empty it falls back
to `$WMSHELL`, then `$SHELL`, then `/bin/sh`.

Worth setting to `/bin/sh` if your login shell is a slow interactive one: a
fresh copy is started for every command, sourcing whatever your profile does
each time. It takes precedence over both environment variables so the rc file
can settle this without you having to change your login shell.

---

## 7. Locking the screen

**mWizard does not lock the screen.** It runs a locker you choose. There is no
build or runtime dependency on any particular one.

**On demand** — bind `f.exec` to your locker:

```
Keys DefaultKeyBindings
{
    Alt Ctrl<Key>l  root|icon|window  f.exec "i3lock -c 000000"
}
```

**On idle** — set both `lockCommand` and `lockTimeout`:

```
Settings
{
    lockCommand   "xscreensaver-command -lock"
    lockTimeout   10
}
```

After 10 idle minutes the command runs once; it runs again only after there has
been activity, so a locker is never stacked on top of itself.

The idle timer reads the X idle counter through the MIT-SCREEN-SAVER extension,
which is why the build links `-lXss`. If your build has `IDLE_LOCK` disabled
(see `src/common.mf`), `lockTimeout` prints a warning and is ignored — the
`f.exec` binding above still works. `lockTimeout 0`, the default, disables the
timer entirely.

---

## 8. System tray

mWizard has no tray of its own. `trayCommand` names one to run at startup —
[stalonetray](https://github.com/kolbusa/stalonetray) is the usual pick:

```
Settings
{
    trayCommand   "stalonetray -c /etc/X11/stalonetrayrc"
}
```

Empty by default, in which case nothing is started. The command only runs when
nothing already owns the `_NET_SYSTEM_TRAY_S<screen>` selection, so a tray
mWizard did not start — one from your session file, say — is never duplicated.

A tray mWizard *did* start is ended by `f.restart` along with the `Startup`
block, and started again by the instance that replaces it, so a change to
`trayCommand` or to the tray's own configuration takes effect on a restart.
Icons docked in it re-dock themselves; a tray that handles that badly is an
argument for starting it from `~/.xinitrc` instead.

A stalonetray config tuned for mWizard is installed as
`/etc/X11/stalonetrayrc`. There is no build-time dependency on stalonetray or
on any other tray — it is a string you set.

It configures the tray as an **ordinary framed window**: `decorations all` and
`window_type normal`, so it carries the same Motif borders as anything else and
can be moved and resized by them, while `sticky true` keeps it on every
workspace. To trade that for dock behaviour, set `window_type dock` and
`window_strut auto` — see the sections below for what each implies.

Note that stalonetray's *own* default is `window_type dock`, so a tray started
without `-c` pointing at this file takes the dock path regardless of what the
rc file's `Client` block says.

### What mWizard does for a tray

A window that sets `_NET_WM_WINDOW_TYPE_DOCK` gets:

- the frame and functions named by **`dockDecoration`** and **`dockFunctions`**
  — see below;
- **presence on every workspace** — internally this is the same
  occupy-all-workspaces state `f.occupy_all` sets, so the Occupy Workspace
  dialog shows it as such;
- **reserved screen space**, if it also sets `_NET_WM_STRUT` or
  `_NET_WM_STRUT_PARTIAL`. Maximized windows then stop at the tray edge instead
  of passing underneath, and `_NET_WORKAREA` reports the reduced area to any
  other EWMH client that asks.

`_NET_WM_STATE_STICKY` works on any window, not just docks, and maps onto the
same occupy-all-workspaces state.

Struts that would leave no usable space are ignored rather than honoured — a
client asking to reserve the whole screen is broken, and obeying it would make
everything unmaximizable.

### Giving a dock a minimize button — new in 1.2

EWMH treats a dock as furniture — no frame, move and nothing else. That is
wrong for a tray specifically: it **grows every time something docks an icon
in it**, and with no button to press and only the window itself to grab, a
tray that has spread across the screen can only be dealt with by restarting
it.

So mWizard's default is a title bar with a minimize button. The shipped rc
file also states it explicitly, which is only documentation — the default is
built in, so it applies to an `~/.mwizardrc` that predates 1.2 and says
nothing about docks:

```
Settings
{
    dockDecoration   minimize border
    dockFunctions    move minimize
}
```

Minimize it and it becomes an icon like anything else; the icons docked in it
are still there when it comes back. `minimize` implies `title` — the buttons
live in the title bar — and the matching *function* has to be granted too,
since a button whose function is not permitted is removed again. Add `resizeh`
and `resize` to drag its edges as well; the tray sizes itself to its icons
either way.

Close is deliberately absent: closing a tray strands every icon in it.

For the bare EWMH behaviour instead:

```
Settings
{
    dockDecoration   none
    dockFunctions    move
}
```

**This is the only place either can be set.** The window type is applied
*after* `Client` blocks and overwrites what they set, so `clientDecoration` in
a `Client` block cannot reach a dock — and it is applied *before* the client's
own `_MOTIF_WM_HINTS`, with the frame then intersected against that property,
so a tray asking for `decorations all` cannot add back anything
`dockDecoration` did not grant.

### Taking Close away from the tray

A framed tray can be closed like anything else, which strands whatever icons
are docked in it — nothing short of `f.restart` brings it back. stalonetray has
no option for this: it can ask for decorations, but not for window-manager
functions. Do it from this side instead, as the shipped rc does:

```
Client stalonetray
{
    clientFunctions       -close
}
```

A leading `-` starts from every function and subtracts, so the tray keeps move,
resize, minimize and maximize and picks up anything added later. List the
functions instead — `move minimize` — to allow only those. The block name is
matched against `WM_CLASS`, so check yours with `xprop WM_CLASS`.

### A tray that does not announce itself

Older trays may set neither the dock type nor a strut. Configure one by hand:

```
Client stalonetray
{
    clientDecoration      none
    clientFunctions       move
    occupyWorkspaces      all
}
```

That covers decoration and stickiness. Reserved space is not available this
way — it requires the strut property.

---

## 8a. Media and other XF86 keys

Key names are passed to `XStringToKeysym(3)`, so every name in `XF86keysym.h`
works with no special handling — volume, brightness, playback, and the rest:

```
Keys DefaultKeyBindings
{
    <Key>XF86AudioRaiseVolume root|icon|window f.exec "pactl set-sink-volume @DEFAULT_SINK@ +5%"
    <Key>XF86AudioLowerVolume root|icon|window f.exec "pactl set-sink-volume @DEFAULT_SINK@ -5%"
    <Key>XF86AudioMute        root|icon|window f.exec "pactl set-sink-mute @DEFAULT_SINK@ toggle"
    <Key>XF86AudioMicMute     root|icon|window f.exec "pactl set-source-mute @DEFAULT_SOURCE@ toggle"
    <Key>XF86MonBrightnessUp  root|icon|window f.exec "brightnessctl set +10%"
}
```

A binding needs no modifier at all, and each one is grabbed once per
lock-modifier combination, so it still fires with NumLock or CapsLock on.

Three things to check when one does nothing:

- **The key must be on your keymap.** An unmapped keysym has no keycode and the
  grab silently does nothing. `xev(1)` shows what your key actually sends.
- **The context list decides where it works.** `root|icon|window` grabs it
  everywhere; anything less and the key dies whenever a client has focus.
- **Something else may have grabbed it first.** A desktop agent such as
  `xfce4-volumed` takes these keys before mWizard sees them.

The same applies to any bare key — which is worth remembering for `<Key>Home`:
bound in the `window` context it is taken from every application, so text
fields and terminals stop receiving it.

---

## 8b. Notifications

mWizard has no notification daemon, the same way it has no tray. A dunst
configuration matching the default Motif colors is installed as
`/etc/X11/dunstrc`; copy it to `~/.config/dunst/dunstrc`. It is a plain
configuration file with no build-time dependency on dunst.

dunst sets `_NET_WM_WINDOW_TYPE_DOCK`, so mWizard gives notifications no frame
without any `Client` block — see section 8 for what dock type implies.

### The workspace switch notice — new in 1.3

`workspaceFeedback` announces the new workspace when it changes:

| Value | Does |
|---|---|
| `none` | nothing (the default) |
| `box` | a small popup centred on the monitor that switched |
| `command` | runs `workspaceNotifyCommand` |

`box` needs no daemon and nothing installed. It reuses the same override-redirect
box that shows coordinates during a move, so it follows the `feedback` colors and
font from `~/.mstylesrc` and costs no extra window. It shows "Workspace 2", with
the workspace's title after it when one has been set.
`workspaceFeedbackTimeout` is how long it stays, in milliseconds, default 1000.

`command` is for anyone who would rather it went through their notification
daemon. `$WORKSPACE` holds the number and `$WORKSPACE_NAME` the title; both are
exported rather than substituted, so quoting is the shell's problem the same way
it is for `f.exec`:

```
workspaceFeedback       command
workspaceNotifyCommand  "notify-send -t 1000 \"Workspace $WORKSPACE\""
```

With `perMonitorWorkspaces` on the box appears on the monitor that switched,
which is also the one you are looking at.

---

## 9. Wallpaper

This is the reason mWizard exists.

EMWM created a full-screen, override-redirect window over the root window of
every workspace and kept it lowered but mapped. Anything that paints the root
window painted onto a surface that was permanently covered, so `feh`,
`xsetroot`, `hsetroot` and `xwallpaper` all appeared to do nothing.

mWizard creates no such window. Root-window wallpaper setters work normally:

```sh
feh --bg-scale ~/pictures/wall.png
xsetroot -solid '#2e4053'
hsetroot -fill ~/pictures/wall.png
xwallpaper --zoom ~/pictures/wall.png
```

Put one in your session startup (`~/.xinitrc`, `~/.xsession`) before launching
mWizard, or bind it to `f.exec`.

There is no per-workspace wallpaper. If you want one, bind workspace switching
to a script that changes the wallpaper and then switches:

```
"Web"  f.exec "feh --bg-scale ~/wall-web.png"
```

---

## 10. Where appearance lives

Not here, and no longer in the X resource database either. Colors, shadows,
pixmaps and fonts are in the **style file**, `~/.mstylesrc`, which mWand reads
as well:

```
Fonts
{
    font  fixed
}

Colors
{
    client.background        #8C8C8C
    client.activeBackground  #7399BA
    icon.activeBackground    #7399BA
}
```

The components you can address are `client`, `title`, `icon`, `feedback` and
`menu`, and every color name may be qualified `active` for the window that has
the focus. `mwizard(1)` lists the underlying resources under "Component
Appearance Resources"; [STYLE.md](STYLE.md) documents the file itself,
including the font roles and how a font is written.

**Names are validated** in the style file the same way they are in the rc
file, so a misspelled color is reported rather than ignored.

What is left in the X resource database is `configFile`, which names the rc
file and therefore cannot come from it — and nothing else.

---

## 11. Migrating from EMWM

### Rename your files

```sh
mv ~/.emwmrc ~/.mwizardrc
```

Menus, key bindings and mouse bindings carry over unchanged, with one
exception: **`f.set_behavior` no longer exists.** Remove any binding that uses
it. "Standard behavior" mode was a second, competing configuration source and
was removed along with its four duplicate resource tables.

### Move your behaviour resources

For each `Emwm*something: value` line in your `.Xdefaults` or app-defaults
file, decide which it is:

- **behaviour** — move it into a `Settings` block in `~/.mwizardrc`, dropping
  the prefix and the colon.
- **appearance** (a color, shadow, pixmap or font) — move it into
  `~/.mstylesrc`; see [STYLE.md](STYLE.md).

```
Emwm*moveOpaque:            True         →   Settings { moveOpaque True }
Emwm*keyboardFocusPolicy:   pointer      →   Settings { keyboardFocusPolicy pointer }
Emwm*XTerm*clientDecoration: -resizeh    →   Client XTerm { clientDecoration -resizeh }
Emwm*ws0*title:             Web          →   Workspace ws0 { title Web }
Emwm*client*background:     #8C8C8C      →   Colors { client.background #8C8C8C }
```

Neither file will take the other's names: both validate what they are given
and report a name that does not belong, which is how you find out which of
the two a resource was.

### What was removed, and what replaced it

| EMWM feature | In mWizard |
|---|---|
| Workspace backdrops (`backdrop*image`, `backdropDirectories`) | Removed. Use `feh`/`xsetroot` — see section 8 |
| XSMP session management, `sessionClientDB`, `~/.emwmclientdb` | Removed. `f.logout` / `f.reboot` / `f.shutdown` / `f.suspend` |
| `_MWM_WM_REQUEST` property protocol | Removed. Use the rc functions below |
| `f.set_behavior`, "standard behavior" mode | Removed |
| `f.set_context` | Removed (it only served the request protocol) |
| `cppCommand` | Removed (it drove code that was already compiled out) |
| `ignoreWMSaveHints`, `_MWM_WMSAVE_HINT` | Removed with session management |
| Behaviour X resources | `Settings` / `Client` / `Workspace` blocks |
| Appearance X resources, `app-defaults/Emwm` | `~/.mstylesrc`, shared with mWand |

### `_MWM_WM_REQUEST` replacements

The protocol existed so an external helper could drive workspaces. Each verb
now has an rc function:

| Request verb | rc function |
|---|---|
| `WRKSPACE <id>` | `f.goto_workspace <name>` |
| `ADDWKSPC <title>` | `f.create_workspace` |
| `DELWKSPC <id>` | `f.delete_workspace [<name>]` |
| `NAMWKSPC <id> <title>` | `f.rename_workspace [<name>] <title>` |
| `BACKDROP …` | *(no replacement — backdrops are gone)* |

The functions take workspace **names** (`ws0`, `ws1`) where the protocol took
numeric atom IDs. `f.rename_workspace` renames the active workspace unless the
argument starts with the name of an existing one:

```
"Rename to Web"   f.rename_workspace "Web"
"Name ws0 Web"    f.rename_workspace "ws0 Web"
```

### Properties that did not change

`_MOTIF_WM_HINTS`, `_MWM_WM_HINTS`, `_MWM_WORKSPACE_*`, `WM_STATE`,
`WM_PROTOCOLS` and the `_NET_*` (EWMH) set are unchanged. They are the
interoperability contract that Motif and other toolkits rely on, so renaming
them would break every client that speaks them.

`_MWM_WORKSPACE_INFO` keeps its name but is shorter: it published five backdrop
fields that no longer exist, and now carries the workspace title only.

---

## See also

- `mwizard(1)` — command line options and appearance resources
- `mwizardrc(4)` — full rc file syntax, every function, every binding context
- `doc/STYLE.md` — the style file: fonts and colors, for mWizard and mWand
- `doc/MWAND.md` — configuring mWand, the optional launcher
- `NOTICE` — fork lineage and licensing
