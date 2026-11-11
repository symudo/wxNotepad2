#include "pch.h"

#include "App.h"
#include "mainFrame.h"
#include "Tray.h"

MyTrayIcon::MyTrayIcon(MainFrame* frame): m_frame(frame), wxTaskBarIcon(){
    wxIcon appIcon;
    if (appIcon.LoadFile(wxT("MAINICON"))){
        SetIcon(appIcon,L"wxNotepad2");
    }
    Bind(wxEVT_TASKBAR_LEFT_DCLICK, [this](wxTaskBarIconEvent&){m_frame->Show(true); m_frame->Iconize(false); SetForegroundWindow((HWND)m_frame->GetHandle()); });
}

wxMenu* MyTrayIcon::CreatePopupMenu(){
    wxMenu* menu = new wxMenu();
    menu->Append(MainFrame::MENU_TRAY_RESTORE, L"还原窗口");
    menu->Append(MainFrame::MENU_TRAY_EXIT, L"退出");
    Bind(wxEVT_MENU, [this](wxCommandEvent&){m_frame->Show(true); m_frame->Iconize(false); SetForegroundWindow((HWND)m_frame->GetHandle());  }, MainFrame::MENU_TRAY_RESTORE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&){m_frame->OnExit(); }, MainFrame::MENU_TRAY_EXIT);
   
    return menu;
}


