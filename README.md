# tfm-linux — Terminal File Manager

A modern two-pane terminal file manager for Linux (Debian 13+).

Built in C11 with GCC, statically linked — minimal runtime dependencies.

This is the Linux port of the original tfm for Windows 11.

## Building

### Quick build (development, dynamic linking)

```bash
make release     # optimized build
make debug       # debug build with -g -O0
```

### Static release build (musl, portable)

```bash
./build.sh       # auto-detects musl-gcc, installs musl-tools on Debian if missing
```

The `build.sh` script produces a fully statically linked binary in `bin/tfm`
that runs on any Linux system regardless of the host's libc version.

### Build requirements

| Method | Dependencies |
|--------|-------------|
| `make` | `gcc`, `make` (`build-essential` on Debian) |
| `./build.sh` | `musl-tools` (auto-installed on Debian) or `musl-gcc` |

### Install

```bash
make install                    # installs to /usr/local
make install PREFIX=/usr        # installs to /usr
make install DESTDIR=/tmp/pkg   # for packaging
```

## Keyboard Shortcuts

### Navigation
| Key | Action |
|-----|--------|
| `Up`/`Down` | Move cursor |
| `PgUp`/`PgDn` | Page up/down |
| `Home`/`End` | First/last entry |
| `Enter` | Enter directory / open file via `xdg-open` |
| `Backspace` | Parent directory |
| `Ctrl+D` | Mount selector |
| `Tab` | Rotate focus (Left -> Right -> CmdLine) |

### File Operations
| Key | Action |
|-----|--------|
| `F5` | Copy to opposite panel |
| `F6` | Move to opposite panel |
| `F7` | Create directory |
| `F8` | Delete (with confirmation) |
| `Space` | Toggle tag (multi-select) |
| `F3` | Progress/history panel |
| `F2` | Refresh panel |

### Tabs (1 main + up to 4 extra)
| Key | Action |
|-----|--------|
| `Alt+Shift+N` | New tab (starts in home directory) |
| `Alt+Shift+B` | Cycle to next tab |
| `Alt+Shift+M` | Close current tab (main tab locked) |

### Shell Line
| Key | Action |
|-----|--------|
| `Enter` | Execute command via `/bin/sh -c` |
| `Up`/`Down` | Command history |
| `Esc` | Clear command line |

### System
| Key | Action |
|-----|--------|
| `F1` | Toggle help screen |
| `F12` | Exit |
| `Esc` | Clear tags / dismiss overlays |

## Configuration

Saved to `$XDG_CONFIG_HOME/tfm/config.json` (or `~/.config/tfm/config.json`) on exit:
- Startup directory per panel (main tab only)
- Per-mount last-visited paths
- Theme selection, sort order, show hidden files, panel split

## Theming

Themes are stored in `$XDG_CONFIG_HOME/tfm/themes/` (or `~/.config/tfm/themes/`).
On first run, the default theme is copied there automatically.
Default: Catppuccin-inspired dark theme.

## Platform Differences from Windows Version

| Aspect | Windows | Linux |
|--------|---------|-------|
| Strings | `wchar_t` (UTF-16LE) | `char` (UTF-8) |
| Console I/O | Win32 Console API | termios + ANSI escape codes |
| File I/O | `CreateFileW`/`ReadFile` | `open`/`read` (POSIX) |
| Directory listing | `FindFirstFileW` | `opendir`/`readdir` |
| Threading | `CreateThread` + `CRITICAL_SECTION` | `pthread_create` + `pthread_mutex_t` |
| Shell execution | `cmd.exe /c` | `/bin/sh -c` |
| File open | `ShellExecuteW` | `xdg-open` |
| Drive selector | Windows drive letters | Mount points from `/proc/mounts` |
| Config path | `%USERPROFILE%\.tfm\` | `$XDG_CONFIG_HOME/tfm/` |
