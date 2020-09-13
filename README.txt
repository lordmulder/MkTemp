mktemp, by LoRd_MuldeR <MuldeR2@GMX.de>
Generates a unique temporary file name or temporary sub-directory.

Usage:
  mktemp.exe [--dir] [--path <path>] [<suffix>]

Options:
  --dir   Create a temporary sub-directory instead of a file
  --path  Use the specified path, instead of the system's TEMP path

If 'suffix' is not specified, the default suffix '.tmp' is used!
