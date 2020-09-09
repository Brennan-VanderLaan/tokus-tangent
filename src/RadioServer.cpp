//
// Created by bvanderlaan on 8/19/2020.
//

#include "network.h"
#include "plugin.hpp"
#include "Server.h"
#include <thread>


enum ServerState {
    NOT_CONNECTED,
    LISTENING,
    CONNECTED,
    NEGOTIATING,
    ERROR_STATE,
    IN_BUFFER_OVERFLOW,
    IN_BUFFER_UNDERFLOW,
    OUT_BUFFER_OVERFLOW,
    OUT_BUFFER_UNDERFLOW,
    NUM_STATES
};


struct RadioServer : Module {

    SOCKADDR_IN addr;

    dsp::SampleRateConverter<2> inputSrc;
    dsp::SampleRateConverter<2> outputSrc;

    Server server;

    std::string portField = "5555";

    ServerState moduleState = NOT_CONNECTED;
    int errorCounter = 0;
    int disconnectCheckCounter = 0;

    bool fatalError = false;

    TextField* portFieldWidget;
    TextField* blockSizeFieldWidget;
    TextField* bufferSizeFieldWidget;

    enum ParamIds {
        STATION_PARAM,
        LEVEL_PARAM,
        NUM_PARAMS
    };

    enum InputIds {
        INPUT_JACK,
        NUM_INPUTS
    };
    enum OutputIds {
        OUTPUT_JACK,
        NUM_OUTPUTS
    };

    RadioServer() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, ServerState::NUM_STATES);

        INFO("Init");
        addr = {};
        server = Server();

        moduleState = ServerState::ERROR_STATE;
    }


    ~RadioServer() {
    }

    void reportBufferState() {
        if (server.in_buffer_overflow()) {
            lights[ServerState::IN_BUFFER_OVERFLOW].setBrightness(10.0f);
        } else {
            lights[ServerState::IN_BUFFER_OVERFLOW].setSmoothBrightness(0.0f, 1.0f);
        }

        if (server.in_buffer_underflow()) {
            lights[ServerState::IN_BUFFER_UNDERFLOW].setBrightness(10.0f);
        } else {
            lights[ServerState::IN_BUFFER_UNDERFLOW].setSmoothBrightness(0.0f, 1.0f);
        }

        if (server.out_buffer_overflow()) {
            lights[ServerState::OUT_BUFFER_OVERFLOW].setBrightness(10.0f);
        } else {
            lights[ServerState::OUT_BUFFER_OVERFLOW].setSmoothBrightness(0.0f, 1.0f);
        }

        if (server.out_buffer_underflow()) {
            lights[ServerState::OUT_BUFFER_UNDERFLOW].setBrightness(10.0f);
        } else {
            lights[ServerState::OUT_BUFFER_UNDERFLOW].setSmoothBrightness(0.0f, 1.0f);
        }
    }

    void bufferInputSamples(const ProcessArgs &args) {

        int channelCount = inputs[INPUT_JACK].getChannels();

        dsp::Frame<8, float> sample = server.getData();
        int outputChannelCount = server.getRemoteChannelCount();

        outputs[OUTPUT_JACK].setChannels(outputChannelCount);
        for (int i = 0; i < outputChannelCount; i++) {
            outputs[OUTPUT_JACK].setVoltage(sample.samples[i], i);
        }

        sample = {};
        for (int i = 0; i < channelCount; i++) {
            sample.samples[i] = inputs[INPUT_JACK].getVoltage(i);
        }
        server.pushData(sample, channelCount);

    }

    void tryToListen() {
        if (moduleState == ServerState::NOT_CONNECTED) {

            int port = 0;
            if (portFieldWidget->text.length() > 0) {
                port = std::stoi(portFieldWidget->text);
            } else {
                port = 5555;
            }
            INFO("Listening on port: %d", port);

            ConnectionNegotiation settings = ConnectionNegotiation();
            settings.outputChannels = 1;
            settings.inputChannels = 1;
            settings.sampleRate = 44100;
            settings.bufferSize = 4096;
            server.setConnectionSettings(settings);

            server.startListen(port);
            moduleState = ServerState::LISTENING;
        }
    }

    void clearBuffers() {
        server.clearBuffers();
    }

    void resetLights() {
        for (int i = 0; i < ServerState::NUM_STATES; i++) {
            lights[i].setBrightness(0.0f);
        }
    }

    void process(const ProcessArgs &args) override {

        //TODO: Maybe don't reset the lights per frame?
        switch (moduleState) {
            case ServerState::CONNECTED:
                lights[ServerState::CONNECTED].setSmoothBrightness(10.0f, .1f);
                reportBufferState();
                bufferInputSamples(args);
                if (!server.isConnected() || server.inErrorState()) {
                    server.stop();
                    moduleState = ServerState::ERROR_STATE;
                }

                break;
            case ServerState::NEGOTIATING:
                resetLights();
                lights[ServerState::NEGOTIATING].setSmoothBrightness(10.0f, .1f);
                reportBufferState();
                clearBuffers();
                if (server.isConnected()) {
                    moduleState = ServerState::CONNECTED;
                    resetLights();
                }
                break;
            case ServerState::LISTENING:
                resetLights();
                lights[ServerState::LISTENING].setSmoothBrightness(10.0f, .1f);
                reportBufferState();
                clearBuffers();
                if (server.isNegotiating() || server.isConnected()) {
                    moduleState = ServerState::NEGOTIATING;
                }

                if (server.inErrorState()) {
                    moduleState = ServerState::ERROR_STATE;
                }
                break;
            case ServerState::NOT_CONNECTED:
                resetLights();
                reportBufferState();
                lights[ServerState::NOT_CONNECTED].setSmoothBrightness(10.0f, .1f);
                clearBuffers();
                tryToListen();
                break;
            case ServerState::ERROR_STATE:
                resetLights();
                lights[ServerState::ERROR_STATE].setSmoothBrightness(10.0f, .1f);
                if (!fatalError) {
                    //wait 5s
                    if (errorCounter > (int) (args.sampleRate * 8)) {
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

        addChild((Widget *)createLight<SmallLight<RedLight>>(mm2px(Vec(15.999, 87.880)), module, ServerState::IN_BUFFER_OVERFLOW));
        addChild((Widget *)createLight<SmallLight<RedLight>>(mm2px(Vec(15.999, 91.096)), module, ServerState::IN_BUFFER_UNDERFLOW));
        addChild((Widget *)createLight<SmallLight<RedLight>>(mm2px(Vec(15.999, 111.937)), module, ServerState::OUT_BUFFER_OVERFLOW));
        addChild((Widget *)createLight<SmallLight<RedLight>>(mm2px(Vec(15.999, 115.153)), module, ServerState::OUT_BUFFER_UNDERFLOW));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.862, 116.407)), module, RadioServer::OUTPUT_JACK));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.862, 93.0)), module, RadioServer::INPUT_JACK));

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

    }

};



Model* modelRadioServer = createModel<RadioServer, RadioServerWidget>("RadioServer");

