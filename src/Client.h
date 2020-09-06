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

    void pushData(dsp::Frame<2, float> frame);
    dsp::Frame<2, float> getData();


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
    dsp::RingBuffer<dsp::Frame<2>, 4096> inputBuffer;
    dsp::RingBuffer<dsp::Frame<2>, 4096> outputBuffer;

    void clientLoop();
    void start();
    void shutdownClient();
    bool negotiateSettings();
};


#endif //TOKUS_TANGENT_CLIENT_H
