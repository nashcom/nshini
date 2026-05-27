# nshini - Notes INI File Utility

Nash!Com tool for converting and editing `notes.ini` files.

Notes/Domino stores `notes.ini` in LMBCS encoding. Most editors and tools expect UTF-8.
`nshini` converts in both directions, auto-detects format from file extension, and can
launch an editor with automatic round-trip conversion. When no file is specified it
operates on the active `notes.ini` of the running Domino instance.


## TL;DR

- `notes.ini` is stored in **LMBCS** — opening it in a UTF-8 editor and saving silently corrupts any non-ASCII values
- `nshini edit notes.ini` decodes to UTF-8, opens your editor, re-encodes to LMBCS after save — the editor never sees raw LMBCS
- `nshini diff notes.ini backup.ini` compares two ini files with correct character decoding
- `nshini -` dumps the active `notes.ini` as readable UTF-8 to stdout
- With no prameters it edits the notes.ini
- Runs on Linux and Windows; uses `OSTranslate32` from the Domino C API for byte-identical conversion


## Commands

```
nshini convert <file>              auto-convert based on extension
nshini decode  <input> [output]    convert LMBCS to UTF-8
nshini encode  <input> [output]    convert UTF-8 to LMBCS
nshini edit    <file> [editor]     edit file; auto-converts .ini to/from UTF-8
nshini diff    <file1> [file2]     diff files (decodes .ini to UTF-8 first)
nshini set     <variable>=<value>  set a notes.ini variable via the Domino API
nshini get     <variable>          get a notes.ini variable value
```

```
-h | -help | -?                    show this help
```

### Shortcuts

Long-form commands are always available. These short forms cover the common cases:

| Invocation           | Equivalent to   |
|-|--|
| `nshini`             | `nshini edit .` |
| `nshini file.ini`    | `nshini edit file.ini` |
| `nshini .`           | `nshini edit <default notes.ini>` |
| `nshini -`           | `nshini decode . -` (dump notes.ini as UTF-8 to stdout) |
| `nshini file1 file2` | `nshini convert file1 file2` |

`.` is an alias for the default notes.ini of the running Domino instance.  
`-` as a file argument means stdin (input) or stdout (output).


## Extension rules

| Extension | Format |
|--|--|
| `.ini`    | LMBCS  |
| `.utf8`   | UTF-8  |
| other     | UTF-8  |

### convert

Determines direction from the file extension and derives the output name automatically:

```
nshini convert notes.ini        ->  notes.ini.utf8   (LMBCS -> UTF-8)
nshini convert notes.ini.utf8   ->  notes.ini        (UTF-8 -> LMBCS)
```

### decode / encode

Explicit direction. Output file is optional; default names follow the same extension rules:

```
nshini decode notes.ini                  ->  notes.ini.utf8
nshini decode notes.ini edited.utf8      ->  edited.utf8

nshini encode notes.ini.utf8             ->  notes.ini
nshini encode notes.utf8                 ->  notes
nshini encode notes.txt                  ->  notes.txt.ini
```

### edit

Decodes a `.ini` file to a UTF-8 edit file in the same directory, opens the editor,
then encodes the result back to LMBCS. Non-`.ini` files are opened directly.

```
# uses nshini_editor / $EDITOR / Notepad++ / notepad / vi
nshini edit notes.ini

# explicit editor
nshini edit notes.ini vim
```

If `nshini_backup=1` is set, `nshini` writes a `<original>.bak` copy of the LMBCS file
(e.g. `notes.ini.bak`) before decoding.
The edit file is named `<original>.utf8.edit` (e.g. `notes.ini.utf8.edit`) and is
removed after a successful encode-back. If the encode step fails the file is preserved.

If `nshini_confirm=1` is set (or `-confirm` is passed on the command line), `nshini`
shows a diff of the changes after the editor closes and asks for confirmation before
writing them back to LMBCS. Answering `n` discards the edit file without touching the
original. This is useful when editing a live `notes.ini` where an accidental save could
cause problems.

Confirm only applies to `.ini` (LMBCS) files — the round-trip through a UTF-8 temp file
is what makes the before/after snapshot possible. Non-`.ini` files are opened directly
by the editor and confirm is skipped.

### diff

Compares two files after decoding any `.ini` inputs to UTF-8. The second file
is optional; it defaults to the active notes.ini.

```
nshini diff notes.ini            # diff file against active notes.ini
nshini diff file1.ini file2.ini  # diff two .ini files
nshini diff - notes.ini          # diff UTF-8 from stdin against a file
nshini diff -                    # diff stdin against active notes.ini
```

`.ini` files are decoded to a `.utf8.diff` temp file for the diff call and
removed afterwards. The diff output goes directly to the terminal.

On Windows, the diff tool is selected in this order:

1. `nshini_diff` in `notes.ini` (explicit override)
2. Notepad++ with ComparePlus plugin — side-by-side visual diff (auto-detected)
3. `diff.exe` bundled with Git for Windows — clean GNU diff output (auto-detected from git's install tree)
4. `git --no-pager diff --no-index` — fallback if `diff.exe` is not found in the expected location
5. `wsl` — uses `wsl diff` with automatic Windows-to-WSL path conversion (`D:\...` → `/mnt/d/...`)

**Recommendation: install [Notepad++](https://notepad-plus-plus.org/) with the
[ComparePlus plugin](https://github.com/pnedev/comparePlus).**
ComparePlus gives a side-by-side visual diff with syntax highlighting directly inside
Notepad++. `nshini` detects it automatically — no configuration needed. Install
Notepad++ first, then add ComparePlus via the Notepad++ Plugin Manager
(*Plugins → Plugins Admin → ComparePlus*).

**Recommendation: also install [Git for Windows](https://git-scm.com/download/win).**
Git for Windows bundles `diff.exe` (GNU diff) in its `usr\bin` directory. `nshini`
locates it automatically from the git install tree and uses it directly — giving clean
standard unified diff output without git-specific headers. It handles Windows paths
natively and starts instantly, unlike WSL which may need a cold start.

Set `nshini_diff=git` or `nshini_diff=wsl` in `notes.ini` to pin the choice explicitly.
A full path to any diff-compatible executable is also accepted as the value.

Pass `-nogui` on the command line to skip Notepad++ ComparePlus and get terminal diff
output instead — useful for scripting or when you want the diff in the console.

### set

Sets a notes.ini variable via `OSSetEnvironmentVariable` from the Domino C API.
The API handles character set encoding internally; non-ASCII values (passwords,
international strings) are passed directly and stored correctly.

```
nshini set CERTSTORE_CACHELOG=1
```

A space-separated form is also accepted:

```
nshini set CERTSTORE_CACHELOG 1
```

`OSSetEnvironmentVariable` updates the in-memory cache and writes to the notes.ini
file on disk. When Domino is running this goes through the API's own file locking,
making it safe for live changes. When Domino is stopped it is equivalent to a direct
file write but without the LMBCS encoding complexity.

For bulk edits or changes that require the server to be stopped, use `nshini edit`
instead.

### get

Reads a notes.ini variable via `OSGetEnvironmentString` and writes the UTF-8 decoded
value to stdout.

```
nshini get CERTSTORE_CACHELOG
```

The value reflects what Domino currently holds in memory — the live in-memory state,
not just what is on disk.

**Note on scripting:** `nshini` itself writes nothing to stdout except the value.
However, the Domino runtime may emit diagnostic messages to stderr during startup.
When capturing the output in a script (`VALUE=$(nshini get VAR)`) this is usually
fine since stderr is separate from stdout. Be aware that some environments may merge
the two streams.

### stdin / stdout

```
nshini decode - output.utf8      # read LMBCS from stdin, write UTF-8
nshini encode input.utf8 -       # read UTF-8, write LMBCS to stdout
nshini decode - -                # full pipe: LMBCS in, UTF-8 out
```


## Default notes.ini

When `.` or no file is specified, `nshini` resolves the active notes.ini in order:

1. The `=<path>` argument injected by the Domino server runtime
2. `@ConfigFile` evaluated via the Notes formula engine


## Editing notes.ini directly

### The problem

`notes.ini` is stored in **LMBCS** (Lotus Multi-Byte Character Set), a Lotus-proprietary
encoding. Most editors and terminals expect UTF-8. The two encodings share the ASCII range
(0x00–0x7F), so lines containing only standard ASCII characters are safe in any editor.
Lines that contain non-ASCII characters — passwords with special characters, international
names, paths, any value where Domino stored bytes above 0x7F — will be misread.

The visible symptom is that those characters appear as garbled sequences or question marks.
The dangerous part is what happens on **save**: most editors will write their own
interpretation of those bytes back to disk, silently corrupting the values Domino depends on.

### What is safe to edit manually

- **Pure ASCII values** — flag variables, numbers, simple server names, file paths with no
  special characters. These survive any editor.
- **Adding new variables** with ASCII-only values is safe.
- **`vi` / `vim` on Linux** — treats the file as a raw byte stream. Non-ASCII bytes display
  as garbled characters but are preserved as-is provided you do not modify those lines.
  If you add or change only ASCII content, the rest of the file survives intact.

### What will break things

- **Saving a non-ASCII line through a UTF-8 editor** — the editor reads the LMBCS bytes as
  broken UTF-8, then writes its interpretation back. The original bytes are gone.
- **Windows Notepad** — re-encodes on save. Even opening and saving without any change can
  corrupt LMBCS content.
- **Any editor that normalizes line endings** — Domino is sensitive to unexpected CRLF/LF
  changes in notes.ini.
- **Editors that add a BOM** (Byte Order Mark) at the start of the file — Domino will not
  parse a notes.ini that begins with a UTF-8 BOM.
- **Clipboard paste** — copying a value from a UTF-8 source and pasting it into a raw
  notes.ini editor will store UTF-8 bytes where LMBCS is expected.

### Editing while Domino is running

Modifying `notes.ini` while the Domino server or Notes client is running can cause
unpredictable results. Domino reads and caches many settings at startup or on demand.
A concurrent write can race with a read, produce a half-written file, or simply be
overwritten the next time Domino flushes its own changes back to disk.

The supported way to change a setting on a live server is the Domino console command:

```
set config <variable>=<value>
```

This updates the in-memory value and writes the change safely through Domino's own
file handling. Use `nshini edit` for changes that require a server restart or for
bulk edits done while the server is stopped.

### Best practices for manual edits

- **Know the line before you touch it.** If a value contains anything that looks unusual in
  your editor, leave it alone.
- **Only change what you can see correctly.** If the line looks clean ASCII in your editor,
  it is safe to edit. If it shows garbled characters, use `nshini`.
- **Keep a backup.** Before any manual edit copy the file: `cp notes.ini notes.ini.bak`.
  `nshini edit` can do this automatically when `nshini_backup=1` is set.
- **Use `nshini -` to inspect first.** This decodes the active notes.ini to UTF-8 on stdout
  so you can see the real content before deciding whether to use `nshini edit` or go direct.
- **Use `nshini diff` to verify.** After a manual edit, diff the result against the backup
  to confirm only the intended lines changed.

### The nshini approach

`nshini edit` avoids the problem entirely by never letting the editor see the LMBCS file:

1. If `nshini_backup=1` is set, write a `<file>.bak` copy of the original.
2. Decode the `.ini` file to a clean UTF-8 copy (`<file>.utf8.edit`) using `OSTranslate32`
   from the Domino C API — the same conversion Domino uses internally.
3. Open that UTF-8 file in the editor. All characters display and edit correctly.
4. After the editor closes, encode the UTF-8 copy back to LMBCS and overwrite the original.
5. Remove the temporary edit file on success. On encode failure the file is preserved so
   nothing is lost.

The round-trip uses the official Domino API throughout, so the output is byte-identical to
what Domino would write itself.


## Editor

The editor is resolved in this order (first match wins):

1. Explicit editor argument on the command line (`nshini edit file.ini vim`)
2. `nshini_editor` in `notes.ini`
3. `EDITOR` environment variable
4. Platform default (see table below)

| Platform | Default fallback |
|-|--|
| Linux    | `vi` |
| Windows  | Notepad++ if installed, otherwise `notepad` |

On Windows `nshini` uses `CreateProcess` and waits for the editor process to exit,
so the encode-back only runs after the editor window is closed.

### Notepad++ (Windows)

When Notepad++ is selected — by any of the four resolution steps — `nshini` launches
it with `-multiInst -nosession -notabbar -noPlugin`. This forces a new process instead
of handing off to an existing instance, which allows `nshini` to wait for the editor
to close before encoding the file back to LMBCS.

If no editor is configured and no `EDITOR` variable is set, `nshini` searches the
standard Notepad++ install locations automatically and uses it if found.


## Configuration (notes.ini)

| Variable | Description |
|-|-|
| `nshini_editor` | Editor to use for `edit` command. Overrides `EDITOR` env var; can be overridden by an explicit command-line argument. |
| `nshini_loglevel` | Log verbosity: `0` = none, `1` = normal (default), `2` = verbose, `3` = debug, `4` = developer (adds hex byte dumps). |
| `nshini_backup` | Set to `1` to enable automatic `.bak` creation before editing. Default: `0` (disabled). |
| `nshini_diff` | Diff tool for the `diff` command (Windows). `git` selects Git for Windows (uses its bundled `diff.exe` if found, otherwise `git --no-pager diff --no-index`), `wsl`, or a full path to any diff-compatible executable. Auto-detected if not set. |
| `nshini_confirm` | Set to `1` to show a diff and prompt for confirmation before encoding changes back to LMBCS. Default: `0` (disabled). |

### Command-line flags

| Flag | Description |
|-|-|
| `-debug` | Set log level to debug (overrides `nshini_loglevel`). |
| `-developer` | Set log level to developer — like debug plus raw hex byte dumps for encoding diagnostics. |
| `-verbose` | Set log level to verbose. |
| `-silent` | Suppress all output. |
| `-confirm` | Show diff and prompt for confirmation before applying (overrides `nshini_confirm`). |
| `-nogui` | Skip GUI diff tools (e.g. Notepad++ ComparePlus); use terminal diff output instead. |



## Installation

`nshini` is a Domino add-in. The binary must reside in the Domino binary directory
(`Notes_ExecDirectory`) so the Domino runtime loader can find the shared libraries.

### Linux

Copy the binary to the Domino binary directory, set permissions, and create a symlink.

```sh
cp nshini /opt/hcl/domino/notes/latest/linux/
chmod 755 /opt/hcl/domino/notes/latest/linux/nshini
cd /opt/hcl/domino/bin
ln -s tools/startup nshini
```

`make install` handles the copy step. Set permissions and create the symlink manually afterwards.

The symlink in `/opt/hcl/domino/bin` is what puts `nshini` on the path — without it you would need to invoke it with the full path or from the binary directory.

### Windows

Copy `nshini.exe` to the Domino program directory (e.g. `C:\Program Files\HCL\Domino\`).
No further steps are required. Add the Domino program directory to `PATH` if you want to run `nshini` from any command prompt.


## Notes

- Uses `OSTranslate32` from the Domino C API for all character set conversion.
- The `=<path>` argument injected by the Domino server runtime is automatically ignored for command parsing but used as the default notes.ini path.
- Reads and writes files in binary mode to preserve exact byte content.

### Windows: UTF-8 console output

By default the Windows console uses a legacy code page that cannot display non-ASCII
characters correctly. Any `nshini` output containing non-ASCII bytes will appear as
garbled characters. Switch the console to UTF-8 before running `nshini`:

```cmd
chcp 65001
```

This affects the current console session only. To make it permanent, set the code page
in the Windows Terminal profile or add the command to your shell startup script.


## Building

### Linux

Requires the HCL Domino C API. Set `LOTUS` and `Notes_ExecDirectory` before building.

```sh
make
make install    # copies to /opt/hcl/domino/notes/latest/linux/
```

### Windows

Requires Microsoft Visual Studio and the HCL Domino C API.

```cmd
nmake -f mswin64.mak
```
