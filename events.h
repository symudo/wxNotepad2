#pragma once

#include <wx/event.h>
#include <wx/treebase.h>

//由TreeCtrl发送，传递构造数据，显示所选择的面板
//如果是笔记节点，附加SetInt信息：是否可更改（0，1），要使用的indicator类型（2位数）；计算方法indicator类型<<1+是否可更改
wxDECLARE_EVENT(EVT_SHOW_NODE_CONTENT, wxTreeEvent);
wxDECLARE_EVENT(EVT_SHOW_EMPTY_MESSAGE, wxCommandEvent);//treeCtrl发送，显示空消息
wxDECLARE_EVENT(EVT_CARD_LEFT_DCLICK, wxCommandEvent);//MyCard中双击
wxDECLARE_EVENT(EVT_UPDATE_NOTE_TAGS,wxTreeEvent);
wxDECLARE_EVENT(EVT_HTTP_REQUEST, wxThreadEvent);