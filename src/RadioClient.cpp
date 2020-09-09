//
// Created by bvanderlaan on 8/27/19.
//

#include "plugin.hpp"
#include "Client.h"

enum ClientState {
    NOT_CONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR_STATE,
    IN_BUFFER_OVERFLOW,
    IN_BUFFER_UNDERFLOW,
    OUT_BUFFER_OVERFLOW,
    OUT_BUFFER_UNDERFLOW,
    NUM_STATES
};


struct RadioClient : Module {

    dsp::SampleRateConverter<2> inputSrc;
    dsp::SampleRateConverter<2> outputSrc;

    std::string hostField = "127.0.0.1";
    std::string portField = "5555";

    ClientState moduleState = NOT_CONNECTED;
    int errorCounter = 0;

    TextField *hostFieldWidget;
    TextField *portFieldWidget;
    TextField *blockSizeFieldWidget;
    TextField *bufferSizeFieldWidget;

    Client client;

    enum ParamIds {
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

    RadioClient() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, ClientState::NUM_STATES);
        INFO("Init");
        client = Client(128);

        INFO("Initializing client...");
        client.init();
        moduleState = ClientState::ERROR_STATE;
        INFO("Set ERROR state");
    }


    ~RadioClient() {
        INFO("Shutting down client socket...");
        client.stop();
    }

    void reportBufferState() {
        if (client.in_buffer_overflow()) {
            lights[ClientState::IN_BUFFER_OVERFLOW].setBrightness(10.0f);
        } else {
            lights[ClientState::IN_BUFFER_OVERFLOW].setSmoothBrightness(0.0f, 1.0f);
        }
        if (client.in_buffer_underflow()) {
            lights[ClientState::IN_BUFFER_UNDERFLOW].setBrightness(10.0f);
        } else {
            lights[ClientState::IN_BUFFER_UNDERFLOW].setSmoothBrightness(0.0f, 1.0f);
        }
        if (client.out_buffer_overflow()) {
            lights[ClientState::OUT_BUFFER_OVERFLOW].setBrightness(10.0f);
        } else {
            lights[ClientState::OUT_BUFFER_OVERFLOW].setSmoothBrightness(0.0f, 1.0f);
        }
        if (client.out_buffer_underflow()) {
            lights[ClientState::OUT_BUFFER_UNDERFLOW].setBrightness(10.0f);
        } else {
            lights[ClientState::OUT_BUFFER_UNDERFLOW].setSmoothBrightness(0.0f, 1.0f);
        }
    }

    void bufferSamples(const ProcessArgs &args) {

        int channelCount = inputs[INPUT_JACK].getChannels();

        dsp::Frame<8, float> sample = client.getData();
        int outputChannelCount = client.getRemoteChannelCount();

        outputs[OUTPUT_JACK].setChannels(outputChannelCount);
        for (int i = 0; i < outputChannelCount; i++) {
            outputs[OUTPUT_JACK].setVoltage(sample.samples[i], 0);
        }

        sample = {};
        for (int i = 0; i < channelCount; i++) {
            sample.samples[i] = inputs[INPUT_JACK].getVoltage(i);
        }
        client.pushData(sample, channelCount);

    }

    void tryToConnect(const ProcessArgs &args) {
        ConnectionNegotiation settings = ConnectionNegotiation();
        settings.outputChannels = 2;
        settings.inputChannels = 2;
        if (blockSizeFieldWidget->text.length() > 0) {
            settings.blockSize = std::stoi(blockSizeFieldWidget->text);
        } else {
            settings.blockSize = std::stoi(blockSizeFieldWidget->placeholder);
        }
        settings.sampleRate = (int)args.sampleRate;
        settings.bufferSize = 4096;
        client.setConnectionSettings(settings);

        int port = 0;
        if (portFieldWidget->text.length() > 0) {
            port = std::stoi(portFieldWidget->text);
        } else {
            port = std::stoi(portFieldWidget->placeholder);
        }

        if (hostFieldWidget->text.length() > 0) {
            client.connectToHost(hostFieldWidget->text, port);
        } else {
            client.connectToHost(hostFieldWidget->placeholder, port);
        }
        INFO("STATE -> CONNECTING");
        moduleState = ClientState::CONNECTING;
    }

    void clearBuffers() {
        client.clearBuffers();
    }

    void resetLights() {
        for (int i = 0; i < ClientState::NUM_STATES; i++) {
            lights[i].setSmoothBrightness(0.0f, .8f);
        }
    }

    void process(const ProcessArgs &args) override {
        //TODO: Maybe don't reset the lights per frame?
        switch (moduleState) {
            case ClientState::CONNECTED:
                lights[ClientState::CONNECTED].setSmoothBrightness(10.0f, .5f);
                reportBufferState();
                bufferSamples(args);
                if (!client.isConnected() || client.inErrorState()) {
                    moduleState = ClientState::ERROR_STATE;
                }
                break;
            case ClientState::CONNECTING:
                resetLights();
                lights[ClientState::CONNECTING].setSmoothBrightness(10.0f, .5f);
                if (client.isConnected()) {
                    reportBufferState();
                    bufferSamples(args);
                    clearBuffers();
                    moduleState = ClientState::CONNECTED;
                    resetLights();
                }
                if (client.inErrorState()) {
                    moduleState = ClientState::ERROR_STATE;
                }

                break;
            case ClientState::NOT_CONNECTED:
                resetLights();
                lights[ClientState::NOT_CONNECTED].setSmoothBrightness(10.0f, .5f);
                INFO("Clear Buffers...");
                reportBufferState();
                bufferSamples(args);
                clearBuffers();
                INFO("Connect");
                tryToConnect(args);
                break;
            case ClientState::ERROR_STATE:
                resetLights();
                lights[ClientState::ERROR_STATE].setSmoothBrightness(10.0f, .5f);
                reportBufferState();
                //wait 5s
                if (errorCounter > (int) (args.sampleRate * 8)) {
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

        addChild((Widget *) createLight<SmallLight<RedLight>>(mm2px(Vec(3.2, 70.9)), module, ClientState::ERROR_STATE));
        addChild((Widget *) createLight<SmallLight<YellowLight>>(mm2px(Vec(9.082, 70.9)), module,
                                                                 ClientState::NOT_CONNECTED));
        addChild((Widget *) createLight<SmallLight<BlueLight>>(mm2px(Vec(14.93, 70.9)), module,
                                                               ClientState::CONNECTING));
        addChild((Widget *) createLight<SmallLight<GreenLight>>(mm2px(Vec(20.779, 70.9)), module,
                                                                ClientState::CONNECTED));


        addChild((Widget *)createLight<SmallLight<RedLight>>(mm2px(Vec(15.998, 87.881)), module, ClientState::IN_BUFFER_OVERFLOW));
        addChild((Widget *)createLight<SmallLight<RedLight>>(mm2px(Vec(15.998, 91.097)), module, ClientState::IN_BUFFER_UNDERFLOW));
        addChild((Widget *)createLight<SmallLight<RedLight>>(mm2px(Vec(15.998, 111.984)), module, ClientState::OUT_BUFFER_OVERFLOW));
        addChild((Widget *)createLight<SmallLight<RedLight>>(mm2px(Vec(15.998, 115.200)), module, ClientState::OUT_BUFFER_UNDERFLOW));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.862, 116.407)), module, RadioClient::OUTPUT_JACK));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.862, 93.0)), module, RadioClient::INPUT_JACK));

        hostField = createWidget<TextField>(mm2px(Vec(2.8, 18)));
        hostField->box.size = mm2px(Vec(24, 8));
        hostField->placeholder = "127.0.0.1";
        hostField->text = "127.0.0.1";
        hostField->multiline = false;
        if (module) module->hostFieldWidget = hostField;
        addChild(hostField);

        portField = createWidget<TextField>(mm2px(Vec(2.8, 29)));
        portField->box.size = mm2px(Vec(24, 8));
        portField->placeholder = "5555";
        portField->text = "5555";
        portField->multiline = false;
        if (module) module->portFieldWidget = portField;
        addChild(portField);

        blockSizeField = createWidget<TextField>(mm2px(Vec(2.8, 52)));
        blockSizeField->box.size = mm2px(Vec(24, 8));
        blockSizeField->placeholder = "256";
        blockSizeField->text = "256";
        blockSizeField->multiline = false;
        if (module) module->blockSizeFieldWidget = blockSizeField;
        addChild(blockSizeField);

        bufferSizeField = createWidget<TextField>(mm2px(Vec(2.8, 41)));
        bufferSizeField->box.size = mm2px(Vec(24, 8));
        bufferSizeField->placeholder = "4096";
        bufferSizeField->text = "4096";
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

