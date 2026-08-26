# Configuring mWand

mWand is a small panel for mWizard: launcher menus, a workspace switcher, a
clock, and an optional session menu. It is optional and is built and installed
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

Appearance comes from `/etc/X11/app-defaults/MWand`.

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
| `title` | string | `user@host` |
| `horizontal` | boolean | `False` |
| `separators` | boolean | `True` |
| `workspaceSwitcher` | boolean | `True` |
| `dateTimeDisplay` | boolean | `True` |
| `dateTimeFormat` | `strftime(3)` format | `"%m/%d %l:%M %p"` |
| `occupyAllWorkspaces` | boolean | `True` |
| `hotkey` | keysym | *(unset)* |
| `sessionMenu` | boolean | `True` |
| `lockCommand` | command | *(empty)* |
| `logoutCommand` | command | *(empty)* |
| `suspendCommand` | command | `systemctl suspend` |
| `rebootCommand` | command | `systemctl reboot` |
| `shutdownCommand` | command | `systemctl poweroff` |

`workspaceSwitcher` needs an EWMH window manager, and the switcher is hidden
regardless when there is only one workspace.

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

```
&Utilities
{
    &XTerm: xterm
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

The command menu always holds **Execute...**, which prompts for a command to
run. With `sessionMenu True` it also holds the session actions and is labelled
*Session*; with `sessionMenu False` it holds only Execute and is labelled
*Commands*.

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

## 6. What stays in the X resource database

Fonts and render tables only, in `/etc/X11/app-defaults/MWand`. The addressable
parts are `XmPushButtonGadget` (menu items), `XmCascadeButtonGadget` (menu
titles), `dateTime` (the clock) and `mainFrame`.

```
MWand.x: 8
MWand.y: 28
MWand.mwmDecorations: 58

*XmPushButtonGadget.renderTable: menu
*renderTable.menu.fontType: FONT_IS_XFT
*renderTable.menu.fontName: Liberation Sans
*renderTable.menu.fontSize: 10
```

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

- **fonts and render tables** — change the prefix to `MWand*` and leave them
  where they are.
- **everything else** — move it into a `Settings` block, dropping the prefix
  and the colon.

```
XmToolbox*horizontal:      True    →   Settings { horizontal True }
XmToolbox*dateTimeFormat:  %R      →   Settings { dateTimeFormat "%R" }
XmToolbox*renderTable...           →   MWand*renderTable...
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
| One 1400-line `tbmain.c` | Split by concern; see `mwand/src/mwand.h` |

---

## See also

- `mwand(1)`
- `doc/CONFIGURATION.md` — configuring mWizard itself
- `NOTICE` — licensing, including the notices inherited from emwm-utils
