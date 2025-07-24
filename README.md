# Complex DWM Slock

`Complex_DWM_SLock` is an enhanced, secure, and visually appealing screen locker for the DWM (Dynamic Window Manager) and other X11-based environments. It provides a modern lock screen experience with features such as screen blur, time display, and robust user authentication.

## Features

- **Blurred Screen Background**: Blurs the entire screen for privacy and focus while locked.
- **Customizable Information Display**: Shows the current time, username, and a password prompt.
- **Responsive Feedback**: Provides clear visual feedback for incorrect password attempts.
- **Secure Authentication**: Uses the Pluggable Authentication Modules (PAM) framework for password verification and securely wipes credentials from memory after use.
- **Robust Security**: Attempts to restart the display manager after multiple failed attempts, with a fallback to TTY to prevent brute-force attacks.
- **Automatic Versioning**: Embeds the build date and Git commit hash into the binary for easy version tracking.
- **Signal Handling**: Ensures a graceful exit and resource cleanup, even if the process is interrupted.

## How It Works

1.  **Initialization**: `slock` connects to the X server and creates a fullscreen, top-level window.
2.  **Screen Capture & Blur**: It takes a screenshot of the desktop, applies a blur effect to it, and sets it as the lock screen background.
3.  **Information Display**: It renders the time, username, and password prompt on the screen.
4.  **Password Input**: It securely captures keyboard input for the user's password.
5.  **Authentication**: It uses PAM to verify the password against the system's user database. The password is cleared from memory immediately after.
6.  **Unlocking**: On success, it cleans up all resources and returns control to the user.
7.  **Security Fallback**: After too many failed attempts, it tries to restart the display manager or switch to a TTY.

## Getting Started

### Prerequisites

- X11 development libraries (`libX11`, `libXext`, `libXrandr`)
- Xft and Fontconfig development libraries (`libXft`, `libfontconfig`)
- Imlib2 library (`libimlib2`)
- Pluggable Authentication Modules (`libpam`)
- C Compiler (GCC or Clang)
- Make
- Git (for version information)

### Installation

1.  **Clone the Repository**
    ```bash
    git clone https://github.com/fam007e/Complex_DWM_SLock.git
    cd Complex_DWM_SLock
    ```

2.  **Compile the Source Code**
    ```bash
    make
    ```

3.  **Install the Binary**
    ```bash
    sudo make install
    ```
    The binary is installed with `setuid` permissions to allow it to perform authentication correctly. You can customize the installation directory by setting `PREFIX` or `BINDIR`:
    ```bash
    # Install to /opt/Complex_DWM_SLock/bin
    sudo make PREFIX=/opt/Complex_DWM_SLock install

    # Install to a local user directory
    sudo make BINDIR=/home/user/bin install
    ```

### Arch User Repository (AUR)

This project is available on the Arch User Repository as [`complex-dwm-slock-git`](https://aur.archlinux.org/packages/complex-dwm-slock-git).

[![AUR package](https://img.shields.io/aur/version/complex-dwm-slock-git?logo=arch-linux)](https://aur.archlinux.org/packages/complex-dwm-slock-git)

You can install it using your favorite AUR helper, such as `yay` or `paru`:

```bash
# Using yay
yay -S complex-dwm-slock-git

# Using paru
paru -S complex-dwm-slock-git
```

## Usage

To lock your screen, simply run the command:

```bash
slock
```

### Command-line Options

-   `-v`: Display version information, including the build date and Git commit hash.

## Configuration

All configuration is done at compile-time by editing the `src/config.h` file. You can customize settings like the blur radius, maximum password attempts, fonts, and colors.

After modifying `src/config.h`, you must recompile and reinstall the project:
```bash
make clean
make
sudo make install
```

## Security Considerations

-   **Setuid**: The `slock` binary requires `setuid` root permissions to access PAM and lock the screen effectively. This is a standard requirement for screen lockers.
-   **Password Handling**: Passwords are read into a memory buffer that is securely zeroed out immediately after use to prevent credential exposure.
-   **System Hardening**: For maximum security, consider disabling virtual terminal switching (`Ctrl+Alt+F*`) and the X server restart key combination (`Ctrl+Alt+Backspace`) in your system's Xorg configuration.

## Contributing

Contributions are welcome! Please feel free to submit a pull request or open an issue to discuss potential features or report bugs.

## License

This project is licensed under the [MIT License](LICENSE).

## Acknowledgments

This project builds upon the foundational work of the suckless `slock` and `dwm` projects, as well as the broader X11 open-source community.
