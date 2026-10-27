/*
 * Copyright (c) 2022 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <PktMgr/PktMgr.hpp>
#include <3rdParty/signal/signal.hpp>

/// Stands in for the serial link to the control-surface microcontroller in simulator builds
/// (SIM_BUILD). input_events obtains the implementation through
/// `Factory<SimulatorInterface>::makeObject( 0 )`; apps/kickem registers one.
class SimulatorInterface
{
public:
    virtual ~SimulatorInterface() {}

    /// Application -> simulated control surface (LEDs, pad colours, system commands).
    virtual void sendMessageToHW( CPktMgr::HWMsg_t& msg ) = 0;

    /// Simulated control surface -> application (pads, buttons, knobs, slider, system).
    sigslot::signal<CPktMgr::HWMsg_t&> hwInputReceived;
};
