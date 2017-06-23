/*
    Csound.hxx:

    Copyright (C) 2016 Michael Gogins

    This file is part of Csound.

    The Csound Library is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Csound is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Csound; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA

    As a special exception, if other files instantiate templates or
    use macros or inline functions from this file, this file does not
    by itself cause the resulting executable or library to be covered
    by the GNU Lesser General Public License. This exception does not
    however invalidate any other reasons why the library or executable
    file might be covered by the GNU Lesser General Public License.
*/

#ifndef __CSOUND2_HPP__
#define __CSOUND2_HPP__

#include "csound.h"
#if defined(_MSC_VER)
#include <concurrent_queue.h>
#else
#include <boost/lockfree/queue.hpp>
#endif
#include <cstring>
#include <string>
#include <string.h>
#include <thread>
#include <vector>

#if defined(__cplusplus)

namespace csound
{

struct ArgParser {
    char *buffer;
    std::vector<char *> argv;
    ArgParser(const std::string buffer_) :
        buffer(0)
    {
        const char *delimiters = " \t\r\n";
        buffer = strdup(buffer_.c_str());
        char *token = std::strtok(buffer, delimiters);
        if (token) {
            argv.push_back(token);
        }
        argv.push_back(token);
        while (token != nullptr) {
            token = std::strtok(nullptr, delimiters);
            if (token) {
                argv.push_back(token);
            }
        }
    }
    ~ArgParser()
    {
        argv.clear();
        if (buffer) {
            free(buffer);
        }
    }
};

struct CsoundThreadEvent {
    CsoundThreadEvent()
    {
    };
    virtual ~CsoundThreadEvent()
    {
    }
    virtual int operator()(CSOUND *csound)
    {
        return -1;
    }
};

struct CsoundThreadEventScoreEvent : CsoundThreadEvent {
    char opcode;
    std::vector<MYFLT> pfields;
    CsoundThreadEventScoreEvent(char opcode_, MYFLT *pfields_, int pfield_count)
    {
        opcode = opcode_;
        for (int i = 0; i < pfield_count; ++pfield_count) {
            pfields.push_back(pfields[i]);
        }
    }
    virtual int operator()(CSOUND *csound)
    {
        return csoundScoreEvent(csound, opcode, pfields.data(), pfields.size());
    }
};

struct CsoundThreadEventScore : CsoundThreadEvent {
    std::string score;
    CsoundThreadEventScore(std::string score_)
    {
        score = score_;
    }
    virtual int operator()(CSOUND *csound)
    {
        return csoundReadScore(csound, score.c_str());
    }
};

/**
 * This C++ header file only library declares and defines all native
 * functionality for the JavaScript Csound APIs.  This library can of course
 * also can be used for other purposes. Csound is run in a separate thread
 * and some requests to Csound are issued via one lockfree queue that is
 * dequeued only between calls to csoundPerformKsmps.
 *
 * Please note, this class is not completely thread-safe. Initialization,
 * creation, compilation, and destruction are not thread-safe. Once
 * Csound is running, most Csound API calls, all channels, score handling,
 * and so on are thread-safe.
 */
class PUBLIC CSound
{
protected:
    CSOUND *csound;
    char is_running;
#if defined(_MSC_VER)
    concurrency::concurrent_queue<CsoundThreadEvent *> csound_event_queue;
#else
    boost::lockfree::queue<CsoundThreadEvent *, boost::lockfree::fixed_sized<false> > csound_event_queue(0);
#endif
    std::thread *performance_thread;
public:
    virtual ~CSound();
    CSound();
    virtual int compileCsd(std::string pathname);
    virtual int compileCsdText(std::string csd);
    virtual int compileOrc(std::string orc);
    virtual CSOUND *create();
    virtual CSOUND *create(void *userdata);
    virtual void destroy();
    virtual MYFLT evalCode(std::string orc);
    virtual MYFLT get0dBFS() const;
    static int getAPIVersion();
    virtual MYFLT getControlChannel(std::string channel) const;
    virtual int64_t getCurrentTimeSamples() const;
    virtual CSOUND *getCsound();
    virtual std::string getEnv(std::string name) const;
    virtual uint32_t getKsmps() const;
    virtual uint32_t getNchnls() const;
    virtual uint32_t getNchnlsInput() const;
    virtual std::string getOutputName() const;
    virtual MYFLT getScoreOffsetSeconds() const;
    virtual MYFLT getScoreTime() const;
    virtual MYFLT getSr() const;
    virtual std::string getStringChannel(std::string channel) const;
    static int getVersion();
    static void initialize(int flags);
    virtual bool isPerforming() const;
    virtual bool isScorePending() const;
    virtual void message(std::string message);
    virtual std::thread *perform();
    virtual int performanceThreadRoutine();
    virtual void readScore(std::string score);
    virtual void rewindScore();
    virtual int runUtility(std::string command);
    virtual void scoreEvent(char opcode, MYFLT *pfields, int pfield_count);
    virtual void setCsound(CSOUND *csound);
    virtual void setControlChannel(std::string name, MYFLT value);
    virtual int setGlobalEnv(std::string name, std::string value);
    virtual void setInput(std::string name);
    virtual int setOption(std::string token);
    virtual void setOutput(std::string name, std::string type, std::string format);
    virtual void setScoreOffsetSeconds(MYFLT seconds);
    virtual void setScorePending(bool is_pending);
    virtual void setStringChannel(std::string name, std::string value);
    virtual int stop();
    virtual MYFLT tableGet(int table, int index) const;
    virtual int tableLength(int table) const;
    virtual void tableSet(int table, int index, MYFLT value);
};

inline int CSound::performanceThreadRoutine()
{
    if (is_running) {
        return is_running;
    }
    is_running = 1;
    int is_finished = 0;
    CsoundThreadEvent *event = 0;
    while((is_running == 1) && (is_finished == 0)) {
#if defined(_MSC_VER)
        while (csound_event_queue.try_pop(event)) {
#else
        while (csound_event_queue.pop(event)) {
#endif
            (*event)(csound);
            delete event;
        }
        is_finished = csoundPerformKsmps(csound);
    }
    int result = csoundCleanup(csound);
    csoundReset(csound);
#if defined(_MSC_VER)
    while (csound_event_queue.try_pop(event)) {
#else
    while (csound_event_queue.pop(event)) {
#endif
        delete event;
    }
    return result;
}

inline CSound::~CSound()
{
    destroy();
}

inline CSound::CSound() : csound(0), performance_thread(0), is_running(0)
{
}

inline int CSound::compileCsd(std::string pathname)
{
    return csoundCompileCsd(csound, const_cast<char *>(pathname.c_str()));
}

inline int CSound::compileCsdText(std::string csd)
{
    return csoundCompileCsd(csound, const_cast<char *>(csd.c_str()));
}

inline int CSound::compileOrc(std::string orc)
{
    return csoundCompileOrc(csound, orc.c_str());
}

inline CSOUND *CSound::create()
{
    return create(0);
}

inline CSOUND *CSound::create(void *userdata)
{
    destroy();
    csound = csoundCreate(userdata);
    return csound;
}

inline void CSound::destroy()
{
    if (csound) {
        stop();
        csoundDestroy(csound);
    }
}

inline MYFLT CSound::evalCode(std::string orc)
{
    return csoundEvalCode(csound, orc.c_str());
}

inline MYFLT CSound::get0dBFS() const
{
    return csoundGet0dBFS(csound);
}

int CSound::getAPIVersion()
{
    return csoundGetAPIVersion();
}

inline MYFLT CSound::getControlChannel(std::string channel) const
{
    MYFLT value = 0;
    int error = 0;
    value = csoundGetControlChannel(csound, channel.c_str(), &error);
    if (error) {
        return error;
    } else {
        return value;
    }
}

inline int64_t CSound::getCurrentTimeSamples() const
{
    return csoundGetCurrentTimeSamples(csound);
}

inline CSOUND *CSound::getCsound()
{
    return csound;
}

inline std::string CSound::getEnv(std::string name) const
{
    return csoundGetEnv(csound, name.c_str());
}

inline uint32_t CSound::getKsmps() const
{
    return csoundGetKsmps(csound);
}

inline uint32_t CSound::getNchnls() const
{
    return csoundGetNchnls(csound);
}

inline uint32_t CSound::getNchnlsInput() const
{
    return csoundGetNchnlsInput(csound);
}

inline std::string CSound::getOutputName() const
{
    return csoundGetOutputName(csound);
}

inline MYFLT CSound::getScoreOffsetSeconds() const
{
    return csoundGetScoreOffsetSeconds(csound);
}

inline MYFLT CSound::getScoreTime() const
{
    return csoundGetScoreTime(csound);
}

inline MYFLT CSound::getSr() const
{
    return csoundGetSr(csound);
}

inline std::string CSound::getStringChannel(std::string channel) const
{
    char buffer[0x1000];
    buffer[0] = '\0';
    csoundGetStringChannel(csound, channel.c_str(), buffer);
    return buffer;
}

int CSound::getVersion()
{
    return csoundGetVersion();
}

void CSound::initialize(int flags)
{
    csoundInitialize(flags);
}

inline bool CSound::isPerforming() const
{
    return is_running;
}

inline bool CSound::isScorePending() const
{
    return csoundIsScorePending(csound);
}

inline void CSound::message(std::string message)
{
    csoundMessage(csound, message.c_str());
}

inline std::thread *CSound::perform()
{
    stop();
    performance_thread = new std::thread(&CSound::performanceThreadRoutine, this);
    return performance_thread;
}

inline void CSound::readScore(std::string score)
{
    csound_event_queue.push(new CsoundThreadEventScore(score));
}

inline void CSound::rewindScore()
{
    csoundRewindScore(csound);
}

inline int CSound::runUtility(std::string command)
{
    ArgParser arg_parser(command);
    return csoundRunUtility(csound, arg_parser.argv[0], arg_parser.argv.size() - 1, &arg_parser.argv[1]);
}

inline void CSound::scoreEvent(char opcode, MYFLT *pfields, int pfield_count)
{
    csound_event_queue.push(new CsoundThreadEventScoreEvent(opcode, pfields, pfield_count));
}

inline void CSound::setCsound(CSOUND *csound_)
{
    csound = csound_;
}

inline void CSound::setControlChannel(std::string name, MYFLT value)
{
    csoundSetControlChannel(csound, name.c_str(), value);
}

inline int CSound::setGlobalEnv(std::string name, std::string value)
{
    return csoundSetGlobalEnv(name.c_str(), value.c_str());
}

inline void CSound::setInput(std::string token)
{
    csoundSetInput(csound, const_cast<char *>(token.c_str()));
}

inline int CSound::setOption(std::string token)
{
    return csoundSetOption(csound, const_cast<char *>(token.c_str()));
}

inline void CSound::setOutput(std::string name, std::string type, std::string format)
{
    csoundSetOutput(csound, const_cast<char *>(name.c_str()), const_cast<char *>(type.c_str()), const_cast<char *>(format.c_str()));
}

inline void CSound::setScoreOffsetSeconds(MYFLT seconds)
{
    csoundSetScoreOffsetSeconds(csound, seconds);
}

inline void CSound::setScorePending(bool is_pending)
{
    csoundSetScorePending(csound, is_pending);
}

inline void CSound::setStringChannel(std::string name, std::string value)
{
    csoundSetStringChannel(csound, name.c_str(), const_cast<char *>(value.c_str()));
}

inline int CSound::stop()
{
    int temp = is_running;
    is_running = 0;
    if (performance_thread) {
        if (performance_thread->joinable()) {
            performance_thread->join();
        }
        delete performance_thread;
        performance_thread = 0;
    }
    return temp;
}

inline MYFLT CSound::tableGet(int table, int index) const
{
    return csoundTableGet(csound, table, index);
}

inline int CSound::tableLength(int table) const
{
    return csoundTableLength(csound, table);
}

inline void CSound::tableSet(int table, int index, MYFLT value)
{
    csoundTableSet(csound, table, index, value);
}

};

#endif  // __cplusplus

#endif  // __CSOUND_HPP__
