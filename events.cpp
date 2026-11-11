#include "pch.h"
#include "events.h"

wxDEFINE_EVENT(EVT_SHOW_EMPTY_MESSAGE, wxCommandEvent);
wxDEFINE_EVENT(EVT_SHOW_NODE_CONTENT, wxTreeEvent);

wxDEFINE_EVENT(EVT_CARD_LEFT_DCLICK, wxCommandEvent);
wxDEFINE_EVENT(EVT_UPDATE_NOTE_TAGS, wxTreeEvent);
wxDEFINE_EVENT(EVT_HTTP_REQUEST, wxThreadEvent);