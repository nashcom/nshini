# Changelog

## v1.0.1 — 2026-05-24

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

- **`convert` direction from both extensions**
  Input and output extensions together determine conversion mode.
  Same-format files are copied without conversion.

- **All diagnostic output to `stderr`**
  All `printf` replaced with `fprintf(stderr)` throughout.
  Only file content goes to `stdout`, making pipe usage safe.

- **Debug tracing**
  Format detection logged per file in `convert`, `diff`, and `edit`.
  `TranslateModeStr` gives readable `LMBCS -> UTF-8` / `UTF-8 -> LMBCS` labels.
  Diff tool resolution source logged.

- **Command-line flags**
  `-debug`, `-verbose`, `-silent`, `-confirm`, `-nogui` documented in help output
  and README.

- **`ENV_*` defines**
  All notes.ini variable names centralized as `#define ENV_*` constants, used in
  both code and help text.


## v1.0.0 — 2026-05-24

Initial release.
