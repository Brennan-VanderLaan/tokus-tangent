//
// Created by bvanderlaan on 8/19/2020.
//

#include "network.h"
#include "plugin.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>


enum ServerState {
    NOT_CONNECTED,
    LISTENING,
    CONNECTED,
    NEGOTIATING,
    ERROR_STATE,
    NUM_STATES
};


struct RadioServer : Module {
    WSADATA WSAData;
    SOCKET server;
    SOCKET client;
    SOCKADDR_IN addr;
    dsp::RingBuffer < dsp::Frame<2>, 512> inputBuffer;
    dsp::RingBuffer < dsp::Frame<2>, 512> outputBuffer;

    dsp::SampleRateConverter<2> inputSrc;
    dsp::SampleRateConverter<2> outputSrc;

    std::string portField = "5555";

    ServerState moduleState = NOT_CONNECTED;
    int errorCounter = 0;
    int negotiateCounter = 0;
    int acceptConnCounter = 0;
    int disconnectCheckCounter = 0;

    bool fatalError = false;

    int clientSampleRate = 0;
    int clientBufferSize = 0;
    int clientBlockSize = 0;
    int clientInputChannels = 0;
    int clientOutputChannels = 0;

    std::thread io_thread;


    TextField* portFieldWidget;
    TextField* blockSizeFieldWidget;
    TextField* bufferSizeFieldWidget;

    enum ParamIds {
        STATION_PARAM,
        LEVEL_PARAM,
        NUM_PARAMS
    };

    enum InputIds {
        TUNE_INPUT,
        IN1_INPUT,
        IN2_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        OUT1_OUTPUT,
        OUT2_OUTPUT,
        OUT3_OUTPUT,
        OUT4_OUTPUT,
        OUT5_OUTPUT,
        OUT6_OUTPUT,
        OUT7_OUTPUT,
        OUT8_OUTPUT,
        NUM_OUTPUTS
    };

    RadioServer() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, ServerState::NUM_STATES);
        //configParam(STATION_PARAM, 0.0, 1.0, 1.0, "Station Tune", "%", 0, 100);
        //configParam(LEVEL_PARAM, 0.0, 1.0, 1.0, "Out level", "%", 0, 100);

        INFO("Init");

        server = 0;
        client = 0;

        WSAStartup(MAKEWORD(2,0), &WSAData);
        server = socket(AF_INET, SOCK_STREAM, 0);

        u_long on = 1;
        int err = setsockopt(server, SOL_SOCKET,  SO_REUSEADDR, (char *)&on, sizeof(on));

        if (err < 0) {
            INFO("Couldn't set socket option!");
            fatalError = true;
        }

        ioctlsocket(server, FIONBIO, &on);
        if (err < 0)
        {
            INFO("Couldn't set non-blocking socket option!");
            fatalError = true;
        }

        moduleState = ServerState::ERROR_STATE;
    }


    ~RadioServer() {
        closesocket(server);
    }


    void getData() {

    }

    void sendData() {

    }

    void bufferInputSamples(const ProcessArgs &args) {
        float in_1 = inputs[IN1_INPUT].getVoltage();
        float in_2 = inputs[IN2_INPUT].getVoltage();
    }

    void tryToListen() {
        if (moduleState == ServerState::NOT_CONNECTED) {

            addr.sin_family = AF_INET;

            if (portField.length() == 0) {
                addr.sin_port = htons(5555);
            } else {
                addr.sin_port = htons(std::stoi(portField));
            }

            if (addr.sin_port < 1024 || addr.sin_port >= 65535) {
                moduleState = ServerState::ERROR_STATE;
                return;
            }

            addr.sin_addr.s_addr = inet_addr("0.0.0.0");
            int err = bind(server, (SOCKADDR *)&addr, sizeof(addr));

            if (err == SOCKET_ERROR) {
                moduleState = ServerState::ERROR_STATE;
                return;
            }

            listen(server, 1);
            acceptConnCounter = 0;
            moduleState = ServerState::LISTENING;
        }
    }

    void negotiateSettings(const ProcessArgs &args) {

        /*
         Super simple

        Server -> Client
            connectionInfo

         Client -> Server
            connectionInfo

         Advance to CONNECTED
         */

        connectionNegotiation *neg;

        char buffer[sizeof(connectionNegotiation)+1];



        INFO("setting negotionation info...");

        neg = (connectionNegotiation *)buffer;
        neg->sampleRate = (int)args.sampleRate;

        neg->blockSize = std::stoi(blockSizeFieldWidget->placeholder);
        neg->bufferSize = std::stoi(blockSizeFieldWidget->placeholder);
        neg->inputChannels = 2;
        neg->outputChannels = 2;


        INFO("sending negotionation info...");

        int err = send(client, buffer, sizeof(connectionNegotiation), 0);

        if (err == 0 || err == SOCKET_ERROR) {
            clientDisconnect();
        }

        INFO("SENT negotionation info...");

        //memset(buffer, 0, sizeof(connectionNegotiation));
        err = recv(client, buffer, sizeof(connectionNegotiation), MSG_WAITALL);
        if (err == 0 || err == SOCKET_ERROR) {
            clientDisconnect();
            return;
        }


        INFO("RECV negotionation info...");

        clientBlockSize = neg->blockSize;
        clientBufferSize = neg->bufferSize;
        clientInputChannels = neg->inputChannels;
        clientOutputChannels = neg->outputChannels;
        clientSampleRate = neg->sampleRate;

        moduleState = ServerState::CONNECTED;

//        //TODO...
//        if (negotiateCounter > (int)(args.sampleRate * 3)) {
//            negotiateCounter = 0;
//            moduleState = ServerState::CONNECTED;
//        } else {
//            negotiateCounter += 1;
//        }

    }

    void clientDisconnect() {
        closesocket(client);
        client = 0;
        moduleState = ServerState::LISTENING;
    }

    void acceptConnection() {
        acceptConnCounter += 1;
        if (acceptConnCounter > 1000) {
            acceptConnCounter = 0;
            client = accept(server, NULL, NULL);
            if (client == INVALID_SOCKET) {
                client = 0;
            } else {
                moduleState = ServerState::NEGOTIATING;
            }
        }
    }

    void clearBuffers() {
        inputBuffer.clear();
        outputBuffer.clear();
    }

    void resetLights() {
        for (int i = 0; i < ServerState::NUM_STATES; i++) {
            lights[i].setSmoothBrightness(0.0f, 1.5f);
        }
    }

    void process(const ProcessArgs &args) override {

        //TODO: Maybe don't reset the lights per frame?
        switch (moduleState) {
            case ServerState::CONNECTED:
                resetLights();
                lights[ServerState::CONNECTED].setSmoothBrightness(10.0f, .1f);
                bufferInputSamples(args);
                sendData();
                getData();
                break;
            case ServerState::NEGOTIATING:
                resetLights();
                lights[ServerState::NEGOTIATING].setSmoothBrightness(10.0f, .1f);
                clearBuffers();
                negotiateSettings(args);
                break;
            case ServerState::LISTENING:
                resetLights();
                lights[ServerState::LISTENING].setSmoothBrightness(10.0f, .1f);
                clearBuffers();
                acceptConnection();
                break;
            case ServerState::NOT_CONNECTED:
                resetLights();
                lights[ServerState::NOT_CONNECTED].setSmoothBrightness(10.0f, .1f);
                clearBuffers();
                tryToListen();
                break;
            case ServerState::ERROR_STATE:
                resetLights();
                lights[ServerState::ERROR_STATE].setSmoothBrightness(10.0f, .1f);
                if (!fatalError) {
                    //wait 5s
                    if (errorCounter > (int) (args.sampleRate * 5)) {
                        errorCounter = 0;
                        moduleState = ServerState::NOT_CONNECTED;
                    } else {
                        errorCounter += 1;
                    }
                }
                break;
            default:
                break;
        }
    }


};


struct RadioServerWidget : ModuleWidget {

    TextField* portField;
    TextField* blockSizeField;
    TextField* bufferSizeField;

    RadioServerWidget(RadioServer* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RServer.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild((Widget *)createLight<SmallLight<RedLight>>(mm2px(Vec(3.2, 70.9)), module, ServerState::ERROR_STATE));
        addChild((Widget *)createLight<SmallLight<YellowLight>>(mm2px(Vec(9.082, 70.9)), module, ServerState::NOT_CONNECTED));
        addChild((Widget *)createLight<SmallLight<BlueLight>>(mm2px(Vec(14.93, 70.9)), module, ServerState::LISTENING));
        addChild((Widget *)createLight<SmallLight<GreenLight>>(mm2px(Vec(20.779, 70.9)), module, ServerState::NEGOTIATING));
        addChild((Widget *)createLight<SmallLight<GreenLight>>(mm2px(Vec(26.705, 70.9)), module, ServerState::CONNECTED));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.862, 116.407)), module, RadioServer::OUT1_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(19.933, 116.407)), module, RadioServer::OUT2_OUTPUT));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.862, 93.0)), module, RadioServer::IN1_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(19.933, 93.0)), module, RadioServer::IN2_INPUT));

        portField = createWidget<TextField>(mm2px(Vec(2.8, 29)));
        portField->box.size = mm2px(Vec(24, 8));
        portField->placeholder = "5555";
        portField->multiline = false;
        if (module) module->portFieldWidget = portField;
        addChild(portField);

        blockSizeField = createWidget<TextField>(mm2px(Vec(2.8, 52)));
        blockSizeField->box.size = mm2px(Vec(24, 8));
        blockSizeField->placeholder = "256";
        blockSizeField->multiline = false;
        if (module) module->blockSizeFieldWidget = blockSizeField;
        addChild(blockSizeField);

        bufferSizeField = createWidget<TextField>(mm2px(Vec(2.8, 41)));
        bufferSizeField->box.size = mm2px(Vec(24, 8));
        bufferSizeField->placeholder = "4096";
        bufferSizeField->multiline = false;
        if (module) module->bufferSizeFieldWidget = bufferSizeField;
        addChild(bufferSizeField);
    }

    struct RadioTextItem : MenuItem {
        RadioServer* module;
        std::string value = "";
        void onAction(const event::Action& e) override {
        }
        void step() override {
            rightText = value;
            MenuItem::step();
        }
    };

    struct RadioIntItem : MenuItem {
        RadioServer* module;
        int value = 0;
        void onAction(const event::Action& e) override {
        }
        void step() override {
            rightText = std::to_string(value);
            MenuItem::step();
        }
    };

    void appendContextMenu(Menu* menu) override {
        RadioServer* module = dynamic_cast<RadioServer*>(this->module);
        assert(module);

//        menu->addChild(new MenuSeparator);
//        menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Connection Settings"));
//        menu->addChild(construct<RadioTextItem>(&MenuItem::text, "IP", &RadioTextItem::module, module, &RadioTextItem::value, "127.0.0.1"));
//        menu->addChild(construct<RadioIntItem>(&MenuItem::text, "Port", &RadioIntItem::module, module, &RadioIntItem::value, 5555));
//        menu->addChild(construct<RadioIntItem>(&MenuItem::text, "Buffersize", &RadioIntItem::module, module, &RadioIntItem::value, 512));
//        menu->addChild(construct<RadioIntItem>(&MenuItem::text, "Blocksize", &RadioIntItem::module, module, &RadioIntItem::value, 64));

    }

};



Model* modelRadioServer = createModel<RadioServer, RadioServerWidget>("RadioServer");

