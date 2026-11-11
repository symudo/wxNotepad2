#include "MongooseServer.h"
#include "pch.h"
#include "events.h"
#include "mainFrame.h"
#include "App.h"
MongooseServer::MongooseServer(MainFrame* mainFrame) :m_mainFrame(mainFrame) {
}

MongooseServer::~MongooseServer() {
    Stop();
}

void MongooseServer::SendResponse(uint64_t requestId, const std::string& json) {
    std::lock_guard<std::mutex> lock(m_responseMutex);
    m_responseQueue.push_back({ requestId, json });
}

bool MongooseServer::Start(int port, const wxString& html) {
    if (m_running) return false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_port = port;
    m_htmlContent = html;
    m_running = true;

    m_host_127 = "127.0.0.1:" + std::to_string(m_port);
    m_host_local = "localhost:" + std::to_string(m_port);

    m_thread = std::thread(&MongooseServer::Run, this, m_host_127);

    return true;
}

void MongooseServer::Stop() {
    m_running = false;
    
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void MongooseServer::Run(std::string address) {
    mg_mgr_init(&m_mgr);
    //尝试监听
    struct mg_connection* fn_conn = mg_http_listen(&m_mgr, address.c_str(), EventHandler, this);

    if (fn_conn == NULL) {
        m_running = false;
        mg_mgr_free(&m_mgr);
        return;
    }

    while (m_running) {
        mg_mgr_poll(&m_mgr, 100); // 每 100 毫秒检查一次网络事件
        // 处理待发送队列
        std::vector<NoteResponse> toSend;
        {
            std::lock_guard<std::mutex> lock(m_responseMutex);
            if (!m_responseQueue.empty()) toSend.swap(m_responseQueue);
        }

        for (const auto& res : toSend) {
            std::lock_guard<std::mutex> lock(m_sessionMutex);
            // 根据 ID 找连接
            auto it = m_sessions.find(res.requestId);
            if (it != m_sessions.end()) {
                struct mg_connection* c = it->second;
                mg_http_reply(c, 200,
                    "Content-Type: application/json; charset=utf-8\r\n"
                    "Access-Control-Allow-Origin: *\r\n",
                    "%s", res.jsonData.c_str());
                m_sessions.erase(it);
            }
        }
    }
    mg_mgr_free(&m_mgr);
}
/*
    HTTP POST请求：Query储存请求类型和参数，Body储存笔记内容
    1.获取存在笔记: action=get_by_url,url=...,Body=NULL
    2.储存关联笔记: action=save_by_url,url=...,title=...,Body=TEXT
    3.追加到指定笔记:action=append_to_note,title=...,Body=TEXT
*/
void MongooseServer::EventHandler(struct mg_connection* c, int ev, void* ev_data) {
    MongooseServer* self = static_cast<MongooseServer*>(c->fn_data);

    switch (ev) {
    case MG_EV_HTTP_MSG:
    {
        struct mg_http_message* hm = (struct mg_http_message*)ev_data;
        //仅本地Host
        struct mg_str* host = mg_http_get_header(hm, "Host");

        struct mg_str h1 = mg_str(self->m_host_127.c_str());
        struct mg_str h2 = mg_str(self->m_host_local.c_str());

        if (host == NULL || (mg_strcmp(*host, h1) != 0 && mg_strcmp(*host, h2) != 0)) {
            mg_http_reply(c, 403, "", "Forbidden: Localhost Only");
            return;
        }
        // OPTIONS 预检请求
        if (mg_strcmp(hm->method, mg_str("OPTIONS")) == 0) {
            mg_http_reply(c, 204,
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                "Access-Control-Allow-Headers: X-Notepad-Token, Content-Type\r\n"
                "Access-Control-Max-Age: 86400\r\n",
                "");
            return;
        }
        //验证 Token（全局变量验证）
        struct mg_str* tokenHeader = mg_http_get_header(hm, "X-Notepad-Token");
        bool isTokenValid = false;
        if (tokenHeader != NULL) {
            wxString receivedToken = wxString::FromUTF8(tokenHeader->buf, tokenHeader->len);
            if (receivedToken == GetMyAppInstance()->GetToken()) {
                isTokenValid = true;
            }
        }

        if (mg_strcmp(hm->method, mg_str("GET")) == 0) {
            // 仅允许访问主页
            if (mg_strcmp(hm->uri, mg_str("/")) == 0) {
                mg_http_reply(c, 200, "Content-Type: text/html; charset=utf-8\r\n",
                    "%s", self->m_htmlContent.ToUTF8().data());
            } else {
                mg_http_reply(c, 404, "", "Not Found");
            }
            return;
        }
        if (mg_strcmp(hm->method, mg_str("POST")) == 0) {
            if (!mg_match(hm->uri, mg_str("/api"), NULL)) {
                mg_http_reply(c, 404, "Access-Control-Allow-Origin: *\r\n", "Not Found");
                return;
            }
            if (!isTokenValid) {
                mg_http_reply(c, 401,
                    "Content-Type: application/json\r\n"
                    "Access-Control-Allow-Origin: *\r\n",
                    "{\"status\":\"error\",\"message\":\"Unauthorized: Invalid Token\"}");
                return;
            }
            // 仅处理 /api 路径的 POST 请求

            char action[32] = { 0 };
            // 如果 POST 请求中没有 action 参数，也视为无效请求
            if (mg_http_get_var(&hm->query, "action", action, sizeof(action)) <= 0) {
                mg_http_reply(c, 200,
                    "Content-Type: application/json\r\n"
                    "Access-Control-Allow-Origin: *\r\n",
                    "{\"status\":\"online\"}");
                break;
            }

            // 生成唯一 ID 并建立映射
            uint64_t rid;
            {
                std::lock_guard<std::mutex> lock(self->m_sessionMutex);
                rid = self->m_nextRequestId++;
                self->m_sessions[rid] = c;
                *(uint64_t*)&c->data[0] = rid;
            }

            // 封装纯数据任务
            auto task = new NoteTask();
            task->requestId = rid;
            task->action = wxString::FromUTF8(action);

            char buf[4096] = { 0 };
            if (mg_http_get_var(&hm->query, "url", buf, sizeof(buf)) > 0) {
                task->url = wxString::FromUTF8(buf);
            }

            memset(buf, 0, sizeof(buf));
            if (mg_http_get_var(&hm->query, "title", buf, sizeof(buf)) > 0) {
                task->title = wxString::FromUTF8(buf);
            }

            if (hm->body.len > 0) {
                task->content = wxString::FromUTF8(hm->body.buf, hm->body.len);
            }

            if (self->m_mainFrame) {
                wxThreadEvent* evt = new wxThreadEvent(wxEVT_THREAD, EVT_HTTP_REQUEST);
                evt->SetPayload(task);
                wxQueueEvent(self->m_mainFrame, evt);
            }
        } else {
            mg_http_reply(c, 405, "Access-Control-Allow-Origin: *\r\n", "Method Not Allowed");
        }

        break;
    }

    case MG_EV_CLOSE:
    case MG_EV_ERROR:
    {
        uint64_t rid = *(uint64_t*)&c->data[0];
        if (rid > 0) {
            std::lock_guard<std::mutex> lock(self->m_sessionMutex);
            self->m_sessions.erase(rid);
        }
        break;
    }
    default:
        break;
    }
}