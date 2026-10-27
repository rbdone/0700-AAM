/*
 * Copyright (c) 2024 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#ifdef SIM_BUILD

namespace ui
{
    /// Simulator only (SIM_BUILD): an honest "not available in the simulator" panel for screens
    /// whose job is to drive device hardware (Wi-Fi, Ethernet, CV/Gate DAC, firmware update,
    /// SD / USB storage). Add it as a child of the screen or view: it covers `area` (or the whole
    /// parent when `area` is empty), swallows clicks so the device controls underneath cannot be
    /// used, and logs "SimUnavailable: <feature> shown" / "hidden" as it appears and goes, so
    /// scripts can check it.
    class SimUnavailable : public juce::Component
        , private juce::Timer
    {
    public:
        SimUnavailable( juce::String Feature, juce::String Reason, juce::Rectangle<int> Area = {} )
            : feature( std::move( Feature ) ), reason( std::move( Reason ) ), area( Area )
        {
            setTitle( feature + " is not available in the simulator" );
            setInterceptsMouseClicks( true, false );
            setAlwaysOnTop( true );
            startTimer( 250 ); // a parent screen being shown does not notify its children
        }

        void paint( juce::Graphics& g ) override
        {
            g.fillAll( ui::Theme::backgroundColour( 4 ).withAlpha( 0.94f ) );

            auto box = getLocalBounds().toFloat().withSizeKeepingCentre( juce::jmin( 640.f, getWidth() - 40.f ), juce::jmin( 190.f, getHeight() - 20.f ) );
            g.setColour( ui::Theme::backgroundColour( 1 ) );
            g.fillRoundedRectangle( box, 8.f );
            g.setColour( ui::Theme::titleColour( 1 ) );
            g.drawRoundedRectangle( box, 8.f, 1.5f );

            const float k    = juce::jlimit( 0.55f, 1.f, box.getHeight() / 190.f ); // smaller areas, smaller type
            auto        text = box.reduced( 24.f * k, 18.f * k );
            g.setFont( juce::Font( 26.f * k, juce::Font::bold ) );
            g.drawFittedText( feature, text.removeFromTop( 34.f * k ).toNearestInt(), juce::Justification::centred, 1 );
            g.setColour( ui::Theme::textColour( 1 ) );
            g.setFont( juce::Font( 20.f * k ) );
            g.drawFittedText( "Not available in the simulator", text.removeFromTop( 30.f * k ).toNearestInt(), juce::Justification::centred, 1 );
            g.setColour( ui::Theme::textColour( 1 ).withAlpha( 0.75f ) );
            g.setFont( juce::Font( 16.f * k ) );
            g.drawFittedText( reason, text.toNearestInt(), juce::Justification::centred, 5, 0.9f );
        }

        void parentHierarchyChanged() override { fit(); }
        void parentSizeChanged() override { fit(); }

        // Swallow touches: nothing under this panel may reach the device code.
        void mouseDown( const juce::MouseEvent& ) override {}

    private:
        void fit()
        {
            if( auto* p = getParentComponent() )
                setBounds( area.isEmpty() ? p->getLocalBounds() : area );
        }

        void timerCallback() override
        {
            const bool showing = isShowing();
            if( showing != wasShowing )
                juce::Logger::writeToLog( "SimUnavailable: " + feature + ( showing ? " shown" : " hidden" ) );
            wasShowing = showing;
        }

        juce::String         feature, reason;
        juce::Rectangle<int> area;
        bool                 wasShowing{ false };
    };
}

#endif
