//
// Created by brenn on 9/1/2020.
//

#ifndef TOKUS_TANGENT_CLIENT_H
#define TOKUS_TANGENT_CLIENT_H
#include <winsock2.h>
#include <WS2tcpip.h>
#include <string.hpp>
#include <thread>
#include "network.h"
#include "plugin.hpp"
#include <vector>
#include <mutex>

class Client {

public:
    Client();
    Client(int blocksize);
    ~Client();
    void init();
    bool connectToHost(const std::string& host, int port);
    void stop();

    void clearBuffers();
    bool isConnected();
    bool inErrorState();

    void setConnectionSettings(ConnectionNegotiation settings);

    int getRemoteChannelCount();

    void pushData(dsp::Frame<8, float> frame, int channelCount);
    dsp::Frame<8, float> getData();


    bool in_buffer_overflow();
    bool in_buffer_underflow();
    bool out_buffer_overflow();
    bool out_buffer_underflow();


private:
    SOCKET clientSocket;
    std::string remoteHost;
    int port;
    ConnectionNegotiation localSettings;
    ConnectionNegotiation remoteSettings;
    bool running;
    bool connected;
    bool errorState;

    int blockSize;
    std::deque<dsp::Frame<8>> inputBuffer;
    std::deque<dsp::Frame<8>> outputBuffer;

    std::mutex * inputBufferLock;
    std::mutex * outputBufferLock;

    void clientLoop();
    void start();
    void shutdownClient();
    bool negotiateSettings();
};


#endif //TOKUS_TANGENT_CLIENT_H
