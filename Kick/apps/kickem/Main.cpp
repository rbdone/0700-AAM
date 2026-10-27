/*
 * Copyright (c) 2022 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * KT_KickEM: the Kick software simulator.
 *
 * Boots exactly like the monolithic Kick application (apps/kick/Main.cpp): same audio
 * interface, engine adapter and device adapter. The only difference is that input_events is
 * compiled with SIM_BUILD, so instead of a serial port the control surface is the on-screen
 * simulator around the real Kick screen (see simulator/kick_Simulator.h).
 */

#include "simulator/kick_Simulator.h"

#include "kick_ui/kick_ui.h"
#include "audio_engine/audio_engine.h"
#include "kick_device_adapter.h"
#include "kick_audio_engine_adapter.h"
#include "kick_audio_interface.h"

class KickSimulatorApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Kick Simulator"; }
    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }
    bool               moreThanOneInstanceAllowed() override { return false; }

    void initialise( const juce::String& ) override
    {
        // Same boot sequence as apps/kick/Main.cpp ...
        audioInterface.reset( new KickAudioInterface() );
        engineAdapter.reset( new KickAudioEngineAdapter( audioInterface ) );
        adapter.reset( new KickDeviceAdapter() );

        juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement( true, true );
        juce::Desktop::getInstance().setKioskModeComponent( nullptr );

        mt::getSettings();
        mt::getAudioInterface();

        window = std::make_unique<MainWindow>( getApplicationName() );
    }

    void shutdown() override
    {
        window  = nullptr;
        adapter = nullptr;
    }

    void systemRequestedQuit() override
    {
        auto mgr = juce::ModalComponentManager::getInstance();
        if( mgr->cancelAllModalComponents() )
            juce::Timer::callAfterDelay( 100, [this] { quit(); } );
        else
            quit();
    }

    void anotherInstanceStarted( const juce::String& ) override {}

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow( const juce::String& name )
            : DocumentWindow( name, juce::Colours::black, DocumentWindow::allButtons )
        {
            setUsingNativeTitleBar( true );
            setContentOwned( new sim::ScaledPanel(), true );

            // Resizable, locked to the unit's proportions. Start as large as the display allows.
            setResizable( true, true );
            setResizeLimits( 620, 625, 2480, 2500 );
            getConstrainer()->setFixedAspectRatio( (double)sim::Simulator::canvasW / (double)sim::Simulator::canvasH );
            auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
            const int h = juce::jmin( sim::Simulator::canvasH, area.getHeight() - 60 );
            centreWithSize( juce::roundToInt( h * (double)sim::Simulator::canvasW / sim::Simulator::canvasH ), h );
            setVisible( true );
        }

        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( MainWindow )
    };

private:
    std::unique_ptr<MainWindow>                window;
    std::unique_ptr<KickDeviceAdapter>         adapter;
    std::unique_ptr<KickAudioEngineAdapter>    engineAdapter;
    std::shared_ptr<contracts::AudioInterface> audioInterface;
};

START_JUCE_APPLICATION( KickSimulatorApplication )
