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
#include <vector>
#include <mutex>

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

    int getRemoteChannelCount();
    int getBlockSize();
    void setBlockSize(int size);

    void setBufferSize(int size);

    void pushData(dsp::Frame<engine::PORT_MAX_CHANNELS, float> frame, int channelCount);
    dsp::Frame<engine::PORT_MAX_CHANNELS, float> getData();

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
    int bufferSize;
    std::deque<dsp::Frame<engine::PORT_MAX_CHANNELS>> inputBuffer;
    std::deque<dsp::Frame<engine::PORT_MAX_CHANNELS>> outputBuffer;

    std::mutex * inputBufferLock;
    std::mutex * outputBufferLock;

    void ServerLoop();
    void start();
    void shutdownServer();
    bool negotiateSettings();
};


#endif //TOKUS_TANGENT_Server_H
