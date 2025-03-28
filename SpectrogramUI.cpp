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
#include "Application.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <sys/types.h>
#include <vector>
#include <iostream>

#include "Spectrogram.hpp"


#include "NanoButton.hpp"
#include "Widgets.hpp"

#include "fft.hpp"
#include "colormaps.hpp"

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

    class DragFloatWindowsize : public DragFloat
    {
    public:
        DragFloatWindowsize(NanoTopLevelWidget* const p, KnobEventHandler::Callback* const cb)
        : DragFloat(p, cb)
        {
        }
    protected:
        virtual void getCustomText(char dest[24]) {
            std::snprintf(dest, 23, "%d", to_window_size_po2(getValue()));
        }
        
    };

    SpectrogramUI()
        : UI(1280, 512),
          fButton1(this, this)
    {
#ifdef DGL_NO_SHARED_RESOURCES
        createFontFromFile("sans", "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf");
        #else
        loadSharedResources();
        #endif

        botbin = 0;

        window_size = 1024;
        topbin = window_size / 2 + 1;
        window.resize(window_size);
        hann(window.data(), window_size, true);

        std::sprintf(topbin_text, "%3.3fHz", freqAtBin(topbin));
        std::sprintf(botbin_text, "%3.3fHz", freqAtBin(botbin));
        
        columns_l.init(&window, window_size);
        columns_r.init(&window, window_size);
        columns_l.fct = 2.0;
        columns_r.fct = 2.0;

        plugin_ptr = reinterpret_cast<Spectrogram*>(getPluginInstancePointer());
        columns_l.sampleRate = getSampleRate();
        columns_r.sampleRate = getSampleRate();
        dragfloat_topbin = new DragFloat(this, this);
        dragfloat_topbin->setAbsolutePos(15,15);

        dragfloat_topbin->setRange(2, topbin);
        dragfloat_topbin->setDefault(topbin);
        dragfloat_topbin->setValue(dragfloat_topbin->getDefault(), false);
        dragfloat_topbin->setStep(1);
        // dragfloat_topbin->setUsingLogScale(true);
        dragfloat_topbin->label = "Top bin";
        dragfloat_topbin->unit = "";
        dragfloat_topbin->toFront();

        dragfloat_botbin = new DragFloat(this, this);
        dragfloat_botbin->setAbsolutePos(15, 15 + (45*1));

        dragfloat_botbin->setRange(0, topbin);
        dragfloat_botbin->setDefault(botbin);
        dragfloat_botbin->setValue(dragfloat_botbin->getDefault(), false);
        dragfloat_botbin->setStep(1);
        // dragfloat_botbin->setUsingLogScale(true);
        dragfloat_botbin->label = "Bottom bin";
        dragfloat_botbin->unit = "";
        dragfloat_botbin->toFront();

        dragfloat_gain = new DragFloat(this, this);
        dragfloat_gain->setAbsolutePos(15, 15 + (45*2));
        dragfloat_gain->setRange(0, 15.0);
        dragfloat_gain->setDefault(1.0);
        dragfloat_gain->setValue(dragfloat_gain->getDefault(), false);
        // dragfloat_gain->setUsingLogScale(true);
        dragfloat_gain->label = "Gain";
        dragfloat_gain->unit = "";
        
        dragfloat_windowsize = new DragFloatWindowsize(this, this);
        dragfloat_windowsize->setAbsolutePos(15, 15 + (45*3));
        dragfloat_windowsize->setRange(0, 7);
        dragfloat_windowsize->setDefault(from_window_size_po2(window_size));
        dragfloat_windowsize->setStep(1);
        dragfloat_windowsize->setUsingCustomText(true);
        dragfloat_windowsize->setValue(dragfloat_windowsize->getDefault(), false);
        dragfloat_windowsize->label = "Window size";
        dragfloat_windowsize->unit = "";
        
        initBinAtCursor();

        if (!nimg.isValid())
            initSpectrogramTexture();

        setGeometryConstraints(900, 512, false);
    }

    NanoImage knob_img;
    NanoImage scale_img;
    DragFloat* dragfloat_topbin;
    DragFloat* dragfloat_botbin;
    DragFloat* dragfloat_gain;
    DragFloatWindowsize* dragfloat_windowsize;

    std::vector<float> window;
    int window_size;

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
    bool request_raster_all = false;
    int since_last_raster = -1;
    int requested_window_size = -1;
    void onNanoDisplay() override
    {
        const float lineHeight = 1.5;
        float y;

        fontSize(15.0f * 1);
        textLineHeight(lineHeight);

        if (requested_window_size > 0) {
            window_size = requested_window_size;
            window.resize(window_size);
            hann(window.data(), window_size, true);
            columns_l.columns.clear();
            columns_l.init(&window, window_size);
            columns_r.columns.clear();
            columns_r.init(&window, window_size);
            requested_window_size = -1;

            topbin = (window_size / 2 + 1);
            dragfloat_topbin->setRange(2, (window_size / 2 + 1));
            dragfloat_topbin->setValue(topbin);
            botbin = 0;
            dragfloat_botbin->setRange(0, (window_size / 2 + 1));
            dragfloat_botbin->setValue(botbin);

            initSpectrogramTexture();
            updateSpectrogramTexture();
        }

        if (request_raster_all && (since_last_raster > 4)) {
            rasterAllColumns();
            request_raster_all = false;
            since_last_raster = 0;
        }
        since_last_raster++;

        processRingBuffer();

        drawSpectrogramTexture(128, 16);

        beginPath();
        roundedRect(128, 16, texture_w, texture_h, 4);
        strokeColor(Color(255,255,255,64));
        stroke();

        if (cursor2.getY() > 1 || cursor2.getY() < texture_h)
        {
            beginPath();
            moveTo(128, 16 + cursor2.getY());
            lineTo(128 + texture_w, 16 + cursor2.getY());
            strokeColor(Color(255,255,255,255));
            stroke();
        }

        text(122 + texture_w + 10, 16 + 10, topbin_text, nullptr);
        text(122 + texture_w + 10, 16 + texture_h, botbin_text, nullptr);

        textBox(122 + texture_w + 10, 16 + (texture_h/8), 150, cursor_text, nullptr);

        if (frozen) {
            text(128 + (texture_w/2), 500, frozen_text, nullptr);
        }
    }

    const char* frozen_text = "frozen";
    char topbin_text[32];
    char botbin_text[32];

    void processRingBuffer()
    {
        while (plugin_ptr->ring_buffer.getReadableDataSize() >= sizeof(RbMsg)) {
            RbMsg rbmsg = RbMsg();
            if (plugin_ptr->ring_buffer.readCustomType<RbMsg>(rbmsg)) {
                if (frozen) continue;
                auto n = columns_l.feed(rbmsg.buffer_l, rbmsg.length);
                n = columns_r.feed(rbmsg.buffer_r, rbmsg.length);
                auto l_data = columns_l.columns.data();
                auto r_data = columns_r.columns.data();
                if (n > 0) {
                    shiftRasteredColumns((n_columns), column_w, n);
                    for (int i = 0; i < n; i++) {
                        rasterColumn<texture_w, texture_h>(l_data[columns_l.columns.size() - n + i], (((n_columns) - n + i) * column_w), column_w, texture_l, color_l);
                        rasterColumn<texture_w, texture_h>(r_data[columns_r.columns.size() - n + i], (((n_columns) - n + i) * column_w), column_w, texture_r, color_r);
                    }
                    updateSpectrogramTexture();
                    repaint();
                }
            }
        }
        
    }

    Point<float> cursor1;
    Point<float> cursor2;
    char cursor_text[256];
    char cursor2_text[128];
    Columns::Column colAtCursor(Point<float> cursor, bool leftOrRight)
    {
        auto columns_size = columns_l.columns.size();

        int cur_col = cursor.getX()/(column_w);

        int at = -1;
        char d = 'A';
        if (columns_size < n_columns)
        {
            if (cur_col < (n_columns - columns_size)) {
                at = 0;
                d = 'B';
            } else {
                at = cur_col - (n_columns - columns_size);
                d = 'C';
            }
        } else {
            d = 'D';
            at = columns_size - n_columns + cur_col;
        }
        at = std::min(static_cast<int>(columns_size - 1), at);

        return !leftOrRight ? columns_l.columns.at(at) : columns_r.columns.at(at);
    }

    void initBinAtCursor()
    {
        std::snprintf(cursor_text, 256,
                 "Cursor:\n---------\n\nMouse\ncol: %d bin: %d\nFrequency:\n%3.3fHz\n\nLEFT\nPeak:%3.3fHz\nmag: %.3f\nphase: %.3f\n\nRIGHT\nPeak:%3.3fHz\nmag: %.3f\nphase: %.3f",
                 0, 0,
                 0.0,
                 0, 0, 0,
                 0, 0, 0
        );
    }

    void updateBinAtCursor()
    {
        auto c_l = colAtCursor(cursor1, 0);
        auto c_r = colAtCursor(cursor1, 1);
        
        int cur_col = cursor1.getX()/(column_w);
        int cur_bin = botbin + static_cast<int>(std::floor((texture_h - cursor1.getY()) / (texture_h / static_cast<float>(topbin - botbin))));

        if (cursor2.getY() > 1 || cursor2.getY() < texture_h) {
            std::snprintf(cursor_text, 256,
                    "Cursor:\n%3.3fHz\n\nMouse\ncol: %d bin: %d\nFrequency:\n%3.3fHz\n\nLEFT\nPeak:%3.3fHz\nmag: %.3f\nphase: %.3f\n\nRIGHT\nPeak:%3.3fHz\nmag: %.3f\nphase: %.3f",
                    freqAtBin(cursor2_bin), cur_col, cur_bin,
                    freqAtBin(cur_bin),
                    c_l.peakFrequency, c_l.bins[cur_bin], c_l.bins_phase[cur_bin],
                    c_r.peakFrequency, c_r.bins[cur_bin], c_r.bins_phase[cur_bin]
            );
        } else {
            std::snprintf(cursor_text, 256,
                    "Cursor:\n---------\n\nMouse\ncol: %d bin: %d\nFrequency:\n%3.3fHz\n\nLEFT\nPeak:%3.3fHz\nmag: %.3f\nphase: %.3f\n\nRIGHT\nPeak:%3.3fHz\nmag: %.3f\nphase: %.3f",
                    cur_col, cur_bin,
                    freqAtBin(cur_bin),
                    c_l.peakFrequency, c_l.bins[cur_bin], c_l.bins_phase[cur_bin],
                    c_r.peakFrequency, c_r.bins[cur_bin], c_r.bins_phase[cur_bin]
            );
        }
    }
    
    void rasterAllColumns()
    {
        auto l_data = columns_l.columns;
        auto r_data = columns_r.columns;
        auto columns_size = columns_l.columns.size();
        int start_col = 0;
        int end_col = n_columns;
        if (columns_size < n_columns)
        {
            start_col = n_columns - columns_size;
            end_col = columns_size;
        }
        for (int i = 0; i < end_col; i++) {
            auto col_x = (columns_size < n_columns) ? i : (columns_size - n_columns + i);
            rasterColumn<texture_w, texture_h>(l_data.at(col_x), ((i + start_col) * column_w), column_w, texture_l, color_l);
            rasterColumn<texture_w, texture_h>(r_data.at(col_x), ((i + start_col) * column_w), column_w, texture_r, color_r);
        }

        updateSpectrogramTexture();
        repaint();
    }

    void uiIdle() override
    {
        processRingBuffer();
        if (window_size >= 4096) repaint();
    }

    DGL::Rectangle<double> texture_rect = Rectangle<double>(128, 16, texture_w, texture_h);
    bool frozen = false;

    int cursor2_bin;
   /**
      Mouse events.
    */
    bool onMotion(const MotionEvent& ev) override
    {
        if (texture_rect.contains(ev.pos))
        {

            if ((ev.pos.getX() - texture_rect.getX()) < 0 || columns_l.columns.size() < 1) {
                initBinAtCursor();
            } else {
                cursor1.setX(ev.pos.getX() - texture_rect.getX());
                cursor1.setY(ev.pos.getY() - texture_rect.getY());
                updateBinAtCursor();
            }
            if (cursor2_moving) {
                cursor2.setY(ev.pos.getY() - texture_rect.getY());
                cursor2_bin = botbin + static_cast<int>(std::floor((texture_h - cursor2.getY()) / (texture_h / static_cast<float>(topbin - botbin))));
                updateBinAtCursor();
            }
            repaint();
        }
        return UI::onMotion(ev);
    }

    void dumpToCSV()
    {
        std::string filename = "dump_at_" + std::to_string(getApp().getTime()) + ".csv";
        FILE *datFile = fopen(filename.c_str(), "w");

        if (!datFile)
        {
            std::cout << "Datfile not open at '" << filename << "'" << std::endl;
        }
        else
        {
            auto l_data = columns_l.columns;
            auto r_data = columns_r.columns;
            auto columns_size = columns_l.columns.size();
            int start_col = 0;
            int end_col = n_columns;
            if (columns_size < n_columns)
            {
                start_col = n_columns - columns_size;
                end_col = columns_size;
            }
            for (int i = 0; i < end_col; i++) {
                auto col_x = (columns_size < n_columns) ? i : (columns_size - n_columns + i);
                Columns::Column col_l = l_data.at(col_x);
                Columns::Column col_r = r_data.at(col_x);
                fprintf(datFile, "%04d_left_mag,", i);
                for (int j = 0; j < col_l.size; j++) {
                    fprintf(datFile, "%f,", col_l.bins[j]);
                }
                fprintf(datFile, "\n");
                fprintf(datFile, "%04d_left_phase,", i);
                for (int j = 0; j < col_l.size; j++) {
                    fprintf(datFile, "%f,", col_l.bins_phase[j]);
                }
                fprintf(datFile, "\n");
                fprintf(datFile, "%04d_right_mag,", i);
                for (int j = 0; j < col_l.size; j++) {
                    fprintf(datFile, "%f,", col_r.bins[j]);
                }
                fprintf(datFile, "\n");
                fprintf(datFile, "%04d_right_phase,", i);
                for (int j = 0; j < col_l.size; j++) {
                    fprintf(datFile, "%f,", col_r.bins_phase[j]);
                }
                fprintf(datFile, "\n");
            }
            fclose(datFile);
        }
    }

    bool cursor2_moving = false;
    bool onMouse(const MouseEvent& ev) override
    {
        
        if (texture_rect.contains(ev.pos) && ev.button == 1 && ev.press == true)
        {
            frozen = !frozen;
            if (!frozen) repaint();
            else dumpToCSV();
            return true;
        }
        if (texture_rect.contains(ev.pos) && ev.button == 2 && ev.press == true)
        {
            cursor2_moving = true;
            cursor2.setY(ev.pos.getY() - texture_rect.getY());
            cursor2_bin = botbin + static_cast<int>(std::floor((texture_h - cursor2.getY()) / (texture_h / static_cast<float>(topbin - botbin))));
            updateBinAtCursor();
            repaint();
        }
        if (ev.button == 2 && ev.press == false) cursor2_moving = false;

        return UI::onMouse(ev);
    }

    void onResize(const ResizeEvent& ev) override
    {
        // repaint();
        UI::onResize(ev);
    }

    void buttonClicked(SubWidget* const widget, int) override
    {
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

    float freqAtBin(int bin)
    {
        return bin * (getSampleRate() / (window_size / 2 + 1) / 2 );
    }

    void knobValueChanged(SubWidget* const widget, float value) override
    {
        auto w = static_cast<DragFloat*>(widget);
        if (w == dragfloat_gain) {
            gain = value;
        }
        if (w == dragfloat_topbin) {
            topbin = std::min(float(window_size / 2 + 1), value);
            topbin = std::max(botbin, topbin);
            std::sprintf(topbin_text, "%3.3fHz", freqAtBin(topbin));
            w->setValue(topbin);
            request_raster_all = true;
        }
        if (w == dragfloat_botbin) {
            botbin = std::min(float(window_size / 2 + 1), value);
            botbin = std::min(botbin, topbin);
            std::sprintf(botbin_text, "%3.3fHz", freqAtBin(botbin));
            w->setValue(botbin);
            request_raster_all = true;
        }
        if (w == dragfloat_windowsize) {
            requested_window_size = to_window_size_po2(value);
        }
        repaint();
    }

    void knobDoubleClicked(SubWidget* const widget) override
    {
        auto w = static_cast<DragFloat*>(widget);
        w->setValue(w->getDefault(), true);
    }

    static int to_window_size_po2(float value) { return static_cast<int>(std::pow(2, 7 + static_cast<int>(value))); }
    static int from_window_size_po2(int value) { return std::log2(value) - 7; }

    // -------------------------------------------------------------------------------------------------------

private:
    Spectrogram* plugin_ptr;
    Columns columns_l;
    Color color_l = Color(255,0,0,1);
    Columns columns_r;
    Color color_r = Color(0,0,255,1);

    int botbin;
    int topbin;

    float gain = 1.0;

    static constexpr int texture_w = 1000;
    static constexpr int texture_h = 460;
    static constexpr int column_w = 2;
    static constexpr int n_columns = (texture_w / column_w);

    Button fButton1;

    struct Pixel{
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;

        static Pixel alpha_compose(Pixel A, Pixel B)
        {
            Pixel p;
            p.r = (A.r * 0.5 + B.r * 0.25) / 0.75;
            p.g = (A.g * 0.5 + B.g * 0.25) / 0.75;
            p.b = (A.b * 0.5 + B.b * 0.25) / 0.75;

            p.a = 0.75 * 255;
            return p;
        }
    };

    Pixel texture_l[texture_w][texture_h];
    Pixel texture_r[texture_w][texture_h];
    NanoImage nimg;
    unsigned char data[texture_w*texture_h*4];
    void initSpectrogramTexture()
    {
        for (int x = 0; x < texture_w; x++) {
            for (int y = 0; y < texture_h; y++) {
                if (((y == (texture_h - 1)) || (y == 0) || (x == 0) || (x == (texture_w - 1)))) {
                    texture_l[x][y].r = texture_r[x][y].r = 0;
                    texture_l[x][y].g = texture_r[x][y].g = 255;
                    texture_l[x][y].b = texture_r[x][y].b = 0;
                    texture_l[x][y].a = texture_r[x][y].a = 255;
                }
                else {
                    texture_l[x][y].r = texture_r[x][y].r = 0;
                    texture_l[x][y].g = texture_r[x][y].g = 0;
                    texture_l[x][y].b = texture_r[x][y].b = 127;
                    texture_l[x][y].a = texture_r[x][y].a = 255;
                }
            }
        }

        if (!nimg.isValid()) {
            unsigned char* px = data;
            for (int data_y = 0; data_y < texture_h; data_y++) {
                for (int data_x = 0; data_x < texture_w; data_x++) {
                    px[0] = texture_l[data_x][data_y].r;
                    px[1] = texture_l[data_x][data_y].g;
                    px[2] = texture_l[data_x][data_y].b;
                    px[3] = texture_l[data_x][data_y].a;
                    px += 4;
                }
            }
            nimg = createImageFromRGBA(texture_w, texture_h, data, 0);
        }
    }

    void updateSpectrogramTexture()
    {
        if (nimg.isValid()) {
            unsigned char* px = data;
            for (int data_y = 0; data_y < texture_h; data_y++) {
                for (int data_x = 0; data_x < texture_w; data_x++) {
                    Pixel pl = texture_l[data_x][data_y];
                    Pixel pr = texture_r[data_x][data_y];
                    px[0] = pl.r + pr.r;
                    px[1] = pl.g + pr.g;
                    px[2] = pl.b + pr.b;
                    px[3] = pl.a + pr.r;
                    px += 4;
                }
            }
            nimg.update(data);
        } else {
            d_stdout("?????");
        }
    }

    void drawSpectrogramTexture(float x, float y)
    {
        if (!nimg.isValid()) return;

        beginPath();
        rect(x, y, texture_w, texture_h);
        fillPaint(imagePattern(x, y, texture_w, texture_h, 0, nimg, 1.0f));
        fill();
    }

    float lerp(float a, float b, float t) { return a + t * (b - a); }
    float inverseLerp(float a, float b, float value) { return (value - a) / (b - a); }
    float remap(float value, float fromA, float fromB, float toA, float toB) { return lerp(toA, toB, inverseLerp(fromA, fromB, value)); }
    float interpolate(float x, std::vector<float> bins, size_t size) const {
        int lowerIndex = static_cast<int>(x);
        int upperIndex = lowerIndex + 1;
        float weight = x - lowerIndex;
        return bins[lowerIndex] * (1 - weight) + bins[upperIndex] * weight;
    }

    void shiftRasteredColumns(int total_columns, int w, int n_columns)
    {
        for (int y = 0; y < texture_h; y++)
        {
            for (int x = 0; x < ((total_columns - n_columns) * w); x++) {
                texture_l[x][y] = texture_l[x + (n_columns * w)][y];
                texture_r[x][y] = texture_r[x + (n_columns * w)][y];
            }
        }
    }

    template <size_t size_x, size_t size_y>
    void rasterColumn(Columns::Column col, int at_x, int w, Pixel tex[size_x][size_y], Color color)
    {
        float at = botbin;
        float step = (topbin - at) / texture_h;
        for (int y = 0; y < texture_h; y++)
        {
            float v = interpolate(at, col.bins, col.size);
            v *= gain;
            if (v > 1.0) v = 1.0;
            int idx = static_cast<int>(v * 255);
            for (int x = at_x; x < at_x + w; x++) {
                tex[x][(texture_h - 1) - y].r = (cmaps["magma"][idx][0]) * 255;
                tex[x][(texture_h - 1) - y].g = (cmaps["magma"][idx][1]) * 255;
                tex[x][(texture_h - 1) - y].b = (cmaps["magma"][idx][2]) * 255;
                tex[x][(texture_h - 1) - y].a = 255; // * color.alpha;
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
