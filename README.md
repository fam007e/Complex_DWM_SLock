# Complex DWM Slock

`Complex_DWM_SLock` is an enhanced screen locker for DWM (Dynamic Window Manager). It provides a visually appealing lock screen with features such as screen blur, time display, and user-friendly password prompts.

## Features

- **Blurred Screen Background**: Blurs the screen content for privacy while locked.
- **Time Display**: Shows the current time in bold at the bottom left corner.
- **User Information**: Displays the username and a password prompt.
- **Responsive UI**: Provides feedback on incorrect password attempts.
- **Flexible Security**: Attempts to restart the display manager after multiple failed attempts, with fallback to TTY.
- **Automatic Versioning**: Includes build date and Git commit hash in version information.
- **Display Manager Compatibility**: Works with various display managers (SDDM, LightDM, GDM, XDM) and provides TTY fallback.

## Installation

### Prerequisites

- X11 development libraries (`libX11`, `libXext`, `libXrandr`)
- Xft and Fontconfig development libraries
- Imlib2 library
- C Compiler (GCC or Clang)
- Make
- Git (for version information)
- PAM (Pluggable Authentication Modules) for password handling

### Building from Source

1. **Clone the Repository**
   ```bash
   git clone https://github.com/fam007e/Complex_DWM_SLock.git
   cd Complex_DWM_SLock
   ```

2. **Compile the Source Code**
   ```bash
   make
   ```

3. **Install the Binary**
   ```bash
   sudo make install
   ```

   Note: You can customize the installation directory by setting PREFIX or BINDIR:
   ```bash
   sudo make PREFIX=/opt/Complex_DWM_SLock install
   ```
   or
   ```bash
   sudo make BINDIR=/home/user/bin install
   ```

## Configuration

Configuration for `Complex_DWM_SLock` is done at compile-time by editing the `src/config.h` file. Modify this file to change settings like username display, blur level, and maximum password attempts.

After making changes to `config.h`, recompile the project:
```bash
make clean
make
sudo make install
```

## Usage

To start the screen locker, simply run:

```bash
slock
```

**Command-line Options**
- `-v`: Display version information (includes build date and Git commit hash).

## Security Considerations

- The screen locker binary is installed with setuid permissions to allow it to run with elevated privileges.
- The program attempts to restart the display manager after multiple failed attempts, falling back to TTY if unsuccessful.
- Consider disabling virtual terminal switching and the X server restart key combination in your `xorg.conf` for increased security.

## Customization

- The appearance of the lock screen can be modified by editing the `src/slock.c` file and recompiling.
- Adjust the blur algorithm in `src/blur.c` to experiment with different blur effects.

## Troubleshooting

- **Screen Does Not Lock**: Ensure all dependencies are installed and the binary has the necessary permissions.
- **Blur Effect Not Visible**: Verify that your system supports the required X extensions and Imlib2 is properly installed.
- **Compilation Errors**: Make sure all required development libraries are installed.
- **Version Information Incorrect**: Ensure Git is installed and you're in a Git repository.
- **Display Manager Issues**: Check if your display manager is supported. The program will attempt to fall back to TTY if no supported display manager is found.

## Contributing

Contributions are welcome! Feel free to submit a pull request or open an issue to discuss potential improvements.

## License

This project is licensed under the [license](LICENSE).

## Acknowledgments

Thanks to the slock, dwm, and X11 projects for inspiration and foundational work in this area. Special thanks to the open-source community for their invaluable contributions and support.