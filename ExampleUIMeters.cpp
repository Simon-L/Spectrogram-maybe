/*
 * DISTRHO Plugin Framework (DPF)
 * Copyright (C) 2012-2019 Filipe Coelho <falktx@falktx.com>
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

#include "DistrhoUI.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <vector>
#include <iostream>

#include "ExamplePluginMeters.hpp"

#include "pocketfft.h"

#include "NanoButton.hpp"
#include "Widgets.hpp"

struct Columns {
    std::vector<float> buffer;
    uint32_t consumeThreshold = 2048;
    float sampleRate;

    struct Column {
        float bins[4096];
        size_t size;
        bool processed = false;
    };
    std::vector<Column> columns;

    bool feed(float* data, size_t length) {
        bool fed = false;
        for (int i = 0; i < length; i++) {
            buffer.push_back(data[i]);
            if (buffer.size() == consumeThreshold) {
                processFFT();
                buffer.clear();
                fed = true;
            }
        }
        return fed;
    }

    std::vector<std::complex<float>> fftOutput;
    pocketfft::shape_t shape{consumeThreshold};
    pocketfft::stride_t stride_in{sizeof(float)}; 
    pocketfft::stride_t stride_out{sizeof(std::complex<float>)}; 

    void processFFT() {
        shape[0] = buffer.size();
        fftOutput.clear();
        fftOutput.resize(consumeThreshold / 2 + 1);
        pocketfft::r2c(
            shape,
            stride_in,
            stride_out,
            0,
            pocketfft::FORWARD,
            buffer.data(),
            reinterpret_cast<std::complex<float>*>(fftOutput.data()),
            1.0f
        );

        Column col;
        col.size = fftOutput.size();
        int peakIndex = 0;
        float peakMag = 0;
        for (size_t i = 0; i < fftOutput.size(); ++i) {
            float magnitude = std::abs(fftOutput[i]);
            if (magnitude > peakMag) { peakMag = magnitude; peakIndex = i; }
            col.bins[i] = magnitude;
        }
        columns.push_back(col);
        while (columns.size() > 64) {
            columns.erase(columns.begin());
        }
    }
};

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Button;
using DGL_NAMESPACE::ButtonEventHandler;
using DGL_NAMESPACE::KnobEventHandler;
using DGL_NAMESPACE::SubWidget;

inline void setupButton(Button& btn, const int y)
{
    btn.setAbsolutePos(5, y);
    btn.setLabel("Open...");
    btn.setSize(100, 30);
}

/**
  We need the Color class from DGL.
 */
using DGL_NAMESPACE::Color;

// -----------------------------------------------------------------------------------------------------------

class ExampleUIMeters : public UI,
                        public ButtonEventHandler::Callback,
                        public KnobEventHandler::Callback
{
public:
    ExampleUIMeters()
        : UI(900, 512),
          fButton1(this, this)
    {
// #ifdef DGL_NO_SHARED_RESOURCES
//         createFontFromFile("sans", "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf");
// #else
        loadSharedResources();
// #endif

        // setupButton(fButton1, 5);

        plugin_ptr = reinterpret_cast<ExamplePluginMeters*>(getPluginInstancePointer());
        columns.sampleRate = plugin_ptr->getSampleRate();

        knob_img = createImageFromFile("knob.png", IMAGE_GENERATE_MIPMAPS);
        scale_img = createImageFromFile("scale.png", IMAGE_GENERATE_MIPMAPS);

        knob = new AidaKnob(this, this, knob_img, scale_img);
        knob->toFront();

        if (!nimg.isValid()) initImageTest();

        setGeometryConstraints(900, 512, false);
    }

    NanoImage knob_img;
    NanoImage scale_img;
    AidaKnob* knob;

protected:
   /* --------------------------------------------------------------------------------------------------------
    * DSP/Plugin Callbacks */

   /**
      A parameter has changed on the plugin side.
      This is called by the host to inform the UI about parameter changes.
    */
    void parameterChanged(uint32_t index, float value) override
    {
        // repaint();
    }

   /**
      A state has changed on the plugin side.
      This is called by the host to inform the UI about state changes.
    */
    void stateChanged(const char*, const char*) override
    {
        // repaint();
        // nothing here
    }

   /* --------------------------------------------------------------------------------------------------------
    * Widget Callbacks */

   /**
      The NanoVG drawing function.
    */
    void onNanoDisplay() override
    {
        const float lineHeight = 20 * 1;
        float y;

        fontSize(15.0f * 1);
        textLineHeight(lineHeight);

        while (plugin_ptr->myHeapBuffer.getReadableDataSize() >= sizeof(RbMsg)) {
            RbMsg rbmsg = RbMsg();
            if (plugin_ptr->myHeapBuffer.readCustomType<RbMsg>(rbmsg)) {
                if (columns.feed(rbmsg.buffer, rbmsg.length)) {
                    rasterAllColumns(64, 10);
                    // rasterColumns(columns.columns[0], 128, 64);
                }
                // d_stdout("%d %d remaining %d", rbmsg.length, columns.columns[0].size, plugin_ptr->myHeapBuffer.getReadableDataSize()); 
            }
        }
        drawImageTest(128, 16);

        // repaint();
    }

    void uiIdle() override
    {
        while (plugin_ptr->myHeapBuffer.getReadableDataSize() >= sizeof(RbMsg)) {
            RbMsg rbmsg = RbMsg();
            if (plugin_ptr->myHeapBuffer.readCustomType<RbMsg>(rbmsg)) {
                if (columns.feed(rbmsg.buffer, rbmsg.length)) {
                    rasterAllColumns(64, 10);
                    repaint();
                    // rasterColumns(columns.columns[0], 128, 64);
                }
                // d_stdout("%d %d remaining %d", rbmsg.length, columns.columns[0].size, plugin_ptr->myHeapBuffer.getReadableDataSize()); 
            }
        }
    }
   /**
      Mouse press event.
    */
    // bool onMouse(const MouseEvent& ev) override
    // {
    //     return true;
    // }

    void onResize(const ResizeEvent& ev) override
    {
        // repaint();
        UI::onResize(ev);
    }

    void buttonClicked(SubWidget* const widget, int) override
    {
        // if (widget == &fButton1) d_stdout("ya");
        // d_stdout("ya");

        // repaint();
    }


    void knobDragStarted(SubWidget* const widget) override
    {
        // editParameter(widget->getId(), true);*
        // d_stdout("knobDragStarted");
    }

    void knobDragFinished(SubWidget* const widget) override
    {
        // d_stdout("knobDragFinished");
        // editParameter(widget->getId(), false);
    }

    void knobValueChanged(SubWidget* const widget, float value) override
    {
        // d_stdout("knobValueChanged");
        // setParameterValue(widget->getId(), value);
        repaint();
    }

    void knobDoubleClicked(SubWidget* const widget) override
    {
        // static_cast<AidaKnob*>(widget)->setValue(kParameters[widget->getId()].ranges.def, true);
    }

    // -------------------------------------------------------------------------------------------------------

private:
    ExamplePluginMeters* plugin_ptr;
    Columns columns;

    Button fButton1;

    struct Pixel{
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    };

    Pixel texture[640][480];
    NanoImage nimg;
    unsigned char data[640*480*4];
    void initImageTest()
    {
        for (int x = 0; x < 640; x++) {
            for (int y = 0; y < 480; y++) {
                if (((y == 479) || (y == 0) || (x == 0) || (x == 639))) {
                    texture[x][y].r = 0;
                    texture[x][y].g = 255;
                    texture[x][y].b = 0;
                    texture[x][y].a = 255;
                }
                else {
                    texture[x][y].r = 0;
                    texture[x][y].g = 0;
                    texture[x][y].b = 127;
                    texture[x][y].a = 255;
                }
            }
        }

        if (!nimg.isValid()) {
            unsigned char* px = data;
            for (int data_y = 0; data_y < 480; data_y++) {
                for (int data_x = 0; data_x < 640; data_x++) {
                    px[0] = texture[data_x][data_y].r;
                    px[1] = texture[data_x][data_y].g;
                    px[2] = texture[data_x][data_y].b;
                    px[3] = texture[data_x][data_y].a;
                    px += 4;
                }
            }
            nimg = createImageFromRGBA(640, 480, data, 0);
        }
    }

    void drawImageTest(float x, float y)
    {
        if (nimg.isValid()) {
            unsigned char* px = data;
            for (int data_y = 0; data_y < 480; data_y++) {
                for (int data_x = 0; data_x < 640; data_x++) {
                    px[0] = texture[data_x][data_y].r;
                    px[1] = texture[data_x][data_y].g;
                    px[2] = texture[data_x][data_y].b;
                    px[3] = texture[data_x][data_y].a;
                    px += 4;
                }
            }
            nimg.update(data);
        } else {
            d_stdout("?????");
        }

        beginPath();
        rect(x, y, 640, 480);
        fillPaint(imagePattern(x, y, 640, 480, 0, nimg, 1.0f));
        fill();
    }

    float lerp(float a, float b, float t) { return a + t * (b - a); }
    float inverseLerp(float a, float b, float value) { return (value - a) / (b - a); }
    float remap(float value, float fromA, float fromB, float toA, float toB) { return lerp(toA, toB, inverseLerp(fromA, fromB, value)); }
    float interpolate(float x, float* bins, size_t size) const {
        if (x < 0 || x >= size - 1) {
            throw std::out_of_range("Index out of range for interpolation");
        }
        int lowerIndex = static_cast<int>(x);
        int upperIndex = lowerIndex + 1;
        float weight = x - lowerIndex;
        return bins[lowerIndex] * (1 - weight) + bins[upperIndex] * weight;
    }

    void rasterAllColumns(int n_columns, int w)
    {
        int at_x = 640 - w;
        for (int i = 0; i < n_columns; i++) {
            int col_index = columns.columns.size() - i - 1;
            if (col_index < 0) break;
            rasterColumn(columns.columns[col_index], at_x, w);
            at_x -= w;
        }
    }

    void rasterColumn(Columns::Column col, int at_x, int w)
    {
        for (int y = 0; y < 480; y++)
        {
            float at = float(y/480.0) * col.size;
            float v = interpolate(at, col.bins, col.size);
            uint8_t vv = uint8_t(std::clamp(v, .0f,  255.0f));
            for (int x = at_x; x < at_x + w; x++) {
                texture[x][479 - y].r = vv;
                texture[x][479 - y].g = 0;
                texture[x][479 - y].b = 0;
                texture[x][479 - y].a = 255;
            }
        }
    }

   /**
      Set our UI class as non-copyable and add a leak detector just in case.
    */
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExampleUIMeters)
};

/* ------------------------------------------------------------------------------------------------------------
 * UI entry point, called by DPF to create a new UI instance. */

UI* createUI()
{
    return new ExampleUIMeters();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
