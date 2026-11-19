# Complex DWM Slock

A secure, visually polished screen locker for DWM and other X11 environments.
It blurs the live desktop, shows a live clock, and uses PAM for authentication.

---

## Features

- **Blurred, dimmed background** — captures the desktop before the lock window
  exists, blurs it (downscale → 3-pass box blur ≈ gaussian → bilinear upscale),
  applies a dim and vignette, and caches the result as a server-side pixmap.
  The lock appears almost instantly and nothing is re-blurred on keypress.
- **Flicker-free rendering** — everything is drawn to a back buffer and copied
  to the window once per frame.
- **Two-state UI**
  - *Idle* — large clock, date, and "Click or press a key to unlock" hint.
    The clock updates every second.
  - *Active* — clock moves up; a glass panel shows a circular avatar (anti-aliased,
    with ring and soft shadow), greeting, full name, and a pill-shaped password
    field with dot echo. Activated by any keypress or mouse click.
- **Visual auth feedback**
  - Wrong password: the field shakes, full-screen red tint, "Incorrect password"
    and a colour-coded remaining-attempts counter (yellow → orange → red).
  - Correct password: full-screen green tint + "Access Granted" for one second.
  - Caps Lock warning while typing.
- **Multi-monitor aware** — the UI is centred on the primary monitor; the blurred
  background covers all screens.
- **Per-user avatar and font** — drop files in `~/.config/slock/` (see below).
- **PAM authentication** — uses the system PAM stack; credentials are zeroed
  from memory immediately after verification.
- **Security hardening**
  - `XGrabKeyboard` + `XGrabPointer` (with retries) prevent other X clients from
    stealing input.
  - `getpwuid(getuid())` is used instead of `getenv("USER")` / `$HOME` — immune
    to environment spoofing in a setuid binary.
  - User-owned files (avatar, font) are read with the effective UID temporarily
    dropped to the real user, and only if they are regular files.
  - `execve()` with an empty environment is used instead of `system()`.
  - The signal handler only sets a `volatile sig_atomic_t` flag.
- **Brute-force fallback** — after `MAX_ATTEMPTS` failures the display manager
  is restarted via `systemctl` (probes `display-manager.service` then specific
  service names). Falls back to `chvt 1` if no display manager is found.
- **Automatic versioning** — build date and Git short hash embedded in the
  binary (`slock -v`).

---

## Prerequisites

### Runtime libraries

| Library | Package (Arch) |
|---------|----------------|
| libX11, libXext, libXrandr | `libx11` `libxext` `libxrandr` |
| libXrender | `libxrender` |
| libXft, libfontconfig | `libxft` `fontconfig` |
| Imlib2 | `imlib2` |
| libpam | `pam` |

### Build tools

- GCC or Clang
- GNU Make
- `pkg-config` (`pkgconf` on Arch)
- Git (for version embedding; falls back to `unknown` if absent)

### PAM service file — **required before first use**

`slock` authenticates through the PAM service named `slock`.
Without `/etc/pam.d/slock` every password attempt will fail.
The AUR package installs this file for you; for a manual install create it:

```bash
sudo tee /etc/pam.d/slock << 'PAM'
auth     include  system-auth
account  include  system-auth
PAM
```

On systems without `system-auth` (Debian/Ubuntu), use `common-auth` /
`common-account` instead.

---

## Installation

```bash
git clone https://github.com/fam007e/Complex_DWM_SLock.git
cd Complex_DWM_SLock

make
sudo make install          # installs to /usr/local/bin and /usr/local/share/slock
```

The binary is installed setuid root (mode `4755`) so PAM can authenticate
against the shadow password database. The default avatar and the bundled font
are installed to `$(DATADIR)` (`/usr/local/share/slock/` by default).

### Custom prefix

`DATADIR` is compiled into the binary, so run `make clean` when changing it.

```bash
make clean
make PREFIX=/opt/slock
sudo make PREFIX=/opt/slock install
```

### Uninstall

```bash
sudo make uninstall        # removes the binary and /usr/local/share/slock/
```

### AUR (Arch Linux)

```bash
yay -S complex-dwm-slock-git
# or
paru -S complex-dwm-slock-git
```

The package installs to `/usr/bin/slock` and `/usr/share/slock/`, creates
`/etc/pam.d/slock`, and conflicts with the suckless `slock` package.

[![AUR package](https://img.shields.io/aur/version/complex-dwm-slock-git?logo=arch-linux)](https://aur.archlinux.org/packages/complex-dwm-slock-git)

---

## Usage

```bash
slock              # lock the screen
slock -v           # print version (slock-YYYY.MM.DD.<git-hash>)
slock -t           # test mode (see below)
```

### Test mode (`-t`)

Runs the locker against the live X session without setuid.
- PAM authentication is still active — the correct user password unlocks it.
- `Esc` exits without restarting the display manager, and `q` does too while
  the password field is empty (so passwords containing `q` still work).
- Assets fall back to `assets/images/avatar.png` and `assets/fonts/font.ttf`
  when the installed copies do not exist, so it works from the project root
  before installation.

```bash
make test
# or
./slock -t
```

---

## Customisation

### Avatar

The first file found is used:

1. `~/.config/slock/avatar.png`
2. `~/.face`
3. `$(DATADIR)/avatar.png` (installed default)
4. `assets/images/avatar.png` (project-root fallback)

```bash
mkdir -p ~/.config/slock
cp ~/Pictures/me.png ~/.config/slock/avatar.png
```

Any size and any format Imlib2 supports works; the image is centre-cropped to a
square and scaled. The home directory is taken from the passwd database, not
from `$HOME`.

### Font

The first font file found is registered with fontconfig and used for all text
(the family name is detected automatically):

1. `~/.config/slock/font.ttf`
2. `$(DATADIR)/font.ttf` (the bundled font)
3. `assets/fonts/font.ttf` (project-root fallback)

If none exists, the fontconfig family named by `FONT` in `config.h` is used
(`sans-serif` by default, with the light weight for the clock).

### Display name

The greeting shows the full name from the GECOS field of your passwd entry
(`chfn` to change it), falling back to the login name.

---

## Configuration

Compile-time options live in `src/config.h`. Edit it, then rebuild:

```bash
make clean && make && sudo make install
```

### Key settings

| Define | Default | Description |
|--------|---------|-------------|
| `AVATAR_SIZE` | `150` | Avatar diameter in pixels |
| `RING_W` / `RING_GAP` | `3` / `4` | Avatar ring thickness / gap to photo |
| `SHADOW_R` | `18` | Avatar soft-shadow radius |
| `MAX_ATTEMPTS` | `10` | Failed attempts before DM restart |
| `BLUR_SCALE` | `4` | Blur runs on a 1/N-size copy (speed) |
| `BLUR_RADIUS` | `5` | Blur radius on the small copy; effective radius is `BLUR_SCALE × BLUR_RADIUS` px |
| `DIM_AMOUNT` | `0.30` | Uniform background darkening |
| `VIGNETTE` | `0.35` | Extra darkening towards the corners |
| `FONT` | `sans-serif` | Fontconfig family used when no font file is found |
| `FONT_SIZE` | `12` | Username / password field font size |
| `TIME_FONT_SIZE` | `64` | Clock font size |
| `DATE_FONT_SIZE` | `18` | Date and display-name font size |
| `HINT_FONT_SIZE` | `11` | Hint / warning font size |
| `TIME_FORMAT` | `%H:%M` | `strftime` format for the clock |
| `DATE_FORMAT` | `%A %B %d` | `strftime` format for the date |
| `PASSWD_FIELD_W` | `280` | Password field width (px) |
| `PASSWD_FIELD_H` | `34` | Password field height (px) |
| `COLOR_*` | see file | Success / failure / warning colours |

Tuning tips: if the background still feels busy, raise `DIM_AMOUNT` (≈ 0.4); if
blur edges look soft, lower `BLUR_SCALE` to 3 (slower).

---

## Security considerations

- **Setuid root** is required. The binary has no network access and no shell
  invocations (`system()` is not used anywhere).
- **Image and font loading** happens with the effective UID dropped to the real
  user, so a symlink in `~/.config/slock/` cannot be used to read root-only
  files. Root remains the saved UID, so this is a mitigation, not a sandbox:
  only put images you trust there.
- **VT switching** (`Ctrl+Alt+Fn`) can bypass the locker if not disabled.
  Add the following to your Xorg configuration to prevent it:

  ```
  # /etc/X11/xorg.conf.d/90-nodonttwitch.conf
  Section "ServerFlags"
      Option "DontVTSwitch" "true"
  EndSection
  ```

- **Ctrl+Alt+Backspace** (Xorg kill) should also be disabled:

  ```
  Section "ServerFlags"
      Option "DontZap" "true"
  EndSection
  ```

---

## Contributing

Pull requests and issues are welcome.

## License

[MIT License](LICENSE)

## Acknowledgments

Built on the foundations of the suckless `slock` and `dwm` projects and the
broader X11 open-source ecosystem.
