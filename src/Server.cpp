//
// Created by brenn on 9/1/2020.
//

#include "Server.h"

Server::Server() : Server(64) {

}

Server::Server(int blockSize) {
    running = false;
    errorState = false;
    connected = false;
    negotiating = false;

    addr = {};

    remoteSettings.bufferSize = 4096;
    remoteSettings.inputChannels = 1;
    remoteSettings.outputChannels = 1;

    inputBufferLock = nullptr;
    outputBufferLock = nullptr;

    inputBuffer = std::deque<dsp::Frame<engine::PORT_MAX_CHANNELS> >(16384);
    outputBuffer = std::deque<dsp::Frame<engine::PORT_MAX_CHANNELS> >(16384);

    this->blockSize = blockSize;
}

Server::~Server() {
    shutdownServer();
    if (inputBufferLock != nullptr) {
        delete inputBufferLock;
    }
    if (outputBufferLock != nullptr) {
        delete outputBufferLock;
    }
}

void Server::start() {
    INFO("Creating Server thread...");
    std::thread newThread(&Server::ServerLoop, this);
    newThread.detach();
    INFO("Done creating thread...");
}

void Server::stop() {
    shutdownServer();
}

bool Server::isConnected() {
    return connected;
}

bool Server::inErrorState() {
    return errorState;
}

int Server::getInputBufferSize() {
    return inputBuffer.size();
}

int Server::getOutputBufferSize() {
    return outputBuffer.size();
}

void Server::pushData(dsp::Frame<engine::PORT_MAX_CHANNELS, float> frame, int channelCount) {

    int counter = 0;
    while(inputBuffer.size() > (blockSize * 3) || in_buffer_overflow()) {
        counter += 1;
        if (counter > 25) break;
        std::this_thread::sleep_for (std::chrono::microseconds(15));
    }

    if (channelCount == 0) {
        inputBufferLock->lock();
        inputBuffer.clear();
        inputBufferLock->unlock();
    }

    inputBufferLock->lock();
    inputBuffer.push_front(frame);
    inputBufferLock->unlock();
    
    if (localSettings.inputChannels != channelCount) {
        localSettings.inputChannels = channelCount;
        inputBufferLock->lock();
        outputBufferLock->lock();
        inputBuffer.clear();
        outputBuffer.clear();
        inputBufferLock->unlock();
        outputBufferLock->unlock();
    }
}

dsp::Frame<engine::PORT_MAX_CHANNELS, float> Server::getData() {

    int counter = 0;
    while(outputBuffer.empty()) {
        std::this_thread::sleep_for (std::chrono::microseconds(1));
        counter += 1;
        if (counter > 4) break;
    }


    outputBufferLock->lock();
    if (!outputBuffer.empty()) {
        dsp::Frame<engine::PORT_MAX_CHANNELS, float> sample = outputBuffer.back();
        outputBuffer.pop_back();
        
        outputBufferLock->unlock();
        return sample;
    }
    outputBufferLock->unlock();
    return dsp::Frame<engine::PORT_MAX_CHANNELS, float>{};
}

int Server::getRemoteChannelCount() {
    return remoteSettings.inputChannels;
}

void Server::clearBuffers() {
    inputBuffer.clear();
    outputBuffer.clear();
}

bool Server::isNegotiating() {
    return negotiating;
}

int Server::getBlockSize() {
    return blockSize;
}

void Server::setBlockSize(int size) {
    blockSize = size;
}


void Server::ServerLoop() {

    INFO("Starting Server loop");
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    u_long on = 1;
    int err = setsockopt(serverSocket, SOL_SOCKET,  SO_REUSEADDR, (char *)&on, sizeof(on));

    if (err < 0) {
        INFO("Couldn't set socket option!");
        errorState = true;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (addr.sin_port < 1024 || addr.sin_port >= 65535) {
        errorState = true;
        return;
    }

    addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    err = bind(serverSocket, (SOCKADDR *)&addr, sizeof(addr));

    if (err == SOCKET_ERROR) {
        INFO("socket function failed with error: %ld\n", WSAGetLastError());
        return;
    }

    INFO("Err: %d", err);

    INFO("Starting listen");
    listen(serverSocket, 1);

    if (clientSocket != NULL) {
        closesocket(clientSocket);
        clientSocket = NULL;
    }

    INFO("On accept");
    clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
        INFO("accept failed with error: %ld\n", WSAGetLastError());
        shutdown(serverSocket, SD_BOTH);
        errorState = true;
    } else {
        INFO("ACCEPTED CONNECTION");
    }

    if (!errorState && !negotiateSettings()) {
        running = false;
        INFO("Failed to negotiate...");
    } else {
        INFO("Server THREAD STARTING");
        negotiating = false;
        connected = true;

        
        int floatSize = sizeof(float);
        char * buffer = new char[floatSize * 32768 * engine::PORT_MAX_CHANNELS];

        while (running) {
            if (inputBuffer.size() > blockSize) {

                DataPacket * packet = NULL;

                char packetBuffer[sizeof(DataPacket)] = {};
                packet = (DataPacket *)packetBuffer;
                packet->channels = localSettings.inputChannels;
                packet->len = blockSize;

                int bufferSize = floatSize * packet->channels * packet->len;
                float * samples = (float*) buffer;

                //Samples are striped
                /*
                 * N -> channels
                 * s -> s1 ch1,ch2,ch3,...,chN ,  s2 ch1,ch2...chN
                 */
                inputBufferLock->lock();
                for (int i = 0; i < packet->channels * packet->len; i += packet->channels) {
                    dsp::Frame<engine::PORT_MAX_CHANNELS, float> sample = {};
                    if (!inputBuffer.empty()) {
                        sample = inputBuffer.back();
                        inputBuffer.pop_back();
                    }
                    //Load the live data in only...
                    for (int j = 0; j < packet->channels; j++) {
                        samples[i+j] = sample.samples[j];
                    }
                }
                inputBufferLock->unlock();

                int err = send(clientSocket, packetBuffer, sizeof(DataPacket), 0);
                if (err == SOCKET_ERROR) {
                    INFO("Error sending packet... %d", WSAGetLastError());
                }

                if (bufferSize > 0) {
                    err = send(clientSocket, buffer, bufferSize, 0);
                    if (err == SOCKET_ERROR) {
                        INFO("Error sending data... %ld", WSAGetLastError());
                        running = false;
                        break;
                    }
                }

                err = recv(clientSocket, packetBuffer, sizeof(DataPacket), MSG_WAITALL);
                if (err == SOCKET_ERROR) {
                    INFO("Error receiving datapacket %ld", WSAGetLastError());
                    running = false;
                    break;
                }

                packet = (DataPacket *) packetBuffer;
                bufferSize = floatSize * packet->channels * packet->len;
                if (remoteSettings.inputChannels != packet->channels) {
                    remoteSettings.inputChannels = packet->channels;
                    inputBufferLock->lock();
                    outputBufferLock->lock();
                    inputBuffer.clear();
                    outputBuffer.clear();
                    inputBufferLock->unlock();
                    outputBufferLock->unlock();
                }

                if (bufferSize > 0) {

                    err = recv(clientSocket, buffer, bufferSize, MSG_WAITALL);
                    if (err == SOCKET_ERROR) {
                        INFO("Error receiving datapacket %ld", WSAGetLastError());
                        running = false;
                        break;
                    }

                    samples = (float*) buffer;
                    for (int i = 0; i < packet->channels * packet->len; i+= packet->channels) {
                        dsp::Frame<engine::PORT_MAX_CHANNELS, float> sample = {};
                        for (int j = 0; j < packet->channels; j++) {
                            sample.samples[j] = samples[i+j];
                        }
                        outputBufferLock->lock();
                        outputBuffer.push_front(sample);
                        outputBufferLock->unlock();
                    }
                }
            }


            std::this_thread::sleep_for (std::chrono::microseconds(12));

        }

        delete[] buffer;
    }

    INFO("Server THREAD TERMINATING");
    shutdownServer();
    errorState = true;
}

void Server::setBufferSize(int size) {

    if (size < 0) {
        size = 1;
    }

    bufferSize = size;
}

bool Server::in_buffer_overflow() {
    return inputBuffer.size() > bufferSize;
}

bool Server::in_buffer_underflow() {
    return inputBuffer.empty();
}

bool Server::out_buffer_overflow() {
    return outputBuffer.size() > bufferSize;
}

bool Server::out_buffer_underflow() {
    return outputBuffer.empty();
}

void Server::init() {
    WSADATA WSAData;
    WSAStartup(MAKEWORD(2, 2), &WSAData);

    inputBufferLock = new std::mutex();
    outputBufferLock = new std::mutex();
}

bool Server::startListen(int port) {

    INFO("Starting listen thread...");
    errorState = false;
    negotiating = false;
    this->port = port;

    if (!running) {
        running = true;
        start();
        return true;
    }

    return false;
}

void Server::setConnectionSettings(ConnectionNegotiation settings) {
    localSettings = settings;
}

bool Server::negotiateSettings() {
    ConnectionNegotiation *neg;

    char buffer[sizeof(ConnectionNegotiation) + 1] = {};
    negotiating = true;
    INFO("Sending negotionation info...");

    neg = (ConnectionNegotiation *)buffer;
    neg->sampleRate = localSettings.sampleRate;
    neg->blockSize = localSettings.blockSize;
    neg->bufferSize = localSettings.bufferSize;
    neg->inputChannels = localSettings.inputChannels;
    neg->outputChannels = localSettings.outputChannels;

    int err = send(clientSocket, buffer, sizeof(ConnectionNegotiation), 0);
    if (err == SOCKET_ERROR) {
        INFO("Error sending %ld", WSAGetLastError());
        shutdown(clientSocket, SD_BOTH);
        return false;
    }

    err = recv(clientSocket, buffer, sizeof(ConnectionNegotiation), MSG_WAITALL);
    if (err == 0 || err == SOCKET_ERROR) {
        shutdown(clientSocket, SD_BOTH);
        INFO("Failed to receive %ld", WSAGetLastError());
        return false;
    }

    remoteSettings.blockSize = neg->blockSize;
    remoteSettings.bufferSize = neg->bufferSize;
    remoteSettings.inputChannels = neg->inputChannels;
    remoteSettings.outputChannels = neg->outputChannels;
    remoteSettings.sampleRate = neg->sampleRate;
    INFO("Received connection details...");

    return true;
}

void Server::shutdownServer() {
    running = false;
    connected = false;
    shutdown(serverSocket, SD_BOTH);
    closesocket(serverSocket);
}
