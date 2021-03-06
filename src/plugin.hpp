#pragma once
#include <rack.hpp>


using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin *pluginInstance;

// Declare each Model, defined in each module source file
// extern Model *modelMyModule;


extern Model *modelRadioClient;
extern Model *modelRadioBroadcast;
extern Model *modelRadioServer;
extern Model *modelRadioDelay;