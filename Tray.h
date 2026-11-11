#pragma once
#include "wx/taskbar.h"
#include "SVG_ICON.h"
class MainFrame;
class MyTrayIcon : public wxTaskBarIcon {
public:
    MyTrayIcon(MainFrame* frame);
    virtual wxMenu* CreatePopupMenu() override;
private:
    MainFrame* m_frame;
};