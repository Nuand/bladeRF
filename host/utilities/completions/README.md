# Shell Completions for bladeRF Utilities

This directory contains shell completion files for bash and zsh that provide tab completion for bladeRF command-line utilities.

## Supported Utilities

- `bladeRF-cli` - Main command-line interface
- `bladeRF-power` - Power measurement/calibration utility  
- `bladeRF-fsk` - FSK modulation utility
- `bladeRF-update` - Firmware and FPGA update utility

## Installation

The completion files are automatically installed when you run `make install` from the build directory.

Default installation paths:
- **Bash**: `/usr/share/bash-completion/completions`
- **Zsh**: `/usr/share/zsh/site-functions`

### Custom Installation Paths

You can specify custom installation directories during CMake configuration:

```bash
cmake .. -DBASH_COMPLETION_DIR=/custom/bash/path \
         -DZSH_COMPLETION_DIR=/custom/zsh/path
```

### Disabling Completion Installation

To build without installing completions:

```bash
cmake .. -DINSTALL_SHELL_COMPLETIONS=OFF
```

## Manual Installation

### Bash

Copy the files from `bash/` to one of:
- `/usr/share/bash-completion/completions/`
- `/etc/bash_completion.d/`
- `~/.local/share/bash-completion/completions/`

Then reload your shell or source the completion file:
```bash
source /usr/share/bash-completion/completions/bladeRF-cli
```

### Zsh

Copy the `_bladeRF` file from `zsh/` to one of:
- `/usr/share/zsh/site-functions/`
- `/usr/local/share/zsh/site-functions/`
- Any directory in your `$fpath`

Then rebuild the completion cache:
```zsh
rm -f ~/.zcompdump*
compinit
```

## Features

All completion files provide:
- Option/flag completion
- Context-aware argument suggestions
- File path completion with smart filtering:
  - Firmware files: `*.bin`, `*.img`, `*.fw`
  - FPGA files: `*.rbf`, `*.bit`
  - Script files: `*.txt`, `*.script`, `*.cli`
  - Power calibration files: `*.csv`, `*.tbl`
- Device specifier format suggestions
- Basic value completions for common options

## Troubleshooting

### Bash completions not working

1. Ensure bash-completion is installed:
   ```bash
   # macOS (Homebrew)
   brew install bash-completion@2
   
   # Debian/Ubuntu
   sudo apt-get install bash-completion
   
   # Fedora/RHEL/CentOS  
   sudo dnf install bash-completion
   ```

2. For macOS with Homebrew, add this to your `~/.bash_profile`:
   ```bash
   [[ -r "/opt/homebrew/etc/profile.d/bash_completion.sh" ]] && . "/opt/homebrew/etc/profile.d/bash_completion.sh"
   ```
   
   For Intel Macs, the path may be `/usr/local/etc/profile.d/bash_completion.sh`

3. Reload your shell configuration:
   ```bash
   source ~/.bash_profile
   ```

### Zsh completions not working

1. Ensure the completion files are in your `$fpath`:
   ```zsh
   echo $fpath
   ```

2. Clear the completion cache and reload:
   ```zsh
   rm -f ~/.zcompdump*
   compinit
   ```