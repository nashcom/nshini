# Changelog

## v1.1.0 — 2026-05-27

### Added

- **`set` command**
  Sets a notes.ini variable via `OSSetEnvironmentVariable` from the Domino C API.
  Name and value are translated from UTF-8 to LMBCS, supporting the full Unicode
  range including German umlauts, Asian characters, and other non-ASCII values.
  Accepts `variable=value` or `variable value` syntax.
  Safe for live changes when Domino is running; also works when stopped.

- **`get` command**
  Reads a notes.ini variable via `OSGetEnvironmentString` and writes the UTF-8
  decoded value to stdout. Returns the live in-memory value.
  Note: Domino runtime messages on stderr during startup may interfere with
  script output capture in some environments.

- **`LOG_DEVELOPER` level / `-developer` flag**
  New log level 4 above `-debug`. Adds raw hex byte dumps (`LogDevHex`) for
  encoding diagnostics — shows exact bytes for names and values before and after
  LMBCS conversion. Useful for diagnosing character-set issues; not needed for
  normal debug tracing. Set `nshini_loglevel=4` or pass `-developer` on the
  command line.

### Fixed

- **Non-ASCII command-line arguments (`RebuildArgsUtf8`)**
  The Domino runtime converts `argv` from the shell's encoding (UTF-8) to
  OEM-850 before calling `AddInMain` on both Linux and Windows. Non-ASCII
  values in arguments (passwords, international strings) arrived corrupted.
  `RebuildArgsUtf8` bypasses this by reading the original UTF-8 command line
  directly: `/proc/self/cmdline` on Linux, `GetCommandLineW` on Windows.
  The exe name is preserved at `filtered[0]` so command keywords remain at
  their expected index for Pass 2 parsing.

- **`FileExists` check**
  All file-based commands (encode, decode, convert, edit, diff) now verify that
  the input file exists before proceeding. `-` (stdin/stdout) is always accepted.

- **Non-ASCII file names on Windows**
  `fopen` and `_access` on Windows use the ANSI code page; passing a UTF-8 path
  with non-ASCII characters caused garbled file names and failed opens or existence
  checks. All file I/O now goes through `fopen_utf8` (uses `_wfopen` on Windows)
  and `FileExists` now uses `_waccess` on Windows. Both convert the UTF-8 path to
  wide char before the Windows API call. Linux is unaffected — the filesystem is
  natively UTF-8.


## v1.0.1 — 2026-05-24

### Added

- **`nshini_confirm` ini variable / `-confirm` flag**
  Show diff and prompt for confirmation before encoding changes back to LMBCS.
  Opt-in, default off. Terminal-only diff (skips ComparePlus GUI on Windows).
  "No changes" detected automatically via diff exit code.

- **Windows diff support**
  `diff` command now works on Windows via `diff.exe` from the Git for Windows
  install tree (preferred), `git --no-pager diff --no-index` (fallback), or `wsl`.
  `FindGitDiff` locates `diff.exe` by walking up from `git.exe`.
  `WinPathToWsl` handles path conversion for WSL.

- **Notepad++ ComparePlus plugin**
  Auto-detected as first choice for `diff` on Windows when installed.
  Launched with `-pluginMessage=comparePlus` for side-by-side visual diff.

- **`-nogui` flag**
  Skips GUI diff tools (Notepad++ ComparePlus) for the `diff` command.
  Falls through to `diff.exe` / git / wsl for terminal output.
  Also suppresses ComparePlus when set via `nshini_diff`.

- **`nshini_diff` ini variable**
  Pins diff tool explicitly: `git`, `wsl`, or a full path to any diff executable.

- **Debug tracing**
  Format detection logged per file in `convert`, `diff`, and `edit`.
  `TranslateModeStr` gives readable `LMBCS -> UTF-8` / `UTF-8 -> LMBCS` labels.
  Diff tool resolution source logged.

- **Command-line flags**
  `-debug`, `-verbose`, `-silent`, `-confirm`, `-nogui` documented in help output
  and README.

### Fixed

- **All diagnostic output to `stderr`**
  All `printf` replaced with `fprintf(stderr)` throughout.
  Only file content goes to `stdout`, making pipe usage safe.

### Changed

- **`convert` direction from both extensions**
  Input and output extensions together determine conversion mode.
  Same-format files are copied without conversion.

- **`ENV_*` defines**
  All notes.ini variable names centralized as `#define ENV_*` constants, used in
  both code and help text.


## v1.0.0 — 2026-05-24

Initial release.
