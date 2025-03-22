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
#include <sys/types.h>
#include <vector>
#include <iostream>

#include "Spectrogram.hpp"

#include "pocketfft.h"

#include "NanoButton.hpp"
#include "Widgets.hpp"

// https://github.com/sidneycadot/WindowFunctions/blob/master/c99/window_functions.c
void cosine_window(float * w, unsigned n, const float * coeff, unsigned ncoeff, bool sflag)
{
    if (n == 1)
    {
        // Special case for n == 1.
        w[0] = 1.0;
    }
    else
    {
        const unsigned wlength = sflag ? (n - 1) : n;

        for (unsigned i = 0; i < n; ++i)
        {
            float wi = 0.0;

            for (unsigned j = 0; j < ncoeff; ++j)
            {
                wi += coeff[j] * cos(i * j * 2.0 * M_PI / wlength);
            }

            w[i] = wi;
        }
    }
}

void hann(float * w, unsigned n, bool sflag)
{
    const float coeff[2] = { 0.5, -0.5 };
    cosine_window(w, n, coeff, sizeof(coeff) / sizeof(float), sflag);
}

struct Columns {
    std::vector<float> buffer;
    uint32_t consumeThreshold = 2048;
    float window[2048];
    float sampleRate;

    struct Column {
        float bins[4096];
        size_t size;
        bool processed = false;
    };
    std::vector<Column> columns;

    float fct = 1.0f;

    void init()
    {
        hann(window, consumeThreshold, true);
    }

    int feed(float* data, size_t length) {
        int fed = 0;
        for (int i = 0; i < length; i++) {
            buffer.push_back(data[i]);
            if (buffer.size() == consumeThreshold) {
                for (int i = 0; i < consumeThreshold; i++) 
                    buffer[i] = buffer[i] * window[i];
                
                processFFT();
                buffer.clear();
                fed++;
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
            fct
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

class SpectrogramUI : public UI,
                        public ButtonEventHandler::Callback,
                        public KnobEventHandler::Callback
{
public:
    SpectrogramUI()
        : UI(1280, 512),
          fButton1(this, this)
    {
#ifdef DGL_NO_SHARED_RESOURCES
        createFontFromFile("sans", "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf");
#else
        loadSharedResources();
#endif

        // setupButton(fButton1, 150);

        plugin_ptr = reinterpret_cast<Spectrogram*>(getPluginInstancePointer());
        columns.sampleRate = plugin_ptr->getSampleRate();

        // knob_img = createImageFromFile("knob.png", IMAGE_GENERATE_MIPMAPS);
        // scale_img = createImageFromFile("_", IMAGE_GENERATE_MIPMAPS);

        // dragfloat_topbin = new DragFloat(this, this, knob_img, scale_img);
        dragfloat_topbin = new DragFloat(this, this);
        dragfloat_topbin->setAbsolutePos(15,15);

        dragfloat_topbin->setRange(2, 1025);
        dragfloat_topbin->setDefault(200);
        dragfloat_topbin->setValue(dragfloat_topbin->getDefault(), false);
        dragfloat_topbin->setStep(1);
        // dragfloat_topbin->setUsingLogScale(true);
        dragfloat_topbin->label = "Top bin";
        dragfloat_topbin->unit = "";
        dragfloat_topbin->toFront();

        dragfloat_botbin = new DragFloat(this, this);
        dragfloat_botbin->setAbsolutePos(15, 15 + (45*1));

        dragfloat_botbin->setRange(0, 1023);
        dragfloat_botbin->setDefault(0);
        dragfloat_botbin->setValue(dragfloat_botbin->getDefault(), false);
        dragfloat_botbin->setStep(1);
        // dragfloat_botbin->setUsingLogScale(true);
        dragfloat_botbin->label = "Bottom bin";
        dragfloat_botbin->unit = "";
        dragfloat_botbin->toFront();

        dragfloat_gain = new DragFloat(this, this);
        dragfloat_gain->setAbsolutePos(15, 15 + (45*2));
        dragfloat_gain->setRange(0, 15.0);
        dragfloat_gain->setDefault(5);
        dragfloat_gain->setValue(dragfloat_gain->getDefault(), false);
        columns.fct = dragfloat_gain->getValue();
        // dragfloat_gain->setUsingLogScale(true);
        dragfloat_gain->label = "Gain";
        dragfloat_gain->unit = "";


        columns.init();

        if (!nimg.isValid())
            initSpectrogramTexture();

        setGeometryConstraints(900, 512, false);
    }

    NanoImage knob_img;
    NanoImage scale_img;
    DragFloat* dragfloat_topbin;
    DragFloat* dragfloat_botbin;
    DragFloat* dragfloat_gain;

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

        processRingBuffer();

        drawSpectrogramTexture(128, 16);

        beginPath();
        roundedRect(128, 16, 640, 480, 4);
        strokeColor(Color(255,255,255,64));
        stroke();
    }

    void processRingBuffer()
    {
        while (plugin_ptr->myHeapBuffer.getReadableDataSize() >= sizeof(RbMsg)) {
            RbMsg rbmsg = RbMsg();
            if (plugin_ptr->myHeapBuffer.readCustomType<RbMsg>(rbmsg)) {
                if (frozen) continue;
                auto n = columns.feed(rbmsg.buffer, rbmsg.length);
                if (n > 0) {
                    repaint();
                    shiftRasteredColumns(64, 10, n);
                    for (int i = 0; i < n; i++) {
                        rasterColumn(columns.columns[columns.columns.size() - n + i], ((64 - n + i) * 10), 10);
                    }
                }
            }
        }

    }

    void uiIdle() override
    {
        processRingBuffer();
    }

    DGL::Rectangle<double> texture_rect = Rectangle<double>(128, 16, 640, 480);
    bool frozen = false;
   /**
      Mouse press event.
    */
    bool onMouse(const MouseEvent& ev) override
    {
        
        if (texture_rect.contains(ev.pos) && ev.button == 1 && ev.press == true)
        {
            frozen = !frozen;
            d_stdout("but %d freeze %d", ev.button);
            return true;
        }

        return UI::onMouse(ev);
    }

    void onResize(const ResizeEvent& ev) override
    {
        // repaint();
        UI::onResize(ev);
    }

    void buttonClicked(SubWidget* const widget, int) override
    {
        if (widget == &fButton1) d_stdout("ya");

        repaint();
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
        if (widget == dragfloat_gain)
            columns.fct = value;
        // d_stdout("knobValueChanged");
        // setParameterValue(widget->getId(), value);
        repaint();
    }

    void knobDoubleClicked(SubWidget* const widget) override
    {
        auto w = static_cast<DragFloat*>(widget);
        w->setValue(w->getDefault(), true);
    }

    // -------------------------------------------------------------------------------------------------------

private:
    Spectrogram* plugin_ptr;
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
    void initSpectrogramTexture()
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

    void drawSpectrogramTexture(float x, float y)
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
        int lowerIndex = static_cast<int>(x);
        int upperIndex = lowerIndex + 1;
        float weight = x - lowerIndex;
        return bins[lowerIndex] * (1 - weight) + bins[upperIndex] * weight;
    }

    void shiftRasteredColumns(int total_columns, int w, int n_columns)
    {
        for (int y = 0; y < 480; y++)
        {
            for (int x = 0; x < ((total_columns - n_columns) * w); x++) {
                texture[x][y] = texture[x + (n_columns * w)][y];
            }
        }
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
        float at = dragfloat_botbin->getValue();
        float step = (dragfloat_topbin->getValue() - at) / 480;
        for (int y = 0; y < 480; y++)
        {
            float v = interpolate(at, col.bins, col.size);
            uint8_t vv = uint8_t(std::clamp(v, .0f,  255.0f));
            for (int x = at_x; x < at_x + w; x++) {
                texture[x][479 - y].r = vv;
                texture[x][479 - y].g = vv;
                texture[x][479 - y].b = vv;
                texture[x][479 - y].a = 255;
            }
            at += step;
        }
    }

   /**
      Set our UI class as non-copyable and add a leak detector just in case.
    */
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramUI)
};

/* ------------------------------------------------------------------------------------------------------------
 * UI entry point, called by DPF to create a new UI instance. */

UI* createUI()
{
    return new SpectrogramUI();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
