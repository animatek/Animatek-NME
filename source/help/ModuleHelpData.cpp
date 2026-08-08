//==============================================================================
// ModuleHelpData.cpp — Auto-generated. Do not edit manually.
//==============================================================================

#include "ModuleHelpData.h"

namespace NordHelp
{

const juce::Array<ModuleHelp>& getHelpDatabase()
{
    static juce::Array<ModuleHelp> db = []
    {
        juce::Array<ModuleHelp> d;
        ModuleHelp m;

        // ── 1 Output ──
        m = ModuleHelp();
        m.name = "1 Output";
        m.description = "This module takes one signal and routes it to one of the six mix busses.";
        m.params.add ({"Dest", "Select a destination mix bus with the buttons. Clicking the CVA L or R destination button will route the signal to the corresponding output of the Poly Area In module that can be used in the Common Voice Area of the patch."});
        m.params.add ({"M", "Click to mute the selected mix bus."});
        m.params.add ({"Mix", "The audio input."});
        m.params.add ({"Level", "The total audio level to the mix bus can be attenuated with the level knob."});
        d.add (m);

        // ── 1 to 2 Fader ──
        m = ModuleHelp();
        m.name = "1 to 2 Fader";
        m.description = "This is a fader with one input and two outputs, and a fader rotary knob to fade the input signal between the two outputs.";
        m.params.add ({"Fade", "With the Fade rotary knob you fade the input signal between the two outputs. In the 12 o'clock position both outputs are silent."});
        m.params.add ({"In", "This is the red audio input."});
        m.params.add ({"Out 1 and 2", "Signal: Bipolar"});
        d.add (m);

        // ── 1-4 switch ──
        m = ModuleHelp();
        m.name = "1-4 switch";
        m.description = "This module allows you to route an incoming signal to one of the four outputs.";
        m.params.add ({"Input", "The red audio input with attenuation control [Attenuator Type I]."});
        m.params.add ({"Output selector", "Routes the input to one of the four outputs."});
        m.params.add ({"M", "Mutes the outputs of the module."});
        m.params.add ({"Output", "Signal: Bipolar."});
        d.add (m);

        // ── 2 Outputs ──
        m = ModuleHelp();
        m.name = "2 Outputs";
        m.description = "This module takes two signals and routes them to two of the mix busses, grouped together as a stereo signal.";
        m.params.add ({"Destination", "Select which pair of mix buses to route the incoming signals to. Clicking the CVA destination button will route the signals to the corresponding outputs of the PolyAreaIn module that can be used in the Common Voice Area of the patch."});
        m.params.add ({"M", "Click to mute the selected mix bus pair."});
        m.params.add ({"Mix Bus L, R", "The audio inputs of the module."});
        m.params.add ({"Level", "The total audio level to the mix busses can be attenuated with the level knob."});
        d.add (m);

        // ── 2 to 1 Fader ──
        m = ModuleHelp();
        m.name = "2 to 1 Fader";
        m.description = "This is a fader with two inputs and one output, and a fader rotary knob to fade between the two input signals.";
        m.params.add ({"Fade", "With the Fade rotary knob you fade the between the two input signals. In the 12 o'clock position the output is silent."});
        m.params.add ({"In 1 and 2", "The two red audio inputs."});
        m.params.add ({"Out", "This is the red audio signal output. Signal: Bipolar"});
        d.add (m);

        // ── 3 inputs mixer ──
        m = ModuleHelp();
        m.name = "3 inputs mixer";
        m.description = "This mixer has three inputs and one output. Each input is equipped with an attenuation control.";
        m.params.add ({"Inputs", "Connect audio or control signals to these red inputs. Attenuate the signals with the corresponding knobs [Attenuator Type I]."});
        m.params.add ({"Output", "Signal: Bipolar."});
        d.add (m);

        // ── 4 Outputs ──
        m = ModuleHelp();
        m.name = "4 Outputs";
        m.description = "This module can route four signals to one mix bus each.";
        m.params.add ({"Mix bus", "The audio inputs to the respective mix bus and physical out jack of the synthesizer."});
        m.params.add ({"Level", "The total audio level to the mix busses can be attenuated with the level knob."});
        d.add (m);

        // ── 4-1 switch ──
        m = ModuleHelp();
        m.name = "4-1 switch";
        m.description = "This module allows you to route an incoming signal from one of the four inputs, to the output.";
        m.params.add ({"Inputs", "The red audio inputs. You can attenuate the incoming signal with the knobs next to the corresponding input [Attenuator Type I]."});
        m.params.add ({"Input selector", "Routes one of the inputs to the output."});
        m.params.add ({"M", "Mutes the output of the module."});
        m.params.add ({"Output", "Signal: Bipolar."});
        d.add (m);

        // ── 8 inputs mixer ──
        m = ModuleHelp();
        m.name = "8 inputs mixer";
        m.description = "This mixer has eight inputs and one output. Each input is equipped with a separate attenuation control.";
        m.params.add ({"Inputs", "Connect audio or control signals to these red inputs. Attenuate the signals with the corresponding knobs. The preset attenuation value is set to 100 to reduce the risk of distortion [Attenuator Type I]."});
        m.params.add ({"#-6 dB", "This function attenuates the output signal with -6 dB. If you experience distortion when mixing several signals, try activating this function."});
        m.params.add ({"Output", "The multi-color LED to the left of the output indicates the output level and have the following meaning: Green: normal signal level, Yellow: signal reaching headroom, Red: overload. Signal: Bipolar."});
        d.add (m);

        // ── AD-Envelope ──
        m = ModuleHelp();
        m.name = "AD-Envelope";
        m.description = "This is a smaller envelope with two stages, attack and decay. If the envelope finishes the attack stage while still receiving a high Gate signal, the envelope proceeds with the decay stage. If the logic Gate signal drops to zero during the attack stage, the envelope starts the decay stage with the set decay time.";
        m.params.add ({"Gate/Trig selector", "This determines whether the envelope should behave like a gated envelope or like an unconditional envelope. The gated (Gate mode) envelope needs a high logic signal for at least a short period of the attack stage. The unconditional (Trig mode) envelope only needs a short high logic signal to start. When the envelope has started after a Trig signal, it will proceed to the very end of the cycle even if the Trig signal drops to zero. Please note the arrow that appears next to the yellow input when Trig mode is selected."});
        m.params.add ({"Gate/Trig Input", "A high logic signal at this yellow input will start the envelope. The LED lights up while the envelope is receiving a high logic signal."});
        m.params.add ({"Amp Input", "A blue control signal input used for controlling the overall amplitude of the envelope."});
        m.params.add ({"Attack", "Sets the attack time. When the envelope receives a high logic signal at the Gate/Trig input, the output control signal from the envelope rises up to the maximum output, +64 units. The time to get from 0 to +64 units is the attack time. The attack is linear. If the logic Gate signal drops to zero before the envelope has completed the attack stage, it will immediately proceed with the decay stage. If the logic input is set to Trig, the entire attack phase will be completed. The attack time is displayed in milliseconds or seconds in the display box. Range: 0.5 ms to 45 s. Note: A very short attack time can produce a click in the beginning of the sound. This is only normal according to physics theory. To eliminate any click, just increase the attack time slightly."});
        m.params.add ({"Dcy", "Sets the decay time. After the envelope has completed the attack part, it will drop down to 0 units with the decay time. The decay is exponential. If the logic Gate/Trig signal drops to zero before the envelope has completed the decay stage, it will still continue through the whole decay stage. The decay time is displayed in milliseconds or seconds in the display box. Range: 0.5 ms to 45 s."});
        m.params.add ({"Graph", "The info graph shows an approximation of the envelope gain curve."});
        m.params.add ({"Input", "The red audio signal input. Here you patch a signal to the envelope controlled amplifter."});
        m.params.add ({"Env output", "The blue control signal output from the envelope generator. Signal: Unipolar."});
        m.params.add ({"Output", "The red output from the envelope controlled amplifter. Signal: Bipolar."});
        d.add (m);

        // ── ADSR-Envelope ──
        m = ModuleHelp();
        m.name = "ADSR-Envelope";
        m.description = "This is a four-step ADSR envelope (Attack, Decay, Sustain and Release). The envelope contains a gain control function, which is automatically modulated by the envelope. This enables you to control the amplitude of an external audio- or control signal.";
        m.params.add ({"Gate input", "A high logic signal appearing at this yellow input will start and can keep the envelope in an open-gate state. The LED lights up while the envelope is receiving a high logic signal."});
        m.params.add ({"Retrig input", "The envelope can be restarted by a high logic signal connected to the yellow Retrig input. The envelope Gate input must be receive a gate signal to make the envelope retrig."});
        m.params.add ({"Amp input", "A blue control signal input used for controlling the overall amplitude of the envelope."});
        m.params.add ({"Attack characteristics buttons", "Set the characteristics of the attack part of the envelope with one of the three buttons, logarithmic, linear or exponential. (This selector can not be assigned to a Morph group)."});
        m.params.add ({"Invert", "This inverts the control signal of the envelope."});
        m.params.add ({"A", "Sets the attack time. When the envelope receives a high logic signal at the Gate input, the output control signal from the envelope rises up to the maximum output, +64 units. The time to get from 0 to +64 units is the attack time. If the logic Gate signal drops to zero before the envelope has completed the attack stage, it will skip the decay and sustain stages and immediately proceed with the release stage. The attack time is displayed in milliseconds or seconds in the corresponding display box. Range: 0.5 ms to 45 s. Note: A very short attack time can produce a click in the beginning of the sound. This is only normal according to physics theory. To eliminate any click, just increase the attack time slightly."});
        m.params.add ({"D", "Sets the decay time. After the envelope has completed the attack part, it will drop down to the sustain level with the decay time. The decay is exponential. If the sustain level is 64, the decay stage will not be needed, there is simply nothing to decay down to. If the logic Gate signal drops to zero before the envelope has completed the decay stage, it will immediately proceed with the release stage. The decay time is displayed in milliseconds or seconds in the corresponding display box. Range: 0.5 ms to 45 s."});
        m.params.add ({"S", "Sets the sustain level. This level will be held (sustained) for as long as the logic Gate signal is high. When the logic Gate signal drops to zero, the envelope will proceed with the release stage. The sustain level is displayed in 'units' in the corresponding display box. Range: 0 to 64 units."});
        m.params.add ({"R", "Sets the release time. When the logic Gate signal drops to zero, the envelope will decrease from the sustain level to zero with the release time. The release is exponential. The release time is displayed in milliseconds or seconds in the corresponding display box. Range: 0.5 ms to 45 s."});
        m.params.add ({"Graph", "The info graph shows an approximation of the envelope gain curve. The yellow line represents the sustain level, which is not defined in time since it depends on the gate signal time."});
        m.params.add ({"Input", "The red audio signal input. Here you patch a signal to the envelope controlled amplifter."});
        m.params.add ({"Envelope output", "The blue control signal output from the envelope generator. Signal: Unipolar."});
        m.params.add ({"Output", "The red output from the envelope controlled amplifter. Signal: Bipolar."});
        d.add (m);

        // ── AHD-Envelope ──
        m = ModuleHelp();
        m.name = "AHD-Envelope";
        m.description = "The Attack-Hold-Decay envelope is an envelope with inputs to control attack, hold and release times.";
        m.params.add ({"Trig", "A logic control signal input that is used to start the envelope. Note that this is a Trig input and not a gate input. This means the envelope will always complete all the envelope stages. A green LED indicates when a trig signal is received"});
        m.params.add ({"Amp", "A blue control signal input used for controlling the overall amplitude of the envelope."});
        m.params.add ({"A, H, D", "a Sets the attack time. When the envelope receives a high logic signal at the Trig input, the output control signal from the envelope rises up to the maximum output, +64 units. The time to get from 0 to +64 units is the attack time. The attack is linear. The attack time is displayed in milliseconds or seconds in the corresponding display box. Range: 0.5 ms to 45 s. Note: A very short attack time can produce a click in the beginning of the sound. This is only normal according to physics theory. To eliminate any click, just increase the attack time slightly. h Sets the time the envelope should remain at +64 units. The hold time is displayed in milliseconds or seconds in the corresponding display box. Range: 0.5 ms to 45 s. d Sets the decay time. After the envelope has completed the hold stage, it will drop down to 0 units with the decay time. The decay is exponential. The decay time is displayed in milliseconds or seconds in the corresponding display box. Range: 0.5 ms to 45 s."});
        m.params.add ({"A, H, D control inputs", "Each of the attack, hold and decay times can be controlled externally by using the corresponding control signal input below the A, H and D knobs. You can attenuate the level of each control signal by turning the corresponding rotary knob. Note that the A, H and D control inputs handles bipolar control signals. Positive control signals on the A and D control inputs shortens the times and negative control signals increase the times. With the H parameter it is the other way around."});
        m.params.add ({"Graph", "The info graph shows an approximation of the envelope gain curve."});
        m.params.add ({"In", "The red audio signal input. Here you patch a signal to the envelope controlled amplifter."});
        m.params.add ({"Env Out", "The blue control signal output from the envelope generator. Signal: Unipolar."});
        m.params.add ({"Out", "The red output from the envelope controlled amplifter. Signal: Bipolar."});
        d.add (m);

        // ── Actions in the Editor ──
        m = ModuleHelp();
        m.name = "Actions in the Editor";
        m.description = "Patch window split bar Click-hold on the patch window split bar and drag up or down to resize the two patch sections, the Poly and Common Voice Areas. Click on the up arrow button to the left in the bar to show only the Common Voice Area, and on the down arrow button to show only the Poly Voice Area. Click on the double arrow button to place the split bar in the middle of the screen, showing both Voice Areas.";
        m.params.add ({"Patch window popup", "Right-clicking on the background of the patch window brings up a popup of the module groups including their modules. Select desired module by clicking on it from the list. The cursor gets a plus-sign next to it. Place the cursor where you want the module to be placed and click to drop the module. You can also select the slot to use for your patch by selecting it from the bottom of this popup."});
        m.params.add ({"Module popup", "Right-clicking on the gray background of a module brings up the module popup. Rename Allows you to rename the module."});
        d.add (m);

        // ── Help ──
        m = ModuleHelp();
        m.name = "Help";
        m.description = "Brings up the help-text for the module type. Delete Allows you to delete the module from a patch. All the cables that is connected to and from the module will be deleted as well. Any serial connections of cables will be rerouted.";
        m.params.add ({"Parameter popup", "Right-clicking on a module parameter brings up the parameter popup. Default value Resets the parameter to the fixed parameter default value. Zero morph Resets any morph range that you have set for the parameter. nob Allows you to assign one of the 18 knobs, the Control pedal input, aftertouch received via MIDI or the On/Off switch to the parameter. The On/Off switch requires that the sustain pedal input is assigned as an On/Off switch in the patch settings. Selecting Disable resets an assignment."});
        d.add (m);

        // ── Morph ──
        m = ModuleHelp();
        m.name = "Morph";
        m.description = "Allows you to assign the parameter to one of the four available morph groups. Selecting Disable resets an assignment. MIDI controller Allows you to assign one of the available MIDI controllers to the parameter. A MIDI controller will always affect the entire range of a parameter. Selecting Disable resets an assignment. Neither velocity nor pitch bend are MIDI Controllers. Velocity must be routed to a patch from the 'Vel' output on the 'eyboard Voice' module or from the 'Latest vel on' output of the 'eyboard Patch' module, with one exception, you can use velocity to affect the knobs of the Morph groups Pitch bend will be added to the Note signal value and the BT function on modules that have this activated. The range of the incoming pitch bend is determined by the bend range parameter. Help Brings up the help-text file for the selected parameter. Note that this is available only in the Windows version of the Editor.";
        d.add (m);

        // ── Cable popup ──
        m = ModuleHelp();
        m.name = "Cable popup";
        m.description = "Right-clicking on a cable connection brings out the cable popup. Highlight Highlights the cable(s) at the connector. All cables in a serial and/or branch connection will be highlighted. This makes it much easier to see an entire signal path in the patch. The highlighting is disabled as soon as you make any connection changes in the patch. Disconnect Deletes the connection. Any remaining cable chains will be rerouted. Break Breaks a serial connection between a selected input connector and the previous connector in the serial chain. The rest of the serial chain will remain unaffected, meaning that the first part of the chain will still work, and the last part will be connected but non-functional (input-to-input connection(s) only). If you choose to break a connection at an output, the connection(s) between the output and the first input of one or more serial chains will be removed. The rest of the chain(s) will remain connected but non-functional (input-to-input connection(s) only. Any non-functional input-to-input connections are indicated by white cable color. Color The six available cable colors are identified by their names. Audio cables are red Control cables are blue Logic cables are yellow Slave cables are gray User1 cables are green User2 cables are orange You can choose another color (name) for a cable in this popup. Changing cable type will not affect the functionality in any way, just the appearance. Cables in a serial cable chain will always have the same color. Cables in a branch connection may have different colors. It's possible to show and hide cables of different colors in the patch to make patching easier. Delete Deletes the entire serial cable chain that the connection is part of. If you want to delete a complete branch connection, this must be done from the cable origin of the branch.";
        m.params.add ({"Computer keyboard short-cuts", "Any commands that may be launched from the computer keyboard are shown next to the command/ function name in the menu dropdown lists. Selecting functions with the computer keys is a very powerful and fast method of using the Editor software."});
        m.params.add ({"The function keys", "You can get a read-out of all parameter settings, Morph range, Morph group assignment, nob and MIDI controller assignment in a patch by pressing the function keys F5 to F12. f5 If you press the F5 key, hintboxes showing every parameter value in the patch will be displayed. Parameters assigned to a Morph group will display their respective Morph range, starting with the initial parameter value. f7 If you press the F7 key, the Morph group assignments will be displayed in hintboxes. f8 If you press the F8 key, the knob assignments will be displayed in hintboxes. f9 If you press the F9 key, the MIDI Controller assignment will be displayed in hintboxes. f12 If you press the F12 key, the current MIDI Controller values of parameters assigned to MIDI Controllers will be displayed in hintboxes."});
        d.add (m);

        // ── Adding a module to a patch ──
        m = ModuleHelp();
        m.name = "Adding a module to a patch";
        m.description = "The modules are grouped together in module groups. You access these groups by clicking the tabs in the toolbar located above the patch window. The various modules in each group are distinguished by icons. Select a group tab, click-hold on a module icon and drag it to the patch window. When you place the cursor over any of the module buttons, a brief description of the module appears together with information of the amount of patch Load (Sound engine power) it will use (not in the Mac version). Drag the \"phantom frame\" of the selected module to the patch window. The other modules will move, if necessary, when you drop a new one. The modules will automatically snap to a grid in the patch window. The patch window will expand itself when needed and scroll-bars will appear at the bottom and to the right if the patch window becomes larger than the available screen space. As you add modules to the patch window, the patch Load indicators on the toolbar will expand, indicating the total use of the Sound engines. Maximum S patch Load is 100%. Another way of adding modules to the patch is by right[PC]/Ctrl[Mac]-clicking on the background of the patch window. A popup of the module groups appears. Select desired module by selecting it from the popup. The cursor gets a plus sign next to it. Place the cursor where you want the module to be placed and click to drop the module.";
        m.params.add ({"Renaming a module", "Double-clicking on the name of the module lets you rename the module. You can also right[PC]/ Ctrl[Mac]-click on the gray background of the module and select Rename."});
        m.params.add ({"Moving a module", "You can move the modules in the patch window by click-holding on its gray \"panel\" and move the frame that appears. Any connected cables will extend themselves and other modules will move out of the way automatically. You can also move several modules at the same time by placing the cursor on the patch window background and click-hold and mark the modules you want to move. Another way of selecting several modules is to Ctrl[PC]/Shift[Mac]-click on the desired modules. The names of the selected modules are highlighted to indicate that they have been selected."});
        m.params.add ({"Deleting a module from a patch", "To delete a module from a patch, either click on the module and press Delete on the computer keyboard or select Clear from the Edit menu. Alternatively, right[PC]/Ctrl[Mac]-click on a module's background and select Delete from the popup. Note that all cable connections made to the module will also be deleted. You can also delete several modules by selecting them as described in the example above. Then, either press the Delete key, select Clear from the Edit menu or right[PC]/Ctrl[Mac]-click on one of the selected modules and choose Delete from the popup."});
        d.add (m);

        // ── Adjustable gain control ──
        m = ModuleHelp();
        m.name = "Adjustable gain control";
        m.description = "The Adjustable gain control module is a signal attenuator.";
        m.params.add ({"Uni", "Select if the control signal should be Unipolar or Bipolar."});
        d.add (m);

        // ── Gain control ──
        m = ModuleHelp();
        m.name = "Gain control";
        m.description = "Set the gain control signal value with the rotary button. Range: -127 to +127 units or 0 to 127 units if Unipolar is selected. A setting of +127 leaves the input signal unaffected. A negative value in the Bipolar mode results in a 180 degree phase-shift of the output signal.";
        m.params.add ({"Display box", "Displays the gain value. Range: -127 to +127 units or 0 to 127 units if Unipolar is selected."});
        m.params.add ({"Input", "The red audio input."});
        m.params.add ({"Output", "Signal: Bipolar."});
        d.add (m);

        // ── Adjustable offset ──
        m = ModuleHelp();
        m.name = "Adjustable offset";
        m.description = "The Adjustable offset module can is used to add or subtract an offset (bias) to a signal.";
        m.params.add ({"Uni", "Select if the control signal should be Unipolar or Bipolar."});
        m.params.add ({"Offset control", "Set the offset value with the rotary button. Range: -64 to +64 units or 0 to +64 units if Unipolar is selected."});
        m.params.add ({"Display box", "Displays the offset value. Range: -64 to +64 units or 0 to +64 units if Unipolar is selected."});
        m.params.add ({"Input", "The red audio input."});
        m.params.add ({"Output", "Signal: Bipolar."});
        d.add (m);

        // ── Amplifier ──
        m = ModuleHelp();
        m.name = "Amplifier";
        m.description = "This module can amplify or attenuate a signal.";
        m.params.add ({"Amplification knob", "Select the desired amplification/attenuation with the slider. Any value above 1.0 amplifies the signal, any value below attenuates it. Range: 0.25 to 4.0 times the input level."});
        m.params.add ({"Display box", "Displays the attenuation or amplification. Range: x0.25 to x4.0"});
        m.params.add ({"Input", "The red audio input."});
        m.params.add ({"Output", "Signal: Bipolar."});
        d.add (m);

        // ── Audio in ──
        m = ModuleHelp();
        m.name = "Audio in";
        m.description = "This module routes the line level audio signals from the input l and input r inputs on the rear panel of Nord Modular to your patch. Two separate audio channels can be patched into the system. The input meters indicate the level of the incoming signals. The 0dB indication on the module indicates the headroom limit of the Nord Modular system. Signals above 0dB will cause the system to clip, causing distortion. It's important that you amplify any input signals to line level externally to get good sound quality. If you put in a too low signal and amplify it, using for example the Amplifter module, the sound quality won't be good. The reason for this is that the Amplifter module amplifies the signal digitally, and a low analog input signal will result in low resolution. A low resolution signal that is digitally amplified will sound distorted.";
        m.params.add ({"Outputs L/R", "Signal: Bipolar. Note: If you want to process a stereo signal, any processing modules (filters etc.) has to be duplicated in the patch."});
        d.add (m);

        // ── Audio modifier group ──
        m = ModuleHelp();
        m.name = "Audio modifier group";
        m.description = "These modifiers are useful tools for changing and transforming audio signals. Feel free to experiment with these on control signals as well.";
        d.add (m);

        // ── Audio signals, red connections ──
        m = ModuleHelp();
        m.name = "Audio signals, red connections";
        m.description = "Audio signals are bipolar as described above. The audio signals in Nord Modular are 24-bit at 96 kHz sampling frequency for extremely high quality, and they have highest priority in the Sound engines. Audio connectors are distinguished by the red color. Do not let the description \"audio\" stop you from experimenting with these signals. They can be used to modulate things too. You can, for example, patch the red audio output from an oscillator to a blue Pitch input of another oscillator.";
        m.params.add ({"Control signals, blue connections", "Control signals are sent from envelope generators, LFOs, the keyboard, sequencers etc. The control signals can be either uni- or bipolar. They are used to control or modulate parameters in a patch. The control signals are 24-bit at 24 kHz sampling frequency, i.e. a quarter of the audio signal bandwidth. This is because they are often low-frequency signals by nature, and do not require a high bandwidth. The control signal connectors are distinguished by the blue color."});
        m.params.add ({"Logic signals, yellow connections", "Logic signals are used to clock, trig or gate different functions. They have two possible levels, low (0 units) or high (+64 units). The logic signals use the same bandwidth and resolution in Nord Modular as the control signals. Logic signal connectors are distinguished by the yellow color. The state when a logic signal switches from 0 units to +64 units, is called the positive edge. When the logic signal change back to 0 units again, is called the negative edge. The logic inputs in the system can react to an incoming signal in four different ways. These are easily recognized by one of three symbols or the absence of a symbol, next to the input. 1. A logic input that responds to both edges of the logic signal has no symbol next to it. Please note that even though they respond to both edges, the response is not the same for the positive and the negative edge. An example of this is the Gate input on the ADSR envelope generator. This input \"starts\" the envelope when the positive edge of a logic signal appears and \"releases\" the envelope when the logic signal switches back to 0 units again. 2. A logic input that reacts only to the positive edge has an arrow, pointing upwards, next to it. This is a typical behaviour of a clock input on a sequencer module or a clock divider. This input is only interested in the positive edges of a logic signal. 3. There are some logic inputs that will react to the positive edge of a logic signal only if there is a clock signal coming in to the module as well. These inputs have an arrow, pointing upwards towards a horizontal marker, next to them. The Rst input on the various sequencer modules is an example of this. These modules will reset in sync with the next clock signal (on the Clk input) when they receive a positive edge at the Rst input. 4. The logic Clock input on the Clock divider module is an example of an input that reacts the same way to both of the edges of an incoming logic signal. This input has a double-sided arrow next to it. The different behaviors of the logic inputs are important to remember. It is possible to patch other signals than logic ones to the logic inputs. The output of a LFO, for instance, can be a good clock source or could be used to start envelopes. The logic input will not mind having a control or an audio signal connected to it. The logic inputs interpret any signal with a level of 0 units or less as a low signal and any signal with a level greater than 0 units as a high signal."});
        m.params.add ({"Slave signals, gray connections", "There are two types of oscillator and LFO modules in Nord Modular, masters and slaves. These two types of modules are equipped with gray connectors. A slave module must be connected to a master module to receive a frequency reference (the coarse pitch). In practice this means that the slave will follow the master as the frequency changes. A slave module can, however, act on its own without having to be connected to a master. In these cases the slave module transmits a steady frequency. This signal is not particularly suited to connect to anything else in Nord Modular except another gray connector. The outputs are labelled \"Slv\" and the inputs are labelled \"Mst\"."});
        m.params.add ({"Master and slave concept", "The concept of the master and slave modules is to help you reduce the load on the Sound engines. A slave module requires less Sound engine power, allowing for more modules in the patch, or more voices. They can also make your work a lot easier when building a multi-oscillator patch with one master oscillator and a couple of slaves. If you need to change the over-all tuning of the sound, this can be done for all the oscillators by just changing the master oscillator."});
        m.params.add ({"Bandwidth considerations", "The two different bandwidths of the signals in Nord Modular are important to keep in mind. You can patch a red audio output to a blue control input and vice versa but sometimes the results might be surprising. Some of the LFOs are capable of producing audible frequencies. Since the output of an LFO is a control signal, updated at the quarter-speed of an audio signal, the quality of an LFO generated signal might not be good enough to be used as an audio signal. If high audio quality is important in a patch, use oscillators as audio signal generators instead. The LFOs can, however, provide excellent signals to be used as a frequency modulators in an FM-type patch. Another example could be the Control Mixer, which could be used to mix audio signals, but with a lower sound quality."});
        m.params.add ({"Experiment", "You can always try to patch the three different types of signals to wherever you want. You may run into situations where the result of a connection will not be what you expected, but that is part of the beauty with a modular system like this. A blue control signal output can be very useful modulating on a yellow logic signal input, and an audio signal output can certainly produce interesting results connected to a control signal input. The colors are only there to help you identify the various signal types, not to restrict any experiments."});
        d.add (m);

        // ── Basic functions ──
        m = ModuleHelp();
        m.name = "Basic functions";
        m.description = "introduction to nord modular A modular synthesizer could be described as a flexible electronics kit. It contains a lot of parts, the modules, which have various functions. You can build your own, customized synthesizer by connecting different modules and functions with patch cables. A modular synthesizer has the advantage of being a very flexible instrument, leaving you in charge of the routing of the modules and functions included in the synth. Nord Modular takes this concept several steps further, being programmable, polyphonic, and multitimbral. Nord Modular also features a very powerful editing system in the supplied Editor software. modules A module in Nord Modular can be, for example, an oscillator, an envelope generator, a filter or a step sequencer. There are over 100 different types of modules available in Nord Modular, and the number is constantly increasing with software updates. You are not limited to use only one module of a specific type in a patch. Several identical modules can be used together, creating, for example, really fat multi-oscillator patches. connections Every module and nearly every function of a module can be patched to other modules and functions, using virtual cables. Each module has one or more connectors. These connectors come in two different shapes: circular inputs and square outputs, and four different variants: red audio-, blue control-, yellow logic- and gray slave-connectors. Most of the modules share the same basic layout, with the input connector(s) to the left and the output connector(s) to the right. parameters There are usually one or more parameters on each module. A parameter could be a knob, a slider or a selector switch (button). You change the setting of a parameter either with the mouse in the Editor software, with an assigned knob on the front panel or with the rotary dial (not Micro Modular). A knob parameter in the Editor is \"turned\" by click-holding it and moving the mouse. A selector switch is toggled by clicking on it. display boxes and graphs Some modules feature one or more display boxes that display alphanumeric and/or graphical information. Some oscillator modules, for example, display the frequency. The read-out of the oscillators is selectable between Hz and semitones, by clicking on the display window. Graphical information in modules can be envelope curves, wave shapes, frequency diagrams etc. leds Some modules have one or several LEDs to indicate functions. The rate of an LFO, the opening or closing of an envelope or the current step position in a sequencer module are some examples of where LEDs are used. the patch When you have connected a couple of modules together, you have created a patch. A patch can be saved on the computer and/or stored in the Nord Modular internal memory. A patch can produce one particular sound, or several sounds at once, depending on how many sound sources you use in the patch. A patch could be anything from a copy of an existing vintage synthesizer, to a completely unique synthesizer configuration of your own design.";
        m.params.add ({"$#", ""});
        d.add (m);

        // ── Clip ──
        m = ModuleHelp();
        m.name = "Clip";
        m.description = "This module can produce digital distortion by decreasing the clip level limit(s) below the normal headroom.";
        m.params.add ({"Sym", "Toggle switch for the symmetrical mode. If this is set to Off (not depressed), only the positive peaks of a signal will be clipped. If this is set to On (depressed), both the positive and the negative peaks of a signal will be clipped."});
        m.params.add ({"Modulation input [Type ITYPE1]", "Connect a modulator to this red input. The amount of modulation is attenuated with the knob."});
        m.params.add ({"Clip", "Sets the initial clip limit(s)."});
        m.params.add ({"Graph", "Displays the initial clip limit(s) graphically. The Y-axis represents the output signal values, and the X-axis the input signal values."});
        m.params.add ({"In", "The red audio input."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Clock divider ──
        m = ModuleHelp();
        m.name = "Clock divider";
        m.description = "The Clock Divider module can be used for dividing incoming clock pulses by a factor set by you. The module transmits a high logic signal after it has received a user-defined number of signals containing high/low transitions.";
        m.params.add ({"Clock input", "Connect an incoming signal to this yellow input."});
        m.params.add ({"Rst input", "Any high logic signal appearing at this yellow input will reset the counter in the Clock divider."});
        m.params.add ({"Divider disply box", "Displays the set denumerator. Range 1-128."});
        m.params.add ({"Divider", "Set the desired division with the buttons, 1-128. If you want to divide the MIDI clock (which transmits 24 pulses for each quarter note from the clock output on the MIDI Global module) to 16 pulses at a 4/ 4 time signature, use the division of 6. If you want to use 8 note triplets (12 pulses for one bar of 4/4), divide the clock by 8. Range: 1 to 128."});
        m.params.add ({"Output", "Signal: Logic."});
        d.add (m);

        // ── Clock divider fixed ──
        m = ModuleHelp();
        m.name = "Clock divider fixed";
        m.description = "Set the desired division with the buttons, 1-128. If you want to divide the MIDI clock (which transmits 24 pulses for each quarter note from the clock output on the MIDI Global module) to 16 pulses at a 4/ 4 time signature, use the division of 6. If you want to use 8 note triplets (12 pulses for one bar of 4/4), divide the clock by 8. Range: 1 to 128.";
        m.params.add ({"MIDI cl input", "The yellow clock input of the module."});
        m.params.add ({"Rst input", "Any high logic signal appearing at this input will reset the counter in the module."});
        m.params.add ({"8", "The output where 24 incoming pulses are divided to 2 pulses. Signal: Logic."});
        m.params.add ({"T8", "The output where 24 incoming pulses are divided to 3 pulses. Signal: Logic."});
        m.params.add ({"16", "The output where 24 incoming pulses are divided to 4 pulses. Signal: Logic."});
        d.add (m);

        // ── Clock generator ──
        m = ModuleHelp();
        m.name = "Clock generator";
        m.description = "The clock generator module generates a stream of logic signals. The module can also act as a master to slave LFOs. The Clock generator is not dependant on any MIDI clock signals, but acts on its own. If you want to sync to MIDI clock, you must instead use the Clock output of the 'MIDI Global' module.";
        m.params.add ({"Reset input", "The yellow Reset input forces the clock generator to restart each time it receives a signal that changes from 0 units or below to anything above 0 units. This signal could come from a gate output of a eyboard or Sequencer module, for example. When the clock generator is reset, it transmits a high logic signal on the Sync output."});
        m.params.add ({"Slv output", "A gray slave signal output for controlling the rate of slave LFOs. If the ratio of the slave LFO is set to 1:1, 1 BPM on the clock generator module corresponds to 1 Hz on the slave LFO. Patch this output to a Mst input on a slave LFO."});
        m.params.add ({"Rate display box", "Displays the rate in beats per minute, BPM. Range: 24 to 214 BPM."});
        m.params.add ({"Rate knob", "Set the desired rate, in beats per minute, with the knob. Range: 24 to 214 BPM."});
        m.params.add ({"On/Off", "Starts and stops the generation of clock pulses."});
        m.params.add ({"24 pulses/b", "This yellow output transmits 24 clock pulses for each quarter note. Signal: Logic."});
        m.params.add ({"4 pulses/b", "This yellow output transmits 4 clock pulses for each quarter note. Signal: Logic."});
        m.params.add ({"Sync output", "This yellow output transmits a high logic signal when the generator is reset or started. Signal: Logic."});
        d.add (m);

        // ── Clocked random step generator ──
        m = ModuleHelp();
        m.name = "Clocked random step generator";
        m.description = "The Clocked Random Step Generator module generates a random control signal. The module transmits a new random value for each logic signal received at the Clk input.";
        m.params.add ({"Mono", "Synchronizes modules in polyphonic patches to each other. This means that if you play a chord, the module will control all voices in sync. The preset setting of this parameter is Off."});
        m.params.add ({"Col", "This sets the \"color\" of the random control signal. The colored setting is more gentle and contains less radical differences between adjacent values. The \"white\" setting is completely random."});
        m.params.add ({"Clk input", "A logic signal or a periodic control signal from e.g. a clock generator or an LFO can be used to clock the output of the module."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Compare AB ──
        m = ModuleHelp();
        m.name = "Compare AB";
        m.description = "This module produces a high logic signal by comparing two control signals. If the value of a signal appearing at the A input equals, or is greater than the value of a signal appearing at the B input, the module produces a high logic signal. The logic signal will remain high for as long as the incoming control signals meet the condition.";
        m.params.add ({"In A, B", "The two blue control signal inputs."});
        m.params.add ({"A>=B", "The output. Signal: Logic."});
        d.add (m);

        // ── Compare to level ──
        m = ModuleHelp();
        m.name = "Compare to level";
        m.description = "This module produces a high logic signal by comparing a control signal level to a level limit set by you. If the value of a signal appearing at the input equals, or is greater than the value set in the window, the module produces a high logic signal. The logic signal will switch back to zero when the incoming signal drops to a level below the value set in the window.";
        m.params.add ({"Display box", "Displays the set level limit in units. Range: -64 to +64 units."});
        m.params.add ({"Level limit knob", "Set the level limit for the comparison with the knob. Range: -64 to +64 units."});
        m.params.add ({"A", "The blue control signal input. Patch the signal to be compared to the selected value."});
        m.params.add ({"Out", "Signal: Logic."});
        d.add (m);

        // ── Compressor ──
        m = ModuleHelp();
        m.name = "Compressor";
        m.description = "The Compressor module is a combined stereo compressor and limiter. The theory behind a compressor is to compress (decrease) the dynamic range of a signal with high dynamic range. The compressor decreases strong signals and increases weak signals according to the module settings. The practical result of a compressed signal is that the volume is more even.";
        m.params.add ({"Input L, R", "The red stereo audio inputs."});
        m.params.add ({"Side Chain, Act, Mon", "The Side Chain input is used for an external audio signal to control the compressor. The Side Chain signal will not be mixed with the other input signals, it will just be used to control the compressor. Activate the Side Chain function by pressing the Act button. If you want to listen to the Side Chain signal alone, press the Mon button."});
        m.params.add ({"Graph", "Shows the selected compression characteristics graphically. The Y-axis represents the output values and the X-axis the input values. The diagonal line represents 1:1 amplification. The cross hair indicates the set reference level (se explanation below)."});
        m.params.add ({"Gain Reduction", "This LED indicator shows the gain reduction of the sum of the left and right channels in dB."});
        m.params.add ({"Lim Active", "This LED is lit when the limiter is active."});
        m.params.add ({"Attack", "With the Attack rotary knob you set the response time of the compressor, i.e the time between input signal above the Threshold level and compressor activation. Range: Fast (0.5 ms) to 767 ms."});
        m.params.add ({"Release", "With the Release rotary knob you set the release time, i.e the time it takes for the compressor to return to the original input level. Range: 125 ms to 10.2 s. Thresh. With the Threshold rotary knob you set the threshold above which compression is activated, i.e the minimum input value to activate the compression. Range: -30 to 11 dB and Off."});
        m.params.add ({"Ratio", "With the Ratio rotary knob you set the compression ratio above the set Threshold level. 1.0:1 means no compression and 80:1 maximum compression. Range: 1.0:1 to 80:1."});
        m.params.add ({"Ref Lvl", "With the Ref Level rotary knob you set the compression reference level. This is the level that the signals will be compressed towards. The higher the Ref Level, the stronger the output signal(s). Range: -30 to 12 dB."});
        m.params.add ({"Limiter", "With the Limiter rotary knob you can set a maximum output level. If set lower than the Ref Level, it will decrease the output level to the Limiter value. Range: -30 to 11 dB and Off."});
        m.params.add ({"B", "Press this button to deactivate the compressor effect."});
        m.params.add ({"Out L, R", "Signal: Bipolar."});
        d.add (m);

        // ── Constant ──
        m = ModuleHelp();
        m.name = "Constant";
        m.description = "This module produces a control signal at a selectable value.";
        m.params.add ({"Uni switch", "Selects whether to send unipolar or bipolar signals. In bipolar mode (button not depressed) you can send from -64 to +64 units in increments of 1 unit. In unipolar mode (button depressed) you can send from 0 to +64 units in increments of 0.5 units."});
        m.params.add ({"Display box", "Displays set control signal value. Range: 0 to +64 units or -64 to +64 units."});
        m.params.add ({"Control signal knob", "Set the control signal value with the knob. Range: 0 to +64 units or -64 to +64 units."});
        m.params.add ({"Out", "Signal: Unipolar or Bipolar."});
        m.params.add ({"$Contents", "Basics Basic functionsBasic_functions SlotsSlots PatchesPatches Adding a module to a patchAdding_a_module_to_a_patch Patch connectionsPatch_connections Editing parameters in a patchEditing_parameters_in_a_patch Signals in the patchSignals_in_the_patch The signals, colored connectionsAudio_signals__red_connections ModulationModulation Front panel nobs and other controllersThe_front_panel_nobs_and_other_controllers MIDI controllersMIDI_controllers Morph groupsMorph_groups Voices, mono- and polyphonic patchesVoices__mono__and_polyphonic_patches The Mono parameterThe_Mono_parameter The BT parameterThe_BT_parameter Reference Panel referencePanel_reference nobs and buttons Synth settingsSynth_settings Patch settingsPatch_settings Editor referenceEditor_reference FileFile EditEdit PatchPatch SynthSynth SetupProperties ToolsTools WindowsWindows HelpHelp ToolbarToolbar Actions in the EditorActions_in_the_Editor"});
        d.add (m);

        // ── Module reference ──
        m = ModuleHelp();
        m.name = "Module reference";
        m.description = "MorphMorph In/Out groupIn_Out_group eyboard - voiceeyboard eyboard - patcheyboard_patch MIDI globalMIDI_global Audio inAudio_in Poly Area InPoly_Area_In";
        d.add (m);

        // ── Output modules ──
        m = ModuleHelp();
        m.name = "Output modules";
        m.description = "1 Output1_Output 2 Outputs2_Outputs 4 Outputs4_Outputs Note detectNote_detect eyb SpliteybSplit Oscillator groupOscillator_group Master OSCMasterOSC Oscillator AOscillator_A Oscillator BOscillator_B Oscillator COscillator_C Spectral OSCSpectralOSC Formant OSCFormant_OSC";
        d.add (m);

        // ── Slave oscillators ──
        m = ModuleHelp();
        m.name = "Slave oscillators";
        m.description = "Oscillator slave AOscillator_slave_A Oscillator slave BOscillator_slave_B Oscillator slave COscillator_slave_C Oscillator slave DOscillator_slave_D Oscillator slave EOscillator_slave_E Sine BankSine_Bank Oscillator slave FMOscillator_slave_FM NoiseNoise Perc oscillatorPerc_oscillator Drum synthDrum_synth LFO groupLFO_group LFOALFOA LFOBLFOB LFOCLFOC";
        d.add (m);

        // ── Slave LFOs ──
        m = ModuleHelp();
        m.name = "Slave LFOs";
        m.description = "LFO slave ALFO_slave_A LFO slave BLFO_slave_B LFO slave CLFO_slave_C LFO slave DLFO_slave_D LFO slave ELFO_slave_E Clock generatorClock_generator Clocked random step generatorClocked_random_step_generator Random step generatorRandom_step_generator Random generatorRandom_generator Random pulse generatorRandom_pulse_generator Pattern generatorPattern_generator Envelope groupEnvelope_group ADSR-EnvelopeADSR_Envelope AD-EnvelopeAD_Envelope MOD-EnvelopeMOD_Envelope AHD-EnvelopeAHD_Envelope Multistage envelopeMultistage_envelope Envelope followerEnvelope_follower Filter groupFilter_group FilterAFilterA FilterBFilterB FilterCFilterC FilterDFilterD FilterEFilterE FilterFFilterF Vocal FilterVocal_Filter VocoderVocoder Filter BankFilter_Bank EQ MidEQ_Mid Shelving EQShelving_EQ Mixer groupMixer_group 3 inputs mixer3_inputs_mixer 8 inputs mixer8_inputs_mixer Gain controlGain_control X-Fade with modulatorX_Fade_with_modulator PanPan 1 to 2 Fader1_to_2_Fader 2 to 1 Fader2_to_1_Fader Adjustable gain controlAdjustable_gain_control Adjustable offsetAdjustable_offset On/Off switchOn_Off_switch 4-1 switch4_1_switch 1-4 switch1_4_switch AmplifierAmplifier Audio modifier groupAudio_modifier_group ClipClip OverdriveOverdrive WavewrapperWavewrapper QuantizerQuantizer DelayDelay Sample and holdSample_and_hold Diode processingDiode_processing Stereo chorusStereo_chorus PhaserPhaser Inverter & level shifterInverter___level_shifter ShaperShaper CompressorCompressor ExpanderExpander Ring and Amp modulatorRing_and_Amp_modulator DigitizerDigitizer Control Modifier groupControl_Modifier_group ConstantConstant SmoothSmooth PortamentoAPortamentoA PortamentoBPortamentoB Note scalerNote_scaler Note quantizerNote_quantizer ey Quantizerey_Quantizer Partial generatorPartial_generator Control mixerControl_mixer Note and velocity scalerNote_and_velocity_scaler Logic groupLogic_group Positive edge delayPositive_edge_delay Negative edge delayNegative_edge_delay PulsePulse Logic delayLogic_delay Logic inverterLogic_inverter Logic processorLogic_processor Compare to levelCompare_to_level Compare ABCompare_AB Clock dividerClock_divider Clock divider fixedClock_divider_fixed";
        d.add (m);

        // ── Sequencer ──
        m = ModuleHelp();
        m.name = "Sequencer";
        m.description = "Event sequencerEvent_sequencer Control sequencerControl_sequencer Note sequencerASequencer Note sequencerBNote_sequencer";
        d.add (m);

        // ── Control Modifier group ──
        m = ModuleHelp();
        m.name = "Control Modifier group";
        m.description = "This group features a number of control signal generators and modifiers. You can also use these modules for other types of signals, audio signals for example. But note that the quality of audio signals will be degraded if you run them through a control signal module.";
        d.add (m);

        // ── Control mixer ──
        m = ModuleHelp();
        m.name = "Control mixer";
        m.description = "This is a mixer for control signals. You can select between linear and exponential attenuator characteristics to better suit your modulation needs. It can also invert the polarity of an incoming signal.";
        m.params.add ({"Lin switch", "Switch between linear [Type I] and exponential [Type II] characteristics for the two attenuators."});
        m.params.add ({"Invert switches", "Inverts the polarity of the incoming control signal."});
        m.params.add ({"Inputs and knobs", "Patch signals to these two blue inputs. You can attenuate the signal with the corresponding knob."});
        m.params.add ({"Output", "Signal: Bipolar."});
        d.add (m);

        // ── Control sequencer ──
        m = ModuleHelp();
        m.name = "Control sequencer";
        m.description = "This is a Control Sequencer which sends one control signal value for each step.";
        m.params.add ({"Clk input", "This is the yellow input for the clock pulses. These pulses will advance the sequencer one step for each pulse."});
        m.params.add ({"Rst input", "This is a yellow input where a high logic signal will reset the sequencer (force it to step 1 again). The restart isn't performed until the next the clock pulse is received at the Clk input. This guarantees perfect timing."});
        m.params.add ({"Snc output", "This yellow output transmits a high logic signal whenever a sequence starts from step 1. Signal: Logic."});
        m.params.add ({"Loop button", "If the loop mode is on, the Control sequencer will automatically restart from step 1 after the last step. If the loop mode is off, the sequencer will stop at the last step."});
        m.params.add ({"Step", "This sets the last step in the sequence. The sequencer will return to step 1 if loop mode is on, or stop if loop mode is off. Set the last step with the buttons. Range: 1 to 128 steps."});
        m.params.add ({"Clr button", "Pressing this button will reset all control values to 0."});
        m.params.add ({"Rnd button", "This produces a random set of control signal values for each of the 16 steps."});
        m.params.add ({"Sliders", "You set the control signal level of each step by moving the vertical slider or clicking the arrow buttons that appear below each slider when you move the cursor over it. Note that when you click-hold to move the slider, the cursor becomes invisible. Range: +/- 64 in bipolar mode and 0-64 in unipolar mode (see below)."});
        m.params.add ({"Link output", "This yellow output transmits a high logic signal whenever the Control sequencer goes beyond step number 16. This is used for linking several Control sequencers in series. See more about linking at the end of this chapter. Signal: Logic."});
        m.params.add ({"Uni button", "Selects uni- or bipolar control signals of the output of the Control sequencer."});
        m.params.add ({"Out", "Signal: Unipolar or Bipolar."});
        d.add (m);

        // ── Delay ──
        m = ModuleHelp();
        m.name = "Delay";
        m.description = "The Delay module can delay an audio signal. The delay time can be modulated from an external source. This allows you to do e.g. flanger and phaser effects.";
        m.params.add ({"Modulation input [Type ITYPE1]", "You can modulate the delay time with a modulation source connect to this blue input. The amount of modulation is attenuated with the knob."});
        m.params.add ({"Display box", "Displays the initially set delay time. Range: 0 to 2.65 ms."});
        m.params.add ({"Time knob", "Set the delay time with the slider or by clicking on the arrow buttons on either side. Range: 0 to 2.65 ms."});
        m.params.add ({"In", "The red audio input."});
        m.params.add ({"2.65 ms output", "The fixed delay-time output of the Delay module with the maximum possible delay time, 2.65 ms. Signal: Bipolar."});
        m.params.add ({"Output", "The variable delay time output. Signal: Bipolar."});
        d.add (m);

        // ── Digitizer ──
        m = ModuleHelp();
        m.name = "Digitizer";
        m.description = "The Digitizer module continuously samples an incoming signal at a selectable sample rate and bit resolution. The module can e.g. sample a clean audio signal and transform it down to a dirty 8 bit, 5 kHz signal. Great for \"low-fi\" effects with lots of aliasing.";
        m.params.add ({"#bits off", "Clicking this button leaves the signal quantization unmodified. bits display box Displays the bit resolution. Range: 1 to 12 bits."});
        m.params.add ({"#bits", "Select the bit resolution with the arrow buttons. Range: 1 to 12 bits. sample display box Displays selected initial sample rate in Hz. Range: 32.70 Hz to 50.18 kHz."});
        m.params.add ({"#sample off", "Clicking this button disables the sample rate conversion and leaves the signal at full audio bandwidth."});
        m.params.add ({"#sample rate", "Set desired sample rate in Hz. Range: 32.70 Hz to 50.18 kHz. in The red audio input. out Signal: Bipolar."});
        d.add (m);

        // ── Diode processing ──
        m = ModuleHelp();
        m.name = "Diode processing";
        m.description = "The Diode Processing module can change a bipolar signal to a unipolar signal. You can choose whether you want to discard of any negative input levels, or if you want to transform these to positive levels.";
        m.params.add ({"Selector", "Sets the operation of the module. The leftmost position leaves the signal unchanged (bypass), the middle (Half) position discards of any negative input signal levels and the right (Full) position transforms (mirrors) any negative signal levels to positive levels."});
        m.params.add ({"In", "The red audio input."});
        m.params.add ({"Out", "Signal: Bipolar or Unipolar."});
        d.add (m);

        // ── Drum synth ──
        m = ModuleHelp();
        m.name = "Drum synth";
        m.description = "The Drum synth module is designed to generate classic analog drum sounds. It consists of a master and a slave oscillator in combination with a noise source and a multimode noise filter. The global parameters include a bend function and a click and noise mixer.";
        m.params.add ({"Trig", "The yellow Trig input trigs the Drum synth module each time it receives a signal that changes from 0 units or below to anything above 0 units. This signal could come from a gate output of a eyboard or Sequencer module, for example. A green LED indicates when a trig signal is received."});
        m.params.add ({"Vel modulation input", "This blue control input is used to receive velocity information from an external source. The input velocity signal will affect Master and Slave Oscillator Level, Noise Filter Sweep, Bend Amount, Click Level and Noise Level. Maximum input velocity will force the parameters to reach their current settings."});
        m.params.add ({"Pitch modulation input", "This blue control input is used to receive pitch data from an external module such as a eyboard or Sequencer module, for example."});
        m.params.add ({"Master and Slave display boxes", "The Master display box shows the master pitch in Hz, and the Slave display box the pitch ratio related to the master pitch. Range: Master: 20.0 Hz to 784 Hz. Slave: 1:1 to 6.26."});
        m.params.add ({"Master and Slave", "These are the parameters of the two oscillators that generate the basic drum waveform. Tune: The tune of the Master can be set between 20.0 and 784 Hz. The Slave ranges from 1 to 6.26 times the Master frequency. Dcy: Decay determines the decay time for each oscillator. Range: 0.5 ms to 45 s. Lvl: With the Level knobs you set the respective volume of the two oscillators."});
        m.params.add ({"Noise Filter", "Here you can filter and affect the noise component of the Drum synth module. Freq: With the Freq knob you set the cutoff frequency of the noise. Range: 10 Hz to 15.8 kHz. Res: With the Res knob you set the resonance amount around the cutoff frequency. Swp: With the Sweep knob you set a sweep range for the cutoff frequency. The setting results in a sweep from a high cutoff frequency down to the frequency you set with the Freq knob. Range: 0 to 5 octaves. Dcy: The Decay knob sets the noise sweep and decay time. Range: 0.5 ms to 45 s. HP/BP/LP: Click on the HP, BP or LP button to select filter mode: highpass, bandpass or lowpass."});
        m.params.add ({"Bend", "Bend is a global function for the Master and Slave oscillators. Amt: With the Amt knob you set the bend amount, i.e. the frequency range to bend through. The bending always start from the higher frequency and sweeps down in frequency. Range: 0 to 5 octaves. Dcy: With the Dcy knob you set the bend decay time. The bend time can be considered more as a bend rate, since the actual decay time is determined by the Decay knobs of the two oscillators. Range: 0.5 to 45 s."});
        m.params.add ({"Click and Noise", "With the Click knob you can add a clicking sound to the attack of the sound and with the Noise knob you set the noise level in the total mix."});
        m.params.add ({"Preset", "Here you can choose between a number of factory presets by clicking on the up or down buttons. The preset name is shown in the display box."});
        m.params.add ({"M", "Click on this button to mute the audio output of the Drum synth module."});
        m.params.add ({"Out", "Signal: Bipolar"});
        d.add (m);

        // ── EQ Mid ──
        m = ModuleHelp();
        m.name = "EQ Mid";
        m.description = "EqMid offers parametric equalization with center frequency, gain and bandwidth controls.";
        m.params.add ({"Frequency knob", "With the big rotary knob you change the center frequency. Range: 20 Hz to 16 kHz."});
        m.params.add ({"Freq display box", "Displays the center frequency in Hz. Range: 20 Hz to 16 kHz."});
        m.params.add ({"Gain display box", "Displays the gain in dB. Range -18 to 18 dB."});
        m.params.add ({"Gain knob", "With the Gain rotary knob you change the gain at the center frequency. Range -18 to 18 dB."});
        m.params.add ({"BW display box", "Displays the bandwidth in octaves. Range 2 to 0.02 octaves."});
        m.params.add ({"BW knob", "Use the BW rotary knob to set the bandwidth around the center frequency. Range 2 to 0.02 octaves."});
        m.params.add ({"Graph", "Displays a schematic graph of the equalization characteristics. The Y-axis represents level and the X-axis the frequency. The horizontal line in the middle represents the 0 dB level."});
        m.params.add ({"In", "This is the red audio input of the equalizer. With the rotary knob to the left of the input you can attenuate the input signal [Attenuator Type I]."});
        m.params.add ({"B", "Press the B button to bypass the equalization and leave the signal unaffected."});
        m.params.add ({"Out", "The multi-color LED above the output indicates the output level and have the following meaning: Green: normal signal level, Yellow: signal reaching headroom, Red: overload. Signal: Bipolar"});
        m.params.add ({"$Edit", ""});
        m.params.add ({"Undo", "Click to undo your latest operations/commands."});
        m.params.add ({"Redo", "Click to step back through the latest Undo operations. Works like a reversed 'Undo' function."});
        m.params.add ({"Cut", "Cuts out one or several modules, including their common cable connections and parameter settings, and places in the clipboard memory."});
        m.params.add ({"Copy", "Copies one or several modules, including their common cable connections and parameter settings, and places in the clipboard memory."});
        m.params.add ({"Paste", "Pastes one or several modules, including their common cable connections and parameter settings, that previously have been cut or copied to the clipboard memory. The Paste command results in a cursor with a small '+' sign attached to it. Place the cursor where you want in the patch window and click to paste the module(s)."});
        m.params.add ({"Clear", "Deletes one or several selected modules (and their common cable connections) from the patch window."});
        d.add (m);

        // ── Editing parameters in a patch ──
        m = ModuleHelp();
        m.name = "Editing parameters in a patch";
        m.description = "focus A parameter can be a knob or a selector switch (button). Put a parameter \"in focus\" by clicking on it. An increment and decrement button appears below the knob or slider parameter as you move the cursor over it, and the current setting of the parameter displays briefly in a yellow hintbox. When you click on the parameter, the increment/decrement buttons (or button selectors) are highlighted. Note that to be able to put a parameter in focus, the patch has to be active in the synthesizer. If Nord Modular (not Micro Modular) is in Edit mode (by pressing the edit button), the parameter will also be active in the Nord Modular display, and you can adjust it with the rotary dial. You move the focus with the left and right navigator buttons, or with the left and right arrow buttons on the computer keyboard, or by clicking with the mouse. The left and right navigator and arrow buttons will only scroll through the parameters of one module. To move the focus to another module in the patch, press and hold shift on Nord Modular, then jump between the modules with the up/down/left/right navigator buttons. The Ctrl[PC]/Alt[Mac] key on the computer keyboard together with the arrow buttons have the same function. Note that you have to use the up/down arrow or up/down navigator buttons to jump between modules that are placed above/below each other in the patch window. editing You can edit the parameters with the mouse. Place the cursor over a knob, click-hold it (put it in focus) and then move the mouse. The knobs have no end stops; you may jump from maximum to minimum by turning past the 6 o'clock position. When a knob is in focus, two small buttons will appear beneath the knob. Clicking on the 'up' button will increase the value for each click and clicking the 'down' button will decrease. Click-hold any of these buttons for auto-increment/decrement. Click on a selector switch to select e.g. a waveform of an oscillator. The selected button will be \"depressed\". If Nord Modular (not Micro Modular) is in Edit mode (by pressing the edit button), the highlighted parameter will also be active in the Nord Modular display, and you can edit it with the rotary dial";
        d.add (m);

        // ── Editor reference ──
        m = ModuleHelp();
        m.name = "Editor reference";
        m.description = "In the PC version of the Editor, the usual Windows98/NT keyboard commands are available. The drop-down menus can be accessed by pressing the Alt key and the underlined letter in the menu bar. The functions in the drop-down menus can then be accessed by pressing the key corresponding to the underlined letter in the drop-down menus. Most of the commands can also be accessed by pressing the Ctrl[PC]/Cmd[Mac] key together with the letter shown next to the command name in the drop-down menus.";
        d.add (m);

        // ── Envelope follower ──
        m = ModuleHelp();
        m.name = "Envelope follower";
        m.description = "This module will extract an envelope from a signal, i.e follow the amplitude envelope of an incoming signal. When a signal at the input of this module increases in amplitude, this module \"follows\" the amplitude with the time set as Attack time. When a signal decreases, it \"follows\" the amplitude with the time set as Release time.";
        m.params.add ({"Atk", "Sets the time it should take for the envelope follower to track increasing amplitude of the input signal. Range: Fast(0.5 ms) to 767 ms. The set value is shown in the corresponding display box."});
        m.params.add ({"Rel", "Sets the time it should take for the envelope follower to track decreasing amplitude of the input signal. Range: 40 ms to 3.26 s. The set value is shown in the corresponding display box."});
        m.params.add ({"Input", "The red audio signal input. Patch this input to an output of the Audio In module, for example, to track the amplitude of an external audio signal."});
        m.params.add ({"Output", "The output of the generated envelope signal. Signal: Unipolar."});
        d.add (m);

        // ── Envelope group ──
        m = ModuleHelp();
        m.name = "Envelope group";
        m.description = "An envelope generator affects a signal level over time. The envelope starts when it receives a trig or gate signal and it closes when the trig/gate signal switches back to zero. During the active stages, the envelopes can be retriggered. You have control over certain time-dependent parameters and levels (ADSR and Multi-envelope only) in the Nord Modular envelopes. The output signal from an envelope is usually unipolar, from 0 to + 64 units (or if inverted, from +64 to 0 units), but can also be bipolar. If the logic gate signal at the Gate input on an ADSR envelope generator switches to zero before the envelope has completed one or more of the stages, the envelope will jump directly to the release stage. If an envelope is restarted before all the stages were completed, it will restart the attack from the current envelope level.";
        d.add (m);

        // ── Event sequencer ──
        m = ModuleHelp();
        m.name = "Event sequencer";
        m.description = "This is a trigger sequencer. Each step can send two separate logic pulses on the two separate outputs. Activate a step by clicking on one or more of the 32 available trigger buttons.";
        m.params.add ({"Clk input", "This is the yellow input for the clock pulses. These pulses will advance the sequencer one step for each pulse."});
        m.params.add ({"Rst input", "This is a yellow input where a high logic signal will reset the sequencer (force it to step 1 again). The restart isn't performed until the next the clock pulse is received at the Clk input. This guarantees perfect timing."});
        m.params.add ({"Snc output", "This yellow output transmits a high logic signal whenever a sequence starts from step 1. Signal: Logic."});
        m.params.add ({"Clr button", "Pressing this button will reset every trigger button in the two rows."});
        m.params.add ({"Loop button", "If the loop mode is on, the Event sequencer will automatically restart from step 1 after the last step. If the loop mode is off, the sequencer will stop at the last step."});
        m.params.add ({"Step", "Set the last step in the sequence. The sequencer will return to step 1 if loop mode is on, or stop if loop mode is off. Set the last step with the buttons. Range: 1 to 128 steps."});
        m.params.add ({"Link output", "This yellow output transmits a high logic signal whenever the Event sequencer goes beyond step number 16. This is used for linking several Event sequencers in series. See more about linking at the end of this chapter. Signal: Logic."});
        m.params.add ({"Trigger buttons", "Click on the buttons to make the sequencer send a pulse each time it passes the step. Note that the two Trigger Button rows work parallel on the two different outputs."});
        m.params.add ({"G buttons", "Toggle between the trigger and the gate mode with these buttons. In the trigger mode, every step transmits its own logic signal, at a 50% duration cycle. In the gate mode, two or more adjacent activated steps will mix into a \"longer\" logic signal."});
        m.params.add ({"Out", "Signal: Logic."});
        d.add (m);

        // ── Expander ──
        m = ModuleHelp();
        m.name = "Expander";
        m.description = "The Expander module is a combined stereo expander and gate. The theory behind an expander is to increase the dynamic range of a signal with low dynamic range. The expander does not affect strong signals but decreases weak signals according to the settings. Expanders are often used for noise reduction.";
        m.params.add ({"In L, R", "The red stereo audio inputs."});
        m.params.add ({"Side Chain, Act, Mon", "The Side Chain input is used for an external audio signal to control the expander. The Side Chain signal will not be mixed with the other input signals, it will just be used to control the expander. Activate the Side Chain function by pressing the Act button. If you want to listen to the Side Chain signal, press the Mon button."});
        m.params.add ({"Graph", "Shows the selected expansion characteristics graphically. The Y-axis represents the output values and the X-axis the input values. The broken diagonal line represents 1:1 amplification."});
        m.params.add ({"Gain Reduction indicator", "This LED chain shows the gain reduction of the sum of the left and right channels in dB."});
        m.params.add ({"Gate Active", "This LED is lit when the gate is active."});
        m.params.add ({"Attack", "With the Attack rotary knob you set the response time of the expander, i.e the time between input signal below the Threshold level and expander activation. Range: Fast (0.5 ms) to 767 ms."});
        m.params.add ({"Release", "With the Release rotary knob you set the release time, i.e the time it takes for the expander to return to the original input level. Range: 125 ms to 10.2 s. Thresh. With the Threshold rotary knob you set the threshold below which expansion is activated, i.e the maximum input value to activate the expansion. Range: Off and -83 to 0 dB."});
        m.params.add ({"Ratio", "With the Ratio rotary knob you set the expansion ratio below the set Threshold level. 1:1.0 means no expansion and 1:80 maximum expansion. Range: 1:1.0 to 1:80."});
        m.params.add ({"Gate", "With the Gate rotary knob you set the threshold below which gating is activated, i.e the maximum input value to activate gating. Range: Off and -83 to -12 dB"});
        m.params.add ({"Hold", "With the Hold rotary knob you set the gate hold time. This is used to avoid that fluctuating signals open and close the gate too often. Range: Off and 4 to 508 ms."});
        m.params.add ({"B", "Press this button to deactivate the expander effect."});
        m.params.add ({"Out L, R", "Signal: Bipolar."});
        d.add (m);

        // ── File ──
        m = ModuleHelp();
        m.name = "File";
        m.description = "New... Creates a new, empty patch window. In the dialog box that appears, you can select to either load the patch to a slot of Nord Modular, or choose Local to create a patch that will only be active in the Editor. You can later download an inactive patch to the synth. You can also upload the patch that is currently in the selected slot by clicking on the Upload button.";
        m.params.add ({"Open", "Brings up the file selector and allows you to open a patch file from disk. Select a file and click Open to download the patch to a slot in Nord Modular. You can also choose to refrain from downloading the patch to the synth by selecting Local."});
        m.params.add ({"Close", "Closes the current patch window. If the patch has been edited, you will be asked if you want to save it before closing. Closing a patch in the Editor will not remove patches that have been downloaded to a slot in Nord Modular."});
        m.params.add ({"Close All", "Closes all the patch windows. If any of them has been edited, you will be asked if you want to save them before closing. Closing a patch in the Editor will not remove patches that have been downloaded to a slot in Nord Modular."});
        m.params.add ({"Save", "This command will save the current patch to a storage disk on the computer. If the patch has not been saved before, you will be prompted for a file name."});
        m.params.add ({"Save As", "This command will prompt you for a file name before saving the patch to disk. This is useful for renaming a patch file before saving it, leaving any original patch intact on the disk."});
        m.params.add ({"Save All", "This command will save all open patches to a storage disk on the computer. If a patch has not been saved before, you will be prompted for a file name."});
        m.params.add ({"Quit", "Quits the Editor software. If any current patches has been edited, you will be asked if you want to save them before quitting. Closing a patch in the Editor or quitting the Editor will not remove patches that have been downloaded to a slot in Nord Modular."});
        d.add (m);

        // ── Filter Bank ──
        m = ModuleHelp();
        m.name = "Filter Bank";
        m.description = "The Filter Bank is a 14-band static filter with attenuation controls for each frequency band. The Filter Bank is very suitable for simulating different kinds of body resonance (formants). It can also be used to filter out a specific frequency range of an external audio signal. For example, you could connect a CD player or similar to the audio in jacks, and route the signals via the Audio In module to the Filter Bank and filter out kick and snare hits and have them trig different modules in the patch.";
        m.params.add ({"Band attenuator sliders", "Attenuate each filter band with the sliders, or by clicking the up and down arrow buttons that appear when you focus a slider. Above each slider, the center frequency of each band is shown in Hz or kHz."});
        m.params.add ({"Presets", "Click on Min to get minimum output level (maximum attenuation of all bands), and on Max to get maximum output level (minimum attenuation)."});
        m.params.add ({"Input", "The red audio input."});
        m.params.add ({"B", "Click on the B button to bypass the input signal and leave it unaffected."});
        m.params.add ({"Output", "Signal: Bipolar"});
        d.add (m);

        // ── Filter group ──
        m = ModuleHelp();
        m.name = "Filter group";
        m.description = "A filter is one of the primary tools for coloring the sound in a synthesizer. It can attenuate and amplify different frequencies in oscillator waveforms and other signals, and drastically change the timbre of the sound. Most of the Nord Modular filters can be dynamically controlled from various sources. In Nord Modular you have several different filter modules to choose from, ranging from traditional LP/ HP/BP filters to complex special filters such as the vocoder and the Vocal filter. Some of the available filter characteristics are: Low pass filter. This filter will remove the high frequency material above the cut-off frequency and allows the low frequencies below the cut-off frequency to pass through. High pass filter. This filter will remove the frequency material below the cut-off frequency and allows the frequencies above the cut-off frequency to pass through. Band pass filter. This filter will remove the frequencies above and below the cut-off frequency and allows a band of frequencies close to the cut-off frequency to pass through. Band reject filter. This filter will remove the frequencies in a band at the cut-off frequency. The frequencies above and below the band will pass through. This filter type can also be called a notch filter.";
        d.add (m);

        // ── FilterA ──
        m = ModuleHelp();
        m.name = "FilterA";
        m.description = "This is a static, non-resonant lowpass filter with a slope of 6 dB/octave.";
        m.params.add ({"Display box", "Shows the set cut-off frequency. Range: 12 Hz to 20 kHz."});
        m.params.add ({"Frequency knob", "Sets the cut-off frequency of the filter. The cut-off frequency is indicated in the info window. Range: 12 Hz to 20 kHz."});
        m.params.add ({"Input", "The red audio signal input."});
        m.params.add ({"Output", "Signal: Bipolar."});
        d.add (m);

        // ── FilterB ──
        m = ModuleHelp();
        m.name = "FilterB";
        m.description = "This is a static, non-resonant highpass filter with a slope of 6 dB/octave.";
        m.params.add ({"Display box", "Shows the set cut-off frequency. Range: 12 Hz to 20 kHz."});
        m.params.add ({"Frequency knob", "Sets the cut-off frequency of the filter. The cut-off frequency is indicated in the info window. Range: 12 Hz to 20 kHz."});
        m.params.add ({"Input", "The red audio signal input."});
        m.params.add ({"Output", "Signal: Bipolar."});
        d.add (m);

        // ── FilterC ──
        m = ModuleHelp();
        m.name = "FilterC";
        m.description = "This is a static multimode filter with a slope of 12 dB/octave and resonance control. It is a multi-mode filter with three outputs: a high passFilter_group, a low passFilter_group and a band passFilter_group output.";
        m.params.add ({"Freq display box", "Displays the cut-off frequency in Hz or notes. Range: E-1 to B9 or 10 Hz to 15.8 kHz. Click in the display box to change unit."});
        m.params.add ({"Freq knob", "Sets the cut-off frequency of the filter. Range: E-1 to B9 or 10 Hz to 15.8 kHz."});
        m.params.add ({"Resonance display box", "Displays the resonance in 'units'. Range: 0 to 127 units."});
        m.params.add ({"Resonance", "This is a function that emphasizes the frequencies that is at, or close to, the set cut-off frequency. If set to 127, the filter starts to self-oscillate and produces a sine wave. Range: 0 to 127 units."});
        m.params.add ({"GC", "This is the Gain Compensation parameter. When activated, it will lower the gain of the signal inside the filter if the resonance is increased, something that otherwise will boost the level within the filter. If several sound sources are processed in a filter and the resonance control is raised, clipping of the signal might occur inside the filter. Activating the GC parameter will reduce the levels, to reduce the risk of any unwanted clipping."});
        m.params.add ({"Input", "The red audio input of the filter module."});
        m.params.add ({"Outputs", "Three filter characteristics are available at these red outputs. You can use all three at the same time if you like. HP is a Highpass filter, BP is a Bandpass filter and LP is a Lowpass filter. Signals: Bipolar."});
        d.add (m);

        // ── FilterD ──
        m = ModuleHelp();
        m.name = "FilterD";
        m.description = "This is a dynamic multimode filter with a slope of 12 dB/octave and resonance control. It is a multi-mode filter with three outputs: one highpass, one lowpass and one bandpass. The cut-off frequency can be modulated from an external source.";
        m.params.add ({"Freq display box", "Displays the cut-off frequency in Hz or notes. Range: E-1 to B9 or 10 Hz to 15.8 kHz. Click in the display box to change unit."});
        m.params.add ({"Frequency modulation input [Type IIITYPE3]", "The blue input for modulating the cut-off frequency from a control source. The modulation amount is determined by the rotary knob next to the input."});
        m.params.add ({"Frequency knob", "Sets the initial cut-off frequency of the filter. The cut-off frequency is indicated in the display box. Range: E-1 to B9 or 10 Hz to 15.8 kHz."});
        m.params.add ({"BT", "This is a hardwired connection between the cut-off frequency and the keyboard. At the preset value \"ey\", the keyboard will control the cut-off frequency at a rate of one semitone for each key. Turning clockwise from the center position will increase the tracking, turning counter-clockwise, will decrease it. Off disconnects the keyboard tracking completely. Click on the triangle above the control to reset the keyboard tracking to ey."});
        m.params.add ({"Resonance display box", "Displays the resonance in 'units'. Range: 0 to 127 units."});
        m.params.add ({"Resonance", "This is a function that emphasizes the frequencies that is at, or close to, the set cut-off frequency. If set to 127, the filter starts to self-oscillate and produces a sine wave. Range: 0 to 127 units."});
        m.params.add ({"Input", "The red audio input."});
        m.params.add ({"Outputs", "Three filter characteristics are available at these red outputs. You can use all three at the same time if you like. HP is a Highpass filter, BP is a Bandpass filter and LP is a Lowpass filter. Signal: Bipolar."});
        d.add (m);

        // ── FilterE ──
        m = ModuleHelp();
        m.name = "FilterE";
        m.description = "This is a dynamic synthesizer filter with a slope of either 12 or 24 dB/octave. It is a multi-mode filter, providing a high passFilter_group, a low passFilter_group, a band passFilter_group or a band rejectFilter_group filter. The cut-off frequency and the resonance can be modulated from external sources.";
        m.params.add ({"Frequency modulation inputs [Type IIITYPE3]", "The two red inputs for connecting cut-off frequency modulators. The modulation amount is determined by the rotary knobs next to each input."});
        m.params.add ({"Freq display box", "Displays the cut-off (or center in BR mode) frequency in Hz or notes. Range: E-1 to B9 or 10 Hz to 15.8 kHz. Click in the display box to change unit."});
        m.params.add ({"Frequency knob", "Sets the initial cut-off (or center in BR mode) frequency of the filter. Range: E-1 to B9 or 10 Hz to 15.8 kHz."});
        m.params.add ({"BT", "The connection between the filter cut-off frequency and the keyboard. At the preset value \"ey\" the keyboard will control the cut-off frequency at a rate of one semitone for each key. Turning clockwise from the center position will increase the tracking, turning counter clockwise, will decrease it. Off disconnects the keyboard tracking completely. Click on the triangle above the control to reset the keyboard tracking to ey."});
        m.params.add ({"Filter selector", "Select the filter type with the buttons. (This selector cannot be assigned to a Morph group). HP is a Highpass filter, BP is a Bandpass filter and LP is a Lowpass filter. BR is a Band reject filter. When this filter is selected, the Resonance knob will control the width of the frequency band to be rejected."});
        m.params.add ({"GC", "This is the Gain Compensation parameter. When activated, it will lower the gain of the signal inside the filter if the resonance is increased, something that otherwise will boost the level within the filter. If several sound sources are processed in a filter and the resonance control is raised, clipping of the signal might occur inside the filter. Activating the GC parameter will reduce the levels, to reduce the risk of any unwanted clipping."});
        m.params.add ({"Resonance display box", "Displays the resonance in 'units'. Range: 0 to 127 units."});
        m.params.add ({"Res", "This is a function that emphasizes the frequencies that is at, or close to, the set cut-off frequency in LP, HP, and BP mode. If set to 127, the filter starts to self-oscillate and produces a sine wave. In BR mode this controls the width of the frequency band to be rejected. Range: 0 to 127 units."});
        m.params.add ({"Resonance modulation input [Type ITYPE1]", "The red input for modulating the resonance from a control source. The modulation amount is determined by the rotary knob next to the input."});
        m.params.add ({"Graph", "Displays a schematic graph of the selected filter characteristics. The Y-axis represents level and the X-axis the frequency. The horizontal line represents the 0 dB level."});
        m.params.add ({"#dB/Oct", "Select the roll-off slope of the filter, 12 or 24 decibels for each octave."});
        m.params.add ({"Input", "The red audio input of the filter."});
        m.params.add ({"B", "Click to bypass the input signal and leave it unaffected."});
        m.params.add ({"Output", "Signal: Bipolar."});
        d.add (m);

        // ── FilterF ──
        m = ModuleHelp();
        m.name = "FilterF";
        m.description = "This is a lowpass filter which simulates classic analog synthesizer filters. The slope is selectable between 12, 18 or 24 dB/octave. The cut-off frequency can be modulated from two external sources.";
        m.params.add ({"Frequency modulation inputs [Type IIITYPE3]", "Two blue inputs for modulating the cut-off frequency with a control source. The modulation amount is determined by the rotary knobs next to the inputs."});
        m.params.add ({"Freq display box", "Displays the cut-off frequency in Hz or notes. Range: E-1 to B9 or 10 Hz to 15.8 kHz. Click in the display box to change unit."});
        m.params.add ({"Frequency knob", "Sets the initial cut-off frequency of the filter. Range: E-1 to B9 or 10 Hz to 15.8 kHz."});
        m.params.add ({"BT", "The connection between the filter cut-off frequency and the keyboard. At the preset value \"ey\" the keyboard will control the cut-off frequency at a rate of one semitone for each key. Turning clockwise from the center position will increase the tracking, turning counter clockwise, will decrease it. Off disconnects the keyboard tracking completely. Click on the triangle above the control to reset the keyboard tracking to ey."});
        m.params.add ({"Resonance display box", "Displays the resonance in 'units'. Range: 0 to 127 units."});
        m.params.add ({"Res", "This is a function that emphasizes the frequencies that is at, or close to, the cut-off frequency. If you set the control to 127 the filter will start to self-oscillate and produce a sine wave. Range: 0 to 127 units."});
        m.params.add ({"Graph", "Displays a schematic graph of the selected filter characteristics. The Y-axis represents level and the X-axis the frequency. The horizontal line represents the 0 dB level."});
        m.params.add ({"#dB/Oct", "Select the roll-off slope of the filter, 12, 18 or 24 decibels for each octave."});
        m.params.add ({"Input", "The red audio input of the filter."});
        m.params.add ({"B", "Click to bypass the input signal and leave it unaffected."});
        m.params.add ({"Output", "Signal: Bipolar."});
        d.add (m);

        // ── Formant OSC ──
        m = ModuleHelp();
        m.name = "Formant OSC";
        m.description = "The Formant oscillator is used for generating vocal-sounding waveforms. It generates a new type of \"non-transposed\" spectrum with strong \"body resonance\" character. 128 timbre variations can be selected. The timbre can also be modulated.";
        m.params.add ({"Freq display box", "Click on the display box switch between Hz and Notes. Range: C-1 to G9 (7.94 Hz to 12910 Hz)."});
        m.params.add ({"Slv output", "This is a gray slave output for controlling slave oscillators. Patch this output to a Mst input of a slave module. If you control a slave LFO with this signal, the rate of the LFO will be five octaves below the pitch of the master oscillator."});
        m.params.add ({"Coarse", "Sets the coarse tuning of the oscillator. Range: C-1 to G9 (8.18 Hz to 12540 Hz)."});
        m.params.add ({"Fine", "Sets the fine tuning of the oscillator. The range is +/- half a semitone divided into 128 steps. Click on the triangle above the control to reset the fine tuning to 0, which is the preset value."});
        m.params.add ({"BT", "BT is the hardwired connection between the oscillator and the keyboard (and the MIDI input). If BT is activated the oscillator will track the keyboard at the rate of one semitone for each key. If BT is not activated, the keyboard will not affect the oscillator frequency."});
        m.params.add ({"Pitch", "There are two blue modulation inputs for modulating the oscillator pitch on this module. The modulation amount is attenuated with the rotary knob next to each input."});
        m.params.add ({"Timbre", "Use the rotary knob to select different timbres (1-127 and Random). The selected timbre is shown in the display box. There is also a blue modulation input for controlling timbre changes from an external source."});
        m.params.add ({"M", "Click on this button to mute the audio output of the oscillator."});
        m.params.add ({"Out", "Signal: Bipolar"});
        m.params.add ({"Gain control", "This module perform the same functions as a traditional VCA, a voltage controlled amplifier in a analog system would do. It provides you with modulation control over the amplitude of an incoming signal. It can also function as a ringmodulator, if you connect two oscillators to the two inputs."});
        m.params.add ({"Control input", "Connect a modulator to this red input. A signal with a level of 0 units will close the gain control function, a signal with a level of +64 units will open the gain control function completely. A signal with a level of -64 units will invert the polarity of the modulated signal."});
        m.params.add ({"Unipolar button", "The Unipolar function changes a bipolar signal at the Control input to a unipolar signal by dividing the signal at the control input by 2 and adding a bias of +32 units."});
        m.params.add ({"Input", "Connect the signal to be modulated (the carrier) to this red input."});
        m.params.add ({"Output", "The output of the Gain control module. Signal: Bipolar."});
        m.params.add ({"Help", ""});
        d.add (m);

        // ── Contents ──
        m = ModuleHelp();
        m.name = "Contents";
        m.description = "This will launch the help file index. Here you can search by typing in a key-word or just the beginning of a key-word. If you right-click on a module or a parameter in the patch window, you can bring up the help text for individual modules or parameters by choosing Help from the popup menu. Putting a module or parameter in focus and pressing the function key F1will bring up the help text for that specific module/parameter.";
        m.params.add ({"Using Help", "Brings up the standard Windows help sections. These describe how to use help files in general."});
        m.params.add ({"About", "This will display a copyright notice, tell you about the author and inform you about the version of the software."});
        d.add (m);

        // ── In/Out group ──
        m = ModuleHelp();
        m.name = "In/Out group";
        m.description = "This group contains modules that gives you access to incoming MIDI or keyboard information. You will also find modules that can patch the audio signals to the four physical audio out jacks of Nord Modular and a module that allows you to patch signals from the two audio inputs of Nord Modular (input l and input r). The three eyboard modules, the 'Audio In' module and the 'Poly Area In' module have a limit to their availability. You can only use one of each in a patch.";
        m.params.add ({"$#", ""});
        d.add (m);

        // ── Inverter & level shifter ──
        m = ModuleHelp();
        m.name = "Inverter & level shifter";
        m.description = "This is a combined level shifter and inverter module. You can use it either to change polarity of the incoming signal, or change it from a bipolar to a unipolar signal (positive or negative), or both.";
        m.params.add ({"Inv", "Click the Inv button to change polarity of the incoming signal. This button can be used in combination with any of the other three buttons to the right."});
        m.params.add ({"Bipolar", "Click the Bipolar button (the leftmost) if you want to keep the incoming signal bipolar. Use this button in combination with the Inv button to just change polarity of the signal."});
        m.params.add ({"Unipolar (Negative)", "Press this button (in the middle) to change the incoming signal from bipolar to a negative unipolar signal. This function divides the signal by 2 and subtracts a bias of 32 units. If Inv is pressed, the polarity will be inverted but the signal will still be negative unipolar."});
        m.params.add ({"Unipolar (Positive)", "Press this button (the rightmost) to change the incoming signal from bipolar to a positive unipolar signal. This function divides the signal by 2 and adds a bias of 32 units. If Inv is pressed, the polarity will be inverted but the signal will still be positive unipolar."});
        m.params.add ({"In", "The red audio input."});
        m.params.add ({"Out", "Signal: Bipolar or Unipolar (Negative or Positive) ey Quantizer This module quantizes the values of a continuous control signal and generates note values according to a user-defined key. It is great for arpeggio-like effects."});
        m.params.add ({"Notes", "Set the desired key by clicking the notes you want to quantize to. The note interval for the shown octave is automatically duplicated across the whole key Range."});
        m.params.add ({"Range display box", "Displays the set key range. For the display to show the correct note value, it is assumed that the input signal uses its whole dynamic range (+/- 64 units)."});
        m.params.add ({"Range knob", "Set the key range in semitones. Range +/- 64 semitones."});
        m.params.add ({"Cont", "Click this button to force the module to \"split up\" the key quantization grid in equally big sections per octave. This makes it easier to output the selected notes at a steady rate when using a linear control signal."});
        m.params.add ({"In", "The Range control signal input."});
        m.params.add ({"Out", "Signal: Bipolar. eyb Split eyboard split gives you the possibility to create split sounds using only one patch. It functions as a \"key filter\" in which you set the limits for the key range that should pass through."});
        m.params.add ({"Lower", "Here you set the lower limit of the keyboard range. The limit note is displayed in the display boxe. Only notes played above the set limit will pass through to the output(s). Range: C-1 to G9."});
        m.params.add ({"Upper", "Here you set the upper limit of the keyboard range. The limit note is displayed in the display boxe. Only notes played below the set limit will pass through to the output(s). Range: C-1 to G9."});
        m.params.add ({"Note, Gate, Vel inputs", "'Note' is the note control signal input. Patch the Note output of the eyboard Patch or eyboard Voice module to this input. 'Gate' is the logic gate input. This input must always receive a high logic gate signal to activate the module. Connect it to the Gate output of the eyboard Patch or eyboard Voice module, for instance. 'Vel' is the velocity control signal input. If a gate signal and a note control signal that lies within the Lower and Upper limits are received, the velocity value of the 'Vel' input will be transmitted to the 'Vel' output."});
        m.params.add ({"Note, Gate, Vel outputs", "These are the outputs for the note, gate and velocity signals. Note signal: Bipolar Gate signal: Logic Velocity signal: Unipolar eyboard - voice The eyboard voice module provides you with access to a few basic and important signals associated with the keyboard on Nord Modular, or a keyboard connected to the synth via MIDI. The signals are generated from each key played and affect one voice at a time."});
        m.params.add ({"Note", "This blue output provides you with a pitch (note number) signal from the Nord Modular keyboard or from the midi in port. This signal is hardwired to every module that has a BT control or switch. There is no need to patch this output to every oscillator you are controlling from the keyboard or via MIDI. This is also the output for any pitch bend data that appears at the Nord Modular midi in port. The pitch bend will be scaled together with the note information, with the ratio of the pitch bend parameter. This ratio is set in the Patch|Patch SettingsPatch_settings menu. E4 (MIDI note 64), which is the middle E on the Nord Modular keyboard when the oct shift selector is in the center position, represents an output signal level of 0 units. MIDI note 0 (C-1) represents -64 units and MIDI note 127 (G9) represents +63 units. Signal: Bipolar."});
        m.params.add ({"Gate", "This yellow output sends a high logic signal (+64 units) every time a key is pressed on the keyboard, or a MIDI note-on is received at the midi in port. The logic signal switches back to zero (0 units) when the key is released. If a sustain pedal is activated, the logic signal will be high for as long as the pedal is pressed. Signal: Logic."});
        m.params.add ({"Vel", "This blue output transmits the note-on velocity signals from the keys that you play on Nord Modular or any velocity that is received on the midi in port. The velocity response of the Nord Modular keyboard is linear. Signal: Unipolar."});
        m.params.add ({"Rel vel", "This blue output provides you with the release velocity signal from the keys that you play on the Nord Modular, or any release velocity that is received via MIDI. Signal: Unipolar. eyboard patch This module provides four different control signals. The signals are generated from the latest key played and affect all allocated voices, in contrast to the eyboard module described above."});
        m.params.add ({"Latest Note", "This blue output provides you with a pitch (note number) signal from the latest note that was played on the keyboard, or that was received at the midi in port. E4 (MIDI note 64), which is the middle E on the Nord Modular keyboard when the oct shift selector is in the center position, represents a signal level of 0 units. MIDI note 0 (C-1) represents -64 units and MIDI note 127 (G9) represents +63 units. Signal: Bipolar."});
        m.params.add ({"Patch gate", "This yellow output sends a high (+64 units) logic signal every time a key is pressed on the keyboard or a MIDI note-on is received at the midi in port. The logic signal switches back to zero (0 units) when the last key is released. You can use this signal to start envelopes in the single-trigger fashion. If a sustain pedal is activated, the logic signal will be high for as long as the pedal is pressed. Signal: Logic."});
        m.params.add ({"Latest vel on", "This blue output provides you with a control signal from the latest note-on velocity. The velocity response of the Nord Modular keyboard is linear. Signal: Unipolar."});
        m.params.add ({"Latest rel vel", "This blue output provides you with a control signal from the release velocity of the latest note. Signal: Unipolar."});
        d.add (m);

        // ── LFO group ──
        m = ModuleHelp();
        m.name = "LFO group";
        m.description = "LFOs, Low Frequency Oscillators are good sources for periodic modulation. The waveforms that they produce can be used for vibrato, tremolo or as clock sources. Some of the LFOs in Nord Modular have a very wide frequency range, from very low to audible frequencies. The output of the control signal coming from a LFO ranges from -64 to +64 units, peak to peak. There are two main LFO groups, master LFOs and slave LFOs. The master LFOs feature more parameters and provide you with more control but they also require more power from the Sound engine than the slaves. The pitch of a slave LFO can be controlled by a master LFO. You will also find a couple of clock generators in this module group.";
        d.add (m);

        // ── LFO slave A ──
        m = ModuleHelp();
        m.name = "LFO slave A";
        m.description = "This slave LFO produces one of five selectable waveforms. The rate can be controlled from a master LFO, the cycle can be forced to restart and the phase of the waveform can be controlled.";
        m.params.add ({"Mst input", "A gray control input, for the frequency of the slave LFO to be controlled by a master module. Patch this input to a Slv output on a master module. If you connect a master oscillator to this input, the slave LFO will track the oscillator five octaves below the pitch of the oscillator."});
        m.params.add ({"Rst input", "The yellow Rst input forces the LFO to restart each time it receives a signal that changes from 0 units or below to anything above 0 units. This signal could come from a gate output of a eyboard or Sequencer module, for example."});
        m.params.add ({"Display box", "Displays either the LFO ratio related to the master rate, or the rate in Hz or seconds. Click to switch unit."});
        m.params.add ({"Rate", "Sets the frequency of the LFO. The frequency will be displayed as a ratio, in relation to the master module connected to the Mst input. The rate LED above the output will show you an approximation of the frequency. Range: 0.025 to 38.05 times the master rate or 62.9 seconds/cycle to 24.4 Hz."});
        m.params.add ({"Mono", "Synchronizes modules in polyphonic patches to each other. This means that if you play a chord, the module will control all voices in sync. Click hereThe_Mono_parameter for details. The preset setting of this parameter is Off."});
        m.params.add ({"Phase", "Sets the starting point of the LFO cycle. Range: -180 to +177 degrees. The set degree is shown in the display box to the left of the knob."});
        m.params.add ({"Graph", "The graph illustrates one cycle and its phase."});
        m.params.add ({"Waveform selectors", "Selects one of the five available waveforms."});
        m.params.add ({"M", "Click on this button to mute the control signal output of the module."});
        m.params.add ({"Out", "The LED above the output shows an approximation of the current LFO rate. Signal: Bipolar."});
        d.add (m);

        // ── LFO slave B ──
        m = ModuleHelp();
        m.name = "LFO slave B";
        m.description = "This slave LFO produces a Sawtooth waveform.";
        m.params.add ({"Mst input", "A gray control input, for the frequency of the slave LFO to be controlled by a master module. Patch this input to a Slv output on a master module. If you connect a master oscillator to this input, the LFO will track the oscillator five octaves below the pitch of the oscillator."});
        m.params.add ({"Display box", "Displays either the LFO ratio related to the master rate, or the rate in Hz or seconds. Click to switch unit."});
        m.params.add ({"Rate knob", "Sets the frequency of the LFO. The frequency will be displayed as a ratio, in relation to the master module connected to the Mst input. The rate LED above the output will show you an approximation of the frequency. Range: 0.025 to 38.05 times the master rate or 62.9 seconds/cycle to 24.4 Hz."});
        m.params.add ({"Out", "The LED above the output shows an approximation of the current LFO rate. Signal: Bipolar."});
        d.add (m);

        // ── LFO slave C ──
        m = ModuleHelp();
        m.name = "LFO slave C";
        m.description = "This slave LFO produces a Sine wave.";
        m.params.add ({"Mst input", "A gray control input, for the frequency of the slave LFO to be controlled by a master module. Patch this input to a Slv output on a master module. If you connect a master oscillator to this input, the LFO will track the oscillator five octaves below the pitch of the oscillator."});
        m.params.add ({"Display box", "Displays either the LFO ratio related to the master rate, or the rate in Hz or seconds. Click to switch unit."});
        m.params.add ({"Rate knob", "Sets the frequency of the LFO. The frequency will be displayed as a ratio, in relation to the master module connected to the Mst input. The rate LED above the output will show you an approximation of the frequency. Range: 0.025 to 38.05 times the master rate or 62.9 seconds/cycle to 24.4 Hz."});
        m.params.add ({"Out", "The LED above the output shows an approximation of the current LFO rate. Signal: Bipolar."});
        d.add (m);

        // ── LFO slave D ──
        m = ModuleHelp();
        m.name = "LFO slave D";
        m.description = "This slave LFO produces a Square wave.";
        m.params.add ({"Mst input", "A gray control input, for the frequency of the slave LFO to be controlled by a master module. Patch this input to a Slv output on a master module. If you connect a master oscillator to this input, the LFO will track the oscillator five octaves below the pitch of the oscillator."});
        m.params.add ({"Display box", "Displays either the LFO ratio related to the master rate, or the rate in Hz or seconds. Click to switch unit."});
        m.params.add ({"Rate knob", "Sets the frequency of the LFO. The frequency will be displayed as a ratio, in relation to the master module connected to the Mst input. The rate LED above the output will show you an approximation of the frequency. Range: 0.025 to 38.05 times the master rate or 62.9 seconds/cycle to 24.4 Hz."});
        m.params.add ({"Out", "The LED above the output shows an approximation of the current LFO rate. Signal: Bipolar."});
        d.add (m);

        // ── LFO slave E ──
        m = ModuleHelp();
        m.name = "LFO slave E";
        m.description = "This slave LFO produces a Triangle wave.";
        m.params.add ({"Mst input", "A gray control input, for the frequency of the slave LFO to be controlled by a master module. Patch this input to a Slv output on a master module. If you connect a master oscillator to this input, the LFO will track the oscillator five octaves below the pitch of the oscillator."});
        m.params.add ({"Display box", "Displays either the LFO ratio related to the master rate, or the rate in Hz or seconds. Click to switch unit."});
        m.params.add ({"Rate knob", "Sets the frequency of the LFO. The frequency will be displayed as a ratio, in relation to the master module connected to the Mst input. The rate LED above the output will show you an approximation of the frequency. Range: 0.025 to 38.05 times the master rate or 62.9 seconds/cycle to 24.4 Hz."});
        m.params.add ({"Out", "The LED above the output shows an approximation of the current LFO rate. Signal: Bipolar."});
        d.add (m);

        // ── LFOA ──
        m = ModuleHelp();
        m.name = "LFOA";
        m.description = "This LFO produces one of five different modulation waveforms. The rate of the LFO can be modulated from an external source. The wave can be forced to restart, and the phase of the wave can be set.";
        m.params.add ({"Rst input", "The yellow Rst input forces the LFO to restart each time it receives a signal that changes from 0 units or below to anything above 0 units. This signal could come from a gate output of a eyboard or Sequencer module, for example."});
        m.params.add ({"Slv output", "This is a gray output for controlling the rate of a slave LFO. Patch this output to a Mst input on a slave module. If you control a slave oscillator, it will track the LFO five octaves above the LFO rate."});
        m.params.add ({"Rate display box", "The Rate display box shows the LFO rate, either in seconds/cycle or in Hz depending on current rate. Range: 699 seconds/cycle to 392 Hz."});
        m.params.add ({"Rate", "Set the frequency of the LFO, the rate, with the knob. The Output LED will show you an approximation of the rate, while the display box will indicate the exact frequency in Hertz, or in seconds if the range is set to Low or Sub."});
        m.params.add ({"Rate modulation input [Type IITYPE2]", "Input for a modulation source to control the rate of the LFO. The modulation amount is attenuated by the rotary knob next to the input."});
        m.params.add ({"Mono", "Synchronizes modules in polyphonic patches to each other. This means that if you play a chord, the module will control all voices in sync (click hereThe_Mono_parameter for details). The preset setting of this parameter is Off."});
        m.params.add ({"Hi, Lo, Sub", "Selects one of three ranges of the LFO rate, high, low or sub. The Hi range is from 0.26 Hz to 392 Hz, the Lo range is from 0.02 Hz to 24.4 Hz and the Sub range is from one cycle completed in 699 seconds to one cycle completed in 5.46 seconds."});
        m.params.add ({"Phase", "Sets the starting point of the LFO cycle. Range: -180 to +177 degrees. The set degree is shown in the display box to the left of the knob."});
        m.params.add ({"Graph", "The graph illustrates one cycle and its phase."});
        m.params.add ({"BT", "This is the keyboard tracking function. If this is set to ey, the LFO will track the keyboard with a doubling of the LFO frequency for each octave. Turning clockwise from the center position will increase the tracking, turning counter clockwise, will decrease it. Off disconnects the keyboard tracking completely. Click on the triangle above the control to set the keyboard tracking to ey. The preset value is Off."});
        m.params.add ({"Waveform selectors", "Selects one of the five available waveforms."});
        m.params.add ({"M", "Click on this button to mute the control signal output of the module."});
        m.params.add ({"Out", "The LED above the output shows an approximation of the current LFO rate. Signal: Bipolar."});
        d.add (m);

        // ── LFOB ──
        m = ModuleHelp();
        m.name = "LFOB";
        m.description = "This LFO produces a square/pulse wave. The width of the pulse can be controlled and modulated. The rate of the LFO can be modulated by a modulation source and the keyboard. The wave cycle can be forced to restart and the phase of the waveform can be controlled.";
        m.params.add ({"Rst input", "The yellow Rst input forces the LFO to restart each time it receives a signal that changes from 0 units or below to anything above 0 units. This signal could come from a gate output of a eyboard or Sequencer module, for example."});
        m.params.add ({"Slv output", "This is a gray output for controlling the rate of a slave LFO. Patch this output to a Mst input on a slave module. If you control a slave oscillator, it will track the LFO five octaves above the LFO rate."});
        m.params.add ({"Rate display box", "The Rate display box shows the LFO rate, either in seconds/cycle or in Hz depending on current rate. Range: 699 seconds/cycle to 392 Hz."});
        m.params.add ({"Rate", "Set the frequency of the LFO, the rate, with the knob. The Output LED will show you an approximation of the rate, while the display box will indicate the exact frequency in Hertz, or in seconds if the range is set to Low or Sub."});
        m.params.add ({"Rate modulation input [Type IITYPE2]", "Input for a modulation source to control the rate of the LFO. The modulation amount is attenuated by the rotary knob next to the input."});
        m.params.add ({"Mono", "Synchronizes modules in polyphonic patches to each other. This means that if you play a chord, the module will control all voices in sync (click hereThe_Mono_parameter for details). The preset setting of this parameter is Off."});
        m.params.add ({"Hi, Lo, Sub", "Selects one of three ranges of the LFO rate, high, low or sub. The Hi range is from 0.26 Hz to 392 Hz, the Lo range is from 0.02 Hz to 24.4 Hz and the Sub range is from one cycle completed in 699 seconds to one cycle completed in 5.46 seconds."});
        m.params.add ({"Phase", "Sets the starting point of the LFO cycle. Range: -180 to +177 degrees. The set degree is shown in the display box to the left of the knob."});
        m.params.add ({"Graph", "The graph illustrates one cycle and its phase."});
        m.params.add ({"BT", "This is the keyboard tracking function. If this is set to ey, the LFO will track the keyboard with a doubling of the LFO frequency for each octave. Turning clockwise from the center position will increase the tracking, turning counter clockwise, will decrease it. Off disconnects the keyboard tracking completely. Click on the triangle above the control to set the keyboard tracking to ey. The preset value is Off."});
        m.params.add ({"PW modulation input [Type ITYPE1]", "This is a blue input for modulating the width of the pulse wave, starting at the initial width set with the PW control. The modulation amount is determined by the rotary knob next to the input."});
        m.params.add ({"PW", "Sets the initial width of the waveform. The range is from 1% to 99%. Click on the triangle above the knob to reset the initial pulse width to 50%. This is the preset value."});
        m.params.add ({"Out", "The LED above the output shows an approximation of the current LFO rate. Signal: Bipolar."});
        d.add (m);

        // ── LFOC ──
        m = ModuleHelp();
        m.name = "LFOC";
        m.description = "This LFO produces one of four selectable waveforms. The rate of the LFO can be modulated.";
        m.params.add ({"Slv output", "This is a gray control output for controlling the rate of a slave LFO. Patch this output to a Mst input on a slave module. If you control a slave oscillator, it will track the LFO five octaves above the LFO rate."});
        m.params.add ({"Display box", "The display box shows the LFO rate, either in seconds/cycle or in Hz depending on current rate. Range: 699 seconds/cycle to 392 Hz."});
        m.params.add ({"Rate modulation input [Type IITYPE2]", "A blue input for a modulation source to control the rate of the LFO. The modulation amount is set by the rotary knob next to the input."});
        m.params.add ({"Rate", "Set the frequency of the LFO, the rate, with the knob. The Output LED will show you an approximation of the rate, while the display box will indicate the exact frequency in Hertz, or in seconds if the range is set to Low or Sub."});
        m.params.add ({"Hi, Lo, Sub", "Selects one of three ranges of the LFO rate, high, low or sub. The Hi range is from 0.26 Hz to 392 Hz, the Lo range is from 0.02 Hz to 24.4 Hz and the Sub range is from one cycle completed in 699 seconds to one cycle completed in 5.46 seconds."});
        m.params.add ({"Waveform selectors", "Selects one of the four available waveforms."});
        m.params.add ({"Mono", "Synchronizes modules in polyphonic patches to each other. This means that if you play a chord, the module will control all voices in sync. Look hereThe_Mono_parameter for details. The preset setting of this parameter is Off."});
        m.params.add ({"M", "Click on this button to mute the control signal output of the module."});
        m.params.add ({"Out", "The LED above the output shows an approximation of the current LFO rate. Signal: Bipolar."});
        m.params.add ({"$#", ""});
        d.add (m);

        // ── Logic delay ──
        m = ModuleHelp();
        m.name = "Logic delay";
        m.description = "This module delays a signal that increases from 0 units to anything greater than 0 units, and produces a high logic output signal. The cycle length of the input signal is unaffected.";
        m.params.add ({"Display box", "Displays selected pulse delay time. Range: 1.0 ms to 18 s."});
        m.params.add ({"Time knob", "Set the pulse delay time with the knob. Range: 1.0 ms to 18 s."});
        m.params.add ({"Input", "The yellow input of the module. Any signal changing from 0 units or below to anything above 0 units will generate a high logic output signal after the set delay time. When the input signal switches back to 0 units, the output generates a low logic signal after the set delay time."});
        m.params.add ({"Output", "Signal: Logic."});
        d.add (m);

        // ── Logic group ──
        m = ModuleHelp();
        m.name = "Logic group";
        m.description = "These modules can modulate and generate logic signals in a number of different ways.";
        d.add (m);

        // ── Logic inverter ──
        m = ModuleHelp();
        m.name = "Logic inverter";
        m.description = "The Logic Inverter module produces a logic low or high signal depending on the input value. When an incoming signal is between +1 and +64 units, the module transmits a low logic signal. When an incoming signal is between 0 and -64 units it transmits a logic high signal.";
        m.params.add ({"Input", "When an incoming signal is between +1 and +64 units, the module transmits a low logic signal on the output. When an incoming signal is between 0 and -64 units it transmits a logic high signal."});
        m.params.add ({"Output", "Signal: Logic."});
        d.add (m);

        // ── Logic processor ──
        m = ModuleHelp();
        m.name = "Logic processor";
        m.description = "This Logic Processor module transmits a high logic signal whenever any incoming signals meet a condition set by you. The input signals are considered \"high\" at any value equal to, or above, +1 unit. The transmitted logic signal will be high for as long as the incoming signals meet the condition.";
        m.params.add ({"Conditions buttons", "AND produces a high logic signal when the two incoming signals, each with a level greater than 0 units, is present at the two inputs at the same time. OR produces a high logic signal if at least one signal, with a level greater than 0 units, appears at the input(s). XOR produces a high logic signal when only one signal, with a level greater than 0 units, appears at one of the inputs."});
        m.params.add ({"Inputs", "The two yellow inputs of the Logic processor module."});
        m.params.add ({"Output", "Signal: Logic."});
        d.add (m);

        // ── MIDI controllers ──
        m = ModuleHelp();
        m.name = "MIDI controllers";
        m.description = "Almost any parameter in the different modules can be assigned to a MIDI Controller. This is very useful if you want to record filter frequency adjustments to an external sequencer or if you want to control external devices from the knobs. When a parameter is assigned to a MIDI controller, the parameter will transmit MIDI data when being edited, as well as receive data from external MIDI sources (sequencer, master keyboard, etc.). right[PC]/Ctrl[Mac]-click on a parameter and select MIDI controller from the parameter popup. Here you can choose either to assign the parameter to one four pre-defined controllers or to assign to another controller by selecting Other. Choose Other and pick a MIDI controller from the list that appears. Some of the MIDI Controllers have designated functions, like controller 1, Modulation Wheel. These functions are just labels to Nord Modular. You are free to assign up to 119 MIDI Controllers to module parameters. MIDI Controller #32 (Bank Change) and #64 (Sustain Pedal) cannot be selected from this list. If you want to control external MIDI devices, these labels may have greater importance. When you edit a parameter that is assigned to a MIDI controller, it will transmit MIDI controller data. It does not matter if you edit the parameter from the Editor or on the synthesizer with the rotary dial (not Micro Modular). You may also assign a parameter to a knob as described earlier. In that a case, turning a knob will result in editing the parameter, which subsequently generates MIDI controller data. De-assign a parameter from a MIDI controller by highlighting a controller and clicking No controller in the dialog box.";
        m.params.add ({"Using the nobs as MIDI controllers", "If you want to use one or several knobs to exclusively transmit MIDI controller data to external devices, you will need to take a detour and assign the knobs to parameters on modules that are not used (connected) in the patch. Then assign the parameters to MIDI controllers. Very useful for this purpose is the Constant module, which does not use any Sound engine resources. You determine which MIDI channel to use in the Synth Settings menu."});
        d.add (m);

        // ── MIDI global ──
        m = ModuleHelp();
        m.name = "MIDI global";
        m.description = "This module generates logic signals that can be used for trigging and synchronizing modules featuring logic inputs.";
        m.params.add ({"Clock", "This yellow output provides you with logic signals from the MIDI Clock in Nord Modular. This output transmits 24 pulses for each quarter note. (The Clock Divider module in the Logic module group can divide these pulses to a division set by you.) Signal: Logic. To set the clock speed and the clock source of the MIDI Clock, press the system button on the Modular panel, select <SYNTH> and navigate to the MIDI Clock function. You can also set the clock source and speed from the Synth|Synth Settings menu in the Editor. The MIDI protocol allows MIDI devices to continue to send clock pulses even if they are stopped. This means that if you synchronize Nord Modular with an external sequencer as master, this output may keep on sending clock pulses even if the sequencer is stopped. The MIDI Start, Stop and Continue commands are present at the Active output of this module."});
        m.params.add ({"Sync", "This yellow output provides you with a logic signal, which is calculated from the MIDI Clock, at a rate set by the Global SyncSynth_settings parameter. The Sync function provides a method of telling the Nord Modular sequencer modules where the first beat in a bar is. Patch this output to the Rst (reset) input of a sequencer module. This function is absolutely essential to use if you plan to synchronize patches in different slots to each other, or if you want to synchronize Nord Modular to an external sequencer. Signal: Logic. Try to make a habit out of always using this function if you are using more than one sequencer module in a patch, especially if you want to mix modules clocked with e.g. triplet resolutions with other modules clocked with eighth or sixteenth notes."});
        m.params.add ({"Active", "This yellow output provides you with a logic high signal whenever a MIDI Start or MIDI Continue command is received at the midi in port. The logic signal will switch to zero when Nord Modular receives a MIDI Stop signal at the midi in port. Signal: Logic."});
        d.add (m);

        // ── MOD-Envelope ──
        m = ModuleHelp();
        m.name = "MOD-Envelope";
        m.description = "The Mod Envelope is an ADSR envelope with inputs to control attack, decay, sustain and release from external sources.";
        m.params.add ({"Amp", "A blue control signal input used for controlling the overall amplitude of the envelope."});
        m.params.add ({"Gate", "A logic control signal input that is used to start and hold the envelope for as long as the logic signal is high. A green LED indicates when a gate signal is received."});
        m.params.add ({"Retrig", "The envelope can be restarted with a logic signal at this input. Note that the envelope must receive a gate signal at the gate input to be able to retrig."});
        m.params.add ({"A", "Sets the attack time. When the envelope receives a high logic signal at the Gate input, the output control signal from the envelope rises up to the maximum output, +64 units. The time to get from 0 to +64 units is the attack time. If the logic Gate signal drops to zero before the envelope has completed the attack stage, it will skip the decay and sustain stages and immediately proceed with the release stage. The attack time is displayed in milliseconds or seconds in the corresponding display box. Range: 0.5 ms to 45 s. Note: A very short attack time can produce a click in the beginning of the sound. This is only normal according to physics theory. To eliminate any click, just increase the attack time slightly."});
        m.params.add ({"D", "Sets the decay time. After the envelope has completed the attack part, it will drop down to the sustain level with the decay time. The decay is exponential. If the sustain level is 64, the decay stage will not be needed, there is simply nothing to decay down to. If the logic Gate signal drops to zero before the envelope has completed the decay stage, it will immediately proceed with the release stage. The decay time is displayed in milliseconds or seconds in the corresponding display box. Range: 0.5 ms to 45 s."});
        m.params.add ({"S", "Sets the sustain level. This level will be held (sustained) for as long as the logic Gate signal is high. When the logic Gate signal drops to zero, the envelope will proceed with the release stage. The sustain level is displayed in 'units' in the corresponding display box. Range: 0 to 64 units."});
        m.params.add ({"R", "Sets the release time. When the logic Gate signal drops to zero, the envelope will decrease from the sustain level to zero with the release time. The release is exponential. The release time is displayed in milliseconds or seconds in the corresponding display box. Range: 0.5 ms to 45 s."});
        m.params.add ({"A, D, S, R control inputs", "Each of the attack, decay, sustain and release values can be controlled externally by using the corresponding control signal input below the A, D, S and R knobs. You can adjust the level of each control signal by turning the corresponding rotary knob. Note that the A, D and R control inputs handles bipolar control signals. Positive control signals shortens the times and negative control signals increase the times."});
        m.params.add ({"Graph", "The info graph shows an approximation of the envelope gain curve. The yellow line represents the sustain level, which is not defined in time since it depends on the gate signal hold time."});
        m.params.add ({"Invert", "This inverts the control signal of the envelope."});
        m.params.add ({"In", "The red audio signal input. Here you patch a signal to the envelope controlled amplifter."});
        m.params.add ({"Env Out", "The blue control signal output from the envelope generator. Signal: Unipolar."});
        m.params.add ({"Out", "The red output from the envelope controlled amplifter. Signal: Bipolar."});
        m.params.add ({"Master OSC", "The Master Oscillator does not generate any waveforms itself. Instead it can be used to control one or several slave oscillators. For that purpose it is very useful since it offers an easy way of controlling global functions, such as coarse tuning and pitch modulation, of the connected slave oscillators. A big advantage with the Master Oscillator is that it saves DSP power compared to other oscillators."});
        m.params.add ({"Slv output", "This is a gray slave output for controlling slave oscillators. Patch this output to a Mst input of a slave module. If you control a slave LFO with this signal, the rate of the LFO will be five octaves below the pitch of the master oscillator."});
        m.params.add ({"Display box", "Click on the display box to change from Hz to Notes. Range: C-1 to G9 (7.94 Hz to 12910 Hz)."});
        m.params.add ({"Coarse", "Sets the coarse tuning of the oscillator. Range: C-1 to G9 in steps of one semitone."});
        m.params.add ({"Fine", "Sets the fine tuning of the oscillator. The range is +/- half a semitone divided into 128 steps. Click on the triangle above the control to reset the fine tuning to 0, which is the preset value."});
        m.params.add ({"Pitch", "There are two blue modulation inputs for modulating the oscillator pitch on this module. The modulation amount is attenuated with the rotary knob next to each input."});
        m.params.add ({"BT", "BT is the hardwired connection between the oscillator and the keyboard (and the MIDI input). If BT is activated the oscillator will track the keyboard at the rate of one semitone for each key. If BT is not activated, the keyboard will not affect the oscillator frequency."});
        d.add (m);

        // ── Mixer group ──
        m = ModuleHelp();
        m.name = "Mixer group";
        m.description = "The mixer modules in Nord Modular can mix audio signals as well as control signals. If you connect several sound sources to a mixer with high or amplified levels, the signal may distort. If this happens, attenuate the input signals.";
        d.add (m);

        // ── Modulation ──
        m = ModuleHelp();
        m.name = "Modulation";
        m.description = "The method of controlling one function in a module with another function is called to \"modulate\". When you play on a keyboard and the oscillator changes its pitch, you are modulating the pitch with the keyboard signal. Another example is an envelope opening up a filter when a key is pressed. Logic signals from the keys tell the envelope to start modulating the cut-off frequency of the filter. Modulation can be positive or negative, e.g. the cut-off frequency of a filter can increase with positive modulation and decrease with negative modulation. As you will see, there are some modules in Nord Modular that can change the polarity of a modulator signal. Some modules can be set to send either bipolar or unipolar control signals, like the Constant module or the Control Sequencer module.";
        m.params.add ({"Modulation inputs", "A module that has parameters that can be modulated has modulation inputs with a modulation amount control. This is called a mod-input. The modulation amount control attenuates the incoming signal. The mod-inputs can be red, as in the OscillatorA module, which means it is capable of handling signals at full audio bandwidth, or they can be blue, as in the FilterF module, working at 1/4 audio bandwidth. When a modulation signal is routed to a mod-input, the setting of the parameter to be modulated has to be considered to be able to predict the result."});
        m.params.add ({"Modulation examples", "pulse width modulation Let us use the pulse width on the OscillatorA module as an example in two scenarios: If you want to modulate the pulse width from the minimum value (1%) to the maximum value (99%) with a positive envelope (that produces a control signal with a peak to peak level swing from 0 units to +64 units), set the initial pulse width to 1% and the mod-amount to 127. If you want to modulate the pulse width from the minimum value (1%) to the maximum value (99%) with an LFO (that produces a bipolar control signal with a peak to peak level swing from -64 units to +64 units), set the initial pulse width to 50% and the mod-amount to 64. Increasing the setting of the mod-amount can not push the pulse-width beyond the limits (1% - 99%), but it will make the modulation signal reach the maximum/minimum pulse-width earlier. A mod-amount setting of 127 would result in maximum pulse-width modulation at a control signal of +/- 32 units. Note the difference between the total amount of modulation from an envelope (unipolar, 64 units) and from an LFO (bipolar, -64 to + 64 units = 128 units). This explains why the first scenario has the mod-amount set to 127, and the second scenario set to 64 for maximum modulation. pitch modulation A signal routed to a Pitch input on a module affects the pitch by modulating it linearly in the note scale. frequency modulation (fm) A signal routed to an FM input on a module affects the pitch by modulating it linearly in the frequency scale. FM modulation results in equal pitch shifting, in Herz, on either side of the basic pitch, whereas Pitch shifting results in equal shifting in the note scale. sync A waveform of an oscillator with a Sync input can be synchronized with a wave of another oscillator. The synchronization forces the wave to restart each time the modulating wave raises above 0. This results in a complex waveform that depends both on its own pitch and on the modulator pitch. When sync is used, the oscillator pitch is locked to the modulator pitch. If you change the modulator pitch, you will affect the overall pitch, and if you change the oscillator pitch, this will create changes in timbre rather than in pitch. If you let the synchronized oscillator pitch vary continuously, from an LFO or other modulator, you will change the timbre of the wave in a very interesting and characteristic way."});
        m.params.add ({"Mod-amount knobs", "There are three different response behaviors of the mod-amount knobs next to the modulation inputs: linear [Type I], exponential [Type II] and amplified linear [Type III]. The different response type(s) are indicated for each module."});
        m.params.add ({"[Type I]:", "The mod-amount knobs attenuates the incoming signal in a linear fashion. A setting of 127 (maximum) leaves the incoming signal unaffected, a setting of 64 attenuates the incoming signal by a factor 0.5 (leaving half of the level of the incoming signal to modulate). A setting of 0 shuts off the modulation completely. The pulse width in the aforementioned scenarios is an example of Type I attenuation."});
        m.params.add ({"[Type II]:", "The mod-amount knob attenuates the incoming signal in an exponential fashion. A setting of 127 (maximum) leaves the incoming signal unaffected, a setting of 64 attenuates the incoming signal by a factor considerably less than 0.5 (leaving less than half of the level of the incoming signal to modulate). A setting of 0 shuts off the modulation completely. The pitch mod-input on the various oscillators are examples of Type II attenuation."});
        m.params.add ({"[Type III]:", "The mod-amount knob affects the incoming signal in an attenuated and amplified, linear fashion. A setting of 127 (maximum) amplifies the incoming signal to twice its original level, a setting of 64 leaves the incoming signal unaffected and a setting of 32 attenuates the incoming signal by a factor of 0.5 (leaving half of the level of the incoming signal to modulate). A setting of 0 shuts off the modulation completely. The frequency mod-input on the various filters are the sole examples of Type III attenuation."});
        m.params.add ({"Maximum modulation", "The maximum amount of modulation that a module (with one exception) can accept is +/-64 units from the initial setting of the parameter. The exception to this behaviour are all the Filter frequencies with a mod-input. These can accept +/-128 units of modulation. The modulation amount is the sum of all modulation appearing at the modulation inputs. Let us use the Master Oscillator module as an example: there are two pitch modulation inputs and the BT function. The total modulation amount of these three inputs can not be greater than +/-64 semitones. If you turn the coarse tuning down to e.g. E0, add a transpose value of +64 with a Constant module to the first pitch-mod input, you will reach a point, when playing on the keyboard, where the pitch of the oscillator will be fixed. Any additional, positive modulation will have no effect, which could lead to interesting effects. For example, an LFO would be able to modulate the pitch of the oscillator downwards, but not upwards."});
        m.params.add ({"$Module reference", "The modules are grouped in ten module groups, which you access by clicking on the tabs in the toolbar. The modules are visually identified with illustrations. When you place the cursor over an illustration, a brief description appears together with an indication of how much Sound engine power (Load) the module uses. Each time you add a module to a patch, Nord Modular mutes the outputs for a short moment when recalculating the patch data. Theoretically, you could use up to 254 modules in each patch, 127 in the Poly Voice Area and 127 in the Common Voice Area, but you will probably run out of patch Load before that. It is possible to use several modules of the same type in one patch, except from the 'eyboard voice', 'eyboard Patch', 'MIDI Global', 'Audio In' and 'Poly Area In' modules. These modules can only be used once in a patch. A grayed-out illustration indicates that the module in question is already used and cannot be added to the patch."});
        m.params.add ({"$Morph", "There are four Morph groups available in each patch and you may assign up to 25 different parameters, distributed as you wish among the four Morph groups. Here is an example on how to assign a parameter to a Morph group: right-click on a module parameter (knob or a selector) you wish to control with a Morph group. Select Morph and one of the four Morph groups from the menu. The color of the selected knob will now change to the selected Morph group's color. Alternatively, first click on any of the Morph group knobs in the toolbar to select it (the blue frame moves to the selected Morph group). Then double-click on the module parameter you wish to assign to the selected Morph group. Every parameter assigned to a Morph group should also be assigned to a Morph range. Click and hold the Ctr key on the computer keyboard and place the cursor on the knob that you assigned to the Morph group. Click-drag the cursor as if you were turning the knob. A colored slice will appear, indicating the Morph range. The range will also appear in a yellow hintbox above the parameter. You can also double-click-hold on a parameter that is assigned to a Morph group to set the range. If you assign a selector switch (button) to a Morph group, the Morph range will be set by holding down the Ctrl key and clicking on the button in the selector that should be the \"last\" (end limit) to be controlled by the Morph. Turning the Morph group knob in the toolbar will now control the morphed parameter within the selected range. You can assign each Morph group knob to e.g. a knob on the front panel, a pedal (not Micro Modular), an external MIDI controller, or, as an exclusive feature of the Morph group knobs, to MIDI note values or keyboard velocity. You will find these two options at the bottom of the MIDI controller popup. To deassign a parameter from a Morph group, right-click on it and select Disable in the Morph popup."});
        m.params.add ({"Editing the morph range", "The Morph range will always start at the current position of a knob, slider or selector. The relationship between the setting of the parameter and the Morph range will be fixed, even if you move the setting of the parameter after a Morph range has been set. You can edit the morph range (the size of the slice) by double-clicking on the parameter or by using the mouse in conjunction with the Ctrl key on the computer keyboard. Another way is to put Nord Modular in Edit mode by pressing the edit button (not Micro Modular). Put the morphed parameter in focus and simultaneously press- hold the shift and assign buttons and set the amount of Morph control with the rotary dial. You can also get a read-out of which Morph group a parameter is assigned to by pressing the F7 function key on the computer keyboard. Pressing the F5 key displays the Morph ranges (start and end values) of the assigned (morphed) parameters."});
        d.add (m);

        // ── Morph groups ──
        m = ModuleHelp();
        m.name = "Morph groups";
        m.description = "The Morph groups concept of Nord Modular is a very powerful feature. To put it simple: the Morph groups lets you simultaneously control defined ranges of up to 25 parameters in a patch, using only one control source (a knob and/or a MIDI controller, for example). As you have figured out, this lets you produce radical changes in a sound in a very fast and easy way. The Morph groups and their corresponding knobs are located in the Editor toolbar. There are four Morph groups available in each patch and you may assign up to 25 different parameters, distributed as you wish among the four Morph groups. Each Morph group has its own color, making it very easy to see what parameters are assigned to which Morph group. Here is an example on how to assign a parameter to a Morph group: 1. right[PC]/Ctrl[Mac]-click on a parameter (knob or a selector) you wish to control with a Morph group. Almost any module parameter in Nord Modular can be morphed. Select Morph and one of the four Morph groups from the menu. The color of the selected knob will now change to the selected Morph group's color. Alternatively, first click on any of the Morph group knobs in the toolbar to select it (the blue frame moves to the selected Morph group). Then double-click on the module parameter you wish to assign to the selected Morph group. 2. Every parameter assigned to a Morph group should also be assigned to a Morph range. Click and hold the Ctrl[PC]/Alt[Mac] key on the computer keyboard and place the cursor on the knob that you assigned to the Morph group. Click-drag the cursor as if you were turning the knob. A colored slice will appear, indicating the Morph range. The range will also appear in a yellow hintbox above the parameter. You can also double-click-hold on a parameter that is assigned to a Morph group to set the range. If you assign a selector switch (button) to a Morph group, the Morph range will be set by holding down the Ctrl[PC]/Alt[Mac] key and clicking on the button in the selector that should be the \"last\" (end limit) to be controlled by the Morph. Turning the Morph group knob in the toolbar will now control the morphed parameter within the selected range. You can assign each Morph group knob to e.g. a knob on the front panel, a pedal (not Micro Modular), an external MIDI controller, or, as an exclusive feature of the Morph group knobs, to MIDI note values or keyboard velocity. You will find these two options at the bottom of the MIDI controller popup. To deassign a parameter from a Morph group, right[PC]/Ctrl[Mac]-click on it and select Disable in the Morph popup.";
        m.params.add ({"To edit the Morph range", "The Morph range will always start at the current position of a knob, slider or selector. The relationship between the setting of the parameter and the Morph range will be fixed, even if you move the setting of the parameter after a Morph range has been set. You can edit the morph range (the size of the slice) by double-clicking on the parameter or by using the mouse in conjunction with the Ctrl[PC]/Alt[Mac] key on the computer keyboard. Another way is to put Nord Modular in Edit mode by pressing the edit button (not Micro Modular). Put the morphed parameter in focus and simultaneously press- hold the shift and assign buttons and set the amount of Morph control with the rotary dial. You can also get a read-out of which Morph group a parameter is assigned to by pressing the F7 function key on the computer keyboard. Pressing the F5 key displays the Morph ranges (start and end values) of the assigned parameters."});
        d.add (m);

        // ── Multistage envelope ──
        m = ModuleHelp();
        m.name = "Multistage envelope";
        m.description = "The Multi stage envelope is a 5-segment time and level envelope with selectable sustain segment.";
        m.params.add ({"Gate", "A logic control signal is used to start and hold the envelope for as long as the logic signal is high. A green LED indicates when a gate signal is received."});
        m.params.add ({"Amp", "A blue control signal input used for controlling the overall amplitude of the envelope."});
        m.params.add ({"L1-L4", "By turning the rotary knobs L1 to L4 you can set the amplitude of each of the four level segments in the envelope. The levels can be either unipolar or bipolar as described above. The levels are displayed in units in the corresponding display box. Ranges: 0 to +64 units (unipolar) or -64 to +64 units (bipolar)."});
        m.params.add ({"T1-T5", "Here you set the times between the four levels plus a fifth release time from L4 to zero. The times are displayed in milliseconds or seconds in the corresponding display box. Ranges: 0.5 ms to 45 s. Note: Very short times can produce clicks in the sound. This is only normal according to physics theory. To eliminate any click, just increase the times slightly."});
        m.params.add ({"Graph", "The info graph shows an approximation of the envelope gain curve. The yellow line represents the sustain level, which is not defined in time since it depends on the gate signal hold time. If 'bipolar' is selected, a horizontal broken line represents the 0 units output level."});
        m.params.add ({"Sustain", "By clicking on the up and down arrow buttons you define the sustain segment. This segment can be any of the four level segments, or, if you wish, none at all. The sustain segment works like in an ordinary ADSR envelope, i.e this is the level that sustains as you hold down the key(s). After releasing the key(s) the envelope will continue till the end of T5. Range: None and L1 to L4."});
        m.params.add ({"In", "The red audio signal input. Here you patch a signal to the envelope controlled amplifter."});
        m.params.add ({"Env Out", "The blue control signal output of the envelope. Signal: Unipolar or Bipolar"});
        m.params.add ({"Out", "The red output from the envelope controlled amplifter. Signal: Bipolar."});
        d.add (m);

        // ── Negative edge delay ──
        m = ModuleHelp();
        m.name = "Negative edge delay";
        m.description = "This module will delay the negative edge of a logic signal. Set the delay time with the slider. The positive edge of the logic signal will not be affected.";
        m.params.add ({"Disply box", "Displays selected delay time. Range: 1.0 ms to 18 s."});
        m.params.add ({"Time knob", "Set the delay time with the knob. The range is from 1.0 milliseconds to 18 seconds. If the module receives a new positive edge before it has completed a delay cycle, the module will simply extend the duration of the high level signal with the time set with the knob."});
        m.params.add ({"Input", "The yellow input of the module. Any signal changing from 0 units or below to anything above 0 units will generate a high logic output signal. When the input signal switches back to 0 units, the delay time activated."});
        m.params.add ({"Output", "Signal: Logic."});
        d.add (m);

        // ── Noise ──
        m = ModuleHelp();
        m.name = "Noise";
        m.description = "This sound source generates noise, selectable from white to colored.";
        m.params.add ({"White/Colored", "Set the color of the noise with the control knob. Colored noise contains less high frequency energy than white noise."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Note and velocity scaler ──
        m = ModuleHelp();
        m.name = "Note and velocity scaler";
        m.description = "This is a note and velocity scaler for control signals. You can use it to produce control signals based on input note and velocity values. You can set a break point key and different amplification/attenuation slopes for the two key sections. This module is very suitable for controlling the amplitude in modules featuring amplitude modulation input(s).";
        m.params.add ({"Vel", "Patch this input to e.g. the velocity output of any of the Input modules (eyboard voice or eyboard Patch)."});
        m.params.add ({"Note", "Patch this input to e.g. the note output of any of the Input modules (eyboard voice or eyboard Patch)."});
        m.params.add ({"Vel Sens [Attenuator Type I]", "With this rotary knob you set how much the input velocity should affect the output value. If set to min (0) the velocity output component is always 64 units. If set to max (127) the output can vary between 0 and 85 units. If set to max (127), a velocity input of 48 units results in an output value of 64."});
        m.params.add ({"L Gain", "Set the amplification/attenuation slope for the lower key section with the knob. The value is displayed in the corresponding display box. Range: +/-24 dB per octave."});
        m.params.add ({"Brk Pnt", "Set the break point key. The value is displayed in the corresponding display box. Range: C-1 to G9."});
        m.params.add ({"R Gain", "Set the amplification/attenuation slope for the upper key section with the knob. The value is displayed in the corresponding display box. Range: +/-24 dB per octave. graph Displays the two gain slopes and the break point graphically. The Y-axis represents the output level (logarithmic) and the X-axis the entire note range (C-1 to G9). the horizontal line represents the +64 units (0 dB) output level. See figure below for further explanation."});
        m.params.add ({"Output", "The output value is the combined result of the note and velocity input values. Signal: Bipolar."});
        d.add (m);

        // ── Note detect ──
        m = ModuleHelp();
        m.name = "Note detect";
        m.description = "This module can detect a note, either from the Nord Modular keyboard or from the midi input. A logic high signal will be transmitted, together with a velocity control signal, when the selected key is detected. The logic signal will switch to zero, and a release velocity control signal will be sent, when the selected key is released. The Note Detect module is global and affects all voices assigned in a patch. The behavior is similar to the eyboard Patch module described earlier in this chapter.";
        m.params.add ({"Note knob", "Select the note to be detected. Range: C-1 to G9."});
        m.params.add ({"Outputs", "Gate signal: Logic. V (Velocity signal): Unipolar. R (Release velocity signal): Unipolar."});
        d.add (m);

        // ── Note quantizer ──
        m = ModuleHelp();
        m.name = "Note quantizer";
        m.description = "This module will quantize the values of a continuous control signal to produce discrete, semitone steps. The total range of the incoming signal can be attenuated at the input.";
        m.params.add ({"Range display box", "Displays the note range. Range: 0 to +/-64 semitones. Note that for the display box to show the correct limits, it is assumed that the input signal uses the full range -64 to +64 semitones."});
        m.params.add ({"Range knob", "Set the signal range with the knob."});
        m.params.add ({"Notes display box", "Displays the selected quantization grid (interval) in semitones. Range: Off, 1-127."});
        m.params.add ({"Notes", "Set the desired quantization grid (interval), in semitones, with the buttons. Range: Off and 1 to 127 semitones."});
        m.params.add ({"In", "The blue control signal input."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Note scaler ──
        m = ModuleHelp();
        m.name = "Note scaler";
        m.description = "This module works like a control signal attenuator. You set the output peak-to-peak limits in semitones. This could be useful if you want to \"tune\" the output from a controller. The Note Scaler works with either uni- or bipolar signals.";
        m.params.add ({"Display box", "Displays the note range limits. Useful musical intervals will be indicated in the parenthesis (octaves, fifths etc.). Range: 0 to +/-64 semitones. Note that for the display box to show the correct limits, it is assumed that the input signal uses the full range -64 to +64 semitones."});
        m.params.add ({"Range knob", "Set the semitone range with the slider. Range: 0 to +/-64 semitones."});
        m.params.add ({"In", "The blue control signal input."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Note sequencer ──
        m = ModuleHelp();
        m.name = "Note sequencer";
        m.description = "This is a Note Sequencer with one bipolar control value for each step and a grid pattern for easy note editing. It also sends a logic gate signal for each step on a separate output. If you move the cursor over the grid, it becomes a magnifying glass with a + sign next to it. Clicking in the grid will zoom in, and Ctrl-clicking will zoom out. Choose an overview from 1 to 6 octaves.";
        m.params.add ({"Clk input", "This is the yellow input for the clock pulses. These pulses will advance the sequencer one step for each pulse."});
        m.params.add ({"Rst input", "This is a yellow input where a high logic signal will reset the sequencer to step 1. The restart isn't performed until the next the clock pulse is received at the Clk input. This guarantees perfect timing."});
        m.params.add ({"Clr button", "Pressing this button will reset all the note values to 0."});
        m.params.add ({"Rnd button", "This will produce a random set of control signal values for each of the 16 steps. The values generated will be inside the boundaries of the visible grid."});
        m.params.add ({"Snc output", "This output transmits a high logic signal whenever a sequence starts from step 1. Signal: Logic."});
        m.params.add ({"Loop button", "If the loop mode is on, the Note Sequencer will automatically restart from step 1 after the last step. If the loop mode is off, the sequencer will stop at the last step."});
        m.params.add ({"Step", "This sets the last step in the sequence. The sequencer will return to step 1 if loop mode is on, or stop if loop mode is off. Set the last step with the buttons. Range: 1 to 128 steps."});
        m.params.add ({"Rec button", "Activating the record function enables you to use the keyboard or incoming MIDI messages to program the steps in the sequence. Pressing a key will advance the edit point to the next step. The control signal from the keyboard will be present at the sequencer output. When the sequencer reaches step 16, the record function will be deactivated and the sequencer will return to step 1. The sequencer may be sequencing while you program the steps, if the Stop selector is in the Go position. You can move the edit point back and forth by using the arrow keys beneath the Stop button. The edit point can also be selected by clicking on a step LED."});
        m.params.add ({"Stop button", "Activating Stop stops the sequencer module, even if it is receiving a clock pulse at the Clk input. If you activate the record function when the sequencer is stopped, it may be programmed in a step mode fashion. Activating Go will put the sequencer back under the influence of a clock, connected to the Clk input."});
        m.params.add ({"#< > buttons", "Use these buttons to scroll through the steps."});
        m.params.add ({"Control signal arrow buttons", "You set the control signal value for each step in the sequence with the arrow buttons below each step. If you move the cursor over the grid, it becomes a magnifying glass. Clicking in the grid will zoom in, and Ctrl-clicking will zoom out. Choose an overview from 1 to 6 octaves."});
        m.params.add ({"Grid position scroll bar", "With the scroll bar to the right, you move the grid up or down in the window. You can also use the up and down arrow buttons to move the grid. The guide lines in the grid indicates the \"E\" notes of a six octave range. If you move the cursor over the grid, it becomes a magnifying glass. Clicking in the grid will zoom in, and Ctrl-clicking will zoom out. Choose an overview from 1 to 6 octaves."});
        m.params.add ({"Link output", "This yellow output transmits a high logic signal whenever the Note Sequencer goes beyond step number 16. This is used for linking several Note Sequencers in series. See more about linking at the end of this chapter. Signal: Logic."});
        m.params.add ({"Gclk output", "This output transmits a high logic signal when the sequencer moves from one step to another. If the sequencer is stopped and you scroll manually through the steps, the Gclk will transmit one logic signal for each step, even if you scroll backwards. It will also transmit a logic signal when you are recording with the keyboard, if the Stop selector is in the stop position. Signal: Logic."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── On/Off switch ──
        m = ModuleHelp();
        m.name = "On/Off switch";
        m.description = "This is a one input, one output module with an on/off switch, If no signal is patched to the input, the output produces a signal with the value 0 units when the switch is set to Off, and a signal with the value +64 units when switched On.";
        m.params.add ({"On", "Selects whether the incoming signal should pass through to the output or not. If no signal is patched to the input, Off (button not depressed) outputs 0 units and On (button depressed) +64 units."});
        m.params.add ({"Input", "The red audio input."});
        m.params.add ({"Output", "Signal: Bipolar, Unipolar or Logic depending on the input signal."});
        d.add (m);

        // ── Oscillator A ──
        m = ModuleHelp();
        m.name = "Oscillator A";
        m.description = "This oscillator can produce one of four waveforms: Square/Pulse, Sawtooth, Triangle or Sine. This oscillator has two pitch modulation inputs, one frequency modulation (FMA) input, a sync input and an input to modulate the width of the Pulse wave.";
        m.params.add ({"Freq display box", "Click on the display box switch between Hz and Notes. Range: C-1 to G9 (7.94 Hz to 12910 Hz)."});
        m.params.add ({"Slv output", "This is a gray control output for controlling the pitch of a slave oscillator. Patch this output to a Mst input on a slave module. If you control a slave LFO with this signal, the pitch of the LFO will be 5 octaves below the pitch of the master oscillator."});
        m.params.add ({"Coarse", "Sets the coarse tuning of the oscillator. Range: C-1 to G9 (8.18 Hz to 12540 Hz)."});
        m.params.add ({"Fine", "This control fine-tunes the oscillator. The range is +/- half a semitone, divided into 128 increments. Click on the triangle above the control to reset the fine-tuning to 0, which is the preset value."});
        m.params.add ({"BT", "This is the hardwired connection between the oscillator and the keyboard. At the ey value, the oscillator will track the keyboard at the rate of one semitone for each key. Turning the BT control to the maximum clockwise position (2.0) will produce a two-octave span from one octave on the keyboard, a value of 0.50 will produce a one octave span from two octaves played on the keyboard. Off removes the keyboard tracking completely. Click on the triangle above the knob to reset the BT setting to ey, which is the preset value."});
        m.params.add ({"Pitch modulation inputs [Type IITYPE2]", "There are two red modulation inputs for modulating the oscillator pitch on this module. The modulation amount is attenuated with the rotary knob next to each input."});
        m.params.add ({"FMA modulation input [Type IITYPE2]", "A red modulation input where a signal will affect the frequency of the oscillator. The FM amount is attenuated with the rotary knob next to the input."});
        m.params.add ({"Sync input", "This red input is used for synchronizing the oscillator to a control source, which could be another oscillator, an LFO or the keyboard. Synchronization forces the oscillator to restart its waveform cycle, in sync with the waveform cycle of the controlling device. The oscillator will restart whenever a signal present at the sync input increases from 0 units to anything above 0 units. The pitch of the controlling oscillator will interact with the controlled oscillator pitch. For a traditional synthesizer sync-sound, start with the two oscillators to the same pitch and connect only the sync-controlled oscillator to an output. Turning the tuning knob or modulating the pitch of the sync-controlled oscillator will produce radical changes in the timbre."});
        m.params.add ({"PWidth", "Sets the initial width of the two sections of a Pulse waveform period. The range is from 1% to 99%. Click on the triangle above the knob to reset the initial pulse width to 50%, which is the preset value."});
        m.params.add ({"Pulse Width modulation input [Type ITYPE1]", "This is a red input for controlling the width of a Pulse waveform with a modulator, starting at the initial width you have set with the Pulse Width control. The modulation amount is determined by the rotary knob next to the input."});
        m.params.add ({"Waveform selectors", "Selects one of the four available waveforms."});
        m.params.add ({"M", "Click on this button to mute the audio output of the oscillator."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Oscillator B ──
        m = ModuleHelp();
        m.name = "Oscillator B";
        m.description = "This oscillator can produce one of four waveforms: Square/Pulse, Sawtooth, Triangle or Sine. This oscillator has two pitch modulation inputs, one frequency modulation (FMA) input and a Pulse width modulation input.";
        m.params.add ({"Slv output", "This is a gray control output for controlling the pitch of a slave oscillator. Patch this output to a Mst input on a slave module. If you control a slave LFO with this signal, the pitch of the LFO will be 5 octaves below the pitch of the master oscillator."});
        m.params.add ({"Display box", "Click on the display box switch between Hz and Notes. Range: C-1 to G9 (7.94 Hz to 12910 Hz)."});
        m.params.add ({"Coarse", "Sets the coarse tuning of the oscillator. Range: C-1 to G9 (8.18 Hz to 12540 Hz)."});
        m.params.add ({"Fine", "This control fine-tunes the oscillator. The range is +/- half a semitone, divided in 128 increments. Click on the triangle above the control to reset the fine-tuning to 0, which is the preset value."});
        m.params.add ({"BT", "This is the hardwired connection between the oscillator and the keyboard (and the MIDI input). At the \"ey\" value, the oscillator will track the keyboard at the rate of one semitone for each key. Turning the BT control to the maximum clockwise position (2.0) will produce a two-octave span from one octave on the keyboard, a value of 0.50 will produce a one octave span from two octaves played on the keyboard. Off removes the keyboard tracking completely. Click on the triangle above the knob to reset the BT setting to ey, which is the preset value."});
        m.params.add ({"Pitch modulation inputs [Type IITYPE2]", "There are two blue modulation inputs for modulating the oscillator pitch on this module. The modulation amount is attenuated with the rotary knob next to each input."});
        m.params.add ({"FMA modulation input [Type IITYPE2]", "A red modulation input where a signal will affect the frequency of the oscillator. The FM amount is attenuated with the rotary knob next to the input."});
        m.params.add ({"PWidth modulation input [Type ITYPE1]", "This is a blue input for modulating the width of the Pulse waveform, starting at the initial width of 50%. The modulation amount is determined by the rotary knob next to the input."});
        m.params.add ({"Waveform selectors", "Selects one of the four available waveforms. Clicking on a selected button will mute the audio output of the oscillator."});
        m.params.add ({"M", "Click on this button to mute the audio output of the oscillator."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Oscillator C ──
        m = ModuleHelp();
        m.name = "Oscillator C";
        m.description = "This oscillator produces a Sine wave. It has one pitch modulation input, one amplitude modulation input and one FMA modulation input.";
        m.params.add ({"Freq display box", "Click on the display box switch between Hz and Notes. Range: C-1 to G9 (7.94 Hz to 12910 Hz)."});
        m.params.add ({"Slv output", "This is a gray control output for controlling the pitch of a slave oscillator. Patch this output to a Mst input on a slave module. If you control a slave LFO with this signal, the pitch of the LFO will be 5 octaves below the pitch of the master oscillator."});
        m.params.add ({"Coarse", "Sets the coarse tuning of the oscillator. Range: C-1 to G9 (8.18 Hz to 12540 Hz)."});
        m.params.add ({"Fine", "This control fine-tunes the oscillator. The range is +/- half a semitone, divided in 128 increments. Click on the triangle above the control to reset the fine-tuning to 0, which is the preset value."});
        m.params.add ({"Pitch modulation input [Type IITYPE2]", "There is one red modulation input for modulating the oscillator pitch on this module. The modulation amount is attenuated with the rotary knob next to the input. See \"Pitch modulation\" on page 35 for more info."});
        m.params.add ({"AM modulation input", "This red input allows you to modulate the amplitude of the oscillator wave. The amount of this modulation is fixed at a ratio of 1:1."});
        m.params.add ({"FMA modulation input [Type IITYPE2]", "A red modulation input where a signal will affect the pitch of the oscillator in a linear way. The FM amount is attenuated with the rotary knob next to the input. See \"Frequency modulation (FM)\" on page 36 for more info."});
        m.params.add ({"BT", "This is the hardwired connection between the oscillator and the keyboard (and the MIDI input). When this function is activated, the oscillator will track the keyboard at the rate of one semitone for each key."});
        m.params.add ({"M", "Click on this button to mute the audio output of the oscillator."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Oscillator group ──
        m = ModuleHelp();
        m.name = "Oscillator group";
        m.description = "There are two main types of oscillators in Nord Modular, master oscillators and slave oscillators. The master oscillators has more parameters and provide you with more control options, but they require more power from the Sound engine than the slaves. The pitch of a slave oscillator can be controlled from a master oscillator. If you run short on Sound engine power, try to find a way to replace a master oscillator with a slave. The oscillators (except from the Master Oscillator) produce a constant sound. If you connect an oscillator output to a mix bus, the waveform will sound constantly. If you want the behaviour of a traditional synthesizer, remember to control the output of the oscillator with an envelope generator.";
        d.add (m);

        // ── Oscillator slave A ──
        m = ModuleHelp();
        m.name = "Oscillator slave A";
        m.description = "This slave oscillator can produce one of four waveforms. You can select Sine wave, Triangle wave, Sawtooth or a Square/Pulse wave. It has an FMA, an AM and a Sync modulation input.";
        m.params.add ({"Sync input", "This red input is used for synchronizing the oscillator to a control source, which could be another oscillator, an LFO or the keyboard. Synchronization forces the oscillator to restart its waveform cycle, in sync with the waveform cycle of the controlling device. The oscillator will restart whenever a signal present at the sync input increases from 0 units to anything above 0 units. The pitch of the controlling oscillator will interact with the controlled oscillator pitch. For a traditional synthesizer sync-sound, start with the two oscillators to the same pitch and connect only the sync-controlled oscillator to an output. Turning the tuning knob or modulating the pitch of the sync-controlled oscillator will produce radical changes in the timbre."});
        m.params.add ({"Mst input", "The gray control input from a master oscillator. Connect this input to a Slv output from a master module. If you connect a master LFO to this input, the slave oscillator will track the LFO five octaves above the pitch of the LFO."});
        m.params.add ({"Display box", "Click on the info window to switch between Semitones, Hz and Ratio. Range: C-1 to G9 (7.94 Hz to 12910 Hz or x 0.0241 to x 39.152)."});
        m.params.add ({"Partials", "Select a preset transposition value of the slave oscillator. The transposition ratio will be displayed as multiples of the frequency of the master oscillator. Range 1:32 to 32:1."});
        m.params.add ({"Detune", "Set the detune of the slave oscillator, in semitone steps. The tuning will relate to the tuning of the master oscillator connected to the Mst input. The preset value is 0 (1:1). If you select the Semitone display, a second number inside a parenthesis will indicate a musical interval (a fifth, a seventh etc.)."});
        m.params.add ({"Fine", "Fine-tunes the slave oscillator. The range is +/- half a semitone, divided in 128 increments. Click on the triangle above the control to reset the fine-tuning to 0, which is the preset value."});
        m.params.add ({"Waveform selectors", "Select one of the available waveforms."});
        m.params.add ({"FMA modulation input [Type IITYPE2]", "A red modulation input where a signal will affect the pitch of the oscillator in a linear way. The FM amount is attenuated with the rotary knob next to the input."});
        m.params.add ({"AM modulation input", "This red input allows you to modulate the amplitude of the slave oscillator. The amount of this modulation is fixed at a ratio of 1:1."});
        m.params.add ({"M", "Click on this button to mute the audio output of the oscillator."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Oscillator slave B ──
        m = ModuleHelp();
        m.name = "Oscillator slave B";
        m.description = "This slave oscillator produces a Square/Pulse waveform. The pulse width can be modulated.";
        m.params.add ({"Mst input", "The gray control input from a master oscillator. If you connect a master LFO to this input, the slave oscillator will track the LFO five octaves above the pitch of the LFO."});
        m.params.add ({"Display box", "Click on the info window to switch between Semitones, Hz and Ratio. Range: C-1 to G9 (7.94 Hz to 12910 Hz or x 0.0241 to x 39.152)."});
        m.params.add ({"Partials", "Select a preset transposition value of the slave oscillator. The transposition ratio will be displayed as multiples of the frequency of the master oscillator. Range 1:32 to 32:1."});
        m.params.add ({"Detune", "Set the detune of the slave oscillator, in semitone steps. The tuning will relate to the tuning of the master oscillator connected to the Mst input. The preset value is 0 (1:1). If you select the Semitone display, a second number inside a parenthesis will indicate a musical interval (a fifth, a seventh etc.)."});
        m.params.add ({"Fine", "Fine-tunes the slave oscillator. The range is +/- half a semitone, divided in 128 increments. Click on the triangle above the control to reset the fine-tuning to 0, which is the preset value."});
        m.params.add ({"PW modulation input [Type ITYPE1]", "This is a red input for modulating the width of the Pulse waveform, starting at the initial width set with the PW control. The modulation amount is determined by the rotary knob next to the input."});
        m.params.add ({"PW", "Sets the initial width of the two periods of a Pulse waveform. The range is from 1% to 99%. Click on the triangle above the knob to reset the initial pulse width to 50%, which is the preset value."});
        m.params.add ({"M", "Click on this button to mute the audio output of the oscillator."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Oscillator slave C ──
        m = ModuleHelp();
        m.name = "Oscillator slave C";
        m.description = "This slave oscillator produces a Sawtooth wave. It has a FMA modulation input.";
        m.params.add ({"Mst input", "The gray control input from a master oscillator. If you connect a master LFO to this input, the slave oscillator will track the LFO five octaves above the pitch of the LFO."});
        m.params.add ({"Display box", "Click on the info window to switch between Semitones, Hz and Ratio. Range: C-1 to G9 (7.94 Hz to 12910 Hz or x 0.0241 to x 39.152)."});
        m.params.add ({"Partials", "Select a preset transposition value of the slave oscillator. The transposition ratio will be displayed as multiples of the frequency of the master oscillator. Range 1:32 to 32:1."});
        m.params.add ({"Detune", "Set the detune of the slave oscillator, in semitone steps. The tuning will relate to the tuning of the master oscillator connected to the Mst input. The preset value is 0 (1:1). If you select the Semitone display, a second number inside a parenthesis will indicate a musical interval (a fifth, a seventh etc.)."});
        m.params.add ({"Fine", "Fine-tunes the slave oscillator. The range is +/- half a semitone, divided in 128 increments. Click on the triangle above the control to reset the fine-tuning to 0, which is the preset value."});
        m.params.add ({"FMA modulation input [Type IITYPE2]", "A red modulation input where a signal will affect the pitch of the oscillator in a linear way. The FM amount is attenuated with the rotary knob next to the input."});
        m.params.add ({"M", "Click on this button to mute the audio output of the oscillator."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Oscillator slave D ──
        m = ModuleHelp();
        m.name = "Oscillator slave D";
        m.description = "This slave oscillator produces a Triangle wave. It has an FMA modulation input.";
        m.params.add ({"Mst input", "The gray control input from a master oscillator. If you connect a master LFO to this input, the slave oscillator will track the LFO five octaves above the pitch of the LFO."});
        m.params.add ({"Display box", "Click on the info window to switch between Semitones, Hz and Ratio. Range: C-1 to G9 (7.94 Hz to 12910 Hz or x 0.0241 to x 39.152)."});
        m.params.add ({"Partials", "Select a preset transposition value of the slave oscillator. The transposition ratio will be displayed as multiples of the frequency of the master oscillator. Range 1:32 to 32:1."});
        m.params.add ({"Detune", "Set the detune of the slave oscillator, in semitone steps. The tuning will relate to the tuning of the master oscillator connected to the Mst input. The preset value is 0 (1:1). If you select the Semitone display, a second number inside a parenthesis will indicate a musical interval (a fifth, a seventh etc.)."});
        m.params.add ({"Fine", "Fine-tunes the slave oscillator. The range is +/- half a semitone, divided in 128 increments. Click on the triangle above the control to reset the fine-tuning to 0, which is the preset value."});
        m.params.add ({"FMA modulation input [Type IITYPE2]", "A red modulation input where a signal will affect the pitch of the oscillator in a linear way. The FM amount is attenuated with the rotary knob next to the input."});
        m.params.add ({"M", "Click on this button to mute the audio output of the oscillator."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Oscillator slave E ──
        m = ModuleHelp();
        m.name = "Oscillator slave E";
        m.description = "This slave oscillator produces a Sine wave. It has an FMA- and an amplitude modulation input.";
        m.params.add ({"Mst input", "The gray control input from a master oscillator. If you connect a master LFO to this input, the slave oscillator will track the LFO five octaves above the pitch of the LFO."});
        m.params.add ({"Display box", "Click on the info window to switch between Semitones, Hz and Ratio. Range: C-1 to G9 (7.94 Hz to 12910 Hz or x 0.0241 to x 39.152)."});
        m.params.add ({"Partials", "Select a preset transposition value of the slave oscillator. The transposition ratio will be displayed as multiples of the frequency of the master oscillator. Range 1:32 to 32:1."});
        m.params.add ({"Detune", "Set the detune of the slave oscillator, in semitone steps. The tuning will relate to the tuning of the master oscillator connected to the Mst input. The preset value is 0 (1:1). If you select the Semitone display, a second number inside a parenthesis will indicate a musical interval (a fifth, a seventh etc.)."});
        m.params.add ({"Fine", "Fine-tunes the slave oscillator. The range is +/- half a semitone, divided in 128 increments. Click on the triangle above the control to reset the fine-tuning to 0, which is the preset value."});
        m.params.add ({"FMA modulation input [Type IITYPE2]", "A red modulation input where a signal will affect the frequency of the oscillator. The FM amount is attenuated with the rotary knob next to the input."});
        m.params.add ({"AM input", "This red input allows you to modulate the amplitude of the slave oscillator. The amount of this modulation is fixed at a ratio of 1:1."});
        m.params.add ({"M", "Click on this button to mute the audio output of the oscillator."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Oscillator slave FM ──
        m = ModuleHelp();
        m.name = "Oscillator slave FM";
        m.description = "This slave oscillator produces a Sine wave. It has a sync and a special FMB-modulation input. This oscillator is especially suitable for creating classic FM sounds";
        m.params.add ({"Mst input", "The gray control input from a master oscillator. If you connect a master LFO to this input, the slave oscillator will track the LFO five octaves above the pitch of the LFO."});
        m.params.add ({"Display box", "Click on the info window to switch between Semitones, Hz and Ratio. Range: C-1 to G9 (7.94 Hz to 12910 Hz or x 0.0241 to x 39.152)."});
        m.params.add ({"Partials", "Select a preset transposition value of the slave oscillator. The transposition ratio will be displayed as multiples of the frequency of the master oscillator. Range 1:32 to 32:1."});
        m.params.add ({"Detune", "Set the detune of the slave oscillator, in semitone steps. The tuning will relate to the tuning of the master oscillator connected to the Mst input. The preset value is 0 (1:1). If you select the Semitone display, a second number inside a parenthesis will indicate a musical interval (a fifth, a seventh etc.)."});
        m.params.add ({"Fine", "Fine-tunes the slave oscillator. The range is +/- half a semitone, divided in 128 increments. Click on the triangle above the control to reset the fine-tuning to 0, which is the preset value."});
        m.params.add ({"Sync input", "This red input is used for synchronizing the oscillator to a control source, which could be another oscillator, an LFO or the keyboard. Synchronization forces the oscillator to restart its waveform cycle, in sync with the waveform cycle of the controlling device. The oscillator will restart whenever a signal present at the sync input increases from 0 units to anything above 0 units. The pitch of the controlling oscillator will interact with the controlled oscillator pitch. For a traditional synthesizer sync-sound, start with the two oscillators to the same pitch and connect only the sync-controlled oscillator to an output. Turning the Detune knob of the sync-controlled oscillator will produce radical changes in the timbre."});
        m.params.add ({"FMB modulation input [Attenuator Type II]", "A red modulation input where a signal will affect the oscillator pitch creating classic FM-type sounds. The FM amount is attenuated with the rotary knob next to the input."});
        m.params.add ({"#-3 Oct", "Transposes the coarse pitch of the oscillator to three octaves below the master oscillator."});
        m.params.add ({"M", "Click on this button to mute the audio output of the oscillator."});
        m.params.add ({"Out", "Signal: Bipolar."});
        m.params.add ({"$Output modules", "The output modules of Nord Modular route the audio signals to mix buses. Mix buses marked 1-4 correspond to the physical out jacks of the Nord Modular. Mix buses marked CVA L and R or just CVA should be used when you want to route audio signals from the Poly Voice Area to the Common Voice Area of the patch via the Poly Area In module."});
        d.add (m);

        // ── Overdrive ──
        m = ModuleHelp();
        m.name = "Overdrive";
        m.description = "This module distorts an audio signal by amplifying the input signal and force it to hit the headroom. The special amplification characteristics makes this module produce a warm distortion.";
        m.params.add ({"Modulation input [Type ITYPE1]", "Connect a modulator to this blue input. The amount of modulation is attenuated with the knob."});
        m.params.add ({"Overdrive", "Sets the initial overdrive amount."});
        m.params.add ({"Graph", "Displays the initial overdrive amount graphically. The Y-axis represents the output signal values, and the X-axis the input signal values."});
        m.params.add ({"In", "The red audio input."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Pan ──
        m = ModuleHelp();
        m.name = "Pan";
        m.description = "This module takes a signal and places it somewhere in a stereo panorama.";
        m.params.add ({"Pan modulation input [Type ITYPE1]", "The red modulation input of the Pan module. Connect a modulator to modulate the position of the signal in the two outputs. The amount of modulation is controlled with the knob."});
        m.params.add ({"L/R knob", "Sets the initial pan position. Click on the triangle to reset the initial position to an equal amount of the incoming signal at both the outputs."});
        m.params.add ({"Input", "The red audio input."});
        m.params.add ({"L/R outputs", "Signal: Bipolar."});
        d.add (m);

        // ── Panel reference ──
        m = ModuleHelp();
        m.name = "Panel reference";
        m.description = "nobs and buttons of Nord Modular";
        m.params.add ({"Master Volume", "Adjust the master volume of Nord Modular with this knob. Note that this knob cannot be routed to MIDI Volume or any other controller. It is separated from the rest of the parameters of the synth. To affect the volume of a patch from MIDI, you have to assign a controller to any of the Level rotary knobs of the output modules."});
        m.params.add ({"Panel Split", "Pressing the panel split button splits the 18 knobs in four groups, one for each of the Slots a, b, c and d. The a and the b slot will be assigned to six knobs each, the c and d slot will be assigned to three. The first six or three assigned knobs of each patch will be rerouted to the knobs of the different groups. For example, a patch in slot c that originally had parameters assigned to knobs 1-3, will now have them reassigned to knobs 13-15. The Panel Split knob assignment is only temporary for as long as panel split is active. The original knob assignment in the patch will not be changed."});
        m.params.add ({"Find/Panic", "When a parameter is assigned to a knob, the indicator next to the knob is lit. Pressing and holding find and turning the knob will show the parameter in the display. The arrows that appears will help you to set the physical position of the knob to coincide with the actual setting of the parameter in the patch. As long as you hold down the find button, the value of the parameter will not change. If Nord Modular is in Edit mode, the parameter will stay in focus in the display when you release the find button. Pressing shift+find activates the panic function. This will send an internal MIDI Note Off message to all the voices in Nord Modular. Sound sources (oscillators, for example) that are connected directly to an output module will not be affected by the panic function."});
        m.params.add ({"Oct shift (eyboard version only)", "You can quickly transpose the Nord Modular keyboard in octave steps with the oct shift buttons. The total range of the two-octave keyboard can be extended to six octaves. You can transpose the patches in the four slots individually. This transposition is saved with the other parameters of a patch. The oct shift buttons are an exclusive feature on the keyboard version only. The MIDI output from Nord Modular is also affected by the oct shift buttons."});
        m.params.add ({"MIDI Trig LED (rackmount version only)", "The midi trig led indicates incoming MIDI messages on the MIDI channels Nord Modular is set to receive on. nobs The 18 knobs can be assigned to parameters in a patch. The parameters will then be controllable in real time. When a knob is assigned the knob led is lit. The knobs can also be set to send MIDI controller messages. If you want a knob to control external MIDI devices, without affecting any parameter in Nord Modular, assign the knob to a parameter in a \"dummy\" module (that is not a part of the sound in the patch) and assign that parameter to the MIDI Controller you wish to transmit."});
        m.params.add ({"Slot buttons", "Pressing the slot buttons a, b, c and/or d selects the slots for loading, editing or playing. A patch always has to be loaded into a slot before it can be used. If you select only one slot at a time, the slot led will indicate this by a flashing green light. The Editor software will follow the slot selection (change patch window), provided that the patch is uploaded or opened in the Editor. If several slots are selected, by simultaneously pressing several slot buttons, a solid green LED will indicate a selected slot and a flashing LED will indicate the currently active slot. Every selected slot will receive MIDI messages on their set channels, but only the active slot will transmit MIDI messages. To deselect slots in a multi-slot setup, press shift and the slot(s) you want to deselect. To deselect all slots simultaneously, you can also press an unused slot button (if there are any). However, the unused Slot will become active. It is also possible to play several Slots simultaneously (layered) from the keyboard. To do that you must first change keyboard mode to \"Selected Slots\" in \"eyboard mode\" of the Synth Settings menu. On the Nord Modular rack model this function works only when playing from the eyboard Floater in the Editor. To play several slots layered on the rack model from a master keyboard, the slots have to be set to the same MIDI channel."});
        m.params.add ({"Shift", "The shift button adds secondary functions to other buttons."});
        m.params.add ({"Assign/Morph", "The assign/morph button allows you to assign a parameter in a patch to be controlled by one of the knobs. Press and hold assign while the desired parameter is in focus and turn a knob. Nord Modular must be in edit mode when you make a knob assignment. To deselect a knob assignment, turn the knob while pressing shift. shift+assign allows you to edit the Morph range of the parameter in focus (if it is assigned to a Morph group). Edit the Morph range with the rotary dial."});
        m.params.add ({"Navigator buttons", "The navigator buttons can be used to select patches and banks or menus and parameters in the menus. When Nord Modular is in Load mode (Patch/Load LED flashing), you select patches with the left and right navigator buttons, and change banks with the up and down navigator buttons. When Nord Modular is in Edit mode, you select parameters within a module by scrolling with the left and right navigator buttons, and between the toolbar, PVA and CVA sections with the up and down navigator buttons. Pressing the shift+navigator buttons sets the focus on different modules in a patch, on screen as well as in the display."});
        m.params.add ({"Rotary dial", "The rotary dial is used to enter parameter values. Pressing either the left or right navigator button while turning the rotary dial will scroll through the parameters of a module, one parameter for each step of the rotary dial. Scroll through the menus in Nord Modular by using the up an down navigator buttons together with the rotary dial."});
        m.params.add ({"Store", "There are 891 patch memory locations and one Synth Settings memory in the Nord Modular. The internal patch memory is divided into 9 Banks, each with 99 patch locations. Generally, there is not enough room for 891 patches in the internal memory at the same time.The number of patches that can be stored in the internal memory depends on the patch sizes. Typically, several hundreds of patches fit in the internal memory. The store button allows you to store patches and Synth Settings. Activate a slot with a patch by pressing the slot button. If the edit or system led is lit, press the patch/load button once. You can not store any patches if Nord Modular is in System or Edit mode. Press store once. The store led will flash. Select a Bank with the up/down navigator buttons. Select a memory location with the rotary dial or with the left/right navigator buttons and confirm by pressing store one more time. Pressing any other button will cancel the operation. The Memory Protect function must be turned Off before storing a patch. Click here14E1O94 for info on how to disable the Memory Protect function. If a memory location is occupied by a patch, you will see the name of that patch in the display. Pressing store will overwrite an existing patch."});
        m.params.add ({"Storing Synth Settings", "Pressing shift+store will store any settings that you have made in the Synth Settings menus of Nord Modular or in the Editor. This function saves the slot selection including the patch locations that were loaded to the slots, MIDI channels for the slots and panel split activation. The settings saved with save synth settings will automatically be recalled when you turn on the Nord Modular."});
        m.params.add ({"System button", "Press the system button below the display, to activate the System menu. Scroll to the desired menu with the navigator buttons and enter a value with the rotary dial. The System menu is divided in two main parts, Synth and Patch settings. Every parameter in these two parts are automatically duplicated in the Editor. The Synth settingsSynth_settings are global, affecting the whole instrument. If you want to keep any changes that you make in Synth settings, they must be stored in the Synth Settings memory by pressing the shift and the store buttons simultaneously. If you use Nord Modular in a multitimbral setup with a sequencer, it can be very useful to store the settings of the MIDI channels etc. in the Synth Settings memory. The Patch settingsPatch_settings affect the patches in the slots individually and are stored together with the rest of the patch data when you store the patch in the Nord Modular internal memory or on a computer disk from the Editor."});
        d.add (m);

        // ── Edit ──
        m = ModuleHelp();
        m.name = "Edit";
        m.description = "With the edit button you put Nord Modular in Edit mode. In this mode you can edit the functions and parameters of a patch without having the Editor running or the computer connected to your Nord Modular. The module and its parameters will appear in the display and you can edit them with the rotary dial. You navigate within a module with the left and right navigator buttons. You can jump to other modules in the patch by pressing and holding the shift button and navigating with all four navigator buttons. Note that the navigation between the modules in a patch depends on how the modules were placed graphically in the Editor patch window. Press the up and down navigator buttons to jump between the Poly Voice Area, the Common Voice Area and the Morph group section (in the Editor toolbar). In the upper right corner of the Display is shown which section you're currently in: T means Toolbar, P means Poly Voice Area and C means Common Voice Area. The image on the screen, provided that the Editor is connected, will follow the navigator buttons and vice versa. If you select a parameter with the mouse, the display on Nord Modular will echo that selection.";
        m.params.add ({"Patch/Load", "Press the patch/load button to put Nord Modular in Patch Mode. This button is also used when you want to load a patch from the internal memory to a slot. The display will indicate the name and the number of voices assigned to the current patch. By pressing shift while turning the rotary dial, you can change the number of requested voices of the active slot. This is especially useful in multitimbral setups. To load a patch into a slot, do as follows: 1. Select the slot in which you want to load a patch by pressing a slot button. 2. Press the patch/load button. The patch/load led will flash. A memory location number and the patch name will appear in the display. 3. Select the desired patch to load with the rotary dial or with the left/right navigator buttons and load the patch to the slot by pressing the patch/load button again. To switch between entire patch banks, you can also use the up/down navigator buttons. If Nord Modular is in Patch Mode (the patch/load led lit), turning the rotary dial will automatically present the memory locations and patch names in the display. Loading a patch will also send a MIDI Bank Change (Controller #32) and Program Change message on the midi out port, if the Program Change function in Synth Settings is active. You can also load a patch from a memory location to a slot by sending a MIDI Bank Change and Program Change message to Nord Modular. You have to send on the MIDI channel assigned to the slot. nobs and buttons of Nord Micro Modular"});
        m.params.add ({"Shift", "The Shift button is used to access secondary functions of some of the other nobs and buttons of Micro Modular."});
        m.params.add ({"Volume", "Adjust the master volume with the Volume knob Note that this knob cannot be routed to MIDI Volume or any other controller. It is separated from the rest of the parameters of the synth. To affect the volume from MIDI you have to assign a controller to any of the Level rotary knobs of the output modules."});
        m.params.add ({"1/Master Tune", "The first of the 3 knobs that can be assigned to a parameter in a patch. The parameter will then be controllable in real time. The knobs can also be set to send MIDI controller messages. If you want a knob to control external MIDI devices, without affecting any parameter in Micro Modular, assign the knob to a parameter in a \"dummy\" module (that is not a part of the sound in the patch) and assign that parameter to the MIDI Controller you wish to transmit.. If Shift is pressed, the knob is used to change the master tune (+/- 1 semitone in steps of 1%)."});
        m.params.add ({"2/MIDI Channel", "The second of the 3 knobs that can be assigned to a parameter in a patch. If Shift is pressed, the knob is used to change the MIDI Channel (1-16)."});
        m.params.add ({"3/Patch Select", "The third of the 3 knobs that can be assigned to parameters in a patch. If Shift is pressed, the knob is used to select patches from the internal memory. Selecting a patch will also send a MIDI Bank Change and Program Change message on the MIDI Out port, if the Program Change function is active. You can also load a patch from a memory location by sending a MIDI Bank Change and Program Change message to Micro Modular. You have to send on the selected MIDI channel"});
        m.params.add ({"Display", "The Display shows the current patch number. A red flashing dot at the bottom between the numbers indicates that the Sound engine is re-calculating and therefore does not produce any sound. The recalculation occurs when you add or remove modules in the patch window in the PC Editor. A red flashing dot to the bottom right in the display indicates that MIDI data is received on the MIDI In port."});
        m.params.add ({"Patch Increment/Note Trig", "Use this button to select (increment) patches from the internal memory. Selecting a patch will also send a MIDI Bank Change and Program Change message on the MIDI Out port, if the Program Change function is active. You can also load a patch from a memory location by sending a MIDI Bank Change and Program Change message to Micro Modular. You have to send on the selected MIDI channel. If shift is pressed, you can use this button to trig a note. Great for checking the sound of a patch without having a master keyboard connected."});
        m.params.add ({"Patch Decrement/4", "Use this button to select (decrement) patches from the internal memory. Selecting a patch will also send a MIDI Bank Change and Program Change message on the MIDI Out port, if the Program Change function is active. You can also load a patch from a memory location by sending a MIDI Bank Change and Program Change message to Micro Modular. You have to send on the selected MIDI channel. If shift is pressed, you can use this button to control an assigned parameter in a patch, similar to the assignable knobs described above."});
        d.add (m);

        // ── Partial generator ──
        m = ModuleHelp();
        m.name = "Partial generator";
        m.description = "This module generates a control signal that will transpose an OSC to one of its harmonic partials. The range of the partial generator is 0 to +/- 64 partials in steps of 0.5 partials. Note that the practical limit of Nord Modular is +/- 32 partials. If the range is set above +/- 32 partials, the oscillator will remain on its 32nd partial until the control signal amplitude has decreased below +/- 32 partials.";
        m.params.add ({"Range display box", "Displays the control signal range set with the attenuator knob. For the display to show the correct range, it is assumed that the input signal uses its whole dynamic range (+/- 64 units). Values exceeding +/- 32 partials are shown with an asterisk, indicating that the practical output limit is exceeded."});
        m.params.add ({"Range knob", "Set the control signal range. Range +/- 64 units."});
        m.params.add ({"In", "The blue control signal input."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Patch ──
        m = ModuleHelp();
        m.name = "Patch";
        m.description = "Patch settings... Opens up the Patch Settings dialog box. (These functions are also available for editing if you press the system button and select the <PATCH> settings on Nord Modular).";
        m.params.add ({"Voices", "Set the Requested polyphony of the patch by clicking on the up/down buttons. Nord Modular will always try to assign as many voices as you request. The Current number of assigned voices is displayed below the Requested voices, in the Editor toolbar and in the Nord Modular display (not Micro Modular). Click 'Get current notes' to save the current notes sounding in the patch. This can be used to ensure that a patch with e.g. a continuous drone (\"infinite\" sound) will sound with the desired notes whenever it is downloaded. This information is saved automatically if you store a patch in the internal memory of the synthesizer. If you save a patch only in the Editor, these notes have to be requested from the synthesizer. If you have clicked 'Get current notes', the notes will be saved with the patch and transmitted to the synthesizer when the patch is opened in the Editor."});
        m.params.add ({"Velocity range", "With this function you define a velocity range for a patch. Set the lowest and highest velocity values. The patch will only sound when it receives velocity values within this range. eyboard range This function is used to define a keyboard range for a patch. Set the lowest and the highest key. The patch will only recognize keyboard information when you play within this range."});
        m.params.add ({"Pedal mode", "Select if a footswitch connected to the sustain pedal input on the rear panel of Nord Modular should act as a Sustain pedal or as an On/Off switch. Note! If you use it as an on/off switch, you have to assign the switch to a module parameter in a patch."});
        m.params.add ({"Bend range", "Here you set the range of the incoming pitch bend data in Semitones. The pitch bend data will be added to the control signals from the Note outputs on the eyboard modules and to the BT (keyboard tracking) parameter. The range is from 0 to 24 semitones. LFOs and filters that are using the BT parameter will be affected by the incoming pitch bend data as well. Bend range can also be set directly in the Editor toolbar."});
        m.params.add ({"Portamento", "Here you select Portamento mode and Time. Portamento is an effect where the notes slide from one note to the next when you play consecutive notes on the keyboard. The portamento has two modes: Normal and Auto. In the Normal mode the portamento is always active, in the Auto mode you activate the portamento by playing legato. With the Time parameter you set the time it should take to reach the new note. The portamento function is available only if a patch is set to 1 voice, i.e monophonic. The Portamento function is always active on any keyboard tracking oscillators used in the Common Voice Area, since this is a monophonic area. Portamento can also be set directly in the Editor toolbar. Portamento can also be achieved using any of the Portamento modules. Using these modules, the portamento can be polyphonic as well."});
        m.params.add ({"Octave shift", "Here you select the octave setting of the patch. This information is saved with the rest of the patch data in the patch. Note that this setting is active only if you play the patch from a Nord Modular keyboard version."});
        m.params.add ({"Voice retrig", "Place a check in the Voice retrig boxes if you want Nord Modular to retrig notes when playing a monophonic patch. In practice this means that if you hold down two keys and release one of them, the other key will retrig. The retrig function is available only if a patch is set to 1 requested voice, i.e monophonic. You can select retrig for the Poly and Common Voice Areas of the patch individually."});
        m.params.add ({"Resources used", "At the bottom of the Patch settings dialog box, you can view information on how much Sound engine resources the patch is using. The first figure corresponds to the Poly Voice Area and the second to the Common Voice Area. In most cases the Cycles information is the most interesting since this is the type of resource often used up first. The PVA Cycles information is the same as the PVA Load information in the patch window toolbar, and the PVA Cycles + CVA Cycles information is the same as the S Load information in the toolbar. This information is very useful when calculating the maximum number of available voices for a patch. ok Click O to activate the patch settings and close the dialog box"});
        m.params.add ({"Download To Slot", "Brings up a dialog box in which you can choose to download the currently active Editor patch to a slot in the connected synthesizer(s)."});
        m.params.add ({"Save In Synth", "This function lets you save the patch of the active or a selected slot in any of the synthesizer's memory locations. Select Bank and Patch location from the drop-down menus. Click Save to execute and exit the dialog box, or Cancel to exit without saving."});
        d.add (m);

        // ── Patch connections ──
        m = ModuleHelp();
        m.name = "Patch connections";
        m.description = "Inputs and outputs There are two types of main connectors on the modules in the Nord Modular Editor: inputs and outputs. The inputs have circular, and the outputs have square connectors.";
        m.params.add ({"The connection colors", "There are four different types of connectors that are used for different signals. These connectors are distinguished by different colors: audio signal connectors: Red (96 kHz sampling frequency) control signal connectors: Blue (24 kHz sampling frequency) logic signal connectors. Yellow (24 kHz sampling frequency) slave signal connectors: Gray (24 kHz sampling frequency)"});
        m.params.add ({"Make a connection", "Place the cursor on a module connector and click-hold. The cursor will change to a plug. Drag the cursor to a suitable connection elsewhere in the patch. As you drag the cursor away from the source connector, a line will appear between the cursor and the connector. When you reach the destination connector, the cursor will change to a cable with a white dot instead of a plug. As you release the mouse button, a cable will appear between the two connections. The color of the output connection will determine the color of the resulting cable. You can later change the cable color if you like. It is also possible to connect cables between connectors of different colors, e.g. connect an audio signal output to a control signal input etc. This depends on the actual application. If a connection is not possible to make, this will be shown; the cursor will not change to a cable with a dot as you reach the \"illegal\" destination connector. It is not possible to damage the system in any way by connecting \"wrong\" - feel free to experiment! You can connect one output to several inputs to make a branch connection. You can also make a serial connection, from input to input, provided that the first input in the chain is connected to an output. The result is exactly the same as in a branch connection. If a module within a serial cable chain is removed, the remains of the cable chain will be re-routed. It is also possible to make a serial connection between several inputs, without connecting to an output. This won't result in any signal flow, but can be useful if you want to choose an output after having connected all inputs. Such \"non-functional\" input-to-input connections are indicated by white cable color. When you connect such a chain to an output, the cable color will change to the output's color. It is also possible to combine branch and serial connections in several ways. For example, you could have a serial connection branch off anywhere in the chain."});
        m.params.add ({"Disconnecting a cable", "To remove a cable, right[PC]/Ctrl[Mac]-click on a connection (input or output) and select Disconnect, or double-click-hold or Ctrl[PC]/Alt[Mac]-click (left mouse button) on a connection (an extra wire appears next to the connector cursor) and \"pull out\" the connector by dragging the connector symbol away from the input/output and release the mouse button."});
        d.add (m);

        // ── Patch settings ──
        m = ModuleHelp();
        m.name = "Patch settings";
        m.description = "These functions affect the individual patches loaded into the slots of Nord Modular. Select a patch to edit by loading it into a slot. The changes you make in a patch will be stored/saved in Nord Modular or in the computer together with each patch after having selected store or Save. (Note that it is possible to jump between the four slots in any of the sub menus if you wish.)";
        m.params.add ({"Voices", "Set the requested polyphony of the selected patch. Nord Modular will always try to assign as many voices as you request. The current actual number of voices assigned is displayed to the bottom right in the display. Note that voices will only be assigned to the selected slot(s) (slot led lit or flashing)."});
        m.params.add ({"Ctrl Snap Shot", "In this menu you can choose to send a snapshot of the current values of all assigned MIDI Controllers in the active patch. This is very useful if you are recording in a sequencer program and want to make sure the sound sounds exactly as you want. Press the right navigator button to send the Controllers. The snapshot is sent on the midi out of the synthesizer, not on the pc out."});
        m.params.add ({"Voice Retrig", "Here you can select if you want Nord Modular to retrig notes when playing a monophonic patch. In practice this means that if you hold down two keys and release one of them, the other key will automatically retrig. You can select retrig for the Poly and Common Voice Areas of the patch individually."});
        m.params.add ({"Patch Name", "With this function you can name a patch. Select characters with the rotary dial and change the \"cursor\" position with the left/right navigator buttons. The memory locations in Nord Modular are identified by the location number, not the patch-name. You can name all your patches \"MyBestSound\" if you like, as long as you don't ask us to sort them out for you later..."});
        m.params.add ({"Bend Range", "Here you set the range of the incoming pitch bend data in semitones. The pitch bend data will be added to the control signals from the Note outputs on the eyboard modules and to the BT (keyboard tracking) function. The range is from 0 to 24 semitones if BT is set to 1. LFOs and filters using the BT parameter will be affected by incoming pitch bend data as well. eyb Range This function is used to define a keyboard range for a patch. Set the lowest and the highest note to respond to note information. The patch will only receive keyboard information when you play within this range. If you want to use Nord Modular in a keyboard split situation, select two slots, make sure that the eyboard Mode is set to Selected Slots, and set the actual split point with the high key for one of the slots and the low key for the other one."});
        m.params.add ({"Vel Range", "With this function you define a velocity range for a patch. Set the lowest and highest velocity values. The patch will only sound when it receives velocity within this range. If you want to use Nord Modular to switch between two or more patches that are set to receive on the same MIDI channel (velocity switching), select two slots, set them to the same MIDI channel, make sure that the eyboard Mode is set to Selected Slots, and set the velocity range individually for the patches."});
        m.params.add ({"Portamento", "Portamento is an effect where the notes slide from one note to the next when you play consecutive notes on the keyboard. With the Time parameter you set the time it will take to reach the new note. The portamento has two modes: Normal and Auto. In the Normal mode the portamento is always active, in the Auto mode you activate the portamento by playing legato. This portamento function is available only if a patch is set to 1 voice, i.e. monophonic. The Portamento function is always active on any keyboard tracking oscillators used in the Common Voice Area, since this is a monophonic area. Portamento can also be achieved using the Portamento modules described on page 169. Using the modules, the portamento can be polyphonic as well."});
        m.params.add ({"Pedal Mode", "Select if a footswitch connected to the sustain pedal input on the rear panel should act as a sustain pedal or as an on/off switch. If you use it as an on/off switch, you have to assign the switch to a module parameter in a patch."});
        d.add (m);

        // ── Patches ──
        m = ModuleHelp();
        m.name = "Patches";
        m.description = "New patches must be created in the Editor. You can, however, store several hundreds of patches in the Nord Modular internal memory (not Micro Modular, which has 99 memory locations) and play these patches without having the Editor running or even the computer connected. If Nord Modular is in Edit mode (the edit button pressed), you can edit a patch by navigating among the parameters with the navigator keys, and adjusting the values with the rotary dial (not Micro Modular). There are special functions that apply to each patch individually, regardless of which slot it occupies. These functions are described in Patch settingsPatch_settings. poly and common voice areas A Nord Modular Patch can consist of two parts: one part that affects each voice in a patch separately, and one part that affects the sum of all voices in a patch. In the Editor, these two parts are represented by two sections of the patch window. The upper section is called the Poly Voice Area and the lower section the Common Voice Area. In the Poly Voice Area you place modules that should be duplicated for each voice, e.g. oscillators, envelope generators, filters etc. In the lower section, the Common Voice Area, you can place modules that should act equally on all voices in the patch, e.g. different types of Audio modules etc. Modules used in the Common Voice Area will act on the sum of the signals output from the Poly Voice Area, and consequently will not be duplicated for each voice in the patch. This gives two big advantages: A module is able to process whole chords, and not just a single voice, producing effects the same way as external effects processors. In most situations you will be able to free up Sound engine power (Load) so you could increase the polyphony of the patch. Cables cannot be connected from modules in one patch window to modules in the other. The only signals that can be routed from the Poly Voice Area to the Common Voice Area are two separate audio signals. The routing is one-way only; from the Poly Voice Area to the Common Voice Area. See an example on page 45 of how to use the two patch sections. create a new patch Create a new patch by selecting File|New. Select a slot in the dialog box that appears and click OK. This opens up a new, empty patch window in the Editor and clears the selected slot in Nord Modular. You can also choose not to select any slot by selecting Local. This means that you work \"off-line\", i.e. you cannot play the patch, only edit. You may then later download the patch to the synthesizer. download a patch to the synthesizer If you selected Local in the example above, you can easily download the Editor patch to the synthesizer by doing either of the following: right[PC]/Ctrl[Mac]-click on the patch window background and select a slot from the bottom of the popup. This will download your Editor patch, overwriting the patch that is currently in the destination slot. Select Patch|Download To Slot and select slot in the dialog box that appears. store a patch A patch can be stored in two different locations: in the internal memory of the synthesizer, and/or on disk on the computer. The examples below describes three different ways of saving/storing a patch. 1. Save a patch only on the computer by selecting File|Save. File|Save As will let you rename the patch before saving to disk. File|Save All will save all open Editor patches. Note! patches that are saved from a PC Editor automatically gets the extension '.pch'. For patches saved in a Mac Editor to be readable in a PC Editor, you must manually type in the extension '.pch' in the file name. 2. Store a patch in one of the Nord Modular internal memory locations by pressing the store button on the front panel once. The led above the store button will flash. Select a bank (1-9) with the up/down navigator buttons and a memory location (1-99) with the rotary dial. Confirm by pressing store again. Abort by pressing any other button. (This example is not valid for Micro Modular.) 3. To store a patch in Nord Modular internal memory from the Editor, select Tools|Browser. Right[PC]/Ctrl[Mac]-click on a patch in the Flash Memory tab and select 'Save Slot X To:...'. Note that the original patch in the selected memory location will be overwritten by your new patch. Make sure you do not overwrite patches you want to keep! There is a memory protect function to minimize the risk of accidentally overwriting patches (not in Micro Modular). Read more about the internal memory protection in the section Synth SettingsSynth_settings.";
        d.add (m);

        // ── Pattern generator ──
        m = ModuleHelp();
        m.name = "Pattern generator";
        m.description = "The Clocked Pattern Generator generates 16384 different patterns (128 banks with 128 patterns each) with selectable length.";
        m.params.add ({"Clk", "The yellow logic Clk input can be connected to a Clk output of an external module, such as the Clock generator. At each clock pulse the Pattern generator advances one step."});
        m.params.add ({"Rst", "The yellow logic Rst input is used to restart the selected pattern. When receiving a logic restart signal, the Pattern generator restarts at step one. The restart isn't performed until the next the clock pulse is received at the Clk input. This guarantees perfect timing."});
        m.params.add ({"Pattern and Bank display boxes", "Displays selected Pattern and Bank number."});
        m.params.add ({"Pattern and Bank", "With the rotary buttons you select Pattern (0-127) and Bank (0-127). Pattern and Bank selection can also be controlled externally using the blue control signal input to the left of the knobs."});
        m.params.add ({"Step display box", "Displays the selected number of Steps in the pattern(s)."});
        m.params.add ({"Step", "Click on the up and down arrow to select number of steps in the pattern (1-128)."});
        m.params.add ({"Mono", "Synchronizes modules in polyphonic patches to each other. This means that if you play a chord, the module will control all voices in sync."});
        m.params.add ({"Delta", "Click to select High or Low. High Delta value produces great difference in output level between steps, and Low Delta produces low difference. The default setting is High."});
        m.params.add ({"Out", "Signal: Unipolar"});
        d.add (m);

        // ── Perc oscillator ──
        m = ModuleHelp();
        m.name = "Perc oscillator";
        m.description = "This is an oscillator that generates percussive sounds. The amplitude and pitch of the sound can be modulated from external sources.";
        m.params.add ({"Amp input", "A signal at this audio input modulates the amplitude of the sound. The attenuation is fixed at a 1:1 ratio."});
        m.params.add ({"Trig input", "Use this input to trig the sound. Any signal that increases from 0 units or less, to anything above 0 units will trig the sound. The red color of the input indicates that it also accepts full audio frequency signals."});
        m.params.add ({"Click", "With the Click knob you can add a clicking sound to the attack of the sound."});
        m.params.add ({"Decay", "Sets the decay time of the sound."});
        m.params.add ({"Display box", "Click on the display box to switch between notes and Hz. Range: C-1 to G9 (7.94 Hz to 12910 Hz)."});
        m.params.add ({"Pitch", "Sets the coarse pitch of the oscillator. Range: C-1 to G9 (8.18 Hz to 12540 Hz)."});
        m.params.add ({"Fine", "Sets the fine tuning of the oscillator. The range is +/- half a semitone divided into 128 steps. Click on the triangle above the control to reset the fine tuning to 0, which is the preset value."});
        m.params.add ({"Pitch modulation input [Type II]", "There is a blue modulation input for modulating the oscillator pitch on this module. The modulation amount is attenuated with the rotary knob next to the input."});
        m.params.add ({"Punch", "Changes the characteristics of the sound and adds a more distinct attack."});
        m.params.add ({"M", "Click on this button to mute the audio output of the oscillator."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Phaser ──
        m = ModuleHelp();
        m.name = "Phaser";
        m.description = "This is a 14 pole phaser with peak spread control and adjustable feedback. It features six allpass filters which displace the phase 180 degrees each. It is possible to select the number of allpass filters (1-6) to be used, giving from one to six peak resonance frequencies.";
        m.params.add ({"Rate display box", "Displays the internal LFO rate in Hz or seconds. Range: 62.9 s to 24.4 Hz."});
        m.params.add ({"Rate", "With the Rate knob you set the rate of the built-in LFO. Range: 62.9 s/cycle to 24.4 Hz."});
        m.params.add ({"Depth", "With the Depth rotary knob you set the LFO depth. A high depth amount results in greater changes of the phaser effect."});
        m.params.add ({"LFO on/off", "Below the Depth knob is the LFO on/off button. Use it to switch on and off the LFO."});
        m.params.add ({"Center Freq display box", "Displays the set center frequency in Hz. Range 100 Hz to 16 kHz."});
        m.params.add ({"Center Freq", "With the big Center Freq rotary knob you set the center frequency. Range 100 Hz to 16 kHz."});
        m.params.add ({"Center Freq modulation input", "The center frequency can be controlled externally using the blue control signal input and the attenuator."});
        m.params.add ({"FeedBk", "With the Feedback rotary knob you set the phaser feedback, i.e the feedback to the allpass filters. You can have a negative or positive feedback. At the 12 o'clock position feedback is zero. Click on the green triangle above the knob to set the feedback to 0."});
        m.params.add ({"Peaks", "By clicking on the up and down arrow buttons you select the number of resonance peaks (allpass filters). 1 to 6 peaks can be selected."});
        m.params.add ({"Graph", "Displays the selected phaser characteristics graphically. The Y-axis represents level and the X-axis the frequency."});
        m.params.add ({"Spread", "With the Spread rotary knob you set the distance between the peaks."});
        m.params.add ({"Spread modulation", "The peak distance can be modulated from an external source using the blue control signal input and the level attenuator below the Spread knob."});
        m.params.add ({"Input", "To the bottom right of the module is the red audio input together with an input level attenuator [Attenuator Type I]."});
        m.params.add ({"B", "Click to bypass the signal and leave it unaffected."});
        m.params.add ({"Output", "The audio signal output. The LED above indicates the output level and have the following meaning: Green: normal signal level, Yellow: signal reaching headroom, Red: overload. Signal: Bipolar"});
        d.add (m);

        // ── Poly Area In ──
        m = ModuleHelp();
        m.name = "Poly Area In";
        m.description = "This module should be used when you want to route audio signals from the Poly Voice Area to the Common Voice Area. The PolyAreaIn module can only be used once, and only in the Common Voice Area of the patch window. Since the PolyAreaIn module processes the sum of all voices from the Poly Voice Area, the volume depends on the number of notes you play simultaneously. The input meters indicate the level of the incoming signals. The 0dB indication on the module indicates the headroom limit of the Nord Modular system. Signals above 0dB will cause the system to clip. If you feel the volume is too low, click the '+6dB' button to amplify the input signals by 6dB.";
        m.params.add ({"#+6dB", "Click to amplify the input signals by 6dB."});
        m.params.add ({"Outputs L/R", "From these outputs you can patch signals, from the CVA L and R mix buses of any of the Output modules described below, to modules in the Common Voice Area. Signal: Bipolar."});
        d.add (m);

        // ── PortamentoA ──
        m = ModuleHelp();
        m.name = "PortamentoA";
        m.description = "This module can provide a smooth, gliding transition between the values of a incoming control signal. The transition is activated by a high logic signal at the On input.";
        m.params.add ({"Time", "Set the transition (glide) time with the slider. Range: 5.3 to 1355 ms."});
        m.params.add ({"On", "Patch a high logic signal here to activate the gliding transition between the input signal levels. If no connection is made, the portamento will be constantly active."});
        m.params.add ({"In", "The blue input of the PortamentoA module."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── PortamentoB ──
        m = ModuleHelp();
        m.name = "PortamentoB";
        m.description = "This module can provide a smooth, gliding transition between the values of a control signal. The transition can be temporarily interrupted by a high logic signal at the Jmp input.";
        m.params.add ({"Time", "Set the transition (glide) time with the knob. Range: 5.3 to 1355 ms."});
        m.params.add ({"Jmp", "Patch a high logic trig signal to this yellow input to temporarily interrupt the portamento transition, leaving the signal unaffected. If no connection is made, the portamento will be constantly active."});
        m.params.add ({"In", "The blue input of the PortamentoB module."});
        m.params.add ({"Out", "Signal: Bipolar. To make a legato introduced portamento, patch the Patch gate from the eyboard Patch module to the Jmp input of the Portamento module. Make sure that the BT function is turned off on oscillators that you are using. Patch the Note output from the eyboard module to the input of the Portamento module, and from the output of the Portamento module to the pitch-mod input of the oscillators that you wish to control with the keyboard. Set a desired portamento time with the slider and activate the portamento by playing the keyboard in a legato fashion."});
        d.add (m);

        // ── Positive edge delay ──
        m = ModuleHelp();
        m.name = "Positive edge delay";
        m.description = "This module will delay the positive edge of a logic signal. Set the delay time with the slider. The negative edge of the logic signal will not be affected.";
        m.params.add ({"Display box", "Displays selected delay time. Range: 1.0 ms to 18 s."});
        m.params.add ({"Time knob", "Set the desired delay time with the knob. The module will not transmit a delayed positive edge if the incoming signal switch to zero before the delay time has elapsed. Range: 1.0 ms to 18 s."});
        m.params.add ({"Input", "The yellow input of the module. Any signal changing from 0 units or below to anything above 0 units will activate the delay and then produce a high logic output signal."});
        m.params.add ({"Output", "Signal: Logic."});
        m.params.add ({"Setup", "Options... The functions in this dialog box affects the configuration of the Editor. The parameters are automatically saved when you exit the dialog box."});
        m.params.add ({"Cable style", "This is where you can adjust the appearance of the patch cables in the Editor. Choose between Straight 3D, Curved 3D, Straight Thin and Curved Thin. nob control Here you select if you want the knob and slider parameters in the Editor patch window to respond to Circular, Horizontal or Vertical motions with the mouse."});
        m.params.add ({"Auto upload", "Check in this box to automatically upload any patch that you load in a slot of Nord Modular. Note that to be able to upload a patch to the Editor, you have to load the patch to a slot using the patch/ load button on the Nord Modular front panel."});
        m.params.add ({"Recycle windows", "If you make a new Patch window and assign that window to a slot which is already active in another patch window, the \"old\" window will still be in the Editor but placed in the background. If you check \"Recycle windows\" the old patch window will be closed. Midi... Allows you to choose any MIDI ports available on the computer to be used exclusively by the Editor for the communication with the synthesizer(s). Up to four Nord Modular/Micro Modular synthesizers can be controlled from the Editor. You can also instruct the computer to locate a Nord Modular synthesizer that was connected to the computer after the Editor was launched. Place a check in the Enabled box for each connected Nord Modular synthesizer and select a MIDI port from the drop-down menu(s) Click on the Apply button. When the Editor has found the Nord Modular synth(s), the Name of the connected synth(s) are shown in the Status line. When you're satisfied with the port selection, click OK. (It isn't necessary to click Apply before clicking OK. You can click O directly after having enabled and selected the ports, but this will close the dialog box.) The port selection is automatically saved in the Editor, so the next time you start the Editor, you don't have to redo the selection."});
        d.add (m);

        // ── Pulse ──
        m = ModuleHelp();
        m.name = "Pulse";
        m.description = "This module can use a signal that increases from 0 units to anything greater than 0 units, to produce a high logic signal. You set the duration of the generated high logic signal with the slider.";
        m.params.add ({"Display box", "Displays selected pulse time. Range: 1.0 ms to 18 s."});
        m.params.add ({"Time knob", "Set the duration of the produced high logic signal with the knob. If the module receives another level change, from 0 units to anything greater than 0 units during the duration of the produced high signal, it will extend the duration, with the value set with the knob. Range: 1.0 ms to 18 s."});
        m.params.add ({"Input", "The yellow input of the module. Any signal changing from 0 units or below to anything above 0 units will generate a high logic output pulse."});
        m.params.add ({"Output", "Signal: Logic."});
        d.add (m);

        // ── Quantizer ──
        m = ModuleHelp();
        m.name = "Quantizer";
        m.description = "The Quantizer module modifies an incoming signal by changing its bit resolution to a selected value. This module can e.g. transform a smooth envelope to a jagged curve, or quantize a clean, audio signal down to a dirty 7 bit signal.";
        m.params.add ({"Display box", "Displays the selected bit resolution. Range: Off, 12 to 1 bits."});
        m.params.add ({"Bits", "Select a bit resolution value with the buttons. Range: Off, 12 to 1 bits. Off leaves the signal unmodified."});
        m.params.add ({"In", "The red audio input."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Random generator ──
        m = ModuleHelp();
        m.name = "Random generator";
        m.description = "The Random Generator is a slave LFO that generates smooth random control signal steps at a steady frequency.";
        m.params.add ({"Mst", "A gray control input, for the generator to be controlled by a master module. Patch this input to a Slv output on a master module. If you connect a master oscillator to this input, the Random Step Generator will track the oscillator five octaves below the pitch of the oscillator."});
        m.params.add ({"Display box", "Displays either the LFO ratio related to the master rate, or the rate in Hz or seconds. Click to alter."});
        m.params.add ({"Rate knob", "Sets the frequency of the LFO. The frequency will be displayed as a ratio, in relation to the master module connected to the Mst input. The rate LED above the output will show you an approximation of the frequency. Range: 0.025 to 38.05 times the master rate or 62.9 seconds/cycle to 24.4 Hz."});
        m.params.add ({"Out", "The LED above the output shows an approximation of the current rate. Signal: Bipolar."});
        d.add (m);

        // ── Random pulse generator ──
        m = ModuleHelp();
        m.name = "Random pulse generator";
        m.description = "The Random Pulse Generator module generates random logic pulses with a selectable density.";
        m.params.add ({"Density", "Sets the average frequency and average pulse width of the logic signal. Low density results in few but longer pulses, and high density results in more but shorter pulses. The interval between the high and low logic signal levels are completely random."});
        m.params.add ({"Out", "The LED above the output shows an approximation of the current rate and density. Signal: Bipolar."});
        d.add (m);

        // ── Random step generator ──
        m = ModuleHelp();
        m.name = "Random step generator";
        m.description = "The Random Step Generator module generates a random control signal. The control signal is \"colored\". This means the effect is more gentle than a true, random signal. It contains less radical differences between adjacent values.";
        m.params.add ({"Mst", "A gray control input, for the generator to be controlled by a master module. Patch this input to a Slv output on a master module. If you connect a master oscillator to this input, the Random Step Generator will track the oscillator five octaves below the pitch of the oscillator."});
        m.params.add ({"Display box", "Displays either the LFO ratio related to the master rate, or the rate in Hz or seconds. Click to alter."});
        m.params.add ({"Rate knob", "Sets the frequency of the LFO. The frequency will be displayed as a ratio, in relation to the master module connected to the Mst input. The rate LED above the output will show you an approximation of the frequency. Range: 0.025 to 38.05 times the master rate or 62.9 seconds/cycle to 24.4 Hz."});
        m.params.add ({"Out", "The LED above the output shows an approximation of the current rate. Signal: Bipolar."});
        d.add (m);

        // ── Ring and Amp modulator ──
        m = ModuleHelp();
        m.name = "Ring and Amp modulator";
        m.description = "The Ring/Amplitude modulator can be used to new overtones in a sound. The module has a function which lets you transform the signal gradually from unmodified, via amplitude- to ring modulation. The main practical difference between amplitude- and ring modulation is the sideband amplitudes and the appearance of the carrier wave in the frequency spectrum. Another difference is that the resulting ring modulated signal phase-shifts 180 degrees every half modulator period.";
        m.params.add ({"#mod depth input [attenuator type i]", "You can modulate the AM/RM depth with a modulation source connect to this input. The amount of modulation can be attenuated with the knob."});
        m.params.add ({"#0/am/rm", "Set the modulation amount with this rotary knob. In the 12 o'clock position you get maximum amplitude modulation, and past this position, ring modulation occurs. mod Patch the modulator (oscillator or other sound generator) to this input. in Patch the carrier (oscillator or other sound generator) to this input out Signal: Bipolar."});
        d.add (m);

        // ── Sample and hold ──
        m = ModuleHelp();
        m.name = "Sample and hold";
        m.description = "This module takes samples of the values of an incoming signal. The sampling occurs every time a signal shifting from 0 units or below to anything above 0 units appears at the yellow logic input. Inbetween these trig signals, the module transmits the value of the latest sample to the output. To create the traditional Sample & Hold or random LFO synthesizer effect, connect the output of a white noise generator module to the input of the Sample & Hold module and trig the Sample & Hold module with an LFO. Connect the output of the Sample & Hold module to a Pitch modulation input of an oscillator and route the oscillator's output to an Output module.";
        m.params.add ({"Trig input", "Connect the signal to activate the sampling process to this yellow input."});
        m.params.add ({"In", "The red audio input."});
        m.params.add ({"Out", "Signal: Bipolar."});
        m.params.add ({"Note Sequencer A", "This is a Note Sequencer which sends one bipolar control signal value for each step. It also sends a logic gate signal for each step on a separate output."});
        m.params.add ({"Clk input", "This is the yellow input for the clock pulses. These pulses will advance the sequencer one step for each pulse."});
        m.params.add ({"Rst input", "This is a yellow input where a high logic signal will reset the sequencer to step 1. The restart isn't performed until the next the clock pulse is received at the Clk input. This guarantees perfect timing."});
        m.params.add ({"Snc output", "This yellow output transmits a logic signal at the high level whenever a sequence starts from step 1. Signal: Logic."});
        m.params.add ({"Clr button", "Pressing this button will reset all the control values to 0."});
        m.params.add ({"Loop button", "If the loop mode is on, the Note Sequencer will automatically restart from step 1 after the last step. If the loop mode is off, the sequencer will stop at the last step."});
        m.params.add ({"Step", "This sets the last step in the sequence. The sequencer will return to step 1 if loop mode is on, or stop if loop mode is off. Set the last step with the buttons. Range: 1 to 128 steps."});
        m.params.add ({"Rec button", "Activating the record function enables you to use the keyboard or incoming MIDI messages to program the steps in the sequence. Pressing a key will advance the edit point to the next step. The control signal from the keyboard will be present at the sequencer output. When the sequencer reaches step 16, the record function will be deactivated and the sequencer will return to step 1. The sequencer may be sequencing while you program the steps, if the Stop selector is in the Go position. You can move the edit point back and forth by using the arrow keys beneath the Stop button. The edit point can also be selected by clicking on a step LED."});
        m.params.add ({"Stop button", "Activating Stop stops the sequencer module, even if it is receiving a clock pulse at the Clk input. If you activate the record function when the sequencer is stopped, it may be programmed in a step mode fashion. Activating Go will put the sequencer back under the influence of a clock, connected to the Clk input."});
        m.params.add ({"#< > buttons", "Use these buttons to scroll through the steps."});
        m.params.add ({"Sliders", "You set the control signal level of each step by moving the vertical slider or clicking the arrow buttons that appear below each slider when you move the cursor over it. Note that when you click-hold to move the slider, the cursor becomes invisible. Range: +/- 64."});
        m.params.add ({"Link output", "This yellow output transmits a high logic signal whenever the Note Sequencer goes beyond step number 16. This is used for linking several Note Sequencers in series. See more about linking at the end of this chapter. Signal: Logic."});
        m.params.add ({"Gclk output", "This output transmits a high logic signal when the sequencer moves from one step to another. If the sequencer is stopped and you scroll manually through the steps, the Gclk will transmit one logic signal for each step, even if you scroll backwards. It will also transmit a logic signal when you are recording with the keyboard, if the Stop selector is in the stop position. Signal: Logic."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Sequencer group ──
        m = ModuleHelp();
        m.name = "Sequencer group";
        m.description = "The sequencer modules can perform different functions during the course of a specified time. The sequencer modules in the Nord Modular system has 16 steps. They can be linked together in series to provide longer sequences and they can be clocked by various clock sources, originating from other modules or from the MIDI clock. The sequencer modules can be synchronized to each other in a number of ways. The performance of the sequencer modules is not transmitted at the MIDI output. Read more about some of the possible combinations with the sequencer modules at the end of this chapter.";
        d.add (m);

        // ── Shaper ──
        m = ModuleHelp();
        m.name = "Shaper";
        m.description = "This module transforms a signal using one of five different amplification/attenuation characteristics. The curves on the buttons describes the transformation functions, i.e the amplification/attenuation of each value of the input signal. The middle button can be considered as a \"bypass\" function, i.e the amplification/attenuation is 1:1 on all input signal values.";
        m.params.add ({"Shape buttons", "Set the desired transformation characteristics with the selectors, Log2, Log1, Linear, Exp1 or Exp2."});
        m.params.add ({"In", "The red audio input."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Shelving EQ ──
        m = ModuleHelp();
        m.name = "Shelving EQ";
        m.description = "The Hi and Lo Shelving EQ is a treble and bass equalizer with cutoff frequency, gain and Hi/Low selector.";
        m.params.add ({"Frequency knob", "With the big rotary knob you change the cut-off frequency. Range: 20 Hz to 16 kHz."});
        m.params.add ({"Freq display box", "Displays the cut-off frequency in Hz. Range: 20 Hz to 16 kHz."});
        m.params.add ({"Gain display box", "Displays the gain in dB. Range -18 to 18 dB."});
        m.params.add ({"Gain knob", "With the Gain rotary knob you change the gain at the cut-off frequency. Range -18 to 18 dB."});
        m.params.add ({"Hi and Lo", "Select if you want the gain control to affect the lower or upper frequency band."});
        m.params.add ({"Graph", "Displays a schematic graph of the equalization characteristics. The Y-axis represents level and the X-axis the frequency. The horizontal line in the middle represents the 0 dB level."});
        m.params.add ({"In", "This is the red audio input of the equalizer. With the rotary button to the left you can attenuate the input signal [Attenuator Type I]."});
        m.params.add ({"B", "Press the B button to b ypass the signal and disable the equalization effect."});
        m.params.add ({"Out", "The multi-color LED above the output indicates the output level and have the following meaning: Green: normal signal level, Yellow: signal reaching headroom, Red: overload. Signal: Bipolar"});
        d.add (m);

        // ── Signals in the patch ──
        m = ModuleHelp();
        m.name = "Signals in the patch";
        m.description = "Just as in a traditional analog system, modules and parameters in Nord Modular interact with each other by means of signals being patched from one place to another. In a typical analog system, these signals are represented by voltage ranging from e.g. -10 to +10 volts. The signal levels in the Nord Modular system are represented by \"units\". These units have nothing to do with the internal resolution of the Nord Modular system which is 24-bit, but is used to more easily indicate levels in the system. Nord Modular uses four types of signals in its patches: bipolar audio signals (-64 to +64 units) bipolar and unipolar control signals (-64 to +64 units, 0 to +64 units or 0 to -64 units) The LFO is an example of a bipolar modulator. Bipolar means that it sends both positive and negative levels (peak to peak -64 to +64 units). The keyboard is another example of a bipolar modulator. The key E4 (MIDI note number 64) represents 0 units in the Nord Modular system. The ADSR envelope generator is an example of a unipolar modulator. It will only modulate in one direction, either positive or negative. In the case of Nord Modular ADSR envelopes, they range from 0 to +64 units. logic (high or low) control signals (0 or +64 units) The Clock generator is an example of a module that sends logic signals. A logic signal is also a unipolar signal but it has only two possible values, two states: low (0 units) or high (+64 units). slave module control signals (fix the coarse pitch between master and slave modules) The types of output signals of each module in Nord Modular will be described further on in this manual using the definitions: bipolar, unipolar and logic.";
        m.params.add ({"Resolution and headroom", "The internal resolution of the Nord Modular system is 24 bits. This ensures a supreme audio quality. The headroom of the audio signals in Nord Modular is -12 dB for every sound source. This means that if you mix more than 4 sound sources in a voice, at very high or un-attenuated levels, distortion may occur. This is easily dealt with by attenuating the levels of the sound sources. The mix bus headroom of the output modules is -6 dB per bus. The amplitude of the audio signals increases for each voice you play. A monophonic patch with an amplitude that is perfectly within the headroom, might produce distortion in the Output modules if more voices are added and played together. To determine where any unwanted distortion occurs, first try to lower the level on the Output module(s) in the patch. This action removes any mix bus related distortion. If this does not help, check the input signals to the mixers in the patch for possible distortion."});
        d.add (m);

        // ── Sine Bank ──
        m = ModuleHelp();
        m.name = "Sine Bank";
        m.description = "The Sine slave oscillator bank oscillator features six sine wave oscillators. These can be tuned and AM modulated independently. It is also possible to sync all the waves from an external source.";
        m.params.add ({"Display boxes", "The display boxes display the transposition ratio for each of the six sine wave oscillators. Range: 0.0241 to 39.152 times the master oscillator pitch."});
        m.params.add ({"Partial selectors", "Click on the up or down button to select a partial number based on the frequency of the master oscillator connected to the module (see Mst input below). The partial number selection ranges from 1:32 to 32:1 times the master oscillator frequency."});
        m.params.add ({"Tune", "With the Tune rotary knob you can select partials in steps of one semitone."});
        m.params.add ({"Fine Tune", "With the Fine Tune rotary knob below you can change the pitch in steps of 1/128 of a semitone. Clicking on the green triangle above the knob resets the Fine Tune to 0."});
        m.params.add ({"M", "Click on these buttons to mute the audio output of each oscillator."});
        m.params.add ({"AM input", "This audio input allows you to modulate the amplitude of the slave oscillator. The amount of this modulation is fixed at a ratio of 1:1."});
        m.params.add ({"Level", "These rotary knobs are used for controlling each oscillator's output level."});
        m.params.add ({"Mst", "This gray master input is used to connect the module to a master oscillator. If you connect a master LFO to this input, the slave oscillator will track the LFO five octaves above the pitch of the LFO."});
        m.params.add ({"Sync", "This audio input is used for synchronizing the oscillators to a control source, which could be another oscillator, an LFO or the keyboard. Synchronization forces the oscillators to restart their waveform cycles in sync with the waveform cycle of the controlling device. The oscillators will restart whenever a signal present at the sync input increases from 0 units to anything above 0 units. The pitch of the controlling oscillator will interact with the controlled oscillator pitches. Turning the Tune knobs of the sync-controlled sine wave oscillators will produce radical changes in the timbre. See \"Sync\" on page 36 for more information."});
        m.params.add ({"Mix In", "Use this audio input to mix in another audio signal. The input signal will be mixed with the sine wave oscillator signals and sent to the output."});
        m.params.add ({"Out", "Signal: Bipolar"});
        m.params.add ({"$Slave LFOs", "The rate of slave LFOs can be controlled by a master LFO. The gray Slv output of a master module should be connected to the gray Mst input of the slave module. If you refrain from connecting a master LFO to a slave, it will produce a waveform at the rate set with the Rate knob."});
        m.params.add ({"$Slave oscillators", "A slave oscillator can be slaved to any one of the three master oscillators for multi oscillator purposes. Any pitch modulation or keyboard tracking, that affects a master will also affect a slave oscillator connected to that master. If you refrain from connecting a master oscillator to a slave, it will produce a waveform with a fixed pitch, starting at E4 (329.6 Hz). You can control the fixed pitch with the Detune and Fine controls. * A slave oscillator is not affected by the Master Tune function unless it is controlled by a master oscillator."});
        d.add (m);

        // ── Slots ──
        m = ModuleHelp();
        m.name = "Slots";
        m.description = "There are four slots labelled a, b, c and d, in Nord Modular. You can load one patch to each slot. A slot can be considered as a temporary memory location which can hold a patch for playing or editing. You activate a slot by pressing one of the slot buttons on the Nord Modular front panel. The led above the slot button will flash green to indicate that the slot is active. The display shows the name of the patch and the current number of voices assigned to the patch within parenthesis. By pressing shift while turning the rotary dial, you can change the number of requested voices for the active slot. playing multitimbrally The slots can receive MIDI information on separate MIDI channels, making Nord Modular multitimbral. If you want to use Nord Modular multitimbrally you first have to load the patches you want in each slot. Then, simultaneously press the slot buttons for the slots you want to use. If several slot buttons have been pressed, the active slot led will flash, the others will be solid green. You can change the active slot by pressing the corresponding slot button. To deactivate and reactivate slots in a multitimbral setup, press shift and the desired slot button(s). The display shows the name of the patch in the active slot and the current number of voices of that patch within parenthesis. The other numbers in the display show the current number of voices assigned to the other patches in the setup. By pressing shift while turning the rotary dial, you can change the number of requested voices of the active slot.";
        d.add (m);

        // ── Smooth ──
        m = ModuleHelp();
        m.name = "Smooth";
        m.description = "The Smooth module can smooth out transitions in control signals. Set the time it should take for the module to gradually output the input signal level after a transition. An application could be to input a logic signal, and smooth out its \"sharp\" edges.";
        m.params.add ({"Display box", "Displays set smooth time. Range: 0.32 to 318 ms."});
        m.params.add ({"Time knob", "Set the time it should take for the module to smooth out the transitions. Range: 0.32 to 318 ms."});
        m.params.add ({"In", "The blue control signal input."});
        m.params.add ({"Output", "Signal: Bipolar."});
        m.params.add ({"Spectral OSC", "The Spectral shape oscillator is an oscillator with a built-in overtone generator. The waveforms are generated by synched noise. The oscillator generates a signal that contains either odd and even partials or only odd partials. It is possible to control the amount of overtones generated."});
        m.params.add ({"Slv output", "This is a gray slave output for controlling slave oscillators. Patch this output to a Mst input of a slave module. If you control a slave LFO with this signal, the rate of the LFO will be five octaves below the pitch of the master oscillator."});
        m.params.add ({"Freq display box", "Click on the display box switch between Hz and Notes. Range: C-1 to G9 (7.94 Hz to 12910 Hz)."});
        m.params.add ({"Coarse", "Sets the coarse tuning of the oscillator. Range: C-1 to G9 (8.18 Hz to 12540 Hz)."});
        m.params.add ({"Fine", "Sets the fine tuning of the oscillator. The range is +/- half a semitone divided into 128 steps. Click on the triangle above the control to reset the fine tuning to 0, which is the preset value."});
        m.params.add ({"Pitch", "There are two blue modulation inputs for modulating the oscillator pitch on this module. The modulation amount is attenuated with the rotary knob next to each input."});
        m.params.add ({"FMA", "A red modulation input where a signal will affect the frequency of the oscillator. The FM amount is attenuated with the rotary knob next to the input."});
        m.params.add ({"BT", "BT is the hardwired connection between the oscillator and the keyboard (and the MIDI input). If BT is activated the oscillator will track the keyboard at the rate of one semitone for each key. If BT is not activated, the keyboard will not affect the oscillator frequency."});
        m.params.add ({"Spectral shape", "With the Spectral shape function you select the amount of overtones of the waveform. Use the rotary knob to set the amount. There is also a blue modulation input for controlling the overtone amount from an external source. The rotary knob next to it attenuates the modulation amount."});
        m.params.add ({"Partials", "By pressing one of the buttons All or Odd, you determine whether you want the wave to contain either odd and even partials or only odd partials."});
        m.params.add ({"M", "Click on this button to mute the audio output of the oscillator."});
        m.params.add ({"Out", "Signal: Bipolar"});
        d.add (m);

        // ── Stereo chorus ──
        m = ModuleHelp();
        m.name = "Stereo chorus";
        m.description = "This module simulates the effect of multiple stereo voices.";
        m.params.add ({"Detune", "Sets the detune depth of the chorus effect."});
        m.params.add ({"Amount", "Adjusts the balance between the dry signal and the wet \"chorused\" signal."});
        m.params.add ({"In", "The red audio input."});
        m.params.add ({"B", "Click to bypass the signal and leave it unaffected."});
        m.params.add ({"L, R outputs", "Signals: Bipolar."});
        m.params.add ({"$Synth", "Synth Settings... This dialog box provides you with functions that apply to all patches loaded to the four slots of the Nord Modular. Select a slot on the tabs. Settings that you make will not be activated until the dialog box is exited with the O button. Note that the Synth Settings menu of Micro Modular differs a bit regarding content. Note! The changes you make in this dialog box will not be saved together with the patch data on disk or in the Nord Modular. You must use the Store Synth settings15RBUWY function on Nord Modular to save the settings. On Micro Modular, the changes will be stored automatically as soon as you click OK."});
        m.params.add ({"Name", "ey in a name for your connected Nord Modular. This is especially useful if you run several (up to four) Nord Modular/Micro Modular synthesizers from the Editor"});
        m.params.add ({"MIDI channel", "Set the MIDI channel for each slot. This channel will be used for reception and transmission of MIDI messages. MIDI Active check box: Place a check in the corresponding box to activate the slot to receive and transmit MIDI messages."});
        m.params.add ({"MIDI clock", "Set the clock source for the MIDI Clock. If External is activated, any incoming MIDI clock can be used as a clock source in a patch. If Internal is activated, set the Rate in BPM (beats per minute)."});
        m.params.add ({"Global sync:", "The MIDI Clock transmits 24 pulses per quarter note. The clock pulses can be divided using the Global sync function, and be sent as logic signals at the Sync output of the MIDI Global module15Z21TO. These logic signals can be used to synchronize the internal sequencer modules to an external MIDI sequencer. A setting of 4 (quarter notes) will result in one logic pulse for every 96 pulses from the System Clock, which is equal to one pulse for every fourth quarter note. This logic pulse can be used for resetting the sequencer modules in Nord Modular to the \"first beat in the bar\". If you do not use this function, the sequencer modules have no chance of knowing where they are in a bar. With the Global sync function activated, it will never take longer than the set number of quarter notes for the sequencer modules to realign themselves if you decide to start a MIDI sequence in the middle of a song. If you are synchronizing Nord Modular to a MIDI Clock source, this function will keep track of any incoming MIDI Song Position Pointer messages."});
        m.params.add ({"Master tune", "Use this to tune the Nord Modular to other instruments. The range is +100 to -100 cents. 100 cents is one semitone. Only the tuning of the master oscillators are affected by this function. A slave oscillator that is not connected to a master will not be affected. To the left is shown the master tune frequency in Hz. nob mode Here you set how Nord Modular should react to changes of the 18 assignable knobs (3 knobs on Micro Modular). Immediate means that the value of the assigned parameter will change immediately as you turn the knob. Hook means that the parameter value will not change until you have turned the knob past the current parameter value."});
        m.params.add ({"Pedal polarity", "Some sustain pedals uses inverted polarity to activate the sustain switch. In this box you can select between Normal and Inverted sustain pedal polarity. (This function is not supported in Micro Modular)."});
        m.params.add ({"Program change", "Here you select how Nord Modular should handle Bank Change (MIDI Controller #32) and Program Change MIDI messages. Choose between Off (no boxes checked), Send, Receive or Send and Receive (both boxes checked). eyboard mode Here you choose how the Nord Modular keyboard should control the slots. With Active slot selected, the keyboard will control only the patch of the active slot, and with Selected slots activated, all selected slots are controlled. Use the last function to simultaneously play several selected slots \"layered\". On the Nord Modular rack model this function works only when playing from the eyboard Floater in the Editor. To play several slots layered on the rack model from a master keyboard, the slots have to be set to the same MIDI channel. The eyboard mode function is not supported in Micro Modular, since it's not multitimbral."});
        m.params.add ({"Local off", "Turn the MIDI Local Control on or off. Select Local on (no check in the box) to be able to control Nord Modular from the internal keyboard and the pedals. MIDI data is also transmitted via the midi out port. In the Local off mode, the keyboard and pedal actions are transmitted only via MIDI and do not control Nord Modular itself. Local off should be used with external sequencers as the midi out port of Nord Modular is routed back, via the external sequencer, to the midi in port. Otherwise double notes will occur when playing the keyboard. The Local off function is not supported in Micro Modular)."});
        m.params.add ({"LEDs active", "When a patch gets complex, the LEDs of modules like the LFOs or Sequencers could become inaccurate in the Editor patch window. A lot of blinking LEDs could also slow down the computer. Uncheck the box to disengage the module LEDs in the patch window."});
        m.params.add ({"MIDI velocity scale", "This function is used to rescale the velocity data received at the midi in port. Set the maximum and minimum values. If your external master keyboard transmits maximum velocity as 112, set the maximum value to 112. This ensures that the velocity response from Nord Modular will properly reflect the velocity transmission of the master keyboard."});
        m.params.add ({"Upload Active Slot", "This command will upload the patch from the currently active slot in Nord Modular to the Editor. A new patch window will be created for the uploaded patch. If you load a new patch from the internal memory to the active slot and choose Upload Active Slot, the previous patch window will be recycled."});
        m.params.add ({"Send Controller Snapshot", "Use this command to send all assigned MIDI Controller values to the midi out port of the synthesizer. This is very useful if you are recording in a sequencer program and want to make sure the sound sounds exactly as you want. Note that the MIDI Controller snapshot is sent on the MIDI Out port of the synthesizer, not on the PC Out port."});
        m.params.add ({"Bank Upload (From Modular)", "This feature is a quick way of saving a complete patch bank from the Nord Modular memory to disk without needing to upload and save each patch one by one. Select which bank to upload (1-9 in Nord Modular and 1 in Micro Modular). Click Browse Location to select a destination folder on the computer. Click Save. Now all Patch files of the selected bank are saved individually together with a patch list file named 'Bank#.pchList', with the '#' representing the Bank number you uploaded. (You can change this patch list file name if you wish.) The original memory location of each patch is also saved with the bank file, so you can download them to the correct memory location later. As the patches are uploaded from Nord Modular, a progress bar indicates the elapsed time."});
        m.params.add ({"Tips:", "Don't save several Bank files in the same folder on the computer. The Bank file is saved together with all individual patch files. If several Bank files should contain the same patch name(s), these patch files will be renamed and could cause confusion when downloading the Bank files back to the synthesizer. To avoid this problem, save each Bank file in a separate folder on the computer."});
        m.params.add ({"Bank Download (To Modular)", "Select source by clicking on one of the two buttons: 'Browse for Bank file' lets you select a Bank file (*.pchList) previously saved on the computer. Browse and select the pchList file you want to download to a bank in Nord Modular. 'Browse for folder' lets you select a folder containing separate patch files. The folder doesn't have to contain a pchList file. The patch files of the selected folder will be downloaded to the Nord Modular synthesizer in alphabetical order. If a folder should contain more than 99 patch files the \"overflowing\" patches will be ignored. Note that the folder could also contain other file types, but only '*.pch' files will be downloaded to the Nord Modular synthesizer. Select which bank (1-9 in Nord Modular and 1 in Micro Modular) to replace with your selected bank. If you selected 'Browse for Bank file', click Open to download the patches to their original memory location within the selected bank. If you selected 'Browse for folder', click O to download the patches, in alphabetical order, to the selected bank. A progress bar indicates the elapsed time as the patches are stored in the Nord Modular internal memory. They will remain in the internal memory as if they were stored one by one using the store button or the 'Store' function of the Browser in the Editor."});
        m.params.add ({"Note!", "The entire memory bank you chose to download to will be overwritten in the Nord Modular synthesizer. Even if the bank you downloaded from the computer didn't contain patches in all memory locations, all previously stored patches in the Nord Modular synthesizer bank will be erased. Therefore, it could be wise to consider the banks in the synthesizer more like folders on the computer. When you download an entire bank to the synthesizer it would be similar to replacing a folder on the computer, i.e. the whole content of the bank (folder) would be erased and replaced."});
        d.add (m);

        // ── Synth settings ──
        m = ModuleHelp();
        m.name = "Synth settings";
        m.description = "Master Tune Use this to tune the Nord Modular to other instruments. The range is from +100 to -100 cents. 100 cents is one semitone. Only the tuning of the master oscillators are affected by the Master Tune function. A slave oscillator that is not connected to a master module will not be affected.";
        m.params.add ({"MIDI Clock", "Sets the source for the MIDI Clock and the tempo to be used, if the clock source is set to internal. If External is activated, any incoming MIDI clock can be used as a clock source in a patch. If Internal is activated, the internal clock will be used. Navigate to the tempo indication and set the tempo with the rotary dial. The MIDI clock will be present at the MIDI Global module and at the MIDI output as MIDI Clock. Read more about the MIDI Clock in the MIDI global15Z21TO module text."});
        m.params.add ({"Global Sync", "The MIDI Clock transmits 24 pulses for each quarter note. This clock can be divided with this parameter and will appear as logic signals at the Sync output of the MIDI Global module. These logic signals can be used to ensure proper synchronization when you e.g. want the internal sequencer modules to be synchronized with an external MIDI sequencer. A setting of \"4 Quarter notes\" here, will produce 1 logic pulse for every 96 pulses from the System Clock, which would be one pulse for every fourth quarter note. This logic signal can be used for resetting the sequencer modules in the Nord Modular system, to the \"first beat in the bar\". Without using this function, the Modular sequencer modules will have no chance of knowing, where in a bar they should be playing. With this resetting procedure, it will never take longer than one bar for the Nord Modular sequencers to realign themselves, if you for instance decide to start a MIDI sequence in the middle of a song. If you are synching the Nord Modular to a MIDI Clock source, this function will keep track of any incoming MIDI Song Position Pointer messages that the clock source transmits."});
        m.params.add ({"Local", "Turn the MIDI Local Control on or off. Select Local On to be able to control Nord Modular from the internal keyboard and the pedals. MIDI data is also transmitted via the midi out jack. In the Local Off mode, the keyboard and pedal actions are transmitted only via MIDI and do not control Nord Modular itself. Local Off should be used with external sequencers as the midi out jack of Nord Modular is routed back, via the external sequencer, to the midi in jack. If Echo is active in the sequencer, double notes will appear when playing the keyboard in Local On mode."});
        m.params.add ({"Program Change", "In this sub-menu you select how Nord Modular should handle Program Change and Bank Change (Controller #32) MIDI messages. Choose between Off, Send (only), Receive (only) and Send and Receive. eyboard Mode Here you choose how the Nord Modular keyboard should control the slots. With Active Slot selected, the keyboard will control only the patch of the active slot, and with Selected Slots activated, all selected slots are controlled. Use the last function to play several selected slots \"layered\" on the Nord Modular keyboard. On the Nord Modular rack model this function works only when playing from the eyboard Floater in the Editor. To play several slots layered on the rack model from a master keyboard, the slots have to be set to the same MIDI channel."});
        m.params.add ({"Pedal polarity", "Some sustain pedals uses inverted polarity to activate the sustain switch. In this menu you can select between the different sustain pedal polarities. nob mode Here you set how Nord Modular should react to changes of the 18 assignable knobs. Immediate means that the value of the assigned parameter will change immediately as you turn the knob. Hook means that the parameter value will not change until you have turned the nob past the current parameter value."});
        m.params.add ({"Leds active", "When a patch gets complex, the LEDs of modules like the LFOs or Sequencers could become inaccurate in the Editor patch window. A lot of blinking LEDs could also slow down the computer. Select NO to disengage the LEDs in the patch window."});
        m.params.add ({"MIDI Vel Scale", "This function can be used to rescale any maximum and/or minimum values that are received at the MIDI input, in order for the Nord Modular to respond correctly to the maximum and/or minimum, received velocity. The left number is the Minimum, the right is the Maximum value. If your master keyboard transmits its maximum velocity as 112, set the Max value to that value. This will make sure that the velocity response from the Nord Modular will properly reflect the velocity transmission of the master keyboard."});
        m.params.add ({"MIDI Channels", "Set the MIDI channel for each slot. This channel will be used for receiving and transmission of MIDI data. If you select -- the slot will not receive or transmit MIDI at all."});
        m.params.add ({"Synth Name", "Here you can name your synth. This is especially useful if you use several Nord Modulars/Micro Modulars with the Editor. The Editor supports up to four Modular synthesizers simultaneously. Select characters with the rotary dial and change the \"cursor\" position with the left/right navigator buttons."});
        m.params.add ({"Memory Protect", "Turns the memory protection for the internal patch memory locations on or off."});
        m.params.add ({"The BT parameter", "BT is short for eyBoard Tracking. The BT parameter controls the frequency step response from a keyboard to the module (e.g. the master oscillators, some of the master LFOs and some of the filters). In the oscillator modules, the pitch can track the keyboard, in the LFO modules the keyboard can modulate the rate of the LFO and in the filter modules, the cut-off frequency can track the keyboard. When the BT parameter in these modules is set to ey (the 12 o'clock position), the keyboard controls the parameter at the rate of one semitone for each key. Any pitch bend appearing at the MIDI input will be added to the BT tracking. Read more about scaling the pitch bend here.BL.FH If you, for instance, set up a sequencer patch, with BT active on the oscillators, you can transpose the sequence in realtime by pressing keys on the Nord Modular keyboard. If any filters in a patch have the BT parameter active, the cut-off frequency will also track the keyboard."});
        d.add (m);

        // ── The Mono parameter ──
        m = ModuleHelp();
        m.name = "The Mono parameter";
        m.description = "If you change a patch with 1 requested voice in the Poly Voice Area to be polyphonic, there are a few things that you need to be aware of. All modules in the Poly Voice Area are independent from each other, performing their functions on a \"voice\" level. This provides you with a big advantage in comparison to almost any other polyphonic synthesizer. If a traditional polyphonic synth had e.g. 4 voices, it has been common practice to provide the user with only one LFO for all voices together. This is not the case with Nord Modular. An LFO used in the Poly Voice Area is unique for each single voice in the patch. If you want modules in the Poly Voice Area of a polyphonic patch, e.g. one LFO per voice in a 4-voice patch, to be synchronized and behave like one single LFO, this can be done with the Mono parameter. The Mono parameter makes sure that certain functions in a polyphonic patch are \"in sync\" with the other voices. It is not possible to use, for example, one LFO in the Common Voice Area and let it control modules in the Poly Voice Area. Control source(s) and destination(s) must be in the same Voice Area.";
        m.params.add ({"Front panel nobs and other controllers", "The 18 knobs on the front panel of Nord Modular (3 knobs + 1 button on Micro Modular) are useful sources for modulation. Also control- and on/off pedals can be used for modulation (not on Micro Modular). The knobs and controllers can be assigned to almost any parameter in Nord Modular. They can also become an important part of the patch itself, controlling, for example, a mod-amount in real time, or affecting the entire path of a signal by controlling a switch module or a mixer."});
        m.params.add ({"Assigning a nob to a parameter", "A parameter can be either a continuous parameter (knob), or a selector switch (button). There are two ways of assigning and deassigning a knob to a parameter. 1. right[PC]/Ctrl[Mac]-click on a parameter in the Editor window, select nob from the popup menu and select one of the nobs in the menu. To deassign a knob, select Disable at the bottom of the popup menu. You can also re-assign a knob by selecting another (unused) nob in the popup menu. This method also allows you to assign the other available, external controllers (Pedal, After touch and the On/Off switch) to a parameter. These controllers are found at the bottom of the nob popup. 2. (Not Micro Modular) Put a parameter in the Editor window in focus, press the edit button on the front panel, press and hold the assign button and turn a knob. The LED next to the knob will light up, indicating an assignment. To deassign a knob, press the shift button and turn the knob a knob or a controller will always control the entire range of a parameter. If you need to control the range as well, use a Morph group instead. The Morph concept is described hereMorph_groups."});
        m.params.add ({"The nob floater", "The nob Floater window is a graphical representation of the knobs of the Nord Modular front panel. The nob Floater gives you both visual indication and the possibility to edit the parameters currently assigned to a knob. A lit LED next to each knob indicates that the knob is assigned to a module parameter, and the name of the module and parameter is shown above the knob. When you edit a knob in the nob Floater window, by click-holding and turning the knob, the corresponding module parameter in the patch window will be focused and change too. If you turn an assigned knob on the Nord Modular front panel, the knob in the nob Floater and the corresponding module parameter will change, also visually. At the bottom of the nob Floater, any Control pedal, Velocity and On/Off Switch assignments are shown. These can't be edited from the nob Floater, only viewed. Bring up the nob Floater by selecting nob Floater from the Tools menu. This is a floating window, meaning it can be positioned anywhere in the patch window. The nob Floater only shows the assignment for the current patch. It does not indicate assignment in a Panel Split situation."});
        d.add (m);

        // ── Toolbar ──
        m = ModuleHelp();
        m.name = "Toolbar";
        m.description = "Patch (name) Here is the Patch name for the active patch shown. Click in the box to key in a Patch name. Press Enter on the computer keyboard to enter the name and exit the Patch name box. If the patch is active in a slot of the Nord Modular, the name will be shown in the display as well. A standard English character set is available. Any illegal characters that you may type will be substituted with empty spaces.";
        m.params.add ({"Voices", "Use the arrow buttons to set the requested polyphony for the patch. The 'R' box shows requested voices and the 'C' box the current actual number of voices. A read-out of the current actual number of voices assigned to a patch is also displayed in the display of Nord Modular (not on Micro Modular). The current voice allocation depends on the usage of the Sound engines, the number of selected slots etc."});
        m.params.add ({"Load", "This indicates how much Sound engine resources the patch uses. The PVA indicator shows the Load for the Poly Voice Area, and the S indicator for the entire patch (Poly + Common Voice Area). The reason for having two separate Load indicators is because it makes it easier for the user to calculate the maximum polyphony of a patch . If you run out of load in a patch (if 100% Load is exceeded), the S indicator turns red and the outputs of Nord Modular will be muted. Delete one or several modules to reduce the Load. It is only the modules themselves that use up Sound engine resources. Cables, connections and settings have no effect on the Load. The Load indicators reflects one way (in most situations, the best way) of measuring the resources of the Sound engines. It may, however, turn to red on readings below 100% S Load, if other system resources are used up first. When you place the cursor over a module icon in a tab of the toolbar, a hint box appears with information of how much Load the module requires."});
        m.params.add ({"Visible cables", "Click on any of the first six colored buttons to select which cable group(s) should be visible/invisible in the patch. \"Invisible\" cables will be indicated by a colored dot on the in- and outputs of the connected modules. The white button represents any remaining connections after you have broke a part of a cable chain."});
        m.params.add ({"Shake cables", "Click on the \"S\" button to reposition, shake, the cables in a patch. This can be useful if it is hard to see where the cables are actually connected, or if they hide visual information (display boxes etc.) in the patch."});
        m.params.add ({"Module group tabs", "The Module group tabs are located in the left section of the Toolbar. Click on a tab to select a module group. The module icons of the selected module group are shown below the tabs. As you move the cursor over each module icon, the module name and Load requirement are shown in a hint box."});
        m.params.add ({"Morph group knobs", "Almost any parameter or function in the Nord Modular modules can be assigned to one of the four available Morph groups. You may assign up to 25 different parameters to the four Morph groups. With the Morph group knobs you control the Morph amount for each group. Right-click on a Morph group knob to assign it to a knob on the front panel and/or a MIDI Controller. As an exclusive feature, a Morph group can be assigned to MIDI Note values or keyboard Velocity. These two functions are found in the eyboard section of the list. The display box below each Morph group knob shows any knob assignment (knob#, Controller#, bd etc.). Note that if you assign a knob to both a knob and a MIDI Controller, the MIDI Controller number will be shown in the display box. See more about the Morph funtions here2GJ23O5. Porta. The portamento has two modes: Normal and Auto. In the Normal mode the portamento is always active, in the Auto mode you activate the portamento by playing legato. With the up and down buttons you set the time it should take to reach the new note. The portamento function is available only if a patch is set to 1 voice, i.e monophonic. The Portamento function is always active on any keyboard tracking oscillators used in the Common Voice Area, since this is a monophonic area. Portamento can also be set in the Patch|Patch Settings dialog box. (Portamento can also be achieved using Portamento modules. Using these modules, the portamento can be polyphonic as well.) Bend r. Here you set the range of the incoming pitch bend data in Semitones. The pitch bend data will be added to the control signals from the Note outputs on the eyboard modules and to the BT (keyboard tracking) parameter. The range is from 0 to 24 semitones. LFOs and filters that are using the BT parameter will be affected by the incoming pitch bend data as well. Bend range can also be set in the Patch|Patch Settings dialog box."});
        m.params.add ({"Connection indicators", "Here, all currently connected Nord Modular synthesizers are visible. Up to four synthesizers can be controlled from the Editor, and their names are shown in the box to the right. You can activate any slot of the connected synthesizers and edit the patch by clicking on the slot button. You can select/deselect several slots on the same instrument by Ctrl-clicking on the corresponding slot buttons. If several slots have been selected, this is indicated by \"depressed\" buttons in the Toolbar. The active slot is highlighted. Note that only one patch can be active in the Editor at the same time even if you have several Nord Modulars connected. The LED chain next to the connection indicator for each connected synthesizer indicates how much of the internal memory is used. Note that it's not only the number of patches that matters, but also the size of each patch. For example, a bank containing 99 \"small\" patches could use less internal memory than a bank containing 30 \"big\" patches."});
        d.add (m);

        // ── Tools ──
        m = ModuleHelp();
        m.name = "Tools";
        m.description = "nob Floater This function activates the nob Floater window. The nob Floater window is a graphical representation of the knobs of the Nord Modular front panel. The nob Floater gives you both visual indication and the possibility to edit the parameters currently assigned to a knob. A lit LED next to each knob indicates that the knob is assigned to a module parameter, and the name of the module and parameter is shown above the knob. Click-hold an assigned knob in the nob Floater and change its value, just like you would change a module parameter. As you can see, the assigned module parameter will change its value, also visually, when the knob is changed. If you turn an assigned knob on the Nord Modular front panel, the knob in the nob Floater and the corresponding module parameter will change, also visually. At the bottom of the nob Floater, any Control pedal, Velocity and On/Off Switch assignments are shown. These can't be edited from the nob Floater, though. The nob Floater only shows the assignment for the current patch. It does not indicate assignment in a Panel Split situation. eyboard Floater This activates the eyboard Floater window. This window can be used to play a patch without having a master keyboard or similar connected to the synthesizer. Click on the keys of the eyboard Floater to play single notes the selected note will be indicated by a black dot on the corresponding key. The note will sustain if you keep the mouse button depressed, just like on a real keyboard. You can expand the keyboard to cover the whole MIDI note range simply by placing the cursor on either side of the window frame. When the double-arrow appears, click-drag horizontally to desired size. You can also show/hide the button bar by placing the cursor on the top or bottom window frame and click-dragging vertically. The four buttons to the left are used to scroll up and down the keyboard, either one octave (the double-arrow buttons) or one note (the single-arrow buttons) at a time. Click on the 'Drone' button to make the next played note start sounding \"infinitely\". Click the Drone button again to disengage. Click on the 'Repeat' button to make the last played note play repeatedly. Click the Repeat button again to disengage.";
        m.params.add ({"Notes Floater", "The Notes Floater can be used to write comments in a patch. The notes are saved only with the Editor patch on the computer - not in the Nord Modular internal memory."});
        m.params.add ({"KBrowser", "This function activates the Patch Browser floating window. The Patch Browser gives a very good overview of all current patches stored in the internal memory of the connected Nord Modular synthesizer(s), and patches stored on the computer. The Patch Browser is automatically updated as soon as you perform any of the operations described below, even if they are done from the Nord Modular front panel. You can use the Patch Browser to save and load patches both from disk and the internal memory of the Nord Modular/Micro Modular. There are two tabs in the Patch Browser window: disk Click on the Disk tab to view folders and patch files stored on the computer. Double-click on a folder to step down one level in the hierarchy. Click the R button to the upper right corner to rescan disks and/or folders to update the contents list. Click the UP button to step up one level in the hierarchy."});
        m.params.add ({"Load patch to active slot:", "Double-click on a patch file to automatically load the patch to the active slot in the synthesizer and open up the patch in the Editor patch window (if 'Auto upload' has been selected in the Setup|Options dialog box). Load patch to slot or store in internal memory: By right-clicking on a patch in the Disk tab, the following dialog box appears. Here you can choose to either load the selected patch to any of the slots of the connected synthesizers and open the patch in the Editor patch window, or to save the selected patch in an internal memory location. The last function is exactly the same as storing a patch using the store button and the rotary dial on the Nord Modular front pane. flash memory Click on the Flash Memory tab to view banks and patches stored in the internal memory of the connected synthesizer(s)."});
        m.params.add ({"Load patch to active slot:", "Double-click on a patch to automatically load the patch to the active slot in the synthesizer and open up the patch in the Editor patch window (if 'Auto upload' has been selected in the Setup|Options dialog box). This function is exactly the same as loading a patch using the patch/load button and the rotary dial on the Nord Modular front panel."});
        m.params.add ({"Store or delete patch from internal memory:", "By right-clicking on a patch in the Flash Memory tab, you can choose to either store the patch of the active slot in the selected memory location, or to delete the selected patch from its memory location. Store patch of active slot to a selected bank: By right-clicking on a bank in the Flash Memory tab, you can choose to store the patch of the active slot in one of the selected bank's memory locations."});
        m.params.add ({"Store patch of active slot to any bank:", "By right-clicking on the gray frame at the bottom of the Flash Memory tab (by scrolling all way down to the bottom of the list), you can choose to store the patch of the active slot in any memory location of any bank."});
        d.add (m);

        // ── Vocal Filter ──
        m = ModuleHelp();
        m.name = "Vocal Filter";
        m.description = "The Vocal Filter module is designed to simulate the vocal tract. You can select between a number of preset vowels and change and modulate them to generate really amazing effects. Waveforms with a lot of overtones, such as sawtooth or pulse waves, are best suited to be used with the Vocal Filter.";
        m.params.add ({"Res", "This function emphasizes the frequency peaks of the vowels. The more resonance, the more clearly the vowels appear. Click on the green triangle above the rotary knob to reset to a medium value."});
        m.params.add ({"Freq", "Sets the initial center frequency offsets of the vowels. The practical result of turning this knob would be like pitch-shifting a sampled voice. Click on the green triangle above the rotary knob to reset to a medium value."});
        m.params.add ({"Frequency modulation input [Type II]", "The input for modulating the center frequency offset from a control source. The modulation amount is determined by the rotary knob next to the inputs."});
        m.params.add ({"Vowel display boxes", "Displays the three different selected vowels. Presets: A, E, I, O, U, Y, AA, AE, OE."});
        m.params.add ({"Vowel navigator buttons", "Selects the vowels to be used. You can select up to three vowels and navigate between these with the navigator knob (see below). Presets: A, E, I, O, U, Y, AA, AE, OE."});
        m.params.add ({"Vowel modulation input [Type I]", "The input for modulating the navigation between the selected vowels. The knob next to the input is used for attenuating the input level."});
        m.params.add ({"Vowel navigator knob", "Navigates between the vowels you selected with the vowel selectors. Note that this is a transformation function - not a mix function."});
        m.params.add ({"Input", "The audio input of the filter module. The knob next to it is used for attenuating the input level."});
        m.params.add ({"Output", "Signal: Bipolar."});
        d.add (m);

        // ── Vocoder ──
        m = ModuleHelp();
        m.name = "Vocoder";
        m.description = "The Vocoder module is a 16 band vocoder with the ability to reroute the analysis bands. The basic principle of a vocoder is to filter a synthesizer sound with the help of another sound - a human voice for example. The result when filtering a synth sound with a voice would be a \"singing\" synthesizer.The actual notes that come out of the vocoder are the notes played on the synthesizer. To reach this effect the analysis frequency spectrum is divided into separate frequency bands, in this case 16. These 16 frequency bands work like 16 bandpass filters, each controlling a defined frequency band of the synthesizer bank. An envelope follower for each band determines the amplitude changes of the modulated sound. With this vocoder module it is possible to reroute the analysis bands to any of the frequency bands of the synthesis bank, creating really interesting frequency combinations. Of course you can use any kind of sound in the analysis bank to shape the synthesizer sound. Some like to use drum sounds to get percussive synth sounds, for example. Feel free to experiment.";
        m.params.add ({"Analysis bank Ctrl input", "Patch the signal you want to use as \"modulator\" to the red audio signal input on the upper left of the module."});
        m.params.add ({"High frequency emphasis", "Click on the button below the Analysis bank input to emphasize the high frequencies of the analysis signal. This is a very useful function to get a more even frequency response in the modulated sound."});
        m.params.add ({"Mon", "Click on this button to bypass the modulator (Ctrl) signal to the output."});
        m.params.add ({"Graph", "This graph shows the routing between the Analysis and Synthesis bands."});
        m.params.add ({"Reroute buttons", "Click on the up and down buttons to reroute each of the synthesizer signal's frequency bands to any of the frequency bands of the Analysis bank."});
        m.params.add ({"Presets", "Click on the Preset buttons to reroute all Synthesis bands the number of steps indicated on the buttons. The Inv button inverts the band routing, i.e routes the Analysis band 1 to Synthesis band 16 and so on. The Rnd button reroutes all bands completely randomly - great for experiments."});
        m.params.add ({"Output Gain", "Use this knob to adjust the level of the modulated synthesizer signal. Range: 0.25 to 4 times the input level. Click on the green triangle to leave the signal level unaffected."});
        m.params.add ({"Synthesizer input", "The red audio signal input to the lower right is where you patch the synthesizer audio signal."});
        m.params.add ({"Out", "Signal: Bipolar"});
        d.add (m);

        // ── Voices, mono- and polyphonic patches ──
        m = ModuleHelp();
        m.name = "Voices, mono- and polyphonic patches";
        m.description = "A patch can be set to a polyphony between 1 and 32 voices, as long as there is enough Sound engine power available. It is not necessary to manually duplicate all the modules and settings for each voice to create a polyphonic patch, as it would in a traditional modular system. All you have to do is change the requested polyphony in the toolbar of the Editor, in the Patch|Patch Settings menu or in the Nord Modular synthesizer (not Micro Modular). To be able to make a patch polyphonic at all, you have to create the patch, or the part of the patch, you want to be polyphonic, in the Poly Voice Area of the patch window. The current actual polyphony of a patch is displayed in the display box next to the requested polyphony box in the toolbar, and in the Nord Modular display (not Micro Modular) in Patch mode (press the patch/load button). Should your request for polyphony exceed the current capacity of the Sound engine(s), the system assigns the highest possible amount of voices to the patch instead. All patches must have a requested number of voices assigned to them (minimum 1 voice). The dynamic allocation method used by other multitimbral hardware synthesizers is not applicable with Nord Modular. You can adjust the polyphony by selecting a slot, press the shift button and turn the rotary dial (not Micro Modular). This can be useful if you have a couple of patches loaded to several slots and wish to redistribute the polyphony among the patches. The Nord Modular note recognition system operates according to the \"last note\" principle. If you run out of polyphony and continue to play notes, the synthesizer will always add the last note played and remove the first note, with one exception: it will try to keep the lowest note sounding. poly and common voice patch sections A Nord Modular patch can consist of two parts: a polyphonic part and a monophonic part. In the Editor, these two parts are represented by two sections of the patch window, divided by a horizontal bar. The upper section is called the Poly Voice Area and the lower section the Common Voice Area. In the Poly Voice Area you place modules that should be duplicated for each voice, e.g. oscillators, envelope generators, filters etc. In the lower patch window, the Common Voice Area, you can place modules that should act equally on all voices in the patch, e.g. different types of Audio modules etc. Modules used in the Common Voice Area will act on the sum of the signals output from the Poly Voice Area, and consequently will not be duplicated for each voice in the patch. This gives two big advantages: 1. A module is able to process whole chords, and not just a single voice, affecting the sound the same way an external audio processor would. 2. In most situations you will be able to free up Sound engine power (Load) so you could increase the polyphony of the patch. Cables cannot be connected from modules in one patch section to modules in the other. The only signals that can be routed from the Poly Voice Area to the Common Voice Area are two separate audio signals. The routing is one-way only; from the Poly Voice Area to the Common Voice Area. A patch in the Poly Voice Area set to 1 requested voice would give the same result as having the patch only in the Common Voice Area instead.";
        d.add (m);

        // ── Wavewrapper ──
        m = ModuleHelp();
        m.name = "Wavewrapper";
        m.description = "This module amplifies a signal until it hits the headroom. Instead of clipping the signal, it folds down, \"wraps around\". The waveform of the signal will be heavily transformed, with a lot of new overtones, which gives it distortion- or FM-like characteristics.";
        m.params.add ({"Modulation input [Type ITYPE1]", "Connect a modulator to this red input. The amount of modulation is attenuated with the knob."});
        m.params.add ({"Wrap", "Sets the initial wrap amount."});
        m.params.add ({"Graph", "Displays the initial wrap amount graphically. The Y-axis represents the output signal values, and the X-axis the input signal values."});
        m.params.add ({"In", "The red audio input."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── Windows ──
        m = ModuleHelp();
        m.name = "Windows";
        m.description = "Cascade Arrange multiple patch windows in a cascaded configuration.";
        m.params.add ({"Tile Horizontally", "Arrange multiple patch windows in a tiled configuration."});
        m.params.add ({"Tile Vertically", "Arrange multiple patch windows in a vertically tiled configuration."});
        m.params.add ({"Currently open patches", "Here, all patches open in the Editor are shown. You can select any of the open patches by clicking on them. Note that selecting an open patch from this list does not make it active in the synthesizer. To make the patch active, you have to select Patch|Download To Slot or, if the patch already occupies a selected slot, click the corresponding slot button in the Connection indicator to the upper right in the toolbar."});
        d.add (m);

        // ── X-Fade with modulator ──
        m = ModuleHelp();
        m.name = "X-Fade with modulator";
        m.description = "This mixer can be modulated by a control signal to produce a crossfade between two incoming signals.";
        m.params.add ({"X-fade modulation input [Type ITYPE1]", "The red modulation input of the X-fade module. Connect a modulator here. The amount of modulation is controlled with the knob."});
        m.params.add ({"#1/2 knob", "Sets the initial mix of the two signals. If you set the knob towards 1, the initial mix will have more of the signal connected to the number 1 input, if you set it towards 2, the initial mix will have more of the signal connected to the number 2 input. Clicking on the triangle will set the mix to an equal amount of both signals."});
        m.params.add ({"Input 1, 2", "The red audio inputs."});
        m.params.add ({"Output", "Signal: Bipolar."});
        d.add (m);

        // ── Inverter ──
        m = ModuleHelp();
        m.name = "Inverter";
        m.description = "";
        d.add (m);

        // ── Key Quantizer ──
        m = ModuleHelp();
        m.name = "Key Quantizer";
        m.description = "$#Key Quantizer This module quantizes the values of a continuous control signal and generates note values according to a user-defined key. It is great for arpeggio-like effects.";
        m.params.add ({"Notes", "Set the desired key by clicking the notes you want to quantize to. The note interval for the shown octave is automatically duplicated across the whole key Range."});
        m.params.add ({"Range display box", "Displays the set key range. For the display to show the correct note value, it is assumed that the input signal uses its whole dynamic range (+/- 64 units)."});
        m.params.add ({"Range knob", "Set the key range in semitones. Range +/- 64 semitones."});
        m.params.add ({"Cont", "Click this button to force the module to \"split up\" the key quantization grid in equally big sections per octave. This makes it easier to output the selected notes at a steady rate when using a linear control signal."});
        m.params.add ({"In", "The Range control signal input."});
        m.params.add ({"Out", "Signal: Bipolar."});
        d.add (m);

        // ── KeybSplit ──
        m = ModuleHelp();
        m.name = "KeybSplit";
        m.description = "$#Keyb Split eyboard split gives you the possibility to create split sounds using only one patch. It functions as a \"key filter\" in which you set the limits for the key range that should pass through.";
        m.params.add ({"Lower", "Here you set the lower limit of the keyboard range. The limit note is displayed in the display boxe. Only notes played above the set limit will pass through to the output(s). Range: C-1 to G9."});
        m.params.add ({"Upper", "Here you set the upper limit of the keyboard range. The limit note is displayed in the display boxe. Only notes played below the set limit will pass through to the output(s). Range: C-1 to G9."});
        m.params.add ({"Note, Gate, Vel inputs", "'Note' is the note control signal input. Patch the Note output of the Keyboard Patch or Keyboard Voice module to this input. 'Gate' is the logic gate input. This input must always receive a high logic gate signal to activate the module. Connect it to the Gate output of the Keyboard Patch or Keyboard Voice module, for instance. 'Vel' is the velocity control signal input. If a gate signal and a note control signal that lies within the Lower and Upper limits are received, the velocity value of the 'Vel' input will be transmitted to the 'Vel' output."});
        m.params.add ({"Note, Gate, Vel outputs", "These are the outputs for the note, gate and velocity signals. Note signal: Bipolar Gate signal: Logic Velocity signal: Unipolar"});
        d.add (m);

        // ── Keyboard ──
        m = ModuleHelp();
        m.name = "Keyboard";
        m.description = "$#Keyboard - voice The Keyboard voice module provides you with access to a few basic and important signals associated with the keyboard on Nord Modular, or a keyboard connected to the synth via MIDI. The signals are generated from each key played and affect one voice at a time.";
        m.params.add ({"Note", "This blue output provides you with a pitch (note number) signal from the Nord Modular keyboard or from the midi in port. This signal is hardwired to every module that has a KBT control or switch. There is no need to patch this output to every oscillator you are controlling from the keyboard or via MIDI. This is also the output for any pitch bend data that appears at the Nord Modular midi in port. The pitch bend will be scaled together with the note information, with the ratio of the pitch bend parameter. This ratio is set in the Patch|Patch SettingsPatch_settings menu. E4 (MIDI note 64), which is the middle E on the Nord Modular keyboard when the oct shift selector is in the center position, represents an output signal level of 0 units. MIDI note 0 (C-1) represents -64 units and MIDI note 127 (G9) represents +63 units. Signal: Bipolar."});
        m.params.add ({"Gate", "This yellow output sends a high logic signal (+64 units) every time a key is pressed on the keyboard, or a MIDI note-on is received at the midi in port. The logic signal switches back to zero (0 units) when the key is released. If a sustain pedal is activated, the logic signal will be high for as long as the pedal is pressed. Signal: Logic."});
        m.params.add ({"Vel", "This blue output transmits the note-on velocity signals from the keys that you play on Nord Modular or any velocity that is received on the midi in port. The velocity response of the Nord Modular keyboard is linear. Signal: Unipolar."});
        m.params.add ({"Rel vel", "This blue output provides you with the release velocity signal from the keys that you play on the Nord Modular, or any release velocity that is received via MIDI. Signal: Unipolar."});
        d.add (m);

        // ── Keyboard patch ──
        m = ModuleHelp();
        m.name = "Keyboard patch";
        m.description = "$#Keyboard patch This module provides four different control signals. The signals are generated from the latest key played and affect all allocated voices, in contrast to the Keyboard module described above.";
        m.params.add ({"Latest Note", "This blue output provides you with a pitch (note number) signal from the latest note that was played on the keyboard, or that was received at the midi in port. E4 (MIDI note 64), which is the middle E on the Nord Modular keyboard when the oct shift selector is in the center position, represents a signal level of 0 units. MIDI note 0 (C-1) represents -64 units and MIDI note 127 (G9) represents +63 units. Signal: Bipolar."});
        m.params.add ({"Patch gate", "This yellow output sends a high (+64 units) logic signal every time a key is pressed on the keyboard or a MIDI note-on is received at the midi in port. The logic signal switches back to zero (0 units) when the last key is released. You can use this signal to start envelopes in the single-trigger fashion. If a sustain pedal is activated, the logic signal will be high for as long as the pedal is pressed. Signal: Logic."});
        m.params.add ({"Latest vel on", "This blue output provides you with a control signal from the latest note-on velocity. The velocity response of the Nord Modular keyboard is linear. Signal: Unipolar."});
        m.params.add ({"Latest rel vel", "This blue output provides you with a control signal from the release velocity of the latest note. Signal: Unipolar."});
        d.add (m);

        // ── Level shifter ──
        m = ModuleHelp();
        m.name = "Level shifter";
        m.description = "";
        d.add (m);

        // ── MasterOSC ──
        m = ModuleHelp();
        m.name = "MasterOSC";
        m.description = "$#Master OSC The Master Oscillator does not generate any waveforms itself. Instead it can be used to control one or several slave oscillators. For that purpose it is very useful since it offers an easy way of controlling global functions, such as coarse tuning and pitch modulation, of the connected slave oscillators. A big advantage with the Master Oscillator is that it saves DSP power compared to other oscillators.";
        m.params.add ({"Slv output", "This is a gray slave output for controlling slave oscillators. Patch this output to a Mst input of a slave module. If you control a slave LFO with this signal, the rate of the LFO will be five octaves below the pitch of the master oscillator."});
        m.params.add ({"Display box", "Click on the display box to change from Hz to Notes. Range: C-1 to G9 (7.94 Hz to 12910 Hz)."});
        m.params.add ({"Coarse", "Sets the coarse tuning of the oscillator. Range: C-1 to G9 in steps of one semitone."});
        m.params.add ({"Fine", "Sets the fine tuning of the oscillator. The range is +/- half a semitone divided into 128 steps. Click on the triangle above the control to reset the fine tuning to 0, which is the preset value."});
        m.params.add ({"Pitch", "There are two blue modulation inputs for modulating the oscillator pitch on this module. The modulation amount is attenuated with the rotary knob next to each input."});
        m.params.add ({"KBT", "BT is the hardwired connection between the oscillator and the keyboard (and the MIDI input). If KBT is activated the oscillator will track the keyboard at the rate of one semitone for each key. If KBT is not activated, the keyboard will not affect the oscillator frequency."});
        d.add (m);

        // ── Properties ──
        m = ModuleHelp();
        m.name = "Properties";
        m.description = "$#Setup Options... The functions in this dialog box affects the configuration of the Editor. The parameters are automatically saved when you exit the dialog box.";
        m.params.add ({"Cable style", "This is where you can adjust the appearance of the patch cables in the Editor. Choose between Straight 3D, Curved 3D, Straight Thin and Curved Thin. nob control Here you select if you want the knob and slider parameters in the Editor patch window to respond to Circular, Horizontal or Vertical motions with the mouse."});
        m.params.add ({"Auto upload", "Check in this box to automatically upload any patch that you load in a slot of Nord Modular. Note that to be able to upload a patch to the Editor, you have to load the patch to a slot using the patch/ load button on the Nord Modular front panel."});
        m.params.add ({"Recycle windows", "If you make a new Patch window and assign that window to a slot which is already active in another patch window, the \"old\" window will still be in the Editor but placed in the background. If you check \"Recycle windows\" the old patch window will be closed. Midi... Allows you to choose any MIDI ports available on the computer to be used exclusively by the Editor for the communication with the synthesizer(s). Up to four Nord Modular/Micro Modular synthesizers can be controlled from the Editor. You can also instruct the computer to locate a Nord Modular synthesizer that was connected to the computer after the Editor was launched. Place a check in the Enabled box for each connected Nord Modular synthesizer and select a MIDI port from the drop-down menu(s) Click on the Apply button. When the Editor has found the Nord Modular synth(s), the Name of the connected synth(s) are shown in the Status line. When you're satisfied with the port selection, click OK. (It isn't necessary to click Apply before clicking OK. You can click O directly after having enabled and selected the ports, but this will close the dialog box.) The port selection is automatically saved in the Editor, so the next time you start the Editor, you don't have to redo the selection."});
        d.add (m);

        // ── SpectralOSC ──
        m = ModuleHelp();
        m.name = "SpectralOSC";
        m.description = "$#Spectral OSC The Spectral shape oscillator is an oscillator with a built-in overtone generator. The waveforms are generated by synched noise. The oscillator generates a signal that contains either odd and even partials or only odd partials. It is possible to control the amount of overtones generated.";
        m.params.add ({"Slv output", "This is a gray slave output for controlling slave oscillators. Patch this output to a Mst input of a slave module. If you control a slave LFO with this signal, the rate of the LFO will be five octaves below the pitch of the master oscillator."});
        m.params.add ({"Freq display box", "Click on the display box switch between Hz and Notes. Range: C-1 to G9 (7.94 Hz to 12910 Hz)."});
        m.params.add ({"Coarse", "Sets the coarse tuning of the oscillator. Range: C-1 to G9 (8.18 Hz to 12540 Hz)."});
        m.params.add ({"Fine", "Sets the fine tuning of the oscillator. The range is +/- half a semitone divided into 128 steps. Click on the triangle above the control to reset the fine tuning to 0, which is the preset value."});
        m.params.add ({"Pitch", "There are two blue modulation inputs for modulating the oscillator pitch on this module. The modulation amount is attenuated with the rotary knob next to each input."});
        m.params.add ({"FMA", "A red modulation input where a signal will affect the frequency of the oscillator. The FM amount is attenuated with the rotary knob next to the input."});
        m.params.add ({"KBT", "BT is the hardwired connection between the oscillator and the keyboard (and the MIDI input). If KBT is activated the oscillator will track the keyboard at the rate of one semitone for each key. If KBT is not activated, the keyboard will not affect the oscillator frequency."});
        m.params.add ({"Spectral shape", "With the Spectral shape function you select the amount of overtones of the waveform. Use the rotary knob to set the amount. There is also a blue modulation input for controlling the overtone amount from an external source. The rotary knob next to it attenuates the modulation amount."});
        m.params.add ({"Partials", "By pressing one of the buttons All or Odd, you determine whether you want the wave to contain either odd and even partials or only odd partials."});
        m.params.add ({"M", "Click on this button to mute the audio output of the oscillator."});
        m.params.add ({"Out", "Signal: Bipolar"});
        d.add (m);

        // ── Synth ──
        m = ModuleHelp();
        m.name = "Synth";
        m.description = "";
        m.params.add ({"Name", "ey in a name for your connected Nord Modular. This is especially useful if you run several (up to four) Nord Modular/Micro Modular synthesizers from the Editor"});
        m.params.add ({"MIDI channel", "Set the MIDI channel for each slot. This channel will be used for reception and transmission of MIDI messages. MIDI Active check box: Place a check in the corresponding box to activate the slot to receive and transmit MIDI messages."});
        m.params.add ({"MIDI clock", "Set the clock source for the MIDI Clock. If External is activated, any incoming MIDI clock can be used as a clock source in a patch. If Internal is activated, set the Rate in BPM (beats per minute)."});
        m.params.add ({"Global sync:", "The MIDI Clock transmits 24 pulses per quarter note. The clock pulses can be divided using the Global sync function, and be sent as logic signals at the Sync output of the MIDI Global module15Z21TO. These logic signals can be used to synchronize the internal sequencer modules to an external MIDI sequencer. A setting of 4 (quarter notes) will result in one logic pulse for every 96 pulses from the System Clock, which is equal to one pulse for every fourth quarter note. This logic pulse can be used for resetting the sequencer modules in Nord Modular to the \"first beat in the bar\". If you do not use this function, the sequencer modules have no chance of knowing where they are in a bar. With the Global sync function activated, it will never take longer than the set number of quarter notes for the sequencer modules to realign themselves if you decide to start a MIDI sequence in the middle of a song. If you are synchronizing Nord Modular to a MIDI Clock source, this function will keep track of any incoming MIDI Song Position Pointer messages."});
        m.params.add ({"Master tune", "Use this to tune the Nord Modular to other instruments. The range is +100 to -100 cents. 100 cents is one semitone. Only the tuning of the master oscillators are affected by this function. A slave oscillator that is not connected to a master will not be affected. To the left is shown the master tune frequency in Hz. nob mode Here you set how Nord Modular should react to changes of the 18 assignable knobs (3 knobs on Micro Modular). Immediate means that the value of the assigned parameter will change immediately as you turn the knob. Hook means that the parameter value will not change until you have turned the knob past the current parameter value."});
        m.params.add ({"Pedal polarity", "Some sustain pedals uses inverted polarity to activate the sustain switch. In this box you can select between Normal and Inverted sustain pedal polarity. (This function is not supported in Micro Modular)."});
        m.params.add ({"Program change", "Here you select how Nord Modular should handle Bank Change (MIDI Controller #32) and Program Change MIDI messages. Choose between Off (no boxes checked), Send, Receive or Send and Receive (both boxes checked). eyboard mode Here you choose how the Nord Modular keyboard should control the slots. With Active slot selected, the keyboard will control only the patch of the active slot, and with Selected slots activated, all selected slots are controlled. Use the last function to simultaneously play several selected slots \"layered\". On the Nord Modular rack model this function works only when playing from the Keyboard Floater in the Editor. To play several slots layered on the rack model from a master keyboard, the slots have to be set to the same MIDI channel. The Keyboard mode function is not supported in Micro Modular, since it's not multitimbral."});
        m.params.add ({"Local off", "Turn the MIDI Local Control on or off. Select Local on (no check in the box) to be able to control Nord Modular from the internal keyboard and the pedals. MIDI data is also transmitted via the midi out port. In the Local off mode, the keyboard and pedal actions are transmitted only via MIDI and do not control Nord Modular itself. Local off should be used with external sequencers as the midi out port of Nord Modular is routed back, via the external sequencer, to the midi in port. Otherwise double notes will occur when playing the keyboard. The Local off function is not supported in Micro Modular)."});
        m.params.add ({"LEDs active", "When a patch gets complex, the LEDs of modules like the LFOs or Sequencers could become inaccurate in the Editor patch window. A lot of blinking LEDs could also slow down the computer. Uncheck the box to disengage the module LEDs in the patch window."});
        m.params.add ({"MIDI velocity scale", "This function is used to rescale the velocity data received at the midi in port. Set the maximum and minimum values. If your external master keyboard transmits maximum velocity as 112, set the maximum value to 112. This ensures that the velocity response from Nord Modular will properly reflect the velocity transmission of the master keyboard."});
        m.params.add ({"Upload Active Slot", "This command will upload the patch from the currently active slot in Nord Modular to the Editor. A new patch window will be created for the uploaded patch. If you load a new patch from the internal memory to the active slot and choose Upload Active Slot, the previous patch window will be recycled."});
        m.params.add ({"Send Controller Snapshot", "Use this command to send all assigned MIDI Controller values to the midi out port of the synthesizer. This is very useful if you are recording in a sequencer program and want to make sure the sound sounds exactly as you want. Note that the MIDI Controller snapshot is sent on the MIDI Out port of the synthesizer, not on the PC Out port."});
        m.params.add ({"Bank Upload (From Modular)", "This feature is a quick way of saving a complete patch bank from the Nord Modular memory to disk without needing to upload and save each patch one by one. Select which bank to upload (1-9 in Nord Modular and 1 in Micro Modular). Click Browse Location to select a destination folder on the computer. Click Save. Now all Patch files of the selected bank are saved individually together with a patch list file named 'Bank#.pchList', with the '#' representing the Bank number you uploaded. (You can change this patch list file name if you wish.) The original memory location of each patch is also saved with the bank file, so you can download them to the correct memory location later. As the patches are uploaded from Nord Modular, a progress bar indicates the elapsed time."});
        m.params.add ({"Tips:", "Don't save several Bank files in the same folder on the computer. The Bank file is saved together with all individual patch files. If several Bank files should contain the same patch name(s), these patch files will be renamed and could cause confusion when downloading the Bank files back to the synthesizer. To avoid this problem, save each Bank file in a separate folder on the computer."});
        m.params.add ({"Bank Download (To Modular)", "Select source by clicking on one of the two buttons: 'Browse for Bank file' lets you select a Bank file (*.pchList) previously saved on the computer. Browse and select the pchList file you want to download to a bank in Nord Modular. 'Browse for folder' lets you select a folder containing separate patch files. The folder doesn't have to contain a pchList file. The patch files of the selected folder will be downloaded to the Nord Modular synthesizer in alphabetical order. If a folder should contain more than 99 patch files the \"overflowing\" patches will be ignored. Note that the folder could also contain other file types, but only '*.pch' files will be downloaded to the Nord Modular synthesizer. Select which bank (1-9 in Nord Modular and 1 in Micro Modular) to replace with your selected bank. If you selected 'Browse for Bank file', click Open to download the patches to their original memory location within the selected bank. If you selected 'Browse for folder', click O to download the patches, in alphabetical order, to the selected bank. A progress bar indicates the elapsed time as the patches are stored in the Nord Modular internal memory. They will remain in the internal memory as if they were stored one by one using the store button or the 'Store' function of the Browser in the Editor."});
        m.params.add ({"Note!", "The entire memory bank you chose to download to will be overwritten in the Nord Modular synthesizer. Even if the bank you downloaded from the computer didn't contain patches in all memory locations, all previously stored patches in the Nord Modular synthesizer bank will be erased. Therefore, it could be wise to consider the banks in the synthesizer more like folders on the computer. When you download an entire bank to the synthesizer it would be similar to replacing a folder on the computer, i.e. the whole content of the bank (folder) would be erased and replaced."});
        d.add (m);

        // ── The KBT parameter ──
        m = ModuleHelp();
        m.name = "The KBT parameter";
        m.description = "$#The #KBT parameter BT is short for KeyBoard Tracking. The KBT parameter controls the frequency step response from a keyboard to the module (e.g. the master oscillators, some of the master LFOs and some of the filters). In the oscillator modules, the pitch can track the keyboard, in the LFO modules the keyboard can modulate the rate of the LFO and in the filter modules, the cut-off frequency can track the keyboard. When the KBT parameter in these modules is set to Key (the 12 o'clock position), the keyboard controls the parameter at the rate of one semitone for each key. Any pitch bend appearing at the MIDI input will be added to the KBT tracking. Read more about scaling the pitch bend here.BL.FKH If you, for instance, set up a sequencer patch, with KBT active on the oscillators, you can transpose the sequence in realtime by pressing keys on the Nord Modular keyboard. If any filters in a patch have the KBT parameter active, the cut-off frequency will also track the keyboard.";
        d.add (m);

        // ── The front panel Knobs and other controllers ──
        m = ModuleHelp();
        m.name = "The front panel Knobs and other controllers";
        m.description = "$#Front panel Knobs and other controllers The 18 knobs on the front panel of Nord Modular (3 knobs + 1 button on Micro Modular) are useful sources for modulation. Also control- and on/off pedals can be used for modulation (not on Micro Modular). The knobs and controllers can be assigned to almost any parameter in Nord Modular. They can also become an important part of the patch itself, controlling, for example, a mod-amount in real time, or affecting the entire path of a signal by controlling a switch module or a mixer.";
        m.params.add ({"Assigning a Knob to a parameter", "A parameter can be either a continuous parameter (knob), or a selector switch (button). There are two ways of assigning and deassigning a knob to a parameter. 1. right[PC]/Ctrl[Mac]-click on a parameter in the Editor window, select Knob from the popup menu and select one of the Knobs in the menu. To deassign a knob, select Disable at the bottom of the popup menu. You can also re-assign a knob by selecting another (unused) Knob in the popup menu. This method also allows you to assign the other available, external controllers (Pedal, After touch and the On/Off switch) to a parameter. These controllers are found at the bottom of the Knob popup. 2. (Not Micro Modular) Put a parameter in the Editor window in focus, press the edit button on the front panel, press and hold the assign button and turn a knob. The LED next to the knob will light up, indicating an assignment. To deassign a knob, press the shift button and turn the knob a knob or a controller will always control the entire range of a parameter. If you need to control the range as well, use a Morph group instead. The Morph concept is described hereMorph_groups."});
        m.params.add ({"The Knob floater", "The Knob Floater window is a graphical representation of the knobs of the Nord Modular front panel. The Knob Floater gives you both visual indication and the possibility to edit the parameters currently assigned to a knob. A lit LED next to each knob indicates that the knob is assigned to a module parameter, and the name of the module and parameter is shown above the knob. When you edit a knob in the Knob Floater window, by click-holding and turning the knob, the corresponding module parameter in the patch window will be focused and change too. If you turn an assigned knob on the Nord Modular front panel, the knob in the Knob Floater and the corresponding module parameter will change, also visually. At the bottom of the Knob Floater, any Control pedal, Velocity and On/Off Switch assignments are shown. These can't be edited from the Knob Floater, only viewed. Bring up the Knob Floater by selecting Knob Floater from the Tools menu. This is a floating window, meaning it can be positioned anywhere in the patch window. The Knob Floater only shows the assignment for the current patch. It does not indicate assignment in a Panel Split situation."});
        d.add (m);

        return d;
    }();

    return db;
}

static juce::String normalizeHelpName (const juce::String& s)
{
    return s.toLowerCase().replace ("-", " ").trim();
}

// Maps module fullnames / shortnames that can't be resolved by normalization
// alone to their exact help-database entry names.
static const std::pair<const char*, const char*> kHelpAliases[] = {
    // Switches
    { "1 to 4 Switch",                       "1-4 switch" },
    { "1-4Switch",                           "1-4 switch" },
    { "4 to 1 Switch",                       "4-1 switch" },
    { "4-1Switch",                           "4-1 switch" },
    // Filter bank
    { "14 Band Filter Bank",                 "Filter Bank" },
    { "FilterBank",                          "Filter Bank" },
    // Envelopes
    { "ADH Envelope",                        "AHD-Envelope" },
    { "AHD",                                 "AHD-Envelope" },
    { "Modulated Envelope",                  "MOD-Envelope" },
    { "Mod-Env",                             "MOD-Envelope" },
    { "Multiple Envelope",                   "Multistage envelope" },
    { "Multi-Env",                           "Multistage envelope" },
    // EQ
    { "Center Frequency Equalizer",          "EQ Mid" },
    { "EqMid",                               "EQ Mid" },
    { "Hi/Lo Shelving Equalizer",            "Shelving EQ" },
    { "EqShelving",                          "Shelving EQ" },
    // Pattern / sequencer
    { "Clocked Pattern Generator",           "Pattern generator" },
    { "PatternGen",                          "Pattern generator" },
    // Comparators
    { "Constant Comparator",                 "Compare to level" },
    { "CompareLev",                          "Compare to level" },
    { "Control Signal Comparator",           "Compare AB" },
    { "CompareAB",                           "Compare AB" },
    // Mixer / crossfader
    { "Control Signal Mixer",                "Control mixer" },
    { "ControlMixer",                        "Control mixer" },
    { "Crossfader",                          "X-Fade with modulator" },
    { "X-Fade",                              "X-Fade with modulator" },
    // Diode / drum
    { "Diode Processor",                     "Diode processing" },
    { "Diode",                               "Diode processing" },
    { "Drum Synthesizer",                    "Drum synth" },
    { "DrumSynth",                           "Drum synth" },
    // Oscillators
    { "Multiple Oscillator A",               "Oscillator A" },
    { "OscA",                                "Oscillator A" },
    { "Multiple Oscillator B",               "Oscillator B" },
    { "OscB",                                "Oscillator B" },
    { "Sine Oscillator",                     "Oscillator C" },
    { "OscC",                                "Oscillator C" },
    { "Formant Oscillator",                  "Formant OSC" },
    { "FormantOsc",                          "Formant OSC" },
    // Slave oscillators
    { "Multiple Slave Oscillator",           "Oscillator slave A" },
    { "OscSlvA",                             "Oscillator slave A" },
    { "Square/Pulse Slave Oscillator",       "Oscillator slave B" },
    { "OscSlvB",                             "Oscillator slave B" },
    { "Sawtooth Slave Oscillator",           "Oscillator slave C" },
    { "OscSlvC",                             "Oscillator slave C" },
    { "Triangle Slave Oscillator",           "Oscillator slave D" },
    { "OscSlvD",                             "Oscillator slave D" },
    { "Sine Slave Oscillator",               "Oscillator slave E" },
    { "OscSlvE",                             "Oscillator slave E" },
    { "FM Slave Oscillator",                 "Oscillator slave FM" },
    { "OscSlvFM",                            "Oscillator slave FM" },
    { "Sine Bank Slave Oscillator",          "Sine Bank" },
    { "OscSineBank",                         "Sine Bank" },
    // LFO slaves
    { "Multiple Slave LFO",                  "LFO slave A" },
    { "LFOSlvA",                             "LFO slave A" },
    { "Sawtooth Slave LFO",                  "LFO slave B" },
    { "LFOSlvB",                             "LFO slave B" },
    { "Sine Slave LFO",                      "LFO slave C" },
    { "LFOSlvC",                             "LFO slave C" },
    { "Square/Pulse Slave LFO",              "LFO slave D" },
    { "LFOSlvD",                             "LFO slave D" },
    { "Triangle Slave LFO",                  "LFO slave E" },
    { "LFOSlvE",                             "LFO slave E" },
    // Level controls
    { "Level Multi (Attenuator/Phase Shift)", "Adjustable gain control" },
    { "LevMult",                             "Adjustable gain control" },
    { "Level Offset (Bias)",                 "Adjustable offset" },
    { "LevAdd",                              "Adjustable offset" },
    { "Inversable Level Shifter",            "Inverter & level shifter" },
    { "InvLevShift",                         "Inverter & level shifter" },
    // Note
    { "Note Detector",                       "Note detect" },
    { "NoteDetect",                          "Note detect" },
    { "Note Velocity Scaler",                "Note and velocity scaler" },
    { "NoteVelScal",                         "Note and velocity scaler" },
    // Misc
    { "Percussion Oscillator",               "Perc oscillator" },
    { "PercOsc",                             "Perc oscillator" },
    { "Polyphonic Area In",                  "Poly Area In" },
    { "PolyAreaIn",                          "Poly Area In" },
    { "Ring/Amplitude Modulator",            "Ring and Amp modulator" },
    { "RingMod",                             "Ring and Amp modulator" },
    { "Fixed Clock Divider",                 "Clock divider fixed" },
    { "ClkDivFix",                           "Clock divider fixed" },
    { "Wave Wrapper",                        "Wavewrapper" },
    { "WaveWrap",                            "Wavewrapper" },
};

const ModuleHelp* findModuleHelp (const juce::String& moduleName)
{
    // 1. Exact case-insensitive
    for (auto& m : getHelpDatabase())
        if (m.name.equalsIgnoreCase (moduleName))
            return &m;

    auto norm = normalizeHelpName (moduleName);

    // 2. Normalize hyphens → spaces  ("ADSR-Envelope" ↔ "ADSR Envelope")
    for (auto& m : getHelpDatabase())
        if (normalizeHelpName (m.name) == norm)
            return &m;

    // 3. Strip trailing variant letter A-E  ("Note Sequencer A" → "Note Sequencer")
    auto stripped = norm;
    if (stripped.length() > 2
        && stripped[stripped.length() - 2] == ' '
        && stripped[stripped.length() - 1] >= 'a'
        && stripped[stripped.length() - 1] <= 'e')
    {
        stripped = stripped.dropLastCharacters (2).trim();
        for (auto& m : getHelpDatabase())
            if (normalizeHelpName (m.name) == stripped)
                return &m;
    }

    // 4. Singular / plural  ("2 Output" ↔ "2 Outputs")
    for (auto& m : getHelpDatabase())
    {
        auto normHelp = normalizeHelpName (m.name);
        if (normHelp + "s" == norm || norm + "s" == normHelp)
            return &m;
    }

    // 5. Static alias table for names that differ semantically
    for (auto& [alias, target] : kHelpAliases)
        if (juce::String (alias).equalsIgnoreCase (moduleName))
            return findModuleHelp (juce::String (target));

    return nullptr;
}

} // namespace NordHelp