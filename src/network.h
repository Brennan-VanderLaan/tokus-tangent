//
// Created by brenn on 8/21/2020.
//

#ifndef TOKUS_TANGENT_NETWORK_H
#define TOKUS_TANGENT_NETWORK_H


struct dataPacket {
    int len;
    int channels;
    float * data;
};

struct connectionNegotiation {
    int inputChannels;
    int outputChannels;
    int blockSize;
    int bufferSize;
    int sampleRate;
};

struct fancyPacket {
    int len;
    int inputChannels;
    int outputChannels;
    int sampleRate;
    float * data;
};



#endif //TOKUS_TANGENT_NETWORK_H
