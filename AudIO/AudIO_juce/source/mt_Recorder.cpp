/*
 * Copyright (c) 2022 MUSIC Tribe
 *
 * This file is part of AudIO.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

namespace mt
{
    Recorder::Recorder( AudIO::Design& D, AudIO::BusType Type )
        : AudIO::Object( 2, 2, 0 )
        , AudIO::DawRouting( D, Type, AudIO::BusWidth::Stereo )
    {
        setName( "Recorder" );
        inMeterData.resize( 2 );
        outMeterData.resize( 2 );
    }

    Recorder::~Recorder() = default;

    void Recorder::notify( const std::function<void()>& Fn )
    {
        if( Fn )
            juce::MessageManager::callAsync( [Fn] { Fn(); } );
    }

    void Recorder::notifyLength()
    {
        if( onRecorderLengthChanged )
        {
            auto fn      = onRecorderLengthChanged;
            auto seconds = sampleRate > 0 ? (double)recordedSamples.load() / sampleRate : 0.0;
            juce::MessageManager::callAsync( [fn, seconds] { fn( seconds ); } );
        }
    }

    void Recorder::prepareToPlay( uint32_t Samplerate, uint32_t Blocksize )
    {
        AudIO::Object::prepareToPlay( Samplerate, Blocksize );
        if( Samplerate > 0 && (double)Samplerate != sampleRate )
        {
            sampleRate = (double)Samplerate;
            if( lengthSeconds > 0.0 )
                setLength( lengthSeconds );
        }
    }

    void Recorder::teardown()
    {
        recState.store( RecIdle );
        playing.store( false );
        AudIO::Object::teardown();
    }

    void Recorder::setLength( double TimeInSeconds )
    {
        const juce::SpinLock::ScopedLockType sl( lock );

        lengthSeconds       = juce::jmax( 0.0, TimeInSeconds );
        const int numSample = (int)std::llround( lengthSeconds * sampleRate );
        audioBuffer.setSize( 2, juce::jmax( 0, numSample ), true, true, true );
        if( recordedSamples.load() > numSample )
            recordedSamples.store( numSample );
        if( writePos.load() > numSample ) writePos.store( 0 );
        if( readPos.load() > numSample ) readPos.store( 0 );
    }

    void Recorder::reset()
    {
        const juce::SpinLock::ScopedLockType sl( lock );
        recState.store( RecIdle );
        playing.store( false );
        audioBuffer.clear();
        recordedSamples.store( 0 );
        writePos.store( 0 );
        readPos.store( 0 );
        progress = 0.0;
        hasUndo  = false;
    }

    void Recorder::takeUndoSnapshot()
    {
        // Caller holds the lock.
        undoBuffer.makeCopyOf( audioBuffer, true );
        undoSamples = recordedSamples.load();
        hasUndo     = true;
    }

    void Recorder::startRecording( bool FromBeginning, bool SkipThresholdDetection )
    {
        if( audioBuffer.getNumSamples() == 0 )
        {
            juce::Logger::writeToLog( "Recorder: startRecording called with no length set" );
            return;
        }

        {
            const juce::SpinLock::ScopedLockType sl( lock );
            takeUndoSnapshot();
            if( FromBeginning )
                writePos.store( 0 );
            if( !overdub.load() && FromBeginning )
            {
                audioBuffer.clear();
                recordedSamples.store( 0 );
            }
        }

        const bool armOnly = !SkipThresholdDetection && threshold.load() > 0.f;
        recState.store( armOnly ? RecArmed : RecRecording );
        if( !armOnly )
            notify( onRecorderStarted );
    }

    void Recorder::stopRecording()
    {
        if( recState.exchange( RecIdle ) != RecIdle )
        {
            notify( onRecorderStopped );
            notifyLength();
        }
    }

    void Recorder::startPreview( double FromSeconds )
    {
        if( recordedSamples.load() == 0 )
            return;
        auto pos = (int)std::llround( juce::jmax( 0.0, FromSeconds ) * sampleRate );
        readPos.store( juce::jlimit( 0, juce::jmax( 0, recordedSamples.load() - 1 ), pos ) );
        if( !playing.exchange( true ) )
            notify( onRecorderPreviewStarted );
    }

    void Recorder::stopPreview()
    {
        if( playing.exchange( false ) )
            notify( onRecorderPreviewStopped );
    }

    AudIO::MeterData Recorder::readInputMeter()
    {
        return meter.getMeterIn( inMeterData );
    }

    AudIO::MeterData Recorder::readOutputMeter()
    {
        return meter.getMeterOut( outMeterData );
    }

    void Recorder::saveRecording( const std::string& FilePath )
    {
        saveToFile( juce::File( juce::String( FilePath ) ) );
    }

    bool Recorder::saveToFile( const juce::File& File )
    {
        const juce::SpinLock::ScopedLockType sl( lock );

        const int numSamples = recordedSamples.load();
        if( numSamples <= 0 )
            return false;

        File.deleteFile();
        std::unique_ptr<juce::FileOutputStream> stream( File.createOutputStream() );
        if( !stream || !stream->openedOk() )
            return false;

        juce::WavAudioFormat                    format;
        std::unique_ptr<juce::AudioFormatWriter> writer( format.createWriterFor( stream.get(), sampleRate, (unsigned int)audioBuffer.getNumChannels(), 24, {}, 0 ) );
        if( !writer )
            return false;
        stream.release(); // writer owns it now

        return writer->writeFromAudioSampleBuffer( audioBuffer, 0, numSamples );
    }

    void Recorder::process( AudIO::Buffer<float>& Buffer )
    {
        const auto n = (int)Buffer.getBlockSize();
        Buffer.clearOutputs();

        if( !enabled.load() || n <= 0 )
            return;

        meter.processIn( Buffer );

        const float* inL = Buffer.getInputBuffer( 0 );
        const float* inR = Buffer.getInputBuffer( 1 );
        float*       outL = Buffer.getOutputBuffer( 0 );
        float*       outR = Buffer.getOutputBuffer( 1 );
        if( !inL ) inL = inR;
        if( !inR ) inR = inL;
        if( !outL || !outR )
            return;

        const juce::SpinLock::ScopedTryLockType stl( lock );
        if( stl.isLocked() )
        {
            const int   total   = audioBuffer.getNumSamples();
            const bool  forward = direction.load() == FORWARD;
            const bool  loop    = looping.load();
            const auto  recMode = (Mode)mode.load();
            const float thr     = threshold.load();

            // --- Threshold-armed start -------------------------------------------------
            if( recState.load() == RecArmed && inL )
            {
                for( int i = 0; i < n; ++i )
                {
                    if( std::abs( inL[i] ) >= thr || ( inR && std::abs( inR[i] ) >= thr ) )
                    {
                        recState.store( RecRecording );
                        notify( onRecorderStarted );
                        break;
                    }
                }
            }

            // --- Recording ------------------------------------------------------------
            if( recState.load() == RecRecording && total > 0 && inL )
            {
                auto*      bufL = audioBuffer.getWritePointer( 0 );
                auto*      bufR = audioBuffer.getWritePointer( 1 );
                const bool add  = overdub.load();
                int        wp   = writePos.load();
                int        rec  = recordedSamples.load();

                for( int i = 0; i < n; ++i )
                {
                    float l = 0.f, r = 0.f;
                    switch( recMode )
                    {
                        case L_MONO: l = r = inL[i]; break;
                        case R_MONO: l = r = inR[i]; break;
                        case LR_MONO: l = r = 0.5f * ( inL[i] + inR[i] ); break;
                        case L_STEREO: l = r = inL[i]; break;
                        case R_STEREO: l = r = inR[i]; break;
                        case LR_STEREO:
                        default:
                            l = inL[i];
                            r = inR[i];
                            break;
                    }

                    const int idx = forward ? wp : ( total - 1 - wp );
                    if( add )
                    {
                        bufL[idx] += l;
                        bufR[idx] += r;
                    }
                    else
                    {
                        bufL[idx] = l;
                        bufR[idx] = r;
                    }

                    if( ++wp >= total )
                    {
                        rec = total;
                        if( loop )
                        {
                            wp = 0;
                        }
                        else
                        {
                            recState.store( RecIdle );
                            notify( onRecorderStopped );
                            break;
                        }
                    }
                }
                rec = juce::jmax( rec, wp );
                writePos.store( wp );
                if( rec != recordedSamples.load() )
                {
                    recordedSamples.store( rec );
                    notifyLength();
                }
                if( !playing.load() )
                    progress = total > 0 ? (double)wp / (double)total : 0.0;
            }

            // --- Playback -------------------------------------------------------------
            const int recorded = recordedSamples.load();
            if( playing.load() && recorded > 0 )
            {
                const auto* bufL = audioBuffer.getReadPointer( 0 );
                const auto* bufR = audioBuffer.getReadPointer( 1 );
                const float g    = gain.load();
                const float p    = ( pan.load() + 1.f ) * 0.25f * juce::MathConstants<float>::pi; // constant power
                const float gL   = g * std::cos( p );
                const float gR   = g * std::sin( p );
                int         rp   = readPos.load();

                for( int i = 0; i < n; ++i )
                {
                    const int idx = forward ? rp : ( recorded - 1 - rp );
                    outL[i] += bufL[idx] * gL;
                    outR[i] += bufR[idx] * gR;

                    if( ++rp >= recorded )
                    {
                        if( loop )
                        {
                            rp = 0;
                        }
                        else
                        {
                            playing.store( false );
                            notify( onRecorderPreviewStopped );
                            break;
                        }
                    }
                }
                readPos.store( rp );
                progress = (double)rp / (double)recorded;
            }
        }

        // --- Monitoring ---------------------------------------------------------------
        if( monitor.load() && inL )
        {
            const float mg = monitorGain.load();
            for( int i = 0; i < n; ++i )
            {
                outL[i] += inL[i] * mg;
                outR[i] += inR[i] * mg;
            }
        }

        processOutput( Buffer );
        meter.processOut( Buffer );
    }

    void Looper::undo()
    {
        const juce::SpinLock::ScopedLockType sl( lock );
        if( !hasUndo )
            return;
        std::swap( audioBuffer, undoBuffer );
        const int rec = recordedSamples.exchange( undoSamples );
        undoSamples   = rec;
        if( readPos.load() >= recordedSamples.load() ) readPos.store( 0 );
        if( writePos.load() >= audioBuffer.getNumSamples() ) writePos.store( 0 );
        notifyLength();
    }
} // namespace mt
