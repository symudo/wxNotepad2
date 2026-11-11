#pragma once
#include "mongoose.h"
#include <thread>
#include <atomic>
#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>
class MainFrame;
struct NoteTask{
    uint64_t requestId;  
    wxString action;
    wxString url;
    wxString title;
    wxString content;
};

struct NoteResponse{
    uint64_t requestId;
    std::string jsonData;
};

class MongooseServer{
public:
    MongooseServer(MainFrame* mainFrame);
    ~MongooseServer();

    void SendResponse(uint64_t requestId, const std::string& json);
    bool Start(int port, const wxString& html);
    void Stop();
    bool IsRunning() const{ return m_running; }

private:
    void Run(std::string address);
    uint64_t m_nextRequestId = 1;
    std::unordered_map<uint64_t, struct mg_connection*> m_sessions; // ID -> 连接映射
    std::mutex m_sessionMutex;                                 // 保护映射表

    std::vector<NoteResponse> m_responseQueue;                 // 待发送响应队列
    std::mutex m_responseMutex;
    static void EventHandler(struct mg_connection* c, int ev, void* ev_data);
    int m_port;
    wxString m_htmlContent;
    struct mg_mgr m_mgr;
    std::thread m_thread;
    std::atomic<bool> m_running{ false };
    std::function<void(const std::string&)> m_onMessage;
    MainFrame* m_mainFrame;
    std::string m_host_127;
    std::string m_host_local;
};