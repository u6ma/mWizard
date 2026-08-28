# Configuring mWand

motifWand, abbreviated mWand, is a small panel for motifWizard: launcher menus,
a workspace switcher, a clock, and an optional session menu. It is optional and is built and installed
on its own.

```sh
make mwand
sudo make install-mwand
```

It talks to the window manager only through standard EWMH properties, so it
works under any window manager that implements them — there is no private
protocol with mWizard and no session manager involved.

mWand derives from `xmtoolbox` in
[emwm-utils](https://github.com/alx210/emwm-utils). Two things changed beyond
the name: configuration moved out of the X resource database into the rc file,
and the session menu no longer talks to `xmsm`.

---

## 1. File locations

mWand reads one file, searched for in this order:

1. the path given by `-rcfile` or the `rcFile` X resource
2. `$HOME/.mwandrc`
3. `$RCDIR/system.mwandrc` (usually `/etc/X11/system.mwandrc`)

To start from the shipped sample:

```sh
cp /etc/X11/system.mwandrc ~/.mwandrc
```

Appearance comes from the style file, `~/.mstylesrc`, which mWizard reads too
— see [STYLE.md](STYLE.md). The app-defaults file mWand used to install as
`/etc/X11/app-defaults/MWand` is gone as of 1.2.

> **`rcFile` is the one setting that cannot live in the rc file**, since it
> names the file the settings live in. Set it with `-rcfile`, or as
> `MWand*rcFile:` in the X database.

---

## 2. Syntax

One file holds both the settings and the menus. Lines beginning with `#` or `!`
are comments.

```
Settings
{
    horizontal        False
    dateTimeFormat    "%m/%d %l:%M %p"
    sessionMenu       True
}

&Programs
{
    &Terminal: xterm
}
```

In the `Settings` block the **name** is the first word and the **value** is
everything after it, so an unquoted multi-word value survives. Quote a value to
keep leading or trailing spaces. Braces may sit on the header line, and a short
block can be written on one line.

**Names are validated.** A misspelling — or an appearance resource, which
belongs in the X database — is reported at startup rather than ignored:

```
mwand: /home/you/.mwandrc line 6: unknown setting "horizontl".
```

### Precedence

Settings are merged into the X resource database under mWand's instance name
(`mwand*name`), which outranks the class form (`MWand*name`) that `.Xdefaults`
uses. **The rc file always wins**; a behaviour resource left over from an
`XmToolbox` setup has no effect.

---

## 3. Settings

| Setting | Type | Default |
|---|---|---|
| `title` | string | `mWand` |
| `horizontal` | boolean | `False` |
| `separators` | boolean | `True` |
| `workspaceSwitcher` | boolean | `True` |
| `dateTimeDisplay` | boolean | `True` |
| `dateTimeFormat` | `strftime(3)` format | `"%m/%d %l:%M %p"` |
| `userHostDisplay` | boolean | `True` |
| `occupyAllWorkspaces` | boolean | `True` |
| `occupyAllMonitors` | boolean | `False` |
| `hotkey` | keysym | *(unset)* |
| `sessionMenu` | boolean | `True` |
| `lockCommand` | command | *(empty)* |
| `logoutCommand` | command | *(empty)* |
| `suspendCommand` | command | `systemctl suspend` |
| `rebootCommand` | command | `systemctl reboot` |
| `shutdownCommand` | command | `systemctl poweroff` |

`workspaceSwitcher` needs an EWMH window manager, and the switcher is hidden
regardless when there is only one workspace.

`userHostDisplay` puts who you are and where — `alex@box` — on the last line
of the panel. xmtoolbox showed that in the window title, where a narrow title
bar simply clips it; on a line of its own the panel widens to fit instead, and
`title` is free to say what the program is. The host name is shortened to its
first component, so `box.example.org` shows as `box`. Neither half changes
while mWand runs, so the line is written once. If the system will not name the
host, the login is shown alone; if it will not name either, the line is left
out.

`hotkey` takes a keysym name, optionally with modifiers, e.g. `Super_L` or
`Alt<Key>space`.

---

## 3a. The `Variables` block

Names a program once so the menus can refer to it:

```
Variables
{
    TERMINAL   xterm
    BROWSER    chromium-bin
    EDITOR     nedit
}
```

Each entry is exported to the environment, and menu commands are expanded
before they run, so `$NAME` or `${NAME}` in any command is substituted:

```
&Programs
{
    &Web Browser: $BROWSER
    &Text Editor: $EDITOR
    &E-Mail: $TERMINAL -title "E-Mail" -e mutt
}
```

mWizard's rc file takes the same block with the same spelling, so the two
configurations stay readable side by side. They are separate programs, though:
mWand does not inherit mWizard's variables unless mWizard started it, so keep
the two blocks in step — or export the names from `~/.xinitrc` and delete both.

**One difference from mWizard.** There, `f.exec` runs through `sh -c` and the
shell does the expanding. Here commands are `execvp`'d directly and mWand does
the expanding itself, so there is no shell: no `&`, no pipes or redirection,
and no `${NAME:-default}` fallback. An unset variable expands to nothing and
warns on stderr.

Names must look like shell variable names — a letter or underscore followed by
letters, digits or underscores. Anything else is reported and ignored.

---

## 4. Menus

A top-level menu is a title followed by a brace block. `&` marks the keyboard
mnemonic, a colon separates an item's title from its command, and `SEPARATOR`
places a separator. Menus nest.

`MWINFO` and `MONITORS` are the items that are not commands: they post
**mWinfo**, the window manager's About window, and **mWmonitor**, its monitor
arranger. Both are written bare like `SEPARATOR` because there is nothing to
give them — mWand supplies the labels and asks the window manager for the
windows (see `doc/CONFIGURATION.md` §6b and §6c). They work in any menu, at any
depth, and need no window manager configuration. `MONITORS` needs mWizard 1.3.

How mWand asks is described in `doc/CONFIGURATION.md` §6d: a `_MWIZARD_COMMAND`
ClientMessage to the root window, gated on the `_MWIZARD_COMMANDS` bitmask that
says which windows this mWizard understands. Against a mWizard older than 1.3
it falls back to the signals that mechanism replaced. Under any other window
manager the item reports that the window manager does not provide the window.

```
&Utilities
{
    &XTerm: xterm
    SEPARATOR
    MONITORS
    MWINFO
    SEPARATOR
    X11 &Utilities
    {
        &Text Editor: xedit
        Ca&lculator: xcalc
    }
}

&Locations
{
    &Home: xfile $HOME
    &Media: xfile /media/$LOGNAME
}
```

Command strings may contain environment variables in `sh(1)` syntax, `$name`
or `${name}`.

> A top-level menu titled exactly `Settings` would be taken for the settings
> block. Write `&Settings`, or pick another title.

Send `SIGUSR1` to re-read the file. On a parse error the previous configuration
stays active and the error is reported.

---

## 5. The session menu

> **"Execute...", "Monitors..." and "About mWizard..." are the window
> manager's windows.** mWand used to carry its own command prompt; it now asks
> mWizard to post one, since a run prompt is wanted with or without a panel,
> and mWinfo and mWmonitor work the same way. mWand finds the window manager
> through `_NET_SUPPORTING_WM_CHECK` and sends it a `_MWIZARD_COMMAND`
> ClientMessage, so it needs no helper program. Under a window manager that
> does not answer, the item reports that there is nothing to ask.
> Bind `f.run`, `f.monitors` and `f.about` in `~/.mwizardrc` to reach the same
> windows from the keyboard; two of the three already are.


The command menu always holds three window manager utilities: **Execute...**,
which prompts for a command to run, **Monitors...**, which posts mWmonitor for
arranging the monitors, and **About mWizard...**, which posts mWinfo — the
name, version, licenses and project page. With `sessionMenu True` it also holds
the session actions and is labelled *Session*; with `sessionMenu False` it
holds only those three and is labelled *Commands*.

| Item | Runs | Confirms first |
|---|---|---|
| Lock | `lockCommand` | no |
| Suspend | `suspendCommand` | no |
| Log Out... | `logoutCommand` | yes |
| Reboot... | `rebootCommand` | yes |
| Shut Down... | `shutdownCommand` | yes |

**An entry whose command is empty is not shown at all.** There is no point
offering *Lock* when nothing has been configured to lock with. `lockCommand`
and `logoutCommand` are empty by default, so out of the box the menu shows
Suspend, Reboot and Shut Down.

Nothing here is hardwired to systemd:

```
Settings
{
    lockCommand       "i3lock -c 000000"
    logoutCommand     "pkill mwizard"
    shutdownCommand   "loginctl poweroff"
}
```

mWizard has the same functions in its own root menu (`f.logout`, `f.reboot`,
`f.shutdown`, `f.suspend`). If you use those, `sessionMenu False` avoids
having them in two places.

---

## 6. Where appearance lives

In the **style file**, `~/.mstylesrc`, shared with mWizard. A block with no
program name applies to both; one written `Fonts mwand` applies to mWand
alone.

```
Fonts
{
    font  fixed
}

Fonts mwand
{
    panelFont  "Liberation Sans:9"
}
```

The font roles mWand draws are `font` (everything), `menuFont` (menu items),
`menuTitleFont` (menu titles), `dialogFont` (message boxes) and `panelFont`
(the clock and the `user@host` line). Colors go in a `Colors` block, where
`panel.` addresses everything mWand draws and `menu.` its menus.
[STYLE.md](STYLE.md) has the whole list.

Sharing the file is the point: a panel whose menus are not the window
manager's menus is a panel that looks bolted on.

### What is still an X resource

Three settings are read by Xt when mWand's shell is created, before any file
has been opened, so they cannot come from either file:

```
MWand.x: 8
MWand.y: 28
MWand.mwmDecorations: 58
```

Those are the built-in defaults; override them in `.Xdefaults` if you need to.
`mwmDecorations` is a bitmask composed from `MwmUtil.h`. Useful values:
borderless with title buttons `56`, borderless without `8`, borders with title
buttons `58`, borders without `10`.

---

## 7. Migrating from xmtoolbox

```sh
mv ~/.toolboxrc ~/.mwandrc
```

Menus carry over unchanged. Then, for each `XmToolbox*name: value` line in your
`.Xdefaults`:

- **everything but fonts** — move it into a `Settings` block in `~/.mwandrc`,
  dropping the prefix and the colon.
- **fonts and render tables** — move them into `~/.mstylesrc`; see
  [STYLE.md](STYLE.md).

```
XmToolbox*horizontal:      True    →   Settings { horizontal True }
XmToolbox*dateTimeFormat:  %R      →   Settings { dateTimeFormat "%R" }
XmToolbox*renderTable...           →   Fonts { font "Liberation Sans:10" }
```

`x`, `y` and `mwmDecorations` stay in the X database — they are shell
resources read by Xt before mWand sees them.

### What changed

| xmtoolbox | mWand |
|---|---|
| Session menu driven by `xmsm` over private IPC | Runs commands you configure |
| Menu hidden per `xmsm` config flags | `sessionMenu`, plus per-entry commands |
| "xmsm not running?" error at startup | Gone; mWand needs no session manager |
| Behaviour in `XmToolbox.ad` / `.Xdefaults` | `Settings` block in the rc file |
| Fonts in `XmToolbox.ad` / `.Xdefaults` | `~/.mstylesrc`, shared with mWizard |
| One 1400-line `tbmain.c` | Split by concern; see `mwand/src/mwand.h` |
| Window titled `user@host` | Titled `mWand`; `user@host` is a line in the panel (`userHostDisplay`) |

---

## See also

- `mwand(1)`
- `doc/STYLE.md` — the style file: fonts and colors, for mWand and mWizard
- `doc/CONFIGURATION.md` — configuring mWizard itself
- `NOTICE` — licensing, including the notices inherited from emwm-utils
