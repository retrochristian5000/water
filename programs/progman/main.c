/*
 * Program Manager
 *
 * Copyright 1996 Ulrich Schmid
 * Copyright 2002 Sylvain Petreolle
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OEMRESOURCE

#include "windows.h"
#include "shellapi.h"
#include "progman.h"

GLOBALS Globals;

static VOID MAIN_CreateGroups(void);
static VOID MAIN_MenuCommand(HWND hWnd, WPARAM wParam, LPARAM lParam);
static ATOM MAIN_RegisterMainWinClass(void);
static VOID MAIN_CreateMainWindow(void);
static VOID MAIN_CreateMDIWindow(void);
static VOID MAIN_AutoStart(void);
static VOID MAIN_WinIniStart(void);

#define BUFFER_SIZE 1000

/***********************************************************************
 *
 *           WinMain
 */

int PASCAL WinMain (HINSTANCE hInstance, HINSTANCE prev, LPSTR cmdline, int show)
{
  MSG      msg;

  Globals.lpszIniFile         = "progman.ini";
  Globals.lpszIcoFile         = "progman.ico";

  Globals.hInstance           = hInstance;
  Globals.hGroups             = 0;
  Globals.hActiveGroup        = 0;

  /* Read Options from `progman.ini' */
  Globals.bAutoArrange =
    GetPrivateProfileIntA("Settings", "AutoArrange", 0, Globals.lpszIniFile);
  Globals.bMinOnRun =
    GetPrivateProfileIntA("Settings", "MinOnRun", 0, Globals.lpszIniFile);
  Globals.bSaveSettings =
    GetPrivateProfileIntA("Settings", "SaveSettings", 0, Globals.lpszIniFile);

  /* Load default icons */
  Globals.hMainIcon    = ExtractIconA(Globals.hInstance, Globals.lpszIcoFile, 0);
  Globals.hGroupIcon   = ExtractIconA(Globals.hInstance, Globals.lpszIcoFile, 0);
  Globals.hDefaultIcon = ExtractIconA(Globals.hInstance, Globals.lpszIcoFile, 0);
  if (!Globals.hMainIcon)    Globals.hMainIcon = LoadIconW(0, (LPWSTR)DEFAULTICON);
  if (!Globals.hGroupIcon)   Globals.hGroupIcon = LoadIconW(0, (LPWSTR)DEFAULTICON);
  if (!Globals.hDefaultIcon) Globals.hDefaultIcon = LoadIconW(0, (LPWSTR)DEFAULTICON);

  /* Register classes */
  if (!prev)
    {
      if (!MAIN_RegisterMainWinClass()) return(FALSE);
      if (!GROUP_RegisterGroupWinClass()) return(FALSE);
      if (!PROGRAM_RegisterProgramWinClass()) return(FALSE);
    }

  /* Create main window */
  MAIN_CreateMainWindow();
  Globals.hAccel = LoadAcceleratorsW(Globals.hInstance, MAKEINTRESOURCEW(IDA_ACCEL));

  /* Setup menu, stringtable and resourcenames */
  STRING_LoadMenus();

  MAIN_CreateMDIWindow();

  /* Initialize groups */
  MAIN_CreateGroups();

  /* WIN.INI load=/run= and PROGMAN.INI Startup= are independent startup
   * sources. Program Manager, not KERNEL32's generic profile parser, owns
   * execution; third-party shells may not process these entries. */
  MAIN_WinIniStart();
  MAIN_AutoStart();

  /* Message loop */
  while (GetMessageW (&msg, 0, 0, 0))
    if (!TranslateAcceleratorW(Globals.hMainWnd, Globals.hAccel, &msg))
      {
	TranslateMessage (&msg);
	DispatchMessageW (&msg);
      }
  return 0;
}

/***********************************************************************
 *
 *           MAIN_CreateGroups
 */

/* Detect already-loaded GRP files even when GRPFILE_ReadGroupFile chose
 * an editable .gr copy rather than the original .grp filename. */
static BOOL MAIN_GroupAlreadyLoaded(LPCSTR path)
{
  CHAR alias[MAX_PATHNAME_LEN];
  size_t len = strlen(path);
  HLOCAL hGroup;

  alias[0] = 0;
  if (len >= 4 && len < sizeof(alias) && !lstrcmpiA(path + len - 4, ".grp"))
    {
      memcpy(alias, path, len);
      alias[len - 1] = 0;
    }
  for (hGroup = GROUP_FirstGroup(); hGroup; hGroup = GROUP_NextGroup(hGroup))
    {
      PROGGROUP *group = LocalLock(hGroup);
      LPCSTR loaded = LocalLock(group->hGrpFile);
      if (!lstrcmpiA(loaded, path) || (alias[0] && !lstrcmpiA(loaded, alias)))
        return TRUE;
    }
  return FALSE;
}

static VOID MAIN_LoadIniGroup(LPCSTR key)
{
  CHAR path[MAX_PATHNAME_LEN];

  /* Never attempt to open a truncated group filename. */
  if (GetPrivateProfileStringA("Groups", key, "", path, sizeof(path),
                               Globals.lpszIniFile) >= sizeof(path) - 1)
    return;
  if (path[0] && !MAIN_GroupAlreadyLoaded(path)) GRPFILE_ReadGroupFile(path);
}

static BOOL MAIN_IsIniGroupKey(LPCSTR key)
{
  static const char prefix[] = "Group";
  const char *p;
  unsigned int i;

  if (strlen(key) <= 5) return FALSE;
  for (i = 0; i < 5; i++)
    if (tolower((unsigned char)key[i]) != tolower((unsigned char)prefix[i]))
      return FALSE;
  p = key + 5;
  while (*p && isdigit((unsigned char)*p)) p++;
  return !*p && p > key + 5;
}

/***********************************************************************
 *
 *           MAIN_CreateGroups
 */

static VOID MAIN_CreateGroups(void)
{
  CHAR order[BUFFER_SIZE], key[24];
  CHAR *ptr, *next, *keys;
  DWORD capacity, count;

  /* Settings/Order controls presentation order; [Groups] is the inventory.
   * Advance even on missing GroupN entries: the old loop hung forever. */
  GetPrivateProfileStringA("Settings", "Order", "", order, sizeof(order),
                           Globals.lpszIniFile);
  ptr = order;
  while (*ptr)
    {
      unsigned long num;

      while (*ptr && isspace((unsigned char)*ptr)) ptr++;
      if (!*ptr) break;
      errno = 0;
      num = strtoul(ptr, &next, 10);
      if (next == ptr || errno == ERANGE ||
          (*next && !isspace((unsigned char)*next)) || !num || num > 65535)
        {
          while (*ptr && !isspace((unsigned char)*ptr)) ptr++;
          continue; /* malformed token */
        }
      ptr = next;
      snprintf(key, sizeof(key), "Group%lu", num);
      MAIN_LoadIniGroup(key);
    }

  /* Also load Groups/GroupN entries omitted from Order. The list of group
   * names uses a double-NUL terminated profile buffer, grown as needed. */
  capacity = 1024;
  keys = HeapAlloc(GetProcessHeap(), 0, capacity);
  if (!keys) return;
  for (;;)
    {
      CHAR *grown;
      count = GetPrivateProfileStringA("Groups", NULL, "", keys, capacity,
                                       Globals.lpszIniFile);
      if (count < capacity - 2) break;
      if (capacity >= 65536)
        {
          HeapFree(GetProcessHeap(), 0, keys);
          return; /* no truncated enumeration */
        }
      capacity *= 2;
      grown = HeapReAlloc(GetProcessHeap(), 0, keys, capacity);
      if (!grown)
        {
          HeapFree(GetProcessHeap(), 0, keys);
          return;
        }
      keys = grown;
    }
  for (ptr = keys; *ptr; ptr += strlen(ptr) + 1)
    if (MAIN_IsIniGroupKey(ptr)) MAIN_LoadIniGroup(ptr);
  HeapFree(GetProcessHeap(), 0, keys);
}

/* The WIN.INI [windows] load= / run= entries contain filenames rather
 * than arbitrary command lines. Spaces separate applications; double quotes
 * protect long paths. Do not interpret commas or additional arguments:
 * DOS-Windows 3.0 did not support parameters in these lists. */
static VOID MAIN_RunWinIniList(LPCSTR list, INT show)
{
  const char *p = list;

  while (*p)
    {
      const char *start;
      CHAR command[BUFFER_SIZE];
      size_t len;

      while (*p == ' ' || *p == '\t') p++;
      if (!*p) break;
      start = p;
      if (*p == '"')
        {
          p++;
          while (*p && *p != '"') p++;
          if (!*p) break; /* malformed quoted filename */
          p++; /* keep the quotes for WinExec */
          if (*p && *p != ' ' && *p != '\t')
            {
              /* A closing quote followed by unseparated characters is
               * not a filename boundary. Reject the whole invalid item. */
              while (*p && *p != ' ' && *p != '\t') p++;
              continue;
            }
        }
      else
        while (*p && *p != ' ' && *p != '\t') p++;

      len = p - start;
      if (len > 0 && len < sizeof(command) &&
          !(len == 2 && start[0] == '"' && start[1] == '"'))
        {
          memcpy(command, start, len);
          command[len] = 0;
          WinExec(command, show);
        }
    }
}

static VOID MAIN_WinIniStart(void)
{
  CHAR section[8192];
  LPCSTR load = NULL, run = NULL;
  const CHAR *entry;
  DWORD len;

  /* Read the section as raw key=value strings. GetProfileString strips
   * matched outer quotes, losing the only delimiter around a single
   * filename containing spaces. This API also handles [windows] comments. */
  len = GetProfileSectionA("windows", section, sizeof(section));
  if (len >= sizeof(section) - 2) return; /* incomplete, do not launch */
  for (entry = section; entry < section + len && *entry;
       entry += strlen(entry) + 1)
    {
      if (!load && !lstrnicmpA(entry, "load=", 5)) load = entry + 5;
      if (!run && !lstrnicmpA(entry, "run=", 4)) run = entry + 4;
    }
  if (load) MAIN_RunWinIniList(load, SW_SHOWMINNOACTIVE);
  if (run) MAIN_RunWinIniList(run, SW_SHOWNORMAL);
}

/***********************************************************************
 *
 *           MAIN_AutoStart
 */

VOID MAIN_AutoStart(void)
{
  CHAR buffer[BUFFER_SIZE];
  CHAR *name = buffer, *end;
  HLOCAL hGroup, hProgram;

  /* Windows 3.1 uses Startup=, not AutoStart=. A blank Startup= disables
   * the group; a missing key defaults to Startup. Shift skips autostart. */
  if (GetAsyncKeyState(VK_SHIFT) & 0x8000) return;
  GetPrivateProfileStringA("Settings", "Startup", "Startup", buffer,
                           sizeof(buffer), Globals.lpszIniFile);
  while (*name && isspace((unsigned char)*name)) name++;
  end = name + strlen(name);
  while (end > name && isspace((unsigned char)end[-1])) *--end = 0;
  if (end - name >= 2 && name[0] == '"' && end[-1] == '"')
    {
      name++;
      *--end = 0;
    }
  if (!*name) return;

  for (hGroup = GROUP_FirstGroup(); hGroup; hGroup = GROUP_NextGroup(hGroup))
    if (!lstrcmpiA(name, GROUP_GroupName(hGroup)))
      {
        for (hProgram = PROGRAM_FirstProgram(hGroup); hProgram;
             hProgram = PROGRAM_NextProgram(hProgram))
          PROGRAM_ExecuteProgram(hProgram);
        break;
      }
}

/***********************************************************************
 *
 *           MAIN_MainWndProc
 */

static LRESULT CALLBACK MAIN_MainWndProc(HWND hWnd, UINT msg,
				 WPARAM wParam, LPARAM lParam)
{
  switch (msg)
    {
    case WM_INITMENU:
      CheckMenuItem(Globals.hOptionMenu, PM_AUTO_ARRANGE,
		    MF_BYCOMMAND | (Globals.bAutoArrange ? MF_CHECKED : MF_UNCHECKED));
      CheckMenuItem(Globals.hOptionMenu, PM_MIN_ON_RUN,
		    MF_BYCOMMAND | (Globals.bMinOnRun ? MF_CHECKED : MF_UNCHECKED));
      CheckMenuItem(Globals.hOptionMenu, PM_SAVE_SETTINGS,
		    MF_BYCOMMAND | (Globals.bSaveSettings ? MF_CHECKED : MF_UNCHECKED));
      break;

    case WM_COMMAND:
      if (LOWORD(wParam) < PM_FIRST_CHILD){
	MAIN_MenuCommand(hWnd, LOWORD(wParam), lParam);
      }
      break;

    case WM_DESTROY:
      PostQuitMessage (0);
      break;
    }
  return DefFrameProcW(hWnd, Globals.hMDIWnd, msg, wParam, lParam);
}

/***********************************************************************
 *
 *           MAIN_MenuCommand
 */

static VOID MAIN_MenuCommand(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
  HLOCAL hActiveGroup    = GROUP_ActiveGroup();
  HLOCAL hActiveProgram  = PROGRAM_ActiveProgram(hActiveGroup);
  HWND   hActiveGroupWnd = GROUP_GroupWnd(hActiveGroup);

  switch(wParam)
    {
      /* Menu File */
    case PM_NEW:
      switch (DIALOG_New((hActiveGroupWnd && !IsIconic(hActiveGroupWnd)) ?
			 PM_NEW_PROGRAM : PM_NEW_GROUP))
      {
      case PM_NEW_PROGRAM:
	if (hActiveGroup) PROGRAM_NewProgram(hActiveGroup);
	break;

      case PM_NEW_GROUP:
	GROUP_NewGroup();
	break;
      }
      break;

    case PM_OPEN:
      if (hActiveProgram)
	PROGRAM_ExecuteProgram(hActiveProgram);
      else if (hActiveGroupWnd)
	OpenIcon(hActiveGroupWnd);
      break;

    case PM_MOVE:
    case PM_COPY:
      if (hActiveProgram)
	PROGRAM_CopyMoveProgram(hActiveProgram, wParam == PM_MOVE);
      break;

    case PM_DELETE:
      if (hActiveProgram)
	{
	if (DIALOG_Delete(IDS_DELETE_PROGRAM_s, PROGRAM_ProgramName(hActiveProgram)))
	  PROGRAM_DeleteProgram(hActiveProgram, TRUE);
	}
      else if (hActiveGroup)
	{
	if (DIALOG_Delete(IDS_DELETE_GROUP_s, GROUP_GroupName(hActiveGroup)))
	  GROUP_DeleteGroup(hActiveGroup);
	}
      break;

    case PM_ATTRIBUTES:
      if (hActiveProgram)
	PROGRAM_ModifyProgram(hActiveProgram);
      else if (hActiveGroup)
	GROUP_ModifyGroup(hActiveGroup);
      break;

    case PM_EXECUTE:
      DIALOG_Execute();
      break;

    case PM_EXIT:
      PostQuitMessage(0);
      break;

      /* Menu Options */
    case PM_AUTO_ARRANGE:
      Globals.bAutoArrange = !Globals.bAutoArrange;
      CheckMenuItem(Globals.hOptionMenu, PM_AUTO_ARRANGE,
		    MF_BYCOMMAND | (Globals.bAutoArrange ?
				    MF_CHECKED : MF_UNCHECKED));
      WritePrivateProfileStringA("Settings", "AutoArrange",
				Globals.bAutoArrange ? "1" : "0",
				Globals.lpszIniFile);
      WritePrivateProfileStringA(NULL,NULL,NULL,Globals.lpszIniFile); /* flush it */
      break;

    case PM_MIN_ON_RUN:
      Globals.bMinOnRun = !Globals.bMinOnRun;
      CheckMenuItem(Globals.hOptionMenu, PM_MIN_ON_RUN,
		    MF_BYCOMMAND | (Globals.bMinOnRun ?
				    MF_CHECKED : MF_UNCHECKED));
      WritePrivateProfileStringA("Settings", "MinOnRun",
				Globals.bMinOnRun ? "1" : "0",
				Globals.lpszIniFile);
      WritePrivateProfileStringA(NULL,NULL,NULL,Globals.lpszIniFile); /* flush it */
      break;

    case PM_SAVE_SETTINGS:
      Globals.bSaveSettings = !Globals.bSaveSettings;
      CheckMenuItem(Globals.hOptionMenu, PM_SAVE_SETTINGS,
		    MF_BYCOMMAND | (Globals.bSaveSettings ?
				    MF_CHECKED : MF_UNCHECKED));
      WritePrivateProfileStringA("Settings", "SaveSettings",
				Globals.bSaveSettings ? "1" : "0",
				Globals.lpszIniFile);
      WritePrivateProfileStringA(NULL,NULL,NULL,Globals.lpszIniFile); /* flush it */
      break;

      /* Menu Windows */
    case PM_OVERLAP:
      SendMessageW(Globals.hMDIWnd, WM_MDICASCADE, 0, 0);
      break;

    case PM_SIDE_BY_SIDE:
      SendMessageW(Globals.hMDIWnd, WM_MDITILE, MDITILE_VERTICAL, 0);
      break;

    case PM_ARRANGE:

      if (hActiveGroupWnd && !IsIconic(hActiveGroupWnd))
	ArrangeIconicWindows(hActiveGroupWnd);
      else
	SendMessageW(Globals.hMDIWnd, WM_MDIICONARRANGE, 0, 0);
      break;

      /* Menu Help */
    case PM_CONTENTS:
      if (!WinHelpA(Globals.hMainWnd, "progman.hlp", HELP_CONTENTS, 0))
	MAIN_MessageBoxIDS(IDS_WINHELP_ERROR, IDS_ERROR, MB_OK);
      break;

    case PM_ABOUT_WINE:
      ShellAboutA(hWnd, "WINE", "Program Manager", 0);
      break;

    default:
	MAIN_MessageBoxIDS(IDS_NOT_IMPLEMENTED, IDS_ERROR, MB_OK);
      break;
    }
}

/***********************************************************************
 *
 *           MAIN_RegisterMainWinClass
 */

static ATOM MAIN_RegisterMainWinClass(void)
{
  WNDCLASSW class;

  class.style         = CS_HREDRAW | CS_VREDRAW;
  class.lpfnWndProc   = MAIN_MainWndProc;
  class.cbClsExtra    = 0;
  class.cbWndExtra    = 0;
  class.hInstance     = Globals.hInstance;
  class.hIcon         = Globals.hMainIcon;
  class.hCursor       = LoadCursorW (0, (LPWSTR)IDC_ARROW);
  class.hbrBackground = GetStockObject (NULL_BRUSH);
  class.lpszMenuName  = 0;
  class.lpszClassName = STRING_MAIN_WIN_CLASS_NAME;

  return RegisterClassW(&class);
}

/***********************************************************************
 *
 *           MAIN_CreateMainWindow
 */

static VOID MAIN_CreateMainWindow(void)
{
  INT  left , top, right, bottom, width, height, show;
  CHAR buffer[100];

  Globals.hMDIWnd   = 0;
  Globals.hMainMenu = 0;

  /* Get the geometry of the main window */
  GetPrivateProfileStringA("Settings", "Window", "", buffer, sizeof(buffer), Globals.lpszIniFile);
  if (5 == sscanf(buffer, "%d %d %d %d %d", &left, &top, &right, &bottom, &show))
  {
    width  = right - left;
    height = bottom - top;
  }
  else
  {
    left = top = width = height = CW_USEDEFAULT;
    show = SW_SHOWNORMAL;
  }

  /* Create main Window */
  Globals.hMainWnd =
    CreateWindowW(STRING_MAIN_WIN_CLASS_NAME, NULL,
		  WS_OVERLAPPEDWINDOW, left, top, width, height,
		  0, 0, Globals.hInstance, 0);

  ShowWindow (Globals.hMainWnd, show);
  UpdateWindow (Globals.hMainWnd);
}

/***********************************************************************
 *
 *           MAIN_CreateMDIWindow
 */

static VOID MAIN_CreateMDIWindow(void)
{
  CLIENTCREATESTRUCT ccs;
  RECT rect;

  /* Get the geometry of the MDI window */
  GetClientRect(Globals.hMainWnd, &rect);

  ccs.hWindowMenu  = Globals.hWindowsMenu;
  ccs.idFirstChild = PM_FIRST_CHILD;

  /* Create MDI Window */
  Globals.hMDIWnd =
    CreateWindowW(STRING_MDI_WIN_CLASS_NAME, NULL,
		  WS_CHILD, rect.left, rect.top,
		  rect.right - rect.left, rect.bottom - rect.top,
		  Globals.hMainWnd, 0,
		  Globals.hInstance, &ccs);

  ShowWindow (Globals.hMDIWnd, SW_SHOW);
  UpdateWindow (Globals.hMDIWnd);
}

/**********************************************************************/
/***********************************************************************
 *
 *           MAIN_MessageBoxIDS
 */
INT MAIN_MessageBoxIDS(UINT ids_text, UINT ids_title, WORD type)
{
  CHAR text[MAX_STRING_LEN];
  CHAR title[MAX_STRING_LEN];

  LoadStringA(Globals.hInstance, ids_text, text, sizeof(text));
  LoadStringA(Globals.hInstance, ids_title, title, sizeof(title));

  return(MessageBoxA(Globals.hMainWnd, text, title, type));
}

/***********************************************************************
 *
 *           MAIN_MessageBoxIDS_s
 */
INT MAIN_MessageBoxIDS_s(UINT ids_text, LPCSTR str, UINT ids_title, WORD type)
{
  CHAR text[MAX_STRING_LEN];
  CHAR title[MAX_STRING_LEN];
  CHAR newtext[MAX_STRING_LEN + MAX_PATHNAME_LEN];

  LoadStringA(Globals.hInstance, ids_text, text, sizeof(text));
  LoadStringA(Globals.hInstance, ids_title, title, sizeof(title));
  wsprintfA(newtext, text, str);

  return(MessageBoxA(Globals.hMainWnd, newtext, title, type));
}

/***********************************************************************
 *
 *           MAIN_ReplaceString
 */

VOID MAIN_ReplaceString(HLOCAL *handle, LPSTR replace)
{
  HLOCAL newhandle = LocalAlloc(LMEM_FIXED, strlen(replace) + 1);
  if (newhandle)
    {
      LPSTR  newstring = LocalLock(newhandle);
      strcpy(newstring, replace);
      LocalFree(*handle);
      *handle = newhandle;
    }
  else MAIN_MessageBoxIDS(IDS_OUT_OF_MEMORY, IDS_ERROR, MB_OK);
}
