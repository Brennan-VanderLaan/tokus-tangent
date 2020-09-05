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

    addr = {};

    inputBuffer = dsp::RingBuffer<dsp::Frame<2>, 4096>{};
    outputBuffer = dsp::RingBuffer<dsp::Frame<2>, 4096>{};

    this->blockSize = blockSize;
}

Server::~Server() {
    shutdownServer();
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

void Server::pushData(dsp::Frame<2, float> frame) {
    inputBuffer.push(frame);
}

dsp::Frame<2, float> Server::getData() {
    return outputBuffer.shift();
}

void Server::clearBuffers() {
    inputBuffer.clear();
    outputBuffer.clear();
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

    SOCKET clientSocket;
    INFO("On accept");
    clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
        INFO("accept failed with error: %ld\n", WSAGetLastError());
        closesocket(serverSocket);
        errorState = true;
    } else {
        INFO("ACCEPTED CONNECTION");
    }


    if (!errorState && !negotiateSettings()) {
        running = false;
        INFO("Failed to negotiate...");
    } else {
        INFO("Server THREAD STARTING");
        while (running) {
            INFO("TICK");
            std::this_thread::sleep_for (std::chrono::seconds(1));
        }
    }

    INFO("Server THREAD TERMINATING");
    shutdownServer();
    errorState = true;
}


void Server::init() {
    WSADATA WSAData;
    WSAStartup(MAKEWORD(2, 2), &WSAData);
}

bool Server::startListen(int port) {

    INFO("Starting listen thread...");
    errorState = false;
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

    char buffer[sizeof(ConnectionNegotiation) + 1];
    INFO("receiving negotionation info...");


    INFO("PAST RECV");
    neg = (ConnectionNegotiation *)buffer;
    neg->sampleRate = localSettings.sampleRate;
    neg->blockSize = localSettings.blockSize;
    neg->bufferSize = localSettings.bufferSize;
    neg->inputChannels = localSettings.inputChannels;
    neg->outputChannels = localSettings.outputChannels;

    int err = send(serverSocket, buffer, sizeof(ConnectionNegotiation), 0);
    if (err == 0 || err == SOCKET_ERROR) {
        shutdown(serverSocket, SD_BOTH);
        return false;
    }


    err = recv(serverSocket, buffer, sizeof(ConnectionNegotiation), 0);
    if (err == 0 || err == SOCKET_ERROR) {
        shutdown(serverSocket, SD_BOTH);
        INFO("Failed to receive");
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
