//
// Created by bvanderlaan on 8/27/19.
//

#include "plugin.hpp"

struct RadioBroadcast : Module {


    enum ParamIds {
        STATION_PARAM,
        LEVEL_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        TUNE_INPUT,
        IN1_INPUT,
        IN2_INPUT,
        IN3_INPUT,
        IN4_INPUT,
        IN5_INPUT,
        IN6_INPUT,
        IN7_INPUT,
        IN8_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        OUT1_OUTPUT,
        OUT2_OUTPUT,
        NUM_OUTPUTS
    };



    RadioBroadcast() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(STATION_PARAM, 0.0, 1.0, 1.0, "Station Tune", "%", 0, 100);
        configParam(LEVEL_PARAM, 0.0, 1.0, 1.0, "Out level", "%", 0, 100);

        INFO("Init");

    }

    void processChannel(Input& in, Param& level, Input& lin, Input& exp, Output& out) {


    }


    void process(const ProcessArgs &args) override {



    }


};


struct RadioBroadcastWidget : ModuleWidget {
    RadioBroadcastWidget(RadioBroadcast* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RBroadcast.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(12.7, 26.422)), module, RadioBroadcast::STATION_PARAM));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.467, 41.04)), module, RadioBroadcast::IN1_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(16.933, 41.04)), module, RadioBroadcast::IN2_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.467, 49.8)), module, RadioBroadcast::IN3_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(16.933, 49.8)), module, RadioBroadcast::IN4_INPUT));
    }
};



Model* modelRadioBroadcast = createModel<RadioBroadcast, RadioBroadcastWidget>("RadioBroadcast");

