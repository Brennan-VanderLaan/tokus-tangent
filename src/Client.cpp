//
// Created by brenn on 9/1/2020.
//

#include "Client.h"

Client::Client() : Client(64) {

}

Client::Client(int blockSize) {
    running = false;
    errorState = false;
    connected = false;

    addr = {};

    inputBuffer = dsp::RingBuffer<dsp::Frame<2>, 4096>{};
    outputBuffer = dsp::RingBuffer<dsp::Frame<2>, 4096>{};

    this->blockSize = blockSize;
}

Client::~Client() {
    shutdownClient();
}

void Client::start() {
    INFO("Creating client thread...");
    std::thread newThread(&Client::clientLoop, this);
    newThread.detach();
    INFO("Done creating thread...");
}

void Client::stop() {
    shutdownClient();
}

bool Client::isConnected() {
    return connected;
}

bool Client::inErrorState() {
    return errorState;
}

void Client::pushData(dsp::Frame<2, float> frame) {
    inputBuffer.push(frame);
}

dsp::Frame<2, float> Client::getData() {
    return outputBuffer.shift();
}

void Client::clearBuffers() {
    inputBuffer.clear();
    outputBuffer.clear();
}


void Client::clientLoop() {

    INFO("Starting client loop");

    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (clientSocket == INVALID_SOCKET) {
        INFO("socket function failed with error: %ld\n", WSAGetLastError());
        running = false;
    }

    SOCKADDR_IN tmp_addr = {};
    tmp_addr.sin_family = AF_INET;
    tmp_addr.sin_port = 5555;
    tmp_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int err = connect(clientSocket, (SOCKADDR *) &tmp_addr, sizeof(tmp_addr));
    if (err == -1) {
        INFO("connect function failed with error: %ld\n", WSAGetLastError());
    }

    INFO("ERROR: %d", err);


    if (!negotiateSettings()) {
        running = false;
        INFO("Failed to negotiate...");
    } else {
        INFO("CLIENT THREAD STARTING");
        while (running) {
            INFO("TICK");
            std::this_thread::sleep_for (std::chrono::seconds(1));
        }
    }

    INFO("CLIENT THREAD TERMINATING");
    shutdownClient();
    errorState = true;
}


void Client::init() {
    WSADATA WSAData;
    WSAStartup(MAKEWORD(2, 2), &WSAData);
}

bool Client::connectToHost(const std::string& host, int port) {

    INFO("CONNECT TO HOST");
    errorState = false;

    addr.sin_family = AF_INET;

    if (host.length() == 0) {
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        remoteHost = "127.0.0.1";
        INFO("LOCAL");
    } else {
        addr.sin_addr.s_addr = inet_addr(host.c_str());
        remoteHost = host;
    }

    addr.sin_port = port;
    if (addr.sin_port < 1024 || addr.sin_port >= 65535) {
        INFO("BAD PORT: %d", addr.sin_port);
        return false;
    }

    if (!running) {
        running = true;
        INFO("TRYING TO CONNECT");
        start();
        return true;
    }

    return false;
}

void Client::setConnectionSettings(ConnectionNegotiation settings) {
    localSettings = settings;
}

bool Client::negotiateSettings() {
    ConnectionNegotiation *neg;

    char buffer[sizeof(ConnectionNegotiation) + 1];
    INFO("receiving negotionation info...");
    int err = recv(clientSocket, buffer, sizeof(ConnectionNegotiation), 0);
    if (err == 0 || err == SOCKET_ERROR) {
        shutdown(clientSocket, SD_BOTH);
        INFO("Failed to receive");
        return false;
    }

    INFO("PAST RECV");
    neg = (ConnectionNegotiation *)buffer;
    remoteSettings.blockSize = neg->blockSize;
    remoteSettings.bufferSize = neg->bufferSize;
    remoteSettings.inputChannels = neg->inputChannels;
    remoteSettings.outputChannels = neg->outputChannels;
    remoteSettings.sampleRate = neg->sampleRate;
    INFO("Received connection details...");

    neg->sampleRate = localSettings.sampleRate;
    neg->blockSize = localSettings.blockSize;
    neg->bufferSize = localSettings.bufferSize;
    neg->inputChannels = localSettings.inputChannels;
    neg->outputChannels = localSettings.outputChannels;

    err = send(clientSocket, buffer, sizeof(ConnectionNegotiation), 0);
    if (err == 0 || err == SOCKET_ERROR) {
        shutdown(clientSocket, SD_BOTH);
        return false;
    }

    return true;
}

void Client::shutdownClient() {
    running = false;
    connected = false;
    shutdown(clientSocket, SD_BOTH);
    closesocket(clientSocket);
}
