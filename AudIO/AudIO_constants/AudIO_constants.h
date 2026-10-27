/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 AudIO_constants
  vendor:             MUSIC Tribe
  version:            0.1.0
  name:               AudIO constants (compatibility shim)
  description:        Parameter range constants Kick expects from the internal AudIO_constants module, plus a forward to the public AudIO module.
  website:
  license:            AGPLv3
  dependencies:       AudIO

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

/*
 * Copyright (c) 2022 MUSIC Tribe
 *
 * This file is part of AudIO.
 *
 * AudIO is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * WHY THIS FILE EXISTS
 *
 * Kick/modules/common/common.h includes <AudIO_constants/AudIO_constants.h>, a module
 * from the internal AudIO repository that was not part of the public release. Kick's
 * own `modules/common/source/kick_Constants.h` defines most parameter ranges (BPM, BAR,
 * SEMI, FINE, TRANSPOSE, MUTEGROUP, ...) but five groups, and one sentinel, were only
 * ever defined in AudIO_constants. They are reconstructed here.
 *
 * The VALUES below are inferred from how Kick and AudIO use them, not copied from the
 * original file. See docs/DEVELOPMENT.md for the reasoning behind each one.
 */

#pragma once
#ifndef AUDIO_CONSTANTS_H_INCLUDE
    #define AUDIO_CONSTANTS_H_INCLUDE

    #include <AudIO/AudIO.h>

///
/// Engine geometry. These feed AudIO::DawEngine and the audio device.
///
/// AUDIO_BLOCK_SIZE must not exceed the AudIO memory-pool block (BLOCK_SIZE in Daw/DawEngine.cpp,
/// 480 samples = 10 ms at 48 kHz); DawEngine::startAudioDevice clamps to it anyway.
/// Submix count matches the fixed "Submix 1..4" list in the MIDI control UI.
///
    #define AUDIO_SAMPLE_RATE      ( 48000 )
    #define AUDIO_BLOCK_SIZE       ( 480 )
    #define AUDIO_NUM_INPUTS       ( 2 )
    #define AUDIO_NUM_OUTPUTS      ( 2 )
    #define AUDIO_NUM_SEND_RETURNS ( 4 )
    #define AUDIO_NUM_SUBMIX       ( 4 )
    #define AUDIO_NUM_AUX          ( 2 )

/// Decibel floor used by meters. Matches AudIO::utils::linearTolog(), which returns -100 for silence.
namespace DB
{
    static constexpr float MIN{ -100.f };
    static constexpr float MAX{ 0.f };
}

/// 7-bit MIDI value range. Used as the generic 0..127 range for levels, velocities and
/// note numbers all over the UI, e.g. `juce::jlimit( MIDI::MIN, MIDI::MAX, note )`.
namespace MIDI
{
    static constexpr int   MIN{ 0 };
    static constexpr int   MAX{ 127 };
    static constexpr int   NUMERIC_MAX{ 128 };     // exclusive upper bound, e.g. `for( i = MIN; i < NUMERIC_MAX; ... )`
    static constexpr int   MIDDLE_C{ 60 };
    static constexpr int   NOTE_DEFAULT{ 60 };    // default note number of a new MIDI note event
    static constexpr int   STEP{ 1 };
    static constexpr int   DEFAULT{ 0 };           // default LFO/modulation amount; also used as a "continuous" slider interval
    static constexpr int   LEVEL_DEFAULT{ 100 };   // default level for pads/tracks and the upper bound of level Q-Link knobs
    static constexpr float SCALER{ 1.0f / 127.0f }; // MIDI value -> 0..1 (see kick_common_utils.cpp)
}

/// MIDI channel as shown to the user (1-based).
namespace MIDI_CH
{
    static constexpr int MIN{ 1 };
    static constexpr int MAX{ 16 };
    static constexpr int STEP{ 1 };
    static constexpr int DEFAULT{ 1 };
}

/// Voices per program. ISynthObject allocates POLYPHONY::MAX voices up front.
namespace POLYPHONY
{
    static constexpr int MIN{ 1 };
    static constexpr int MAX{ 64 };
    static constexpr int STEP{ 1 };
    static constexpr int DEFAULT{ 16 };
}

/// Linear gain fed to AudIO::Panner::setParameters( pan, gain ). Sends default to GAIN::MIN.
namespace GAIN
{
    static constexpr double MIN{ 0.0 };
    static constexpr double MAX{ 1.0 };
    static constexpr double STEP{ 0.01 };
    static constexpr double DEFAULT{ 1.0 };
    static constexpr double ROOT_2{ 0.7071067811865476 }; // -3 dB; default metronome gain (kick_settings.cpp)
}

/// Pan position. AudIO::Panner documents its range as -1.0 (left) .. 1.0 (right).
namespace PAN
{
    static constexpr double MIN{ -1.0 };
    static constexpr double MAX{ 1.0 };
    static constexpr double STEP{ 0.01 };
    static constexpr double DEFAULT{ 0.0 };
}

/// Sentinel stored in a juce::Value that is not attached to anything yet
/// (see kick_ui_controls/source/kick_attachments.h).
static constexpr int UNKNOWN_VALUE{ -1 };

#endif // AUDIO_CONSTANTS_H_INCLUDE
