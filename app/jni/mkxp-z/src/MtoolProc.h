//
// Created by root on 5/29/25.
//

#ifndef MKXP_Z_MTOOLPROC_H
#define MKXP_Z_MTOOLPROC_H

#include <string>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "MToolClient.h"
#include "json.hpp"
extern int MtoolServerport;
struct MTOOL_WsTask
{
    long taskid;
    std::condition_variable cv;
    std::mutex mux;
    std::unique_lock<std::mutex> lck = std::unique_lock<std::mutex>(mux);
    nlohmann::json ret;
};
class MtoolProc {
public:
    MtoolProc();

    bool trsLoaded;
    void onEventLoop();
    nlohmann::json doTask(std::string cmd, nlohmann::json args, long timeoutMS = 0);
private:
    bool m_initialized = false;
    std::string m_host;
    int m_port;
    MToolClient* m_client;

    void init();

    void handleTextMessage(const std::string& message);
    void handleBinaryMessage(const std::vector<uint8_t>& data);
    void handleError(const std::string& error);
    void handleConnected();
    void handleClosed();

    std::string processTask(nlohmann::json j);
};


#endif //MKXP_Z_MTOOLPROC_H
