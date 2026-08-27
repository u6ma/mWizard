# Styling mWizard and mWand

Appearance for the whole project lives in **one file**: `~/.mstylesrc`.

Behaviour lives elsewhere — `~/.mwizardrc` for the window manager,
`~/.mwandrc` for the panel. That split is the same one
[CONFIGURATION.md](CONFIGURATION.md) describes; this file is the other half of
it.

Before 1.2 appearance was two app-defaults files, `MWizard` and `MWand`, in X
resource syntax, found through `XFILESEARCHPATH`. Two files in a different
language from the rc file the user had just been editing, describing one
desktop that is meant to look like a single thing — and silently ineffective
if the search path did not happen to include them. The style file replaces
both.

---

## 1. File locations

Both programs look for the style file in this order:

1. the path in the `MSTYLESRC` environment variable, if set
2. `$HOME/.mstylesrc`
3. `$RCDIR/system.mstylesrc` (`$RCDIR` is set at build time, usually
   `/etc/X11`)

To start from the shipped defaults:

```sh
cp /etc/X11/system.mstylesrc ~/.mstylesrc
```

`MSTYLESRC` is an environment variable rather than an X resource or an rc
setting because both programs have to arrive at the same answer and they
share no configuration. It also makes a second style usable without editing
anything:

```sh
MSTYLESRC=~/dark.mstylesrc mwizard
```

There is no style file to find at all on a fresh checkout that was never
installed; both programs then draw everything in Motif's default font, which
is what `font fixed` below asks for anyway.

---

## 2. Syntax

The same syntax the rc files use. A line whose first character is `!` or `#`
is a comment. The **name** is the first word, the **value** is everything
after it, so an unquoted multi-word value survives and quotes are only needed
to keep leading or trailing spaces.

```
Fonts
{
    font   "Liberation Sans:10"
}
```

A comment has to be on a line of its own. `font fixed  ! nice and small` sets
the font to `fixed  ! nice and small`.

### Which program a block applies to

A block with no program name applies to **both**:

```
Fonts { font fixed }
```

One that names a program applies to that program alone:

```
Fonts mwand { panelFont "Liberation Sans:9" }
Colors mwizard { client.activeBackground #7399BA }
```

Sharing is the default on purpose: a panel whose menus are not the window
manager's menus is a panel that looks bolted on.

### Precedence

Entries are merged into the X resource database under each program's
**instance** name — `mwizard*…` and `mwand*…` — which outranks the class-name
form (`MWizard*…`, `MWand*…`) that `.Xdefaults` files use, and replaces an
identical key outright.

**The style file wins.** An appearance resource left over in `.Xdefaults`
from an EMWM setup has no effect on anything the style file also names.

---

## 3. `Fonts`

```
Fonts
{
    font  fixed
}
```

`font` is the **base**: everything both programs draw uses it unless one of
the roles below overrides it. That one line is the whole font configuration
for most people.

The default is `fixed`, Motif's own font — pixelated, on every X server there
is, and needing no fontconfig. It is what mWizard's menus were always drawn
in; before 1.2 it was also the only thing that could not be changed.

### Writing a font

A font is either a **core X font** — an XLFD, or an alias such as `fixed` —
or an **Xft font**, written `family:size` or `family:size:style`. The colon is
what tells them apart, which is unambiguous because an XLFD cannot contain
one.

| Written | Means |
|---|---|
| `fixed` | core font, by alias |
| `-*-helvetica-medium-r-normal--12-*-*-*-*-*-iso8859-1` | core font, by XLFD |
| `Liberation Sans:10` | Xft, 10 point |
| `Liberation Sans:10:bold` | Xft, 10 point bold |
| `Liberation Sans:11:italic` | Xft, 11 point italic |

`xlsfonts(1)` lists the core fonts the server has; `fc-list(1)` lists the Xft
ones. A size given for a core font is ignored — the size is part of the XLFD.

### If the core font is not there

Both programs check that a core font actually loads before handing it to
Motif, and say so if it does not:

```
mwizard: no core font matches "fixed"; using another the server does have.
```

The check exists because Motif does not do it. Name a core font the server
lacks and the failure is carried all the way to the first `XSetFont`, where it
comes back as a `BadFont` from the server and takes the program down without
mentioning a font name — the least informative outcome for the mistake that is
easiest to make.

It matters most for the default. `fixed` is Motif's own font and was on every
X server for twenty years, but an Xorg install without the legacy bitmap font
packages (`xorg-fonts-misc`, `xfonts-base`) has **no core fonts at all**, and
fontconfig will not answer for them either. On such a system, name an Xft font:

```
Fonts { font "Liberation Sans:10" }
```

Xft specs are not checked — fontconfig always answers with something.

### The roles

Every role is optional, and every one falls back to `font`.

| Role | Applies to | Program |
|---|---|---|
| `font` | everything not named below | both |
| `titleFont` | window title bars | mWizard |
| `iconFont` | icon labels | mWizard |
| `menuFont` | menu items | both |
| `menuTitleFont` | menu titles and cascades | both |
| `feedbackFont` | move/resize boxes, workspace presence | mWizard |
| `dialogFont` | the Execute window, mWinfo, message boxes | both |
| `panelFont` | the clock and the `user@host` line | mWand |

A role the reading program does not draw is accepted and does nothing, which
is what lets one unqualified `Fonts` block serve both.

```
Fonts
{
    font           "Liberation Sans:10"
    titleFont      "Liberation Sans:10:bold"
    iconFont       "Liberation Sans:10:italic"
    menuFont       fixed
}
```

### Menus

Menu items are the one thing that does not take its font from the resource
database. Motif resolves the font of a menu gadget from its ancestor menu
shell when nothing more specific applies, which is why menus stayed in
Motif's default font under the old app-defaults files while everything else
followed them. Both programs now name the render table on each entry as it is
built, from `menuFont` or from the base font — see `src/WmMenu.c` and
`mwand/src/launcher.c`.

---

## 4. `Colors`

Motif component appearance resources. Written on their own they apply to
everything; written `component.name` they apply to one part.

```
Colors
{
    client.background        #8C8C8C
    client.activeBackground  #7399BA
    icon.activeBackground    #7399BA
}
```

### Components

| Component | Is | Program |
|---|---|---|
| *(none)* | everything | both |
| `client` | window frames | mWizard |
| `title` | title bars, when they differ from the frame | mWizard |
| `icon` | icons and the icon box | mWizard |
| `feedback` | move/resize boxes, workspace presence | mWizard |
| `menu` | menus | both |
| `panel` | everything mWand draws | mWand |

A component the reading program does not draw is skipped rather than
reported, for the same reason a font role is.

### Names

`background`, `foreground`, `topShadowColor`, `bottomShadowColor`,
`backgroundPixmap`, `topShadowPixmap`, `bottomShadowPixmap` — and each of
those again with an `active` prefix, for the window that has the focus:
`activeBackground`, `activeForeground`, `activeTopShadowColor`,
`activeBottomShadowColor`, `activeBackgroundPixmap`, `activeTopShadowPixmap`,
`activeBottomShadowPixmap`.

Colors are anything `XParseColor(3)` takes: `#8C8C8C`, `rgb:8c/8c/8c`, or a
name from `rgb.txt`. Pixmaps are the names Motif knows —
`background`, `25_foreground`, `50_foreground`, `75_foreground`,
`vertical_tile`, `horizontal_tile`, and the rest.

**Names are validated.** A misspelling, or a behaviour setting that belongs in
an rc file, is reported on stderr at startup rather than silently ignored:

```
mwizard: style file line 12: unknown color "activeBackgroudn".
```

---

## 5. `Resources`

The escape hatch. Entries are written into the resource database untouched,
under the program's own name, for appearance that is neither a font nor a
color — margins, shadow thicknesses, spacing.

```
Resources mwizard
{
    feedback*XmPushButton.marginWidth   5
    feedback*XmPushButton.marginHeight  5
}

Resources mwand
{
    mainFrame.shadowThickness  1
}
```

`feedback*XmPushButton.marginWidth 5` becomes
`mwizard*feedback*XmPushButton.marginWidth: 5`.

Nothing here is validated, because there is no list to validate against. It is
also the only block where a typo is silent.

---

## 6. What is still an X resource

Two of mWand's settings are read by Xt when its shell is created, which
happens before any file has been opened, so they cannot come from here:

```
MWand.x: 8
MWand.y: 28
MWand.mwmDecorations: 58
```

They have built-in defaults and are otherwise `.Xdefaults` matters.
`mwmDecorations` is a bitmask composed from `MwmUtil.h`: borderless with title
buttons `56`, borderless without `8`, borders with title buttons `58`, borders
without `10`.

Everything else that used to live in `app-defaults/MWizard` and
`app-defaults/MWand` is in the style file. Both app-defaults files are gone;
`make uninstall` removes any copy an earlier version installed.

---

## 7. Migrating from 1.1

Your app-defaults files, if you edited them, translate line by line.

```
MWizard*renderTable.variable.fontName: Liberation Sans   →  Fonts { font "Liberation Sans:10" }
MWizard*renderTable.variable.fontSize: 10

MWizard*renderTable.title.fontName: Liberation Sans      →  Fonts { titleFont "Liberation Sans:10:bold" }
MWizard*renderTable.title.fontStyle: Bold

MWizard*client*background: #8C8C8C                       →  Colors { client.background #8C8C8C }

MWand*renderTable.menu.fontName: Liberation Sans         →  Fonts mwand { menuFont "Liberation Sans:10" }
```

The four-line rendition dance — `fontType`, `fontName`, `fontSize`,
`fontStyle` under a tag you had to invent — collapses into one value. The
rendition tags themselves are gone; the style file makes them.

Then delete the old files:

```sh
rm -f /etc/X11/app-defaults/MWizard /etc/X11/app-defaults/MWand
```

`make uninstall` does this too, so an upgrade that later gets uninstalled does
not strand them.

---

## See also

- `doc/CONFIGURATION.md` — configuring mWizard's behaviour
- `doc/MWAND.md` — configuring mWand's behaviour
- `mwizard(1)`, `mwand(1)`
