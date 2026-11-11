#pragma once
#ifndef WINVER
#define WINVER 0x0600 
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600 
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCKAPI_ 

#include <WinSock2.h>
#include <windows.h> 

#include <sqlite3.h>
#include <oniguruma.h>
#include <tinyexpr.h>
#include <memory> 
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <wx/accel.h>
#include <wx/arrstr.h>
#include <wx/artprov.h>
#include <wx/bitmap.h>
#include <wx/bmpbndl.h>
#include <wx/bmpbuttn.h>
#include <wx/bookctrl.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/clipbrd.h>
#include <wx/clrpicker.h>
#include <wx/config.h>
#include <wx/dataobj.h>
#include <wx/dialog.h>
#include <wx/dirdlg.h>
#include <wx/dnd.h>
#include <wx/event.h>
#include <wx/fdrepdlg.h>
#include <wx/fileconf.h>
#include <wx/filename.h> 
#include <wx/font.h>
#include <wx/fontpicker.h>
#include <wx/gdicmn.h>
#include <wx/image.h>
#include <wx/log.h>
#include <wx/menu.h>
#include <wx/msw/private.h> 
#include <wx/panel.h>
#include <wx/popupwin.h>
#include <wx/propdlg.h>
#include <wx/regex.h>
#include <wx/scrolwin.h>  
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/splitter.h>
#include <wx/srchctrl.h>
#include <wx/statbmp.h> 
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/stc/stc.h>
#include <wx/stdpaths.h>
#include <wx/string.h>
#include <wx/taskbar.h>
#include <wx/textctrl.h>
#include <wx/tokenzr.h>
#include <wx/treebase.h>
#include <wx/treectrl.h>
#include <wx/valnum.h>
#include <wx/window.h>
#include <wx/wrapsizer.h> 
#include <wx/wxprec.h>

#ifndef LOGGING_MACROS_H
#define LOGGING_MACROS_H


#ifdef _DEBUG 
// Debug 模式：启用日志功能
#define _wxLogError(format, ...)   wxLogError(format, __VA_ARGS__)
#define _wxLogDebug(format, ...)   wxLogDebug(format, __VA_ARGS__)
#define _wxLogMessage(format, ...) wxLogMessage(format, __VA_ARGS__)
#else
#define _wxLogError(format, ...)   ((void)0)
#define _wxLogDebug(format, ...)   ((void)0)
#define _wxLogMessage(format, ...) ((void)0)
#endif // _DEBUG

#endif // LOGGING_MACROS_H


#include "FontHelper.h"