//
// Created by brenn on 8/21/2020.
//

#ifndef TOKUS_TANGENT_NETWORK_H
#define TOKUS_TANGENT_NETWORK_H


struct DataPacket {
    int len;
    int inputBufferSize;
    int outputBufferSize;
    int channels;
};

struct ConnectionNegotiation {
    int inputChannels;
    int outputChannels;
    int blockSize;
    int bufferSize;
    int sampleRate;
};

struct FancyPacket {
    int len;
    int inputChannels;
    int outputChannels;
    int sampleRate;
    float * data;
};



#endif //TOKUS_TANGENT_NETWORK_H
