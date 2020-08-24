//
// Created by bvanderlaan on 8/27/19.
//

#include "plugin.hpp"
#include <winsock2.h>
#include "network.h"

enum ClientState {
    NOT_CONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR_STATE,
    NUM_STATES
};


struct RadioClient : Module {
    WSADATA WSAData;
    SOCKET clientSocket;
    SOCKADDR_IN addr;
    dsp::RingBuffer<dsp::Frame<2>, 512> inputBuffer;
    dsp::RingBuffer<dsp::Frame<2>, 512> outputBuffer;

    dsp::SampleRateConverter<2> inputSrc;
    dsp::SampleRateConverter<2> outputSrc;

    std::string hostField = "127.0.0.1";
    std::string portField = "5555";

    ClientState moduleState = NOT_CONNECTED;
    int errorCounter = 0;
    int negotiateCounter = 0;

    TextField *hostFieldWidget;
    TextField *portFieldWidget;
    TextField *blockSizeFieldWidget;
    TextField *bufferSizeFieldWidget;

    int serverSampleRate = 0;
    int serverBufferSize = 0;
    int serverBlockSize = 0;
    int serverInputChannels = 0;
    int serverOutputChannels = 0;

    enum ParamIds {
        NUM_PARAMS
    };

    enum InputIds {
        IN1_INPUT,
        IN2_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        OUT1_OUTPUT,
        OUT2_OUTPUT,
        NUM_OUTPUTS
    };

    RadioClient() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, ClientState::NUM_STATES);

        INFO("Init");
        WSAStartup(MAKEWORD(2, 0), &WSAData);
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);

        u_long on = 1;
        int err = ioctlsocket(clientSocket, FIONBIO, &on);
        if (err < 0)
        {
            INFO("Couldn't set non-blocking socket option!");
        }


        moduleState = ClientState::ERROR_STATE;
        INFO("Set ERROR state");

    }


    ~RadioClient() {
        INFO("Shutting down client socket...");
        shutdown(clientSocket, SD_BOTH);
        closesocket(clientSocket);
    }


    void getData() {

    }

    void sendData() {

    }

    void bufferInputSamples(const ProcessArgs &args) {
        float in_1 = inputs[IN1_INPUT].getVoltage();
        float in_2 = inputs[IN2_INPUT].getVoltage();
    }

    void tryToConnect() {
        if (moduleState == ClientState::NOT_CONNECTED) {

            addr.sin_family = AF_INET;

            if (hostField.length() == 0) {
                addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            } else {
                addr.sin_addr.s_addr = inet_addr(hostField.c_str());
            }

            if (portField.length() == 0) {
                addr.sin_port = htons(5555);
            } else {
                addr.sin_port = htons(std::stoi(portField));
            }

            if (addr.sin_port < 1024 || addr.sin_port >= 65535) {
                moduleState = ClientState::ERROR_STATE;
                return;
            }

            int err = connect(clientSocket, (SOCKADDR *) &addr, sizeof(addr));
            if (err == 0) {
                INFO("CONNECTING");
                moduleState = ClientState::CONNECTING;
            } else {
                INFO("FAILED TO CONNECT: %ld\n", WSAGetLastError() );
                moduleState = ClientState::ERROR_STATE;
            }
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
        INFO("receiving negotionation info...");

        int err = recv(clientSocket, buffer, sizeof(connectionNegotiation), MSG_WAITALL);

        if (err == 0 || err == SOCKET_ERROR) {
            shutdown(clientSocket, SD_BOTH);
            moduleState = ClientState::ERROR_STATE;
            INFO("Failed to receive");
            return;
        }

        INFO("PAST RECV");

        neg = (connectionNegotiation *)buffer;

        serverBlockSize = neg->blockSize;
        serverBufferSize = neg->bufferSize;
        serverInputChannels = neg->inputChannels;
        serverOutputChannels = neg->outputChannels;
        serverSampleRate = neg->sampleRate;


        neg->sampleRate = args.sampleRate;
        neg->blockSize = std::stoi(blockSizeFieldWidget->text);
        neg->bufferSize = std::stoi(blockSizeFieldWidget->text);
        neg->inputChannels = 2;
        neg->outputChannels = 2;


        INFO("Received connection details...");

        //memset(buffer, 0, sizeof(connectionNegotiation));
        err = send(clientSocket, buffer, sizeof(connectionNegotiation), 0);
        if (err == 0 || err == SOCKET_ERROR) {
            shutdown(clientSocket, SD_BOTH);
            moduleState = ClientState::ERROR_STATE;
            return;
        }




        moduleState = ClientState::CONNECTED;

//        //TODO...
//        if (negotiateCounter > (int) (args.sampleRate * 3)) {
//            negotiateCounter = 0;
//            moduleState = ClientState::CONNECTED;
//        } else {
//            negotiateCounter += 1;
//        }

    }

    void clearBuffers() {
        inputBuffer.clear();
        outputBuffer.clear();
    }

    void resetLights() {
        for (int i = 0; i < ClientState::NUM_STATES; i++) {
            lights[i].setSmoothBrightness(0.0f, .4f);
        }
    }

    void process(const ProcessArgs &args) override {

        //TODO: Maybe don't reset the lights per frame?
        switch (moduleState) {
            case ClientState::CONNECTED:
                resetLights();
                lights[ClientState::CONNECTED].setSmoothBrightness(10.0f, .1f);
                bufferInputSamples(args);
                sendData();
                getData();
                break;
            case ClientState::CONNECTING:
                resetLights();
                lights[ClientState::CONNECTING].setSmoothBrightness(10.0f, .1f);
                negotiateSettings(args);
                bufferInputSamples(args);
                break;
            case ClientState::NOT_CONNECTED:
                resetLights();
                lights[ClientState::NOT_CONNECTED].setSmoothBrightness(10.0f, .1f);
                clearBuffers();
                tryToConnect();
                break;
            case ClientState::ERROR_STATE:
                resetLights();
                lights[ClientState::ERROR_STATE].setSmoothBrightness(10.0f, .1f);

                //wait 5s
                if (errorCounter > (int) (args.sampleRate * 5)) {
                    errorCounter = 0;
                    moduleState = ClientState::NOT_CONNECTED;
                } else {
                    errorCounter += 1;
                }
                break;
            default:
                break;
        }
    }


};


struct RadioClientWidget : ModuleWidget {

    TextField *hostField;
    TextField *portField;
    TextField *blockSizeField;
    TextField *bufferSizeField;

    RadioClientWidget(RadioClient *module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RClient.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));


//        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(12.7, 26.422)), module, RadioClient::STATION_PARAM));
//        addParam(createParam<RoundLargeBlackKnob>(mm2px(Vec(6.35, 74.80544)), module, RadioClient::LEVEL_PARAM));

        addChild((Widget *) createLight<SmallLight<RedLight>>(mm2px(Vec(3.2, 70.9)), module, ClientState::ERROR_STATE));
        addChild((Widget *) createLight<SmallLight<YellowLight>>(mm2px(Vec(9.082, 70.9)), module,
                                                                 ClientState::NOT_CONNECTED));
        addChild((Widget *) createLight<SmallLight<BlueLight>>(mm2px(Vec(14.93, 70.9)), module,
                                                               ClientState::CONNECTING));
        addChild((Widget *) createLight<SmallLight<GreenLight>>(mm2px(Vec(20.779, 70.9)), module,
                                                                ClientState::CONNECTED));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.862, 116.407)), module, RadioClient::OUT1_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(19.933, 116.407)), module, RadioClient::OUT2_OUTPUT));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.862, 93.0)), module, RadioClient::IN1_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(19.933, 93.0)), module, RadioClient::IN2_INPUT));

        hostField = createWidget<TextField>(mm2px(Vec(2.8, 18)));
        hostField->box.size = mm2px(Vec(24, 8));
        hostField->placeholder = "127.0.0.1";
        hostField->multiline = false;
        if (module) module->hostFieldWidget = hostField;
        addChild(hostField);

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
        RadioClient *module;
        std::string value = "";

        void onAction(const event::Action &e) override {
        }

        void step() override {
            rightText = value;
            MenuItem::step();
        }
    };

    struct RadioIntItem : MenuItem {
        RadioClient *module;
        int value = 0;

        void onAction(const event::Action &e) override {
        }

        void step() override {
            rightText = std::to_string(value);
            MenuItem::step();
        }
    };

    void appendContextMenu(Menu *menu) override {
        RadioClient *module = dynamic_cast<RadioClient *>(this->module);
        assert(module);

//        menu->addChild(new MenuSeparator);
//        menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Connection Settings"));
//        menu->addChild(construct<RadioTextItem>(&MenuItem::text, "IP", &RadioTextItem::module, module, &RadioTextItem::value, "127.0.0.1"));
//        menu->addChild(construct<RadioIntItem>(&MenuItem::text, "Port", &RadioIntItem::module, module, &RadioIntItem::value, 5555));
//        menu->addChild(construct<RadioIntItem>(&MenuItem::text, "Buffersize", &RadioIntItem::module, module, &RadioIntItem::value, 512));
//        menu->addChild(construct<RadioIntItem>(&MenuItem::text, "Blocksize", &RadioIntItem::module, module, &RadioIntItem::value, 64));

    }

};


Model *modelRadioClient = createModel<RadioClient, RadioClientWidget>("RadioClient");

