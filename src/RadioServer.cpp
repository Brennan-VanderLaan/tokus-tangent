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
        TUNE_INPUT,
        IN1_INPUT,
        IN2_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        OUT1_OUTPUT,
        OUT2_OUTPUT,
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

    void bufferInputSamples(const ProcessArgs &args) {
        float in_1 = inputs[IN1_INPUT].getVoltage();
        float in_2 = inputs[IN2_INPUT].getVoltage();

        dsp::Frame<2, float> sample = {};
        sample.samples[0] = in_1;
        sample.samples[1] = in_2;

        server.pushData(sample);
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
            server.startListen(port);
            moduleState = ServerState::LISTENING;
        }
    }

    void clearBuffers() {
        server.clearBuffers();
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
                break;
            case ServerState::NEGOTIATING:
                resetLights();
                lights[ServerState::NEGOTIATING].setSmoothBrightness(10.0f, .1f);
                clearBuffers();
                break;
            case ServerState::LISTENING:
                resetLights();
                lights[ServerState::LISTENING].setSmoothBrightness(10.0f, .1f);
                clearBuffers();
                if (server.inErrorState()) {
                    moduleState = ServerState::ERROR_STATE;
                }
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

    }

};



Model* modelRadioServer = createModel<RadioServer, RadioServerWidget>("RadioServer");

