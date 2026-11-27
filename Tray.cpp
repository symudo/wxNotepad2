#include "pch.h"

#include "App.h"
#include "mainFrame.h"
#include "Tray.h"

MyTrayIcon::MyTrayIcon(MainFrame* frame): m_frame(frame), wxTaskBarIcon(){
    wxIconBundle appIcons(wxT("MAINICON"), wxBITMAP_TYPE_ICO_RESOURCE);
    if (!appIcons.IsEmpty()){
        int smW = ::GetSystemMetrics(SM_CXSMICON);
        int smH = ::GetSystemMetrics(SM_CYSMICON);
        wxIcon trayIcon = appIcons.GetIcon(wxSize(smW > 0 ? smW : 16, smH > 0 ? smH : 16));
        if (trayIcon.IsOk()){
            SetIcon(trayIcon, L"wxNotepad2");
        }
    }
    Bind(wxEVT_TASKBAR_LEFT_DCLICK, [this](wxTaskBarIconEvent&){m_frame->Show(true); m_frame->Iconize(false); SetForegroundWindow((HWND)m_frame->GetHandle()); });
    Bind(wxEVT_MENU, [this](wxCommandEvent&){m_frame->Show(true); m_frame->Iconize(false); SetForegroundWindow((HWND)m_frame->GetHandle()); }, MainFrame::MENU_TRAY_RESTORE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&){m_frame->OnExit(); }, MainFrame::MENU_TRAY_EXIT);
}

wxMenu* MyTrayIcon::CreatePopupMenu(){
    wxMenu* menu = new wxMenu();
    menu->Append(MainFrame::MENU_TRAY_RESTORE, L"还原窗口");
    menu->Append(MainFrame::MENU_TRAY_EXIT, L"退出");
    return menu;
}


