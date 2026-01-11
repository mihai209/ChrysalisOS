# chrys-pkg

Minimal package manager for ChrysalisOS.

## Features
- Parses local `catalog.json`
- Lists available applications
- Installs `.petal` packages (uncompressed TAR archives)

## Structure
- `catalog.json`: Repository metadata and app list.
- `installed/`: Directory containing marker files for installed packages.

## Usage
```bash
pkg list
pkg install /path/to/package.petal
