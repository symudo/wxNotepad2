#include "pch.h"

#include "App.h"
#include "Helper.h"
#include "mainFrame.h"
#include "Tray.h"

MyTrayIcon::MyTrayIcon(MainFrame* frame): m_frame(frame), wxTaskBarIcon(){
    int smW = ::GetSystemMetrics(SM_CXSMICON);
    int smH = ::GetSystemMetrics(SM_CYSMICON);
    if (smW <= 0) smW = 16;
    if (smH <= 0) smH = 16;
    HICON hTrayIcon = static_cast<HICON>(::LoadImageW(
        wxGetInstance(), L"MAINICON", IMAGE_ICON, smW, smH, LR_DEFAULTCOLOR));
    if (hTrayIcon){
        wxIcon trayIcon;
        trayIcon.CreateFromHICON(hTrayIcon);
        if (trayIcon.IsOk()){
            SetIcon(trayIcon, L"wxNotepad2");
        } else {
            ::DestroyIcon(hTrayIcon);
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


