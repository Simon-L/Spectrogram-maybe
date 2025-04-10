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

#include "Spectrogram.hpp"

START_NAMESPACE_DISTRHO

// -----------------------------------------------------------------------------------------------------------

Spectrogram::Spectrogram()
    : Plugin(3, 0, 0), // 3 parameters, 0 programs, 0 states
      fColor(0.0f),
      fOutLeft(0.0f),
      fOutRight(0.0f),
      fNeedsReset(true)
{
    ring_buffer.createBuffer(sizeof(RbMsg) * 64); // 2 seconds of floats @ 48k + 2048 bytes
}

const char* Spectrogram::getLabel() const
{
    return "meters";
}

const char* Spectrogram::getDescription() const
{
    return "Plugin to demonstrate parameter outputs using meters.";
}

const char* Spectrogram::getMaker() const
{
    return "Author Name";
}

const char* Spectrogram::getHomePage() const
{
    return "https://example.com";
}

const char* Spectrogram::getLicense() const
{
    return "Proprietary";
}

uint32_t Spectrogram::getVersion() const
{
    return d_version(1, 0, 0);
}

void Spectrogram::initParameter(uint32_t index, Parameter& parameter)
{
    /**
        All parameters in this plugin have the same ranges.
    */
    parameter.ranges.min = 0.0f;
    parameter.ranges.max = 1.0f;
    parameter.ranges.def = 0.0f;

    /**
        Set parameter data.
    */
    switch (index)
    {
    case 0:
        parameter.hints  = kParameterIsAutomatable|kParameterIsInteger;
        parameter.name   = "color";
        parameter.symbol = "color";
        parameter.enumValues.count = 2;
        parameter.enumValues.restrictedMode = true;
        {
            ParameterEnumerationValue* const values = new ParameterEnumerationValue[2];
            parameter.enumValues.values = values;

            values[0].label = "Green";
            values[0].value = 0;
            values[1].label = "Blue";
            values[1].value = 1;
        }
        break;
    case 1:
        parameter.hints  = kParameterIsAutomatable|kParameterIsOutput;
        parameter.name   = "out-left";
        parameter.symbol = "out_left";
        break;
    case 2:
        parameter.hints  = kParameterIsAutomatable|kParameterIsOutput;
        parameter.name   = "out-right";
        parameter.symbol = "out_right";
        break;
    }
}

float Spectrogram::getParameterValue(uint32_t index) const
{
    switch (index)
    {
        case 0: return fColor;
        case 1: return fOutLeft;
        case 2: return fOutRight;
    }

    return 0.0f;
}

void Spectrogram::setParameterValue(uint32_t index, float value)
{
    if (index != 0) return;

    fColor = value;
}

void Spectrogram::initState(uint32_t index, State& state)
{
    // Initialize states if necessary
}

void Spectrogram::setState(const char* key, const char* value)
{
    if (std::strcmp(key, "reset") != 0)
        return;

    fNeedsReset = true;
}

void Spectrogram::bufferSizeChanged (uint32_t newBufferSize)
{
    d_stdout("buffersize changed %d", newBufferSize);
}

void Spectrogram::sampleRateChanged (double	newSampleRate)
{
    d_stdout("samplerate changed %f", newSampleRate);
}

void Spectrogram::deactivate()
{
    d_stdout("deactivated :(");
}

void Spectrogram::activate()
{
    d_stdout("activated :) samplerate: %f buffersize: %d ", getSampleRate(), getBufferSize());
}

void Spectrogram::run(const float** inputs, float** outputs, uint32_t frames)
{
    if (ring_buffer.getWritableDataSize() >= sizeof(RbMsg)) {
        std::memcpy(rbmsg.buffer_l, inputs[0], sizeof(float) * frames);
        std::memcpy(rbmsg.buffer_r, inputs[1], sizeof(float) * frames);
        rbmsg.length = frames;
        ring_buffer.writeCustomType<RbMsg>(rbmsg);
        ring_buffer.commitWrite();
    }

    // copy inputs over outputs if needed
    if (outputs[0] != inputs[0])
        std::memcpy(outputs[0], inputs[0], sizeof(float) * frames);

    if (outputs[1] != inputs[1])
        std::memcpy(outputs[1], inputs[1], sizeof(float) * frames);
}

/* ------------------------------------------------------------------------------------------------------------
 * Plugin entry point, called by DPF to create a new plugin instance.
 */

Plugin* createPlugin()
{
    return new Spectrogram();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
