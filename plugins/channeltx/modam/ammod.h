///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_CHANNELTX_MODAM_AMMOD_H_
#define PLUGINS_CHANNELTX_MODAM_AMMOD_H_

#include <QMutex>
#include <vector>
#include <iostream>
#include <fstream>

#include "dsp/basebandsamplesource.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/lowpass.h"
#include "dsp/movingaverage.h"
#include "dsp/agc.h"
#include "audio/audiofifo.h"
#include "util/message.h"

class AMMod : public BasebandSampleSource {
    Q_OBJECT

public:
    typedef enum
    {
        AMModInputNone,
        AMModInputTone,
        AMModInputFile,
        AMModInputAudio
    } AMModInputAF;

    class MsgConfigureFileSourceName : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getFileName() const { return m_fileName; }

        static MsgConfigureFileSourceName* create(const QString& fileName)
        {
            return new MsgConfigureFileSourceName(fileName);
        }

    private:
        QString m_fileName;

        MsgConfigureFileSourceName(const QString& fileName) :
            Message(),
            m_fileName(fileName)
        { }
    };

    class MsgConfigureFileSourceSeek : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getPercentage() const { return m_seekPercentage; }

        static MsgConfigureFileSourceSeek* create(int seekPercentage)
        {
            return new MsgConfigureFileSourceSeek(seekPercentage);
        }

    protected:
        int m_seekPercentage; //!< percentage of seek position from the beginning 0..100

        MsgConfigureFileSourceSeek(int seekPercentage) :
            Message(),
            m_seekPercentage(seekPercentage)
        { }
    };

    class MsgConfigureFileSourceStreamTiming : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgConfigureFileSourceStreamTiming* create()
        {
            return new MsgConfigureFileSourceStreamTiming();
        }

    private:

        MsgConfigureFileSourceStreamTiming() :
            Message()
        { }
    };

    class MsgConfigureAFInput : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        AMModInputAF getAFInput() const { return m_afInput; }

        static MsgConfigureAFInput* create(AMModInputAF afInput)
        {
            return new MsgConfigureAFInput(afInput);
        }

    private:
        AMModInputAF m_afInput;

        MsgConfigureAFInput(AMModInputAF afInput) :
            Message(),
            m_afInput(afInput)
        { }
    };

    class MsgReportFileSourceStreamTiming : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        std::size_t getSamplesCount() const { return m_samplesCount; }

        static MsgReportFileSourceStreamTiming* create(std::size_t samplesCount)
        {
            return new MsgReportFileSourceStreamTiming(samplesCount);
        }

    protected:
        std::size_t m_samplesCount;

        MsgReportFileSourceStreamTiming(std::size_t samplesCount) :
            Message(),
            m_samplesCount(samplesCount)
        { }
    };

    class MsgReportFileSourceStreamData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }
        quint32 getRecordLength() const { return m_recordLength; }

        static MsgReportFileSourceStreamData* create(int sampleRate,
                quint32 recordLength)
        {
            return new MsgReportFileSourceStreamData(sampleRate, recordLength);
        }

    protected:
        int m_sampleRate;
        quint32 m_recordLength;

        MsgReportFileSourceStreamData(int sampleRate,
                quint32 recordLength) :
            Message(),
            m_sampleRate(sampleRate),
            m_recordLength(recordLength)
        { }
    };

    //=================================================================

    AMMod();
    ~AMMod();

    void configure(MessageQueue* messageQueue, Real rfBandwidth, Real afBandwidth, float modFactor, bool audioMute, bool playLoop);

    virtual void pull(Sample& sample);
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& cmd);

    Real getMagSq() const { return m_magsq; }

private:
    class MsgConfigureAMMod : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        Real getRFBandwidth() const { return m_rfBandwidth; }
        Real getAFBandwidth() const { return m_afBandwidth; }
        float getModFactor() const { return m_modFactor; }
        bool getAudioMute() const { return m_audioMute; }
        bool getPlayLoop() const { return m_playLoop; }

        static MsgConfigureAMMod* create(Real rfBandwidth, Real afBandwidth, float modFactor, bool audioMute, bool playLoop)
        {
            return new MsgConfigureAMMod(rfBandwidth, afBandwidth, modFactor, audioMute, playLoop);
        }

    private:
        Real m_rfBandwidth;
        Real m_afBandwidth;
        float m_modFactor;
        bool m_audioMute;
        bool m_playLoop;

        MsgConfigureAMMod(Real rfBandwidth, Real afBandwidth, float modFactor, bool audioMute, bool playLoop) :
            Message(),
            m_rfBandwidth(rfBandwidth),
            m_afBandwidth(afBandwidth),
            m_modFactor(modFactor),
            m_audioMute(audioMute),
			m_playLoop(playLoop)
        { }
    };

    //=================================================================

    struct AudioSample {
        qint16 l;
        qint16 r;
    };
    typedef std::vector<AudioSample> AudioVector;

    enum RateState {
        RSInitialFill,
        RSRunning
    };

    struct Config {
        int m_outputSampleRate;
        qint64 m_inputFrequencyOffset;
        Real m_rfBandwidth;
        Real m_afBandwidth;
        float m_modFactor;
        quint32 m_audioSampleRate;
        bool m_audioMute;
        bool m_playLoop;

        Config() :
            m_outputSampleRate(-1),
            m_inputFrequencyOffset(0),
            m_rfBandwidth(-1),
            m_afBandwidth(-1),
            m_modFactor(0.2f),
            m_audioSampleRate(0),
            m_audioMute(false),
			m_playLoop(false)
        { }
    };

    //=================================================================

    Config m_config;
    Config m_running;

    NCO m_carrierNco;
    NCO m_toneNco;
    Complex m_modSample;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;
    Lowpass<Real> m_lowpass;

    Real m_magsq;
    MovingAverage<Real> m_movingAverage;
    SimpleAGC m_volumeAGC;

    AudioVector m_audioBuffer;
    uint m_audioBufferFill;

    AudioFifo m_audioFifo;
    SampleVector m_sampleBuffer;
    QMutex m_settingsMutex;

    std::ifstream m_ifstream;
    QString m_fileName;
    quint64 m_fileSize;     //!< raw file size (bytes)
    quint32 m_recordLength; //!< record length in seconds computed from file size
    int m_sampleRate;

    AMModInputAF m_afInput;

    void apply();
    void pullAF(Real& sample);
    void openFileStream();
    void seekFileStream(int seekPercentage);
};


#endif /* PLUGINS_CHANNELTX_MODAM_AMMOD_H_ */
