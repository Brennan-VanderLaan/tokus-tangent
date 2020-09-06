//
// Created by brenn on 9/1/2020.
//

#include "Client.h"

#include <exception>

Client::Client() : Client(64) {

}

Client::Client(int blockSize) {
    running = false;
    errorState = false;
    connected = false;
    port = 5555;

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

    struct addrinfo hints = {};

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo * result = NULL;

    int err = getaddrinfo(remoteHost.c_str(), std::to_string(port).c_str(), &hints, &result);

    err = connect(clientSocket, result->ai_addr, result->ai_addrlen);
    if (err == -1) {
        INFO("connect function failed with error: %ld\n", WSAGetLastError());
    }

    INFO("ERROR: %d", err);


    if (!negotiateSettings()) {
        running = false;
        INFO("Failed to negotiate...");
    } else {
        INFO("CLIENT THREAD STARTING");
        connected = true;
        try {
            int floatSize = sizeof(float);
            char * buffer = new char[floatSize * 2048];

            while (running) {
                
                //Server is going to send a data packet, 
                // and then we'll know how much data

                DataPacket * packet = NULL;

                
                char packetBuffer[sizeof(DataPacket)] = {};

                err = recv(clientSocket, packetBuffer, sizeof(DataPacket), MSG_WAITALL);
                if (err == SOCKET_ERROR) {
                    INFO("Error receiving datapacket %ld", WSAGetLastError());
                }

                packet = (DataPacket *)packetBuffer;
                int bufferSize = floatSize * packet->channels * packet->len;

                err = recv(clientSocket, buffer, bufferSize, MSG_WAITALL);
                if (err == SOCKET_ERROR) {
                    INFO("Error receiving datapacket %ld", WSAGetLastError());
                }

                float * sampleBuffer = (float *) buffer;

                for (int i = 0; i < packet->channels * packet->len; i+= packet->channels) {
                    dsp::Frame<2, float> sample = {};
                    sample.samples[0] = sampleBuffer[i];
                    sample.samples[1] = sampleBuffer[i+1];
                    outputBuffer.push(sample);
                }

                //Okay, now send our buffered data...
                if (inputBuffer.size() > 400) {
                    packet->len = 400;
                } else {
                    packet->len = inputBuffer.size();
                }

                bufferSize = floatSize * packet->channels * packet->len;

                if (bufferSize > 0) {

                    for (int i = 0; i < packet->channels * packet->len; i += packet->channels) {
                        dsp::Frame<2, float> sample = {};
                        sample = inputBuffer.shift();
                        sampleBuffer[i] = sample.samples[0];
                        sampleBuffer[i+1] = sample.samples[1];
                    }
                }

                err = send(clientSocket, packetBuffer, sizeof(DataPacket), 0);
                if (err == SOCKET_ERROR) {
                    INFO("Error sending packet... %d", WSAGetLastError());
                }

                if (bufferSize > 0) {
                    err = send(clientSocket, buffer, bufferSize, 0);
                    if (err == SOCKET_ERROR) {
                        INFO("Error sending data... %ld", WSAGetLastError());
                    }
                }
            }

            delete[] buffer;

        } catch (std::exception &e) {
            INFO("Error in main client loop...");
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

    if (host.length() == 0) {
        remoteHost = "127.0.0.1";
        INFO("LOCAL");
    } else {
        remoteHost = host;
    }

    if (port < 1024 || port >= 65535) {
        INFO("BAD PORT: %d", port);
        return false;
    }
    this->port = port;

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

    char buffer[sizeof(ConnectionNegotiation) + 1] = {};
    INFO("receiving negotionation info...");
    int err = recv(clientSocket, buffer, sizeof(ConnectionNegotiation), MSG_WAITALL);
    if (err == SOCKET_ERROR) {
        shutdown(clientSocket, SD_BOTH);
        INFO("Failed to receive %d", WSAGetLastError());
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
    if (err == SOCKET_ERROR) {
        shutdown(clientSocket, SD_BOTH);
        INFO("Error sending... %d", WSAGetLastError());
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
