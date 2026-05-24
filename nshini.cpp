/*
###########################################################################
#                                                                         #
# Notes INI file utility -- LMBCS <-> UTF-8 conversion and editing        #
#                                                                         #
# Version 1.0.1 24.05.2026                                                #
# (C) Copyright Daniel Nashed/Nash!Com 2026                               #
#                                                                         #
# Licensed under the Apache License, Version 2.0 (the "License");         #
# you may not use this file except in compliance with the License.        #
# You may obtain a copy of the License at                                 #
#                                                                         #
#      http://www.apache.org/licenses/LICENSE-2.0                         #
#                                                                         #
# Unless required by applicable law or agreed to in writing, software     #
# distributed under the License is distributed on an "AS IS" BASIS,       #
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.#
# See the License for the specific language governing permissions and     #
# limitations under the License.                                          #
#                                                                         #
#                                                                         #
###########################################################################
*/


/*

Additional Information:

Windows:

Change the code page on Windows to Unicode:  chcp 65001

Linux:

On Linux output is automatically in Unicode.

*/


#define NSHINI_VERSION "1.0.1"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#define strncasecmp _strnicmp
#define strcasecmp  _stricmp
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "global.h"
#include "nsfdb.h"
#include "nsfdata.h"
#include "osenv.h"
#include "osmisc.h"
#include "oserr.h"
#include "nsfsearc.h"
#include "osmem.h"
#include "misc.h"
#include "textlist.h"


#define ACTION_ENCODE  1
#define ACTION_DECODE  2
#define ACTION_CONVERT 3
#define ACTION_EDIT    4
#define ACTION_DIFF    5

#define LOG_NONE       0
#define LOG_NORMAL     1
#define LOG_VERBOSE    2
#define LOG_DEBUG      3

/* notes.ini configuration variable names */
#define ENV_LOGLEVEL   "nshini_loglevel"
#define ENV_EDITOR     "nshini_editor"
#define ENV_BACKUP     "nshini_backup"
#define ENV_DIFF       "nshini_diff"
#define ENV_CONFIRM    "nshini_confirm"


/* Defaults */

static int  g_nLogLevel  = LOG_NORMAL;
static int  g_bBackup    = FALSE;
static char g_szEditorIni[MAXPATH+1] = {0};
static char g_szDiffIni[MAXPATH+1]   = {0};
static int  g_bConfirm   = FALSE;
static int  g_bNoGui     = FALSE;


/* NULL or empty string check */
#define IsEmpty(p) (!(p) || !*(p))

/* strlen cast to WORD -- mirrors SIZEOFW for sizeof */
#define STRLENW(p) ((WORD)strlen(p))

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif


#define LogError(...)   do { fprintf (stderr, __VA_ARGS__); } while (0)
#define LogNormal(...)  do { if (g_nLogLevel >= LOG_NORMAL)  fprintf (stderr, __VA_ARGS__); } while (0)
#define LogVerbose(...) do { if (g_nLogLevel >= LOG_VERBOSE) fprintf (stderr, __VA_ARGS__); } while (0)
#define LogDebug(...)   do { if (g_nLogLevel >= LOG_DEBUG)   fprintf (stderr, __VA_ARGS__); } while (0)


static LONG GetEnvLongWithDefault (const char *pszName, LONG lDefault)
{
    char szValue[32] = {0};
    LONG lValue      = lDefault;

    /* Read as text first -- atol alone cannot distinguish "not set" from "set to 0" */
    if (!OSGetEnvironmentString (pszName, szValue, (WORD) sizeof (szValue) - 1))
        return lDefault;

    if (IsEmpty (szValue))
        return lDefault;

    lValue = (LONG) atol (szValue);

    return lValue;
}


static void PrintSyntaxHint (void)
{
    fprintf (stderr, "Syntax Error - Use -help for usage\n");
}


static void PrintUsage (void)
{
    fprintf (stderr, "\n");
    fprintf (stderr, "nshini %s - Notes INI file utility\n\n", NSHINI_VERSION);
    fprintf (stderr, "Usage: nshini <command> [options]\n\n");
    fprintf (stderr, "  convert <file>              auto-convert based on extension\n");
    fprintf (stderr, "  decode  <input> [output]    convert LMBCS to UTF-8\n");
    fprintf (stderr, "  encode  <input> [output]    convert UTF-8 to LMBCS\n");
    fprintf (stderr, "  edit    <file>  [editor]    edit file (auto convert if .ini)\n");
    fprintf (stderr, "  diff    <file1> [file2]     diff files (decodes .ini to UTF-8)\n");
    fprintf (stderr, "\n");
    fprintf (stderr, "Extension Rules:\n\n");
    fprintf (stderr, "  .ini           -> LMBCS format\n");
    fprintf (stderr, "  .utf8 / other  -> UTF-8 format\n");
    fprintf (stderr, "\n");
    fprintf (stderr, "convert: .ini -> creates <file>.utf8 | .utf8 -> strips suffix\n");
    fprintf (stderr, "Use - for stdin/stdout\n");
    fprintf (stderr, "\n");
    fprintf (stderr, "Notes.ini Settings:\n\n");
    fprintf (stderr, "  %-26s log verbosity 0=none 1=normal 2=verbose 3=debug\n", ENV_LOGLEVEL "=<n>");
    fprintf (stderr, "  %-26s editor for edit command\n",                          ENV_EDITOR  "=<editor>");
    fprintf (stderr, "  %-26s set to 1 to create .bak before editing\n",           ENV_BACKUP  "=1");
#ifdef _WIN32
    fprintf (stderr, "  %-26s diff tool: git, wsl, or full path (Windows)\n",     ENV_DIFF    "=<tool>");
    fprintf (stderr, "\n");
    fprintf (stderr, "  Tip: install Notepad++ with ComparePlus plugin for visual side-by-side diff\n");
    fprintf (stderr, "  Tip: install Git for Windows -- diff.exe from its tree is used for terminal diff\n");
#endif
    fprintf (stderr, "  %-26s set to 1 to show diff and confirm before applying\n", ENV_CONFIRM "=1");
    fprintf (stderr, "\n");
    fprintf (stderr, "Command-line Flags:\n\n");
    fprintf (stderr, "  -debug                     set log level to debug\n");
    fprintf (stderr, "  -verbose                   set log level to verbose\n");
    fprintf (stderr, "  -silent                    suppress all output\n");
    fprintf (stderr, "  -confirm                   show diff and confirm before applying\n");
    fprintf (stderr, "  -nogui                     skip GUI diff tools (use terminal diff)\n");
    fprintf (stderr, "\n");
}


/* Returns pointer to extension text (after last dot), or to \0 if no dot found */
static const char *GetFileExt (const char *pszFileName)
{
    const char *pszDot = NULL;

    if (!pszFileName)
        return NULL;

    pszDot = strrchr (pszFileName, '.');

    if (pszDot)
        return pszDot + 1;

    return pszFileName + strlen (pszFileName);
}


static int IsLmbcsFile (const char *pszFileName)
{
    const char *pszExt = NULL;

    if (IsEmpty (pszFileName))
        return FALSE;

    if (0 == strcmp (pszFileName, "-"))
        return FALSE;

    pszExt = GetFileExt (pszFileName);

    return (0 == strcasecmp (pszExt, "ini")) ? TRUE : FALSE;
}


static int ReadFileOrStdin (const char *pszFileName, char **retpszBuffer, DWORD *retpdwSize)
{
    FILE   *fp        = NULL;
    char   *pszBuffer = NULL;
    long    fileSize  = 0;
    size_t  bytesRead = 0;

    if (IsEmpty (pszFileName) || NULL == retpszBuffer || NULL == retpdwSize)
        return FALSE;

    *retpszBuffer = NULL;
    *retpdwSize   = 0;

    if (0 == strcmp (pszFileName, "-"))
    {
        size_t allocSize = 655360;
        size_t used      = 0;
        char   chunk[4096];
        size_t got       = 0;

#ifdef _WIN32
        _setmode (_fileno (stdin), _O_BINARY);
#endif
        pszBuffer = (char *) malloc (allocSize);
        if (!pszBuffer)
            return FALSE;

        while ((got = fread (chunk, 1, sizeof(chunk), stdin)) > 0)
        {
            if (used + got + 1 > allocSize)
            {
                char *pNew = NULL;
                allocSize *= 2;
                pNew = (char *) realloc (pszBuffer, allocSize);
                if (!pNew)
                {
                    free (pszBuffer);
                    return FALSE;
                }
                pszBuffer = pNew;
            }
            memcpy (pszBuffer + used, chunk, got);
            used += got;
        }

        fileSize = (long) used;
    }
    else
    {
        fp = fopen (pszFileName, "rb");
        if (!fp)
        {
            perror (pszFileName);
            return FALSE;
        }

        fseek (fp, 0, SEEK_END);
        fileSize = ftell (fp);

        if (fileSize < 0)
        {
            fclose (fp);
            return FALSE;
        }

        rewind (fp);

        if (0 == fileSize)
        {
            fclose (fp);
            return TRUE;
        }

        pszBuffer = (char *) malloc (fileSize + 1);
        if (!pszBuffer)
        {
            fclose (fp);
            return FALSE;
        }

        bytesRead = fread (pszBuffer, 1, fileSize, fp);
        fclose (fp);
        fp = NULL;

        if ((long) bytesRead != fileSize)
        {
            free (pszBuffer);
            return FALSE;
        }
    }

    pszBuffer[fileSize] = 0;
    *retpszBuffer = pszBuffer;
    *retpdwSize   = (DWORD) fileSize;

    return TRUE;
}


static int WriteFileOrStdout (const char *pszFileName, const char *pszBuffer, DWORD dwSize)
{
    FILE *fp = NULL;

    if (IsEmpty (pszFileName) || NULL == pszBuffer)
        return FALSE;

    if (0 == strcmp (pszFileName, "-"))
    {
#ifdef _WIN32
        _setmode (_fileno (stdout), _O_BINARY);
#endif
        return (fwrite (pszBuffer, 1, dwSize, stdout) == dwSize) ? TRUE : FALSE;
    }

    fp = fopen (pszFileName, "wb");
    if (!fp)
    {
        perror (pszFileName);
        return FALSE;
    }

    if (fwrite (pszBuffer, 1, dwSize, fp) != dwSize)
    {
        fclose (fp);
        return FALSE;
    }

    fclose (fp);
    return TRUE;
}


static int ConvertBuffer (WORD wTranslateMode, const char *pInputBuffer, DWORD dwInputSize,
                          char **retpszOutBuffer, DWORD *retpdwOutSize)
{
    DWORD dwOutSize       = 0;
    char *pszOutputBuffer = NULL;

    if (NULL == pInputBuffer || 0 == dwInputSize || NULL == retpszOutBuffer)
        return FALSE;

    *retpszOutBuffer = NULL;

    if (retpdwOutSize)
        *retpdwOutSize = 0;

    dwOutSize = (dwInputSize * 2);

    pszOutputBuffer = (char *) malloc (dwOutSize + 1);
    if (!pszOutputBuffer)
        return FALSE;

    memset (pszOutputBuffer, 0, dwOutSize + 1);

    dwOutSize = OSTranslate32 (wTranslateMode, pInputBuffer, dwInputSize, pszOutputBuffer, dwOutSize);

    if (0 == dwOutSize)
    {
        free (pszOutputBuffer);
        return FALSE;
    }

    pszOutputBuffer[dwOutSize] = 0;
    *retpszOutBuffer = pszOutputBuffer;

    if (retpdwOutSize)
        *retpdwOutSize = dwOutSize;

    return TRUE;
}


static const char *TranslateModeStr (WORD wTranslateMode)
{
    switch (wTranslateMode)
    {
        case OS_TRANSLATE_LMBCS_TO_UTF8: return "LMBCS -> UTF-8";
        case OS_TRANSLATE_UTF8_TO_LMBCS: return "UTF-8 -> LMBCS";
        default:                         return "unknown";
    }
}


static int ConvertFile (WORD wTranslateMode, const char *pszInputFile, const char *pszOutputFile)
{
    char  *pszInputBuffer  = NULL;
    char  *pszOutputBuffer = NULL;
    DWORD  dwInputSize     = 0;
    DWORD  dwOutputSize    = 0;
    int    ret             = FALSE;

    if (IsEmpty (pszInputFile) || IsEmpty (pszOutputFile))
        return FALSE;

    LogDebug ("Convert [%s]: %s -> %s\n", TranslateModeStr (wTranslateMode), pszInputFile, pszOutputFile);

    if (!ReadFileOrStdin (pszInputFile, &pszInputBuffer, &dwInputSize))
    {
        LogError ("Error reading: %s\n", pszInputFile);
        goto Done;
    }

    if (0 == dwInputSize)
    {
        LogError ("Empty input: %s\n", pszInputFile);
        goto Done;
    }

    if (!ConvertBuffer (wTranslateMode, pszInputBuffer, dwInputSize, &pszOutputBuffer, &dwOutputSize))
    {
        LogError ("Conversion failed\n");
        goto Done;
    }

    if (!WriteFileOrStdout (pszOutputFile, pszOutputBuffer, dwOutputSize))
    {
        LogError ("Error writing: %s\n", pszOutputFile);
        goto Done;
    }

    if (strcmp (pszOutputFile, "-"))
        LogDebug ("Converted %lu -> %lu bytes: %s\n", (unsigned long) dwInputSize, (unsigned long) dwOutputSize, pszOutputFile);

    ret = TRUE;

Done:

    if (pszInputBuffer)
    {
        free (pszInputBuffer);
        pszInputBuffer = NULL;
    }

    if (pszOutputBuffer)
    {
        free (pszOutputBuffer);
        pszOutputBuffer = NULL;
    }

    return ret;
}


/* For convert: .ini -> add .utf8 | .utf8 -> strip .utf8 | other -> add .ini */
static int GetConvertOutputName (const char *pszInput, char *pszOutput, size_t maxLen)
{
    const char *pszExt   = NULL;
    size_t      len      = 0;
    size_t      baseLen  = 0;

    if (IsEmpty (pszInput) || NULL == pszOutput || 0 == maxLen)
        return FALSE;

    len    = strlen (pszInput);
    pszExt = GetFileExt (pszInput);

    if (IsLmbcsFile (pszInput))
    {
        if (len + 5 >= maxLen)
            return FALSE;
        snprintf (pszOutput, maxLen, "%s.utf8", pszInput);
        return TRUE;
    }

    if (0 == strcasecmp (pszExt, "utf8"))
    {
        /* strip ".utf8" -- baseLen points at the dot */
        baseLen = (size_t) (pszExt - pszInput) - 1;
        if (0 == baseLen || baseLen >= maxLen)
            return FALSE;
        strncpy (pszOutput, pszInput, baseLen);
        pszOutput[baseLen] = 0;
        return TRUE;
    }

    /* Non-.ini, non-.utf8: treat as UTF-8, target is .ini */
    if (len + 4 >= maxLen)
        return FALSE;
    snprintf (pszOutput, maxLen, "%s.ini", pszInput);
    return TRUE;
}


/* For encode (UTF-8 -> LMBCS): strip .utf8 if present, else append .ini */
static int GetEncodeOutputName (const char *pszInput, char *pszOutput, size_t maxLen)
{
    const char *pszExt  = NULL;
    size_t      baseLen = 0;

    if (IsEmpty (pszInput) || NULL == pszOutput || 0 == maxLen)
        return FALSE;

    pszExt = GetFileExt (pszInput);

    if (0 == strcasecmp (pszExt, "utf8"))
    {
        baseLen = (size_t) (pszExt - pszInput) - 1;
        if (0 == baseLen || baseLen >= maxLen)
            return FALSE;
        strncpy (pszOutput, pszInput, baseLen);
        pszOutput[baseLen] = 0;
        return TRUE;
    }

    snprintf (pszOutput, maxLen, "%s.ini", pszInput);
    return TRUE;
}


/* For decode (LMBCS -> UTF-8): append .utf8 */
static int GetDecodeOutputName (const char *pszInput, char *pszOutput, size_t maxLen)
{
    if (IsEmpty (pszInput) || NULL == pszOutput || 0 == maxLen)
        return FALSE;

    if (strlen (pszInput) + 5 >= maxLen)
        return FALSE;

    snprintf (pszOutput, maxLen, "%s.utf8", pszInput);
    return TRUE;
}


static int GetTempFilePath (const char *pszSource, char *pszTemp, size_t maxLen)
{
    if (IsEmpty (pszSource) || NULL == pszTemp || 0 == maxLen)
        return FALSE;

    if (strlen (pszSource) + 10 >= maxLen)
        return FALSE;

    snprintf (pszTemp, maxLen, "%s.utf8.edit", pszSource);
    return TRUE;
}


#ifdef _WIN32
/* Locate notepad++.exe via PATH, App Paths registry, then %ProgramFiles% fallbacks */
static int FindNotepadPlusPlus (char *pszPath, size_t maxLen)
{
    char        szBuf[MAXPATH+1] = {0};
    const char *pszPF            = NULL;
    HKEY        hKey             = NULL;
    DWORD       dwSize           = 0;

    /* 1. Search PATH -- covers cases where notepad++ is on the system path */
    if (SearchPathA (NULL, "notepad++.exe", NULL, (DWORD) maxLen, pszPath, NULL))
        return TRUE;

    /* 2. App Paths registry key -- set by the standard Notepad++ installer */
    if (ERROR_SUCCESS == RegOpenKeyExA (HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\notepad++.exe",
            0, KEY_READ | KEY_WOW64_32KEY, &hKey))
    {
        dwSize = (DWORD) maxLen;
        if (ERROR_SUCCESS == RegQueryValueExA (hKey, NULL, NULL, NULL,
                                               (LPBYTE) pszPath, &dwSize))
        {
            RegCloseKey (hKey);
            if (!IsEmpty (pszPath) && 0 == _access (pszPath, 0))
                return TRUE;
        }
        else
        {
            RegCloseKey (hKey);
        }
        *pszPath = 0;
    }

    /* 3. %ProgramFiles%\Notepad++ */
    pszPF = getenv ("ProgramFiles");

    if (!IsEmpty (pszPF))
    {
        snprintf (szBuf, sizeof (szBuf), "%s\\Notepad++\\notepad++.exe", pszPF);
        if (0 == _access (szBuf, 0))
        {
            strncpy (pszPath, szBuf, maxLen - 1);
            pszPath[maxLen - 1] = 0;
            return TRUE;
        }
    }

    /* 4. %ProgramFiles(x86)%\Notepad++ -- 32-bit install on 64-bit Windows */
    pszPF = getenv ("ProgramFiles(x86)");
    if (!IsEmpty (pszPF))
    {
        snprintf (szBuf, sizeof (szBuf), "%s\\Notepad++\\notepad++.exe", pszPF);
        if (0 == _access (szBuf, 0))
        {
            strncpy (pszPath, szBuf, maxLen - 1);
            pszPath[maxLen - 1] = 0;
            return TRUE;
        }
    }

    return FALSE;
}


/* Substring match -- works for bare "notepad++" and full paths alike */
static int IsNotepadPlusPlus (const char *pszEditor)
{
    const char *p = pszEditor;

    if (IsEmpty (pszEditor))
        return FALSE;

    while (*p)
    {
        if (0 == _strnicmp (p, "notepad++", 9))
            return TRUE;
        p++;
    }

    return FALSE;
}

/* Returns TRUE if the ComparePlus plugin DLL is present for the given npp.exe path.
   Checks the install-dir plugins folder and the %APPDATA% user plugins folder. */
static int FindComparePlusPlugin (const char *pszNppExe)
{
    char        szDir[MAXPATH+1]  = {0};
    char        szDll[MAXPATH+1]  = {0};
    char       *pszSlash          = NULL;
    const char *pszAppData        = NULL;

    if (IsEmpty (pszNppExe))
        return FALSE;

    /* Build directory from exe path */
    strncpy (szDir, pszNppExe, sizeof (szDir) - 1);
    pszSlash = strrchr (szDir, '\\');
    if (pszSlash)
        *pszSlash = 0;

    /* 1. <npp_dir>\plugins\ComparePlus\ComparePlus.dll */
    snprintf (szDll, sizeof (szDll), "%s\\plugins\\ComparePlus\\ComparePlus.dll", szDir);
    if (0 == _access (szDll, 0))
        return TRUE;

    /* 2. %APPDATA%\Notepad++\plugins\ComparePlus\ComparePlus.dll */
    pszAppData = getenv ("APPDATA");
    if (!IsEmpty (pszAppData))
    {
        snprintf (szDll, sizeof (szDll), "%s\\Notepad++\\plugins\\ComparePlus\\ComparePlus.dll", pszAppData);
        if (0 == _access (szDll, 0))
            return TRUE;
    }

    return FALSE;
}


/* Locate diff.exe bundled with Git for Windows.
   Git is typically at <root>\cmd\git.exe or <root>\bin\git.exe or <root>\mingw64\bin\git.exe;
   diff.exe lives at <root>\usr\bin\diff.exe in all layouts.
   We strip path components above git.exe one at a time and probe for usr\bin\diff.exe. */
static int FindGitDiff (char *pszPath, size_t maxLen)
{
    char  szGit[MAXPATH+1]  = {0};
    char  szDir[MAXPATH+1]  = {0};
    char  szDiff[MAXPATH+1] = {0};
    char *pszSlash          = NULL;
    int   nLevel            = 0;

    if (!SearchPathA (NULL, "git.exe", NULL, (DWORD) sizeof (szGit), szGit, NULL))
        return FALSE;

    strncpy (szDir, szGit, sizeof (szDir) - 1);

    /* Strip git.exe filename to get its containing directory */
    pszSlash = strrchr (szDir, '\\');
    if (pszSlash)
        *pszSlash = 0;

    /* Walk up up to 3 levels to find the git root where usr\bin\diff.exe lives */
    for (nLevel = 0; nLevel < 3; nLevel++)
    {
        snprintf (szDiff, sizeof (szDiff), "%s\\usr\\bin\\diff.exe", szDir);
        if (0 == _access (szDiff, 0))
        {
            strncpy (pszPath, szDiff, maxLen - 1);
            pszPath[maxLen - 1] = 0;
            return TRUE;
        }

        pszSlash = strrchr (szDir, '\\');
        if (!pszSlash)
            break;
        *pszSlash = 0;
    }

    return FALSE;
}
#endif


static int LaunchEditor (const char *pszFile, const char *pszEditorOverride)
{
    char        szCmd[MAXPATH*2+32] = {0};
    const char *pszEditor           = NULL;
#ifdef _WIN32
    char                szNppPath[MAXPATH+1] = {0};
    int                 bIsNpp               = FALSE;
    STARTUPINFOA        si                   = {0};
    PROCESS_INFORMATION pi                   = {0};
#endif

    if (IsEmpty (pszFile))
        return FALSE;

    if (!IsEmpty (pszEditorOverride))
        pszEditor = pszEditorOverride;
    else if (!IsEmpty (g_szEditorIni))
        pszEditor = g_szEditorIni;
    else
        pszEditor = getenv ("EDITOR");

#ifdef _WIN32
    /* Detect Notepad++ from whatever source gave us the editor name */
    if (IsNotepadPlusPlus (pszEditor))
        bIsNpp = TRUE;

    if (IsEmpty (pszEditor))
    {
        if (FindNotepadPlusPlus (szNppPath, sizeof (szNppPath)))
        {
            pszEditor = szNppPath;
            bIsNpp    = TRUE;
        }
        else
        {
            pszEditor = "notepad";
        }
    }

    /* -multiInst forces a new Notepad++ process we can wait on;
       without it Notepad++ hands off to an existing instance and exits immediately */
    if (bIsNpp)
        snprintf (szCmd, sizeof (szCmd), "\"%s\" -multiInst -nosession -notabbar -noPlugin \"%s\"", pszEditor, pszFile);
    else
        snprintf (szCmd, sizeof (szCmd), "\"%s\" \"%s\"", pszEditor, pszFile);

    LogNormal ("Launching editor: %s\n", szCmd);

    si.cb = sizeof (si);

    if (!CreateProcessA (NULL, szCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        return FALSE;

    WaitForSingleObject (pi.hProcess, INFINITE);
    CloseHandle (pi.hProcess);
    CloseHandle (pi.hThread);

    return TRUE;
#else
    if (IsEmpty (pszEditor))
        pszEditor = "vi";

    snprintf (szCmd, sizeof (szCmd), "\"%s\" \"%s\"", pszEditor, pszFile);
    LogNormal ("Launching editor: %s\n", szCmd);
    return (0 == system (szCmd)) ? TRUE : FALSE;
#endif
}


static int GetAbsPath (const char *pszPath, char *pszAbs, size_t maxLen)
{
    if (IsEmpty (pszPath) || !pszAbs || !maxLen)
        return FALSE;

    *pszAbs = 0;

#ifdef _WIN32
    return (0 != GetFullPathNameA (pszPath, (DWORD) maxLen, pszAbs, NULL)) ? TRUE : FALSE;
#else
    if ('/' == pszPath[0])
    {
        snprintf (pszAbs, maxLen, "%s", pszPath);
        return TRUE;
    }

    if (!getcwd (pszAbs, maxLen))
        return FALSE;

    snprintf (pszAbs + strlen (pszAbs), maxLen - strlen (pszAbs), "/%s", pszPath);
    return TRUE;
#endif
}


static int CopyFilePlain (const char *pszSrc, const char *pszDst)
{
    char   *pszBuffer = NULL;
    DWORD   dwSize    = 0;
    FILE   *fp        = NULL;
    int     ret       = FALSE;

    if (!ReadFileOrStdin (pszSrc, &pszBuffer, &dwSize))
        goto Done;

    fp = fopen (pszDst, "wb");
    if (!fp)
        goto Done;

    if (dwSize > 0 && fwrite (pszBuffer, 1, dwSize, fp) != dwSize)
        goto Done;

    ret = TRUE;

Done:
    if (fp)
        fclose (fp);
    if (pszBuffer)
        free (pszBuffer);

    return ret;
}


#ifdef _WIN32
/* Convert a Windows absolute path to its WSL /mnt/<drive>/... equivalent */
static void WinPathToWsl (const char *pszWin, char *pszWsl, size_t maxLen)
{
    char   drive  = 0;
    size_t pos    = 0;
    size_t i      = 0;

    if (IsEmpty (pszWin) || !pszWsl || !maxLen)
        return;

    *pszWsl = 0;

    if (pszWin[1] == ':')
    {
        drive = pszWin[0] | 0x20;  /* ASCII lowercase of drive letter */
        snprintf (pszWsl, maxLen, "/mnt/%c/", drive);
        pos    = strlen (pszWsl);
        pszWin += 3;                /* skip "X:\" */
    }

    for (i = 0; pos < maxLen - 1 && pszWin[i]; i++)
        pszWsl[pos++] = (pszWin[i] == '\\') ? '/' : pszWin[i];

    pszWsl[pos] = 0;
}

/* Returns TRUE if the named command is available in PATH (suppresses all output) */
static int CommandAvailable (const char *pszCmd)
{
    char szTest[128] = {0};
    snprintf (szTest, sizeof (szTest), "%s --version >NUL 2>&1", pszCmd);
    return (0 == system (szTest)) ? TRUE : FALSE;
}
#endif


static int PromptConfirm (const char *pszMsg)
{
    char szLine[64] = {0};

    fprintf (stderr, "\n%s [y/n]: ", pszMsg);
    fflush (stderr);

    if (!fgets (szLine, sizeof (szLine), stdin))
        return FALSE;

    return (szLine[0] == 'y' || szLine[0] == 'Y') ? TRUE : FALSE;
}


/* Run a terminal (non-GUI) diff between two UTF-8 files.
   Returns 0 = no differences, 1 = differences, -1 = error / no tool. */
static int RunTerminalDiff (const char *pszFile1, const char *pszFile2)
{
    char szCmd[MAXPATH*3] = {0};

    if (IsEmpty (pszFile1) || IsEmpty (pszFile2))
        return -1;

#ifdef _WIN32
    {
        const char         *pszTool             = NULL;
        char                szWsl1[MAXPATH+1]   = {0};
        char                szWsl2[MAXPATH+1]   = {0};
        char                szGitDiff[MAXPATH+1] = {0};
        DWORD               dwExit              = 1;
        STARTUPINFOA        si                  = {0};
        PROCESS_INFORMATION pi                  = {0};

        /* Tool resolution: ini override (ComparePlus is GUI -- skip) -> git diff.exe -> wsl */
        if (!IsEmpty (g_szDiffIni) && !IsNotepadPlusPlus (g_szDiffIni))
            pszTool = g_szDiffIni;

        if (!pszTool)
        {
            if (CommandAvailable ("git"))
            {
                if (FindGitDiff (szGitDiff, sizeof (szGitDiff)))
                {
                    pszTool = szGitDiff;
                    LogDebug ("Confirm diff tool: %s (git diff.exe, auto-detected)\n", pszTool);
                }
                else
                {
                    pszTool = "git";
                    LogDebug ("Confirm diff tool: git (auto-detected)\n");
                }
            }
            else if (CommandAvailable ("wsl"))
            {
                pszTool = "wsl";
                LogDebug ("Confirm diff tool: wsl (auto-detected)\n");
            }
            else
            {
                LogError ("No terminal diff tool found for confirm. Install Git for Windows.\n");
                return -1;
            }
        }

        if (0 == strcasecmp (pszTool, "git"))
        {
            /* diff.exe not found in git tree -- fall back to git porcelain */
            snprintf (szCmd, sizeof (szCmd),
                      "git --no-pager diff --no-index -- \"%s\" \"%s\"",
                      pszFile1, pszFile2);
        }
        else if (0 == strcasecmp (pszTool, "wsl"))
        {
            WinPathToWsl (pszFile1, szWsl1, sizeof (szWsl1));
            WinPathToWsl (pszFile2, szWsl2, sizeof (szWsl2));
            snprintf (szCmd, sizeof (szCmd), "wsl diff \"%s\" \"%s\"", szWsl1, szWsl2);
        }
        else
        {
            /* Full path to diff.exe (from git tree, nshini_diff, or other explicit tool) */
            snprintf (szCmd, sizeof (szCmd), "\"%s\" \"%s\" \"%s\"", pszTool, pszFile1, pszFile2);
        }

        LogDebug ("Confirm diff cmd: %s\n", szCmd);

        si.cb = sizeof (si);

        if (!CreateProcessA (NULL, szCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            LogError ("Failed to launch diff tool for confirm\n");
            return -1;
        }

        WaitForSingleObject (pi.hProcess, INFINITE);
        GetExitCodeProcess (pi.hProcess, &dwExit);
        CloseHandle (pi.hProcess);
        CloseHandle (pi.hThread);

        return (int) dwExit;
    }
#else
    {
        int nRet = 0;

        snprintf (szCmd, sizeof (szCmd), "diff \"%s\" \"%s\"", pszFile1, pszFile2);
        LogDebug ("Confirm diff cmd: %s\n", szCmd);
        nRet = system (szCmd);

        /* system() encodes exit status via waitpid; 0 means diff exited 0 (no differences) */
        return (nRet == 0) ? 0 : 1;
    }
#endif
}


static int EditFile (const char *pszFile, const char *pszEditorOverride)
{
    char szTempFile[MAXPATH+1]  = {0};
    char szBakFile[MAXPATH+1]   = {0};
    char szSnapFile[MAXPATH+16] = {0};
    int  bIsLmbcs               = FALSE;
    int  bEditorOk              = FALSE;
    int  ret                    = FALSE;

    if (IsEmpty (pszFile))
        return FALSE;

    bIsLmbcs = IsLmbcsFile (pszFile);
    LogDebug ("Edit: %s detected as %s\n", pszFile, bIsLmbcs ? "LMBCS" : "UTF-8");

    if (bIsLmbcs)
    {
        if (g_bBackup)
        {
            snprintf (szBakFile, sizeof (szBakFile), "%s.bak", pszFile);

            if (!CopyFilePlain (pszFile, szBakFile))
                LogNormal ("Warning: could not create backup: %s\n", szBakFile);
            else
                LogVerbose ("Backup: %s\n", szBakFile);
        }

        if (!GetTempFilePath (pszFile, szTempFile, sizeof(szTempFile)))
        {
            LogError ("Failed to determine temp file path\n");
            goto Done;
        }

        LogDebug ("Decoding to temp: %s\n", szTempFile);

        if (!ConvertFile (OS_TRANSLATE_LMBCS_TO_UTF8, pszFile, szTempFile))
            goto Done;

        if (g_bConfirm)
        {
            snprintf (szSnapFile, sizeof (szSnapFile), "%s.orig", szTempFile);
            if (!CopyFilePlain (szTempFile, szSnapFile))
            {
                LogNormal ("Warning: could not create snapshot for confirm -- confirm disabled\n");
                szSnapFile[0] = 0;
            }
            else
            {
                LogDebug ("Confirm snapshot: %s\n", szSnapFile);
            }
        }
    }

    bEditorOk = LaunchEditor (bIsLmbcs ? szTempFile : pszFile, pszEditorOverride);

    if (!bEditorOk)
        LogNormal ("Warning: editor returned non-zero exit code\n");

    if (bIsLmbcs && g_bConfirm && !IsEmpty (szSnapFile))
    {
        int nDiff = RunTerminalDiff (szSnapFile, szTempFile);

        remove (szSnapFile);
        szSnapFile[0] = 0;

        if (nDiff == 0)
        {
            LogNormal ("No changes made.\n");
            remove (szTempFile);
            ret = TRUE;
            goto Done;
        }

        if (!PromptConfirm ("Apply changes?"))
        {
            LogNormal ("Changes discarded.\n");
            remove (szTempFile);
            ret = TRUE;
            goto Done;
        }
    }

    if (bIsLmbcs)
    {
        LogDebug ("Encoding back to LMBCS: %s\n", pszFile);

        if (!ConvertFile (OS_TRANSLATE_UTF8_TO_LMBCS, szTempFile, pszFile))
        {
            LogError ("Failed to encode after editing -- temp file preserved: %s\n", szTempFile);
            goto Done;
        }

        remove (szTempFile);
    }

    ret = TRUE;

Done:

    return ret;
}


/* Diff two files; .ini inputs are decoded to UTF-8 temp files first.
   Pass "-" for file1 to read UTF-8 from stdin (passed straight to diff). */
static int DiffFiles (const char *pszFile1, const char *pszFile2)
{
    char        szTemp1[MAXPATH+1]  = {0};
    char        szTemp2[MAXPATH+1]  = {0};
    char        szCmd[MAXPATH*3]    = {0};
    const char *pszArg1             = pszFile1;
    const char *pszArg2             = pszFile2;
    int         bTemp1              = FALSE;
    int         bTemp2              = FALSE;
    int         ret                 = FALSE;
#ifdef _WIN32
    const char *pszTool              = NULL;
    char        szWsl1[MAXPATH+1]    = {0};
    char        szWsl2[MAXPATH+1]    = {0};
    char        szNppPath[MAXPATH+1] = {0};
    char        szGitDiff[MAXPATH+1] = {0};
    int         bComparePlus         = FALSE;
#endif

    if (IsEmpty (pszFile1) || IsEmpty (pszFile2))
        return FALSE;

    LogDebug ("Diff file1: %s detected as %s\n", pszFile1,
              0 == strcmp (pszFile1, "-") ? "UTF-8 (stdin)" : IsLmbcsFile (pszFile1) ? "LMBCS" : "UTF-8");

    if (0 != strcmp (pszFile1, "-") && IsLmbcsFile (pszFile1))
    {
        snprintf (szTemp1, sizeof (szTemp1), "%s.utf8.diff", pszFile1);

        if (!ConvertFile (OS_TRANSLATE_LMBCS_TO_UTF8, pszFile1, szTemp1))
        {
            LogError ("Failed to decode for diff: %s\n", pszFile1);
            goto Done;
        }

        bTemp1  = TRUE;
        pszArg1 = szTemp1;
    }

    LogDebug ("Diff file2: %s detected as %s\n", pszFile2,
              0 == strcmp (pszFile2, "-") ? "UTF-8 (stdin)" : IsLmbcsFile (pszFile2) ? "LMBCS" : "UTF-8");

    if (0 != strcmp (pszFile2, "-") && IsLmbcsFile (pszFile2))
    {
        snprintf (szTemp2, sizeof (szTemp2), "%s.utf8.diff", pszFile2);

        if (!ConvertFile (OS_TRANSLATE_LMBCS_TO_UTF8, pszFile2, szTemp2))
        {
            LogError ("Failed to decode for diff: %s\n", pszFile2);
            goto Done;
        }

        bTemp2  = TRUE;
        pszArg2 = szTemp2;
    }

#ifdef _WIN32
    /* Resolve diff tool: ENV_DIFF ini -> auto-detect git -> auto-detect wsl -> error */
    pszTool = IsEmpty (g_szDiffIni) ? NULL : g_szDiffIni;

    /* -nogui: skip any Notepad++ path even if set explicitly in notes.ini */
    if (g_bNoGui && !IsEmpty (pszTool) && IsNotepadPlusPlus (pszTool))
    {
        LogDebug ("Diff tool: Notepad++ skipped (-nogui)\n");
        pszTool = NULL;
    }

    if (!IsEmpty (pszTool))
    {
        LogDebug ("Diff tool: %s (" ENV_DIFF ")\n", pszTool);
    }
    else
    {
        LogDebug (ENV_DIFF " not set -- auto-detecting\n");

        if (!g_bNoGui &&
            FindNotepadPlusPlus (szNppPath, sizeof (szNppPath)) &&
            FindComparePlusPlugin (szNppPath))
        {
            pszTool      = szNppPath;
            bComparePlus = TRUE;
            LogDebug ("Diff tool: Notepad++ ComparePlus (auto-detected)\n");
        }
        else if (CommandAvailable ("git"))
        {
            if (FindGitDiff (szGitDiff, sizeof (szGitDiff)))
            {
                pszTool = szGitDiff;
                LogDebug ("Diff tool: %s (git diff.exe, auto-detected)\n", pszTool);
            }
            else
            {
                pszTool = "git";
                LogDebug ("Diff tool: git (auto-detected)\n");
            }
        }
        else if (CommandAvailable ("wsl"))
        {
            pszTool = "wsl";
            LogDebug ("Diff tool: wsl (auto-detected)\n");
        }
        else
        {
            LogError ("No diff tool found. Set " ENV_DIFF "=git or " ENV_DIFF "=wsl in notes.ini\n");
            goto Done;
        }
    }

    if (bComparePlus)
    {
        snprintf (szCmd, sizeof (szCmd),
                  "\"%s\" -multiInst -nosession -pluginMessage=comparePlus \"%s\" \"%s\"",
                  pszTool, pszArg1, pszArg2);
    }
    else if (0 == strcasecmp (pszTool, "wsl"))
    {
        WinPathToWsl (pszArg1, szWsl1, sizeof (szWsl1));
        WinPathToWsl (pszArg2, szWsl2, sizeof (szWsl2));
        snprintf (szCmd, sizeof (szCmd), "wsl diff \"%s\" \"%s\"", szWsl1, szWsl2);
    }
    else if (0 == strcasecmp (pszTool, "git"))
    {
        /* diff.exe not found in git tree -- fall back to git porcelain */
        snprintf (szCmd, sizeof (szCmd), "git --no-pager diff --no-index -- \"%s\" \"%s\"", pszArg1, pszArg2);
    }
    else
    {
        /* Full path to diff.exe (from git tree, nshini_diff, or other explicit tool) */
        snprintf (szCmd, sizeof (szCmd), "\"%s\" \"%s\" \"%s\"", pszTool, pszArg1, pszArg2);
    }
#else
    snprintf (szCmd, sizeof (szCmd), "diff \"%s\" \"%s\"", pszArg1, pszArg2);
#endif

    LogDebug ("Diff cmd: %s\n", szCmd);

#ifdef _WIN32
    {
        STARTUPINFOA        si = {0};
        PROCESS_INFORMATION pi = {0};

        si.cb = sizeof (si);

        if (CreateProcessA (NULL, szCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject (pi.hProcess, INFINITE);
            CloseHandle (pi.hProcess);
            CloseHandle (pi.hThread);
        }
        else
        {
            LogError ("Failed to launch diff tool\n");
            goto Done;
        }
    }
#else
    system (szCmd);
#endif

    ret = TRUE;

Done:

    if (bTemp1)
        remove (szTemp1);

    if (bTemp2)
        remove (szTemp2);

    return ret;
}


static WORD EvaluateFormula (NOTEHANDLE hNote, FORMULAHANDLE hFormula, char *pszResult, WORD wResultLen, STATUS *pretError)
{
    STATUS   error         = NOERROR;
    char    *pFormula      = NULL;
    char    *pResult       = NULL;
    char    *pText         = NULL;
    WORD     wDataType     = 0;
    WORD     wDataSize     = 0;
    WORD     wTextLen      = 0;
    WORD     wTextNumLen   = 0;
    WORD     len           = 0;
    int      bFormulaLock  = FALSE;
    int      bResultLock   = FALSE;
    BOOL     bShouldDelete = FALSE;
    BOOL     bModified     = FALSE;
    RANGE   *pRange        = NULL;
    NUMBER  *pNumber       = NULL;
    NUMBER   number        = {0};
    DHANDLE  hResult       = NULLHANDLE;
    HCOMPUTE hCompute      = NULLHANDLE;

    if (pretError)
        *pretError = NOERROR;

    if (NULL == pszResult || 0 == wResultLen)
        return 0;

    *pszResult = 0;

    if (NULLHANDLE == hFormula)
        return 0;

    pFormula     = (char *) OSLockObject (hFormula);
    bFormulaLock = TRUE;

    error = NSFComputeStart (0, pFormula, &hCompute);
    if (error)
        goto Done;

    error = NSFComputeEvaluate (hCompute, hNote, &hResult, &wDataSize,
                                NULL, &bShouldDelete, &bModified);
    if (error)
        goto Done;

    pResult     = (char *) OSLockObject (hResult);
    bResultLock = TRUE;

    wDataType  = *(WORD *) pResult;
    pResult   += sizeof (WORD);
    wDataSize -= (WORD) sizeof (WORD);

    switch (wDataType)
    {
        case TYPE_TEXT:
            len = (WORD) MIN (wResultLen - 1, wDataSize);
            memcpy (pszResult, pResult, len);
            pszResult[len] = 0;
            break;

        case TYPE_TEXT_LIST:
            pText = NULL;
            ListGetText (pResult, FALSE, 0, &pText, &wTextLen);
            if (pText)
            {
                len = (WORD) MIN (wResultLen - 1, wTextLen);
                memcpy (pszResult, pText, len);
                pszResult[len] = 0;
            }
            break;

        case TYPE_NUMBER:
            memcpy (&number, pResult, sizeof (NUMBER));
            ConvertFLOATToText (NULL, NULL, &number,
                                pszResult, (WORD) (wResultLen - 1), &wTextNumLen);
            len = wTextNumLen;
            break;

        case TYPE_NUMBER_RANGE:
            pRange  = (RANGE *) pResult;
            pNumber = (NUMBER *) &pRange[1];
            memcpy (&number, pNumber, sizeof (NUMBER));
            ConvertFLOATToText (NULL, NULL, &number,
                                pszResult, (WORD) (wResultLen - 1), &wTextNumLen);
            len = wTextNumLen;
            break;

        case TYPE_ERROR:
            error = *(WORD *) pResult;
            OSLoadString (NULLHANDLE, ERR (error), pszResult, (WORD) (wResultLen - 1));
            len = STRLENW (pszResult);
            break;

        default:
            snprintf (pszResult, wResultLen, "DataType: %d", (int) wDataType);
            len = STRLENW (pszResult);
            break;
    }

Done:

    if (bResultLock)
    {
        OSUnlockObject (hResult);
        bResultLock = FALSE;
    }

    if (NULLHANDLE != hResult)
    {
        OSMemFree (hResult);
        hResult = NULLHANDLE;
    }

    if (bFormulaLock)
    {
        OSUnlockObject (hFormula);
        bFormulaLock = FALSE;
    }

    if (hCompute)
    {
        NSFComputeStop (hCompute);
        hCompute = NULLHANDLE;
    }

    if (error && pretError)
        *pretError = error;

    return len;
}


/* Compile pszFormula and evaluate it; convenience wrapper around EvaluateFormula */
static WORD EvaluateFormulaText (NOTEHANDLE hNote, const char *pszFormula,
                                  char *pszResult, WORD wResultLen,
                                  STATUS *pretError)
{
    STATUS        error       = NOERROR;
    FORMULAHANDLE hFormula    = NULLHANDLE;
    WORD          wFormulaLen = 0;
    WORD          wdc         = 0;
    WORD          len         = 0;

    if (NULL == pszResult || 0 == wResultLen)
        return 0;

    *pszResult = 0;

    if (IsEmpty (pszFormula))
        return 0;

    error = NSFFormulaCompile (NULL, 0, pszFormula, STRLENW (pszFormula), &hFormula, &wFormulaLen,
                               &wdc, &wdc, &wdc, &wdc, &wdc);
    if (error)
    {
        if (pretError)
            *pretError = error;
        goto Done;
    }

    len = EvaluateFormula (hNote, hFormula, pszResult, wResultLen, pretError);

Done:

    if (NULLHANDLE != hFormula)
    {
        OSMemFree (hFormula);
        hFormula = NULLHANDLE;
    }

    return len;
}


/* Evaluate @ConfigFile to obtain the active notes.ini path at runtime */
static int GetNotesIniPath (char *pszIniPath, size_t maxLen)
{
    STATUS error = NOERROR;

    if (NULL == pszIniPath || 0 == maxLen)
        return FALSE;

    *pszIniPath = 0;

    EvaluateFormulaText (NULLHANDLE, "@ConfigFile",
                         pszIniPath, (WORD) maxLen, &error);

    return (!error && !IsEmpty (pszIniPath)) ? TRUE : FALSE;
}


extern "C" STATUS LNPUBLIC AddInMain (HMODULE hResourceModule, int argc, const char *argv[])
{
    const char *pszInput        = NULL;
    const char *pszOutput       = NULL;
    const char *pszDefaultIni   = NULL;
    const char *pszFiles[2]     = {0};
    char        szOutput[MAXPATH+1]  = {0};
    char        szIniPath[MAXPATH+1] = {0};
    int         action          = 0;
    int         nFiles          = 0;
    int         bCommandSeen    = FALSE;
    const char *filtered[16]    = {0};
    int         filteredArgc    = 0;
    int         i               = 0;

    /* Pass 1: strip Domino's =<notes.ini-path> injected args */
    for (i = 0; i < argc && filteredArgc < 16; i++)
    {
        if (!IsEmpty (argv[i]) && argv[i][0] == '=')
        {
            if (IsEmpty (pszDefaultIni))
                pszDefaultIni = argv[i] + 1;
        }
        else
        {
            filtered[filteredArgc++] = argv[i];
        }
    }

    /* Read settings from notes.ini; command-line flags below will override */
    g_nLogLevel = (int)  GetEnvLongWithDefault (ENV_LOGLEVEL, (LONG) LOG_NORMAL);
    g_bBackup   = (int)  GetEnvLongWithDefault (ENV_BACKUP,   (LONG) FALSE);
    g_bConfirm  = (int)  GetEnvLongWithDefault (ENV_CONFIRM,  (LONG) FALSE);

    OSGetEnvironmentString (ENV_EDITOR, g_szEditorIni, (WORD) sizeof (g_szEditorIni) - 1);
    OSGetEnvironmentString (ENV_DIFF,   g_szDiffIni,   (WORD) sizeof (g_szDiffIni)   - 1);

    LogDebug (ENV_LOGLEVEL " ini: %d\n", g_nLogLevel);
    LogDebug (ENV_BACKUP   " ini: %d\n", g_bBackup);
    LogDebug (ENV_CONFIRM  " ini: %d\n", g_bConfirm);
    LogDebug (ENV_EDITOR   " ini: %s\n", IsEmpty (g_szEditorIni) ? "(not set)" : g_szEditorIni);
    LogDebug (ENV_DIFF     " ini: %s\n", IsEmpty (g_szDiffIni)   ? "(not set)" : g_szDiffIni);

    /* Pass 2: single scan -- classify each arg as command, flag, or positional file */
    for (i = 1; i < filteredArgc; i++)
    {
        const char *pszArg = filtered[i];

        if (IsEmpty (pszArg))
            continue;

        if (pszArg[0] == '-')
        {
            /* Bare - is a positional file arg (stdin/stdout) */
            if (0 == strcmp (pszArg, "-"))
            {
                if (nFiles < 2)
                    pszFiles[nFiles++] = pszArg;
                continue;
            }

            /* Help flags */
            if (0 == strcmp (pszArg, "-h")    ||
                0 == strcmp (pszArg, "-help") ||
                0 == strcmp (pszArg, "-?"))
            {
                PrintUsage();
                return NOERROR;
            }

            /* Log level flags -- override notes.ini setting */
            if (0 == strcmp (pszArg, "-verbose")) { g_nLogLevel = LOG_VERBOSE; continue; }
            if (0 == strcmp (pszArg, "-silent"))  { g_nLogLevel = LOG_NONE;    continue; }
            if (0 == strcmp (pszArg, "-debug"))   { g_nLogLevel = LOG_DEBUG;   continue; }

            /* Confirm flag -- override notes.ini setting */
            if (0 == strcmp (pszArg, "-confirm")) { g_bConfirm = TRUE; continue; }

            /* Disable GUI diff tools (e.g. Notepad++ ComparePlus) -- use terminal diff */
            if (0 == strcmp (pszArg, "-nogui"))   { g_bNoGui   = TRUE; continue; }

            /* Unknown flag */
            LogError ("Unknown option: %s\n", pszArg);
            PrintSyntaxHint();
            return NOERROR;
        }

        /* Command keyword -- must come before any file args */
        if (!bCommandSeen && nFiles == 0)
        {
            if      (0 == strcasecmp (pszArg, "encode"))  { action = ACTION_ENCODE;  bCommandSeen = TRUE; continue; }
            else if (0 == strcasecmp (pszArg, "decode"))  { action = ACTION_DECODE;  bCommandSeen = TRUE; continue; }
            else if (0 == strcasecmp (pszArg, "convert")) { action = ACTION_CONVERT; bCommandSeen = TRUE; continue; }
            else if (0 == strcasecmp (pszArg, "edit"))    { action = ACTION_EDIT;    bCommandSeen = TRUE; continue; }
            else if (0 == strcasecmp (pszArg, "diff"))    { action = ACTION_DIFF;    bCommandSeen = TRUE; continue; }
        }

        /* Positional file arg */
        if (nFiles < 2)
            pszFiles[nFiles++] = pszArg;
    }

    /* Shortcut logic when no explicit command was given */
    if (!bCommandSeen)
    {
        if (nFiles == 0)
        {
            /* nshini  ->  edit active notes.ini */
            action = ACTION_EDIT;
        }
        else if (nFiles == 1 && 0 == strcmp (pszFiles[0], "-"))
        {
            /* nshini -  ->  decode active notes.ini to stdout */
            action      = ACTION_DECODE;
            pszFiles[0] = NULL;
            pszOutput   = "-";
        }
        else if (nFiles == 1)
        {
            /* nshini file.ini  ->  edit file */
            action = ACTION_EDIT;
        }
        else
        {
            /* nshini file1 file2  ->  convert */
            action = ACTION_CONVERT;
        }
    }

    /* Resolve pszInput: positional arg, dot alias, then Domino defaults */
    pszInput = pszFiles[0];

    if (pszInput && 0 == strcmp (pszInput, "."))
        pszInput = NULL;

    if (!pszInput)
    {
        if (!IsEmpty (pszDefaultIni))
            pszInput = pszDefaultIni;
        else if (GetNotesIniPath (szIniPath, sizeof (szIniPath)))
            pszInput = szIniPath;
        else
        {
            LogError ("No file specified and cannot determine notes.ini path\n");
            PrintSyntaxHint();
            return NOERROR;
        }
    }

    if (IsEmpty (pszInput))
    {
        LogError ("Error: empty input filename\n");
        return NOERROR;
    }

    LogDebug ("--- nshini debug ---\n");
    LogDebug ("  log level  : %d\n",   g_nLogLevel);
    LogDebug ("  action     : %d\n",   action);
    LogDebug ("  nFiles     : %d\n",   nFiles);
    LogDebug ("  pszFiles[0]: %s\n",   pszFiles[0] ? pszFiles[0] : "");
    LogDebug ("  pszFiles[1]: %s\n",   pszFiles[1] ? pszFiles[1] : "");
    LogDebug ("  pszInput   : %s\n",   pszInput    ? pszInput    : "");
    LogDebug ("  pszOutput  : %s\n",   pszOutput   ? pszOutput   : "");
    LogDebug ("  defaultIni : %s\n",   pszDefaultIni ? pszDefaultIni : "");
    LogDebug ("--------------------\n");

    switch (action)
    {
        case ACTION_ENCODE:
            if (!pszOutput)
            {
                if (nFiles >= 2)
                    pszOutput = pszFiles[1];
                else
                {
                    GetEncodeOutputName (pszInput, szOutput, sizeof (szOutput));
                    pszOutput = szOutput;
                }
            }
            ConvertFile (OS_TRANSLATE_UTF8_TO_LMBCS, pszInput, pszOutput);
            break;

        case ACTION_DECODE:
            if (!pszOutput)
            {
                if (nFiles >= 2)
                    pszOutput = pszFiles[1];
                else
                {
                    GetDecodeOutputName (pszInput, szOutput, sizeof (szOutput));
                    pszOutput = szOutput;
                }
            }
            ConvertFile (OS_TRANSLATE_LMBCS_TO_UTF8, pszInput, pszOutput);
            break;

        case ACTION_CONVERT:
        {
            WORD wMode = 0;

            /* Resolve output first -- direction depends on both extensions */
            if (!pszOutput)
            {
                if (nFiles >= 2)
                    pszOutput = pszFiles[1];
                else
                {
                    GetConvertOutputName (pszInput, szOutput, sizeof (szOutput));
                    pszOutput = szOutput;
                }
            }

            LogDebug ("Convert: input=%s (%s)  output=%s (%s)\n",
                      pszInput,  IsLmbcsFile (pszInput)  ? "LMBCS" : "UTF-8",
                      pszOutput, IsLmbcsFile (pszOutput) ? "LMBCS" : "UTF-8");

            {
                char szAbsInput[MAXPATH+1]  = {0};
                char szAbsOutput[MAXPATH+1] = {0};

                GetAbsPath (pszInput,  szAbsInput,  sizeof (szAbsInput));
                GetAbsPath (pszOutput, szAbsOutput, sizeof (szAbsOutput));

#ifdef _WIN32
                if (!IsEmpty (szAbsInput) && 0 == _stricmp (szAbsInput, szAbsOutput))
#else
                if (!IsEmpty (szAbsInput) && 0 == strcmp (szAbsInput, szAbsOutput))
#endif
                {
                    LogError ("convert: input and output are the same file: %s\n", szAbsInput);
                    break;
                }
            }

            if (IsLmbcsFile (pszInput) && !IsLmbcsFile (pszOutput))
                wMode = OS_TRANSLATE_LMBCS_TO_UTF8;
            else if (!IsLmbcsFile (pszInput) && IsLmbcsFile (pszOutput))
                wMode = OS_TRANSLATE_UTF8_TO_LMBCS;
            else
            {
                /* Same format -- plain copy */
                LogDebug ("Convert: same format (%s) -- no conversion needed\n",
                          IsLmbcsFile (pszInput) ? "LMBCS" : "UTF-8");
                CopyFilePlain (pszInput, pszOutput);
                break;
            }

            ConvertFile (wMode, pszInput, pszOutput);
            break;
        }

        case ACTION_EDIT:
            /* pszFiles[1] is optional editor override */
            EditFile (pszInput, (nFiles >= 2) ? pszFiles[1] : NULL);
            break;

        case ACTION_DIFF:
        {
            const char *pszFile2           = NULL;
            char        szFile2[MAXPATH+1] = {0};

            if (nFiles == 0)
            {
                LogError ("diff requires at least one file\n");
                PrintSyntaxHint();
                break;
            }

            if (nFiles >= 2)
            {
                pszFile2 = pszFiles[1];
            }
            else if (!IsEmpty (pszDefaultIni))
            {
                pszFile2 = pszDefaultIni;
                LogVerbose ("Diffing against: %s\n", pszFile2);
            }
            else if (GetNotesIniPath (szFile2, sizeof (szFile2)))
            {
                pszFile2 = szFile2;
                LogVerbose ("Diffing against: %s\n", pszFile2);
            }
            else
            {
                LogError ("Cannot determine active notes.ini path -- specify a second file\n");
                break;
            }

            if (0 == strcmp (pszInput, pszFile2))
            {
                LogError ("diff: both files resolve to the same path: %s\n", pszInput);
                break;
            }

            DiffFiles (pszInput, pszFile2);
            break;
        }
    }

    return NOERROR;
}
