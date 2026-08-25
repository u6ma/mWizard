# Configuring mWizard

mWizard is configured from **one file**: `~/.mwizardrc`.

EMWM split configuration in two. Key bindings, mouse bindings and menus lived
in `~/.emwmrc`, while every behaviour knob — focus policy, window placement,
decoration, workspaces — lived in the X resource database, as
`Emwm*someResource:` lines in an app-defaults file or `.Xdefaults`. Changing
how the window manager behaved meant editing two files in two syntaxes.

In mWizard the rc file holds behaviour as well, in `Settings`, `Client` and
`Workspace` blocks. The X resource database is left holding the one thing it
is genuinely good at: **appearance** — colors, shadows, pixmaps and fonts.

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

Appearance still comes from the X resource database, whose app-defaults file is
installed as `/etc/X11/app-defaults/MWizard`.

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

| Setting | Type | Default |
|---|---|---|
| `primaryXineramaScreen` | index | `0` |
| `xineramaScreenFocus` | `pointer` \| `keyboard` | `pointer` |
| `xineramaIconifyToPrimary` | boolean | `False` |
| `multiScreen` | boolean | `True` |
| `screens` | screen names | none |

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

See sections 6 and 7.

---

## 4. `Client` blocks

Per-application overrides. The name is the client's resource name or class —
whatever `xprop WM_CLASS` reports:

```
Client XTerm
{
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
> not draw on the root window at all — see section 8.

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

In a menu:

```
Menu DefaultRootMenu
{
    "Lock Screen"   f.exec "i3lock -c 000000"
     no-label       f.separator
    "Restart..."    f.restart
    "Log Out..."    f.logout
     no-label       f.separator
    "Suspend"       f.suspend
    "Reboot..."     f.reboot
    "Shut Down..."  f.shutdown
}
```

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

## 8. Wallpaper

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

## 9. What stays in the X resource database

Appearance only. These are Motif Component Appearance resources; the rc file has
no business owning fonts and shadow pixmaps.

The shipped `/etc/X11/app-defaults/MWizard`:

```
!! Window decoration colors
MWizard*client*background: #8C8C8C
MWizard*client*activeBackground: #7399BA
MWizard*icon*activeBackground: #7399BA

!! The Workspace Presence dialog
MWizard*feedback*XmPushButton.marginWidth: 5
MWizard*feedback*XmPushButton.marginHeight: 5

!! Default font (menus and dialogs)
MWizard*renderTable: variable
MWizard*renderTable.variable.fontType: FONT_IS_XFT
MWizard*renderTable.variable.fontName: Liberation Sans
MWizard*renderTable.variable.fontSize: 10

!! Title bar font
MWizard*title.renderTable: title
MWizard*renderTable.title.fontType: FONT_IS_XFT
MWizard*renderTable.title.fontName: Liberation Sans
MWizard*renderTable.title.fontSize: 10
MWizard*renderTable.title.fontStyle: Bold

!! Icon label font
MWizard*icon.renderTable: icon
MWizard*renderTable.icon.fontType: FONT_IS_XFT
MWizard*renderTable.icon.fontName: Liberation Sans
MWizard*renderTable.icon.fontSize: 10
MWizard*renderTable.icon.fontStyle: Italic
```

The component parts you can address are `client`, `icon`, `menu`, `feedback`
and `title`, each optionally qualified `active`. `mwizard(1)` lists every
appearance resource under "Component Appearance Resources".

---

## 10. Migrating from EMWM

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

- **appearance** (a color, shadow, pixmap or font) — change the prefix to
  `MWizard*` and leave it where it is.
- **behaviour** (everything else) — move it into a `Settings` block, dropping
  the prefix and the colon.

```
Emwm*moveOpaque:            True         →   Settings { moveOpaque True }
Emwm*keyboardFocusPolicy:   pointer      →   Settings { keyboardFocusPolicy pointer }
Emwm*XTerm*clientDecoration: -resizeh    →   Client XTerm { clientDecoration -resizeh }
Emwm*ws0*title:             Web          →   Workspace ws0 { title Web }
Emwm*client*background:     #8C8C8C      →   MWizard*client*background: #8C8C8C
```

If you are not sure which a resource is, put it in `Settings`: an appearance
name is rejected with a message naming the line, which tells you it belongs in
the X database.

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
- `NOTICE` — fork lineage and licensing
