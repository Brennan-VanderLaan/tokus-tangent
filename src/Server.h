//
// Created by brenn on 9/4/2020.
//

#ifndef TOKUS_TANGENT_Server_H
#define TOKUS_TANGENT_Server_H
#include <winsock2.h>
#include <string.hpp>
#include <thread>
#include "network.h"
#include "plugin.hpp"

class Server {

public:
    Server();
    Server(int blocksize);
    ~Server();
    void init();
    bool startListen(int port);
    void stop();

    void clearBuffers();
    bool isNegotiating();
    bool isConnected();
    bool inErrorState();

    void setConnectionSettings(ConnectionNegotiation settings);

    void pushData(dsp::Frame<2, float> frame);
    dsp::Frame<2, float> getData();

    bool in_buffer_overflow();
    bool in_buffer_underflow();
    bool out_buffer_overflow();
    bool out_buffer_underflow();


private:
    SOCKET serverSocket;
    SOCKET clientSocket;
    SOCKADDR_IN addr;
    std::string remoteHost;
    ConnectionNegotiation localSettings;
    ConnectionNegotiation remoteSettings;
    bool running;
    bool connected;
    bool negotiating;
    bool errorState;

    int port;

    int blockSize;
    dsp::RingBuffer<dsp::Frame<2>, 16384> inputBuffer;
    dsp::RingBuffer<dsp::Frame<2>, 16384> outputBuffer;

    void ServerLoop();
    void start();
    void shutdownServer();
    bool negotiateSettings();
};


#endif //TOKUS_TANGENT_Server_H
