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

    inputBuffer = std::vector<dsp::Frame<8>>(4096);
    outputBuffer = std::vector<dsp::Frame<8>>(4096);

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

void Server::pushData(dsp::Frame<8, float> frame, int channelCount) {
    inputBuffer.insert(inputBuffer.begin(), frame);
    localSettings.inputChannels = channelCount;
}

dsp::Frame<8, float> Server::getData() {
    if (!outputBuffer.empty()) {
        dsp::Frame<8, float> sample = outputBuffer.back();
        outputBuffer.pop_back();
        return sample;
    }


    return dsp::Frame<8, float>{};
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
        char * buffer = new char[floatSize * 16384 * 8];

        while (running) {
            if (inputBuffer.size() > 8192) {

                DataPacket * packet = NULL;

                char packetBuffer[sizeof(DataPacket)] = {};
                packet = (DataPacket *)packetBuffer;
                packet->channels = localSettings.inputChannels;
                packet->len = 8192;

                int bufferSize = floatSize * packet->channels * packet->len;
                float * samples = (float*) buffer;

                //Samples are striped
                /*
                 * N -> channels
                 * s -> s1 ch1,ch2,ch3,...,chN ,  s2 ch1,ch2...chN
                 */
                for (int i = 0; i < packet->channels * packet->len; i += packet->channels) {
                    dsp::Frame<8, float> sample = {};
                    sample = inputBuffer.back();
                    inputBuffer.pop_back();
                    //Load the live data in only...
                    for (int j = 0; j < packet->channels; j++) {
                        samples[i+j] = sample.samples[j];
                    }
                }

                int err = send(clientSocket, packetBuffer, sizeof(DataPacket), 0);
                if (err == SOCKET_ERROR) {
                    INFO("Error sending packet... %d", WSAGetLastError());
                }

                err = send(clientSocket, buffer, bufferSize, 0);
                if (err == SOCKET_ERROR) {
                    INFO("Error sending data... %ld", WSAGetLastError());
                    running = false;
                    break;
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
                        dsp::Frame<8, float> sample = {};
                        for (int j = 0; j < packet->channels; j++) {
                            sample.samples[j] = samples[i+j];
                        }
                        outputBuffer.insert(outputBuffer.begin(), sample);
                    }
                }
            }
        }

        delete[] buffer;
    }

    INFO("Server THREAD TERMINATING");
    shutdownServer();
    errorState = true;
}


bool Server::in_buffer_overflow() {
    return inputBuffer.size() > 32768;
}

bool Server::in_buffer_underflow() {
    return inputBuffer.empty();
}

bool Server::out_buffer_overflow() {
    return outputBuffer.size() > 32768;
}

bool Server::out_buffer_underflow() {
    return outputBuffer.empty();
}

void Server::init() {
    WSADATA WSAData;
    WSAStartup(MAKEWORD(2, 2), &WSAData);
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
