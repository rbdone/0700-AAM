/*
 * Copyright (c) 2022 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <kick_ui/kick_ui.h>
#include <contracts/dcl/dcl_contracts.h>
#include "kick_SimulatorInterfaceImpl.h"

namespace sim
{
    using DCC = contracts::DeviceControlContracts;

    /// Maps a contracts::DeviceControlContracts::PadColourIds value to a display colour.
    juce::Colour padColourFor( int colourId );

    /// Sends a control-surface button packet (BTN_PRESSED / BTN_RELEASED) for a BtnLedSequence id.
    void sendButtonPacket( int Id, int BtnState );

    /// The SHIFT key. On the unit the control-surface firmware reports a key pressed while SHIFT is
    /// held as its secondary function (the caption printed under the key), e.g. SHIFT + MAIN is
    /// reported as EN_GRID. The simulator does the same.
    void setShiftHeld( bool Held ); // sends the SHIFT packet and updates the app's shift state
    bool isShiftHeld();
    int  shiftedIdFor( int Id ); // secondary function of a key, or Id itself if it has none

    //==============================================================================================
    /// Draws juce::Slider rotaries as the unit's knobs.
    class KnobLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        enum class Style
        {
            Pot, // MASTER / REC: black pot with a white pointer
            Encoder, // P1-P4: small metal-capped encoder
            Jog // the centre jog wheel: knurled knob inside a glowing ring
        };

        explicit KnobLookAndFeel( Style S );
        void drawRotarySlider( juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float startAngle, float endAngle, juce::Slider& slider ) override;

        /// Draws a knob of the given style with its pointer at `angle` (radians, 0 = up).
        static void drawKnob( juce::Graphics& g, juce::Rectangle<float> area, float angle, Style S, bool Glow = true );

    private:
        Style style;
    };

    //==============================================================================================
    /// An endless encoder (P1-P4, the jog wheel). Dragging up/right turns it clockwise; the pointer
    /// angle wraps around for ever and every `pixelsPerStep` of travel emits one step through
    /// onStep( +1 / -1 ... ). The mouse wheel steps it too.
    class EndlessKnob : public juce::Component
    {
    public:
        explicit EndlessKnob( KnobLookAndFeel::Style S, float PixelsPerStep = 6.f );

        std::function<void( int Steps )> onStep;

        void paint( juce::Graphics& g ) override;
        void mouseDown( const juce::MouseEvent& e ) override;
        void mouseDrag( const juce::MouseEvent& e ) override;
        void mouseWheelMove( const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel ) override;

    private:
        void turn( float Pixels );

        const KnobLookAndFeel::Style style;
        const float                  pixelsPerStep;
        float                        angle{ 0.f }, accumulated{ 0.f };
        juce::Point<float>           lastPos;
    };

    //==============================================================================================
    /// One of the 16 velocity pads. Click position sets the velocity (top = soft, bottom = hard).
    class PadButton : public juce::Button
    {
    public:
        explicit PadButton( int Index );

        void set( int ColourId, int State );
        void paintButton( juce::Graphics& g, bool highlighted, bool down ) override;
        void mouseDown( const juce::MouseEvent& e ) override;
        void buttonStateChanged() override;

    private:
        void send( int BtnState, int Velocity );

        const int index;
        int       colourId{ DCC::PadColourTrans };
        int       state{ DCC::StateOff };
        int       velocity{ 100 }; // set from the click position; accessibility clicks keep the default
        bool      wasDown{ false };
    };

    //==============================================================================================
    /// A control-surface button: dark key with an LED-lit caption, or a round transport key with
    /// an icon. Momentary buttons send BTN_PRESSED on press; toggles (SHIFT) send PRESSED /
    /// RELEASED as they switch on / off.
    class SurfaceButton : public juce::Button
    {
    public:
        enum class Shape
        {
            Key,
            Round
        };
        enum class Icon
        {
            None,
            Record,
            Overdub,
            Stop,
            Play,
            PlayStart,
            Back,
            Copy,
            Minus,
            Plus
        };

        SurfaceButton( const juce::String& Caption, int Id, juce::Colour LedColour, bool Toggle = false, Shape S = Shape::Key, Icon I = Icon::None );

        int  getId() const noexcept { return id; }
        void setLed( bool On );
        void paintButton( juce::Graphics& g, bool highlighted, bool down ) override;
        void buttonStateChanged() override;
        void clicked() override;

    private:
        void drawIcon( juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c ) const;

        juce::String caption;
        const int    id;
        juce::Colour ledColour;
        const bool   toggle;
        const Shape  shape;
        const Icon   icon;
        bool         led{ false }, wasDown{ false };
    };

    //==============================================================================================
    /// Small printed caption under a key (the SHIFT function). When given an id it is clickable and
    /// sends that id directly, which is handy when developing: no need to hold SHIFT.
    class Caption : public juce::Button
    {
    public:
        Caption( const juce::String& Text, int Id = -1, juce::Justification J = juce::Justification::centred );
        void paintButton( juce::Graphics& g, bool highlighted, bool down ) override;
        void clicked() override;

    private:
        juce::String        text;
        const int           id;
        juce::Justification just;
    };

    //==============================================================================================
    /// The whole front panel at the proportions of the physical unit, in a fixed canvas of
    /// canvasW x canvasH units. The real Kick screen (kick_ui::KickHome, 1024x600) is embedded
    /// scaled into the display cut-out. ScaledPanel below scales the canvas to the window.
    class Simulator : public juce::Component,
                      private juce::Timer
    {
    public:
        Simulator();
        ~Simulator() override;

        void paint( juce::Graphics& g ) override;
        void resized() override;

        static constexpr int canvasW = 1240, canvasH = 1250;

    private:
        void loadLogo();

        void timerCallback() override;
        void handleFromApp( CPktMgr::HWMsg_t& msg );
        void handlePadMessage( const CPktMgr::HWMsg_t& msg );
        void handleLedMessage( const CPktMgr::HWMsg_t& msg );

        SurfaceButton& addButton( const juce::String& Caption, int Id, juce::Colour Led, bool Toggle = false, SurfaceButton::Shape S = SurfaceButton::Shape::Key, SurfaceButton::Icon I = SurfaceButton::Icon::None );
        Caption&       addCaption( const juce::String& Text, int Id = -1, juce::Justification J = juce::Justification::centred );
        void           setupPot( juce::Slider& Knob, double Value );
        void           setupEncoder( EndlessKnob& Knob, int KnobId, int Cmd );

        void sendKnob( int KnobId, int Cmd, int Delta );
        void sendSystem( int Cmd, std::vector<int> Data );

        // Looks...
        KnobLookAndFeel potLnf{ KnobLookAndFeel::Style::Pot };

        // Controls...
        std::vector<std::unique_ptr<PadButton>>     pads;
        std::vector<std::unique_ptr<SurfaceButton>> buttons;
        std::vector<std::unique_ptr<Caption>>       captions;
        std::map<int, SurfaceButton*>               buttonsById;

        juce::Slider master, rec, slider;
        juce::Image  logo; // "behringer" mark printed above the screen
        EndlessKnob  qlinks[4]{ EndlessKnob{ KnobLookAndFeel::Style::Encoder }, EndlessKnob{ KnobLookAndFeel::Style::Encoder }, EndlessKnob{ KnobLookAndFeel::Style::Encoder }, EndlessKnob{ KnobLookAndFeel::Style::Encoder } };
        EndlessKnob  jog{ KnobLookAndFeel::Style::Jog, 4.f };

        int commTicks{ 0 };

        sigslot::scoped_connection fromAppConnection;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( Simulator )
    };

    //==============================================================================================
    /// Window content: keeps the panel at its canvas size and scales it to fit, centred.
    class ScaledPanel : public juce::Component
    {
    public:
        ScaledPanel();
        void resized() override;
        void paint( juce::Graphics& g ) override { g.fillAll( juce::Colours::black ); }

    private:
        Simulator panel;
    };
} // namespace sim
