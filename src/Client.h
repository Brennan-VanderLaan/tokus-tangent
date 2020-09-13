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

    int getInputBufferSize();
    int getOutputBufferSize();
    
    int getRemoteChannelCount();
    int getBlockSize();
    void setBufferSize(int size);
    int getBufferSize();

    void pushData(dsp::Frame<engine::PORT_MAX_CHANNELS, float> frame, int channelCount);
    dsp::Frame<engine::PORT_MAX_CHANNELS, float> getData();


    bool in_buffer_overflow();
    bool in_buffer_underflow();
    bool out_buffer_overflow();
    bool out_buffer_underflow();


private:
    SOCKET clientSocket;
    std::string remoteHost;
    int port;
    int bufferSize;
    ConnectionNegotiation localSettings;
    ConnectionNegotiation remoteSettings;
    bool running;
    bool connected;
    bool errorState;

    int blockSize;
    std::deque<dsp::Frame<engine::PORT_MAX_CHANNELS>> inputBuffer;
    std::deque<dsp::Frame<engine::PORT_MAX_CHANNELS>> outputBuffer;

    std::mutex * inputBufferLock;
    std::mutex * outputBufferLock;

    void clientLoop();
    void start();
    void shutdownClient();
    bool negotiateSettings();
};


#endif //TOKUS_TANGENT_CLIENT_H
