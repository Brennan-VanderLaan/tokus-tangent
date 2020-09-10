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

    inputBufferLock = nullptr;
    outputBufferLock = nullptr;

    inputBuffer = std::deque<dsp::Frame<8> >(16384);
    outputBuffer = std::deque<dsp::Frame<8> >(16384);

    remoteSettings.bufferSize = 4096;
    remoteSettings.inputChannels = 1;
    remoteSettings.outputChannels = 1;

    this->blockSize = blockSize;
}

Client::~Client() {
    shutdownClient();
    
    if (inputBufferLock != nullptr) {
        delete inputBufferLock;
    }
    if (outputBufferLock != nullptr) {
        delete outputBufferLock;
    }
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

void Client::pushData(dsp::Frame<8, float> frame, int channelCount) {
    inputBufferLock->lock();
    inputBuffer.push_front(frame);
    inputBufferLock->unlock();
    localSettings.inputChannels = channelCount;
}

dsp::Frame<8, float> Client::getData() {
    outputBufferLock->lock();
    if (!outputBuffer.empty()) {
        dsp::Frame<8, float> sample = outputBuffer.back();
        outputBuffer.pop_back();
        outputBufferLock->unlock();
        return sample;
    }

    outputBufferLock->unlock();
    return dsp::Frame<8, float>{};
}

int Client::getRemoteChannelCount() {
    return remoteSettings.inputChannels;
}

void Client::clearBuffers() {
    inputBuffer.clear();
    outputBuffer.clear();
}

bool Client::in_buffer_overflow() {
    return inputBuffer.size() > 32768;
}

bool Client::in_buffer_underflow() {
    return inputBuffer.empty();
}

bool Client::out_buffer_overflow() {
    return outputBuffer.size() > 32768;
}

bool Client::out_buffer_underflow() {
    return outputBuffer.empty();
}


void Client::clientLoop() {

    INFO("Starting client loop");

    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        INFO("socket function failed with error: %ld\n", WSAGetLastError());
        running = false;
    }

    DWORD timeout = 5000;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo * result = NULL;
    //This gets us a valid address to connect to *if* we can
    int err = getaddrinfo(remoteHost.c_str(), std::to_string(port).c_str(), &hints, &result);
    if (err == -1) {
        INFO("getaddrinfo failed with error: %ld\n", WSAGetLastError());
        running = false;
        errorState = true;
    }

    //Connect to it...
    err = connect(clientSocket, result->ai_addr, result->ai_addrlen);
    if (err == -1) {
        INFO("connect function failed with error: %ld\n", WSAGetLastError());
        running = false;
        errorState = true;
    }

    if (!negotiateSettings()) {
        running = false;
        INFO("Failed to negotiate...");
    } else {
        INFO("CLIENT THREAD STARTING");
        connected = true;
        try {
            int floatSize = sizeof(float);
            char * buffer = new char[floatSize * 16384 * 8];

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
                //Figure out the size of the data trailer
                int bufferSize = floatSize * packet->channels * packet->len;

                if (packet->channels != remoteSettings.inputChannels) {
                    remoteSettings.inputChannels = packet->channels;
                }

                if (bufferSize > 0) {
                    err = recv(clientSocket, buffer, bufferSize, MSG_WAITALL);
                    if (err == SOCKET_ERROR) {
                        INFO("Error receiving datapacket %ld", WSAGetLastError());
                        running = false;
                        break;
                    }
                }

                //Walk the buffer for samples
                float * sampleBuffer = (float *) buffer;
                for (int i = 0; i < packet->channels * packet->len; i+= packet->channels) {
                    dsp::Frame<8, float> sample = {};
                    for (int j = 0; j < packet->channels; j++) {
                        sample.samples[j] = sampleBuffer[i+j];
                    }
                    outputBufferLock->lock();
                    outputBuffer.push_front(sample);
                    outputBufferLock->unlock();
                }

                //Decide how much we are sending back
                if (inputBuffer.size() > 8192) {
                    packet->len = 8192;
                } else {
                    packet->len = inputBuffer.size();
                }
                packet->channels = localSettings.inputChannels;

                //Load the buffer
                bufferSize = floatSize * packet->channels * packet->len;
                if (bufferSize > 0) {
                    for (int i = 0; i < packet->channels * packet->len; i += packet->channels) {
                        dsp::Frame<8, float> sample = {};
                        inputBufferLock->lock();
                        if (!inputBuffer.empty()) {
                            sample = inputBuffer.back();
                            inputBuffer.pop_back();
                        }
                        inputBufferLock->unlock();

                        for (int j = 0; j < packet->channels; j++) {
                            sampleBuffer[i+j] = sample.samples[j];
                        }
                    }
                }

                //Tell the server how many samples we are sending
                // Can be 0
                err = send(clientSocket, packetBuffer, sizeof(DataPacket), 0);
                if (err == SOCKET_ERROR) {
                    INFO("Error sending packet... %d", WSAGetLastError());
                    running = false;
                    break;
                }

                //Send the samples if we have any
                if (bufferSize > 0) {
                    err = send(clientSocket, buffer, bufferSize, 0);
                    if (err == SOCKET_ERROR) {
                        INFO("Error sending data... %ld", WSAGetLastError());
                        running = false;
                        break;
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
    //TODO: this should be centralized...
    WSADATA WSAData;
    WSAStartup(MAKEWORD(2, 2), &WSAData);

    inputBufferLock = new std::mutex();
    outputBufferLock = new std::mutex();
}

bool Client::connectToHost(const std::string& host, int port) {

    INFO("CONNECT TO HOST");
    errorState = false;

    if (host.length() == 0) {
        remoteHost = "127.0.0.1";
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
