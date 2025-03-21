/*
 * DISTRHO Plugin Framework (DPF)
 * Copyright (C) 2012-2024 Filipe Coelho <falktx@falktx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose with
 * or without fee is hereby granted, provided that the above copyright notice and this
 * permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef EXAMPLE_PLUGIN_METERS_HPP
#define EXAMPLE_PLUGIN_METERS_HPP

#include "DistrhoPlugin.hpp"
#include <cstddef>
#include <cstdint>

#include <extra/RingBuffer.hpp>

struct RbMsg {
    float buffer[48000 * sizeof(float) * 2];
    uint32_t length;
};


START_NAMESPACE_DISTRHO

// -----------------------------------------------------------------------------------------------------------

/**
  Plugin to demonstrate parameter outputs using meters.
 */
class Spectrogram : public Plugin
{
public:
    Spectrogram();

    HeapRingBuffer myHeapBuffer;

protected:
    uint32_t numBuffers;
    RbMsg rbmsg;
   /* --------------------------------------------------------------------------------------------------------
    * Information */

   /**
      Get the plugin label.
      A plugin label follows the same rules as Parameter::symbol, with the exception that it can start with numbers.
    */
    const char* getLabel() const override;

   /**
      Get an extensive comment/description about the plugin.
    */
    const char* getDescription() const override;

   /**
      Get the plugin author/maker.
    */
    const char* getMaker() const override;

   /**
      Get the plugin homepage.
    */
    const char* getHomePage() const override;

   /**
      Get the plugin license name (a single line of text).
    */
    const char* getLicense() const override;

   /**
      Get the plugin version, in hexadecimal.
    */
    uint32_t getVersion() const override;

   /**
      Optional callback to inform the plugin about a sample rate change.
    */
    void sampleRateChanged(double newSampleRate) override;

   /* --------------------------------------------------------------------------------------------------------
    * Parameters */

   /**
      Called by DPF to initialize a parameter.
    */
    void initParameter(uint32_t index, Parameter& parameter) override;

   /**
      Get the current value of a parameter.
    */
    float getParameterValue(uint32_t index) const override;

   /**
      Change a parameter value.
    */
    void setParameterValue(uint32_t index, float value) override;

   /**
      Called by DPF to initialize a state.
    */
    void initState(uint32_t index, State& state) override;

   /**
      Set the state of a parameter.
    */
    void setState(const char* key, const char* value) override;

   /* --------------------------------------------------------------------------------------------------------
    * Process */

   /**
      Called by DPF to run the plugin.
    */
    void run(const float** inputs, float** outputs, uint32_t frames) override;

private:
   /**
      Parameters.
    */
    float fColor, fOutLeft, fOutRight;

   /**
      Boolean used to reset meter values.
      The UI will send a "reset" message which sets this as true.
    */
    bool fNeedsReset;

   /**
      Set our plugin class as non-copyable and add a leak detector just in case.
    */
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Spectrogram)
};

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO

#endif // EXAMPLE_PLUGIN_METERS_HPP
