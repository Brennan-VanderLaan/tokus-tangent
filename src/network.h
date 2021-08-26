//
// Created by brenn on 8/21/2020.
//

#ifndef TOKUS_TANGENT_NETWORK_H
#define TOKUS_TANGENT_NETWORK_H

#include <stdint.h>

struct DataPacket {
    uint32_t len;
    uint32_t inputBufferSize;
    uint32_t outputBufferSize;
    uint8_t channels;
};

struct ConnectionNegotiation {
    uint8_t inputChannels;  //256
    uint8_t outputChannels; //256
    uint32_t blockSize;
    uint32_t bufferSize;
    uint32_t sampleRate;
}; //112

struct FancyPacket {
    uint32_t len;
    uint8_t inputChannels;
    uint8_t outputChannels;
    uint32_t sampleRate;
    float * data;
};



#endif //TOKUS_TANGENT_NETWORK_H
