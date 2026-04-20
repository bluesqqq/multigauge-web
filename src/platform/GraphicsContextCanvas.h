#pragma once

#include <multigauge/graphics/GraphicsContext.h>
#include <string>

class GraphicsContextCanvas : public GraphicsContext {
public:
    explicit GraphicsContextCanvas(std::string canvasId);

    bool init() override;
    void beginFrame() override;

    void clear(rgba color) override;

    void pixel(int x, int y, rgba color) override;
    void line(int x0, int y0, int x1, int y1, rgba color, float thickness) override;

    void rect(int x, int y, int w, int h, rgba color) override;
    void strokeRect(int x, int y, int w, int h, rgba color, float thickness) override;

    void roundRect(int x, int y, int w, int h, float radius, rgba color) override;
    void roundRect(int x, int y, int w, int h, float r1, float r2, float r3, float r4, rgba color) override;

    void strokeRoundRect(int x, int y, int w, int h, float radius, rgba color, float thickness) override;
    void strokeRoundRect(int x, int y, int w, int h, float r1, float r2, float r3, float r4, rgba color, float thickness) override;

    void circle(int cx, int cy, int r, rgba color) override;
    void strokeCircle(int cx, int cy, int r, rgba color, float thickness) override;

    void ellipse(int cx, int cy, int rx, int ry, rgba color) override;
    void strokeEllipse(int cx, int cy, int rx, int ry, rgba color, float thickness) override;

    void ring(int cx, int cy, int r1, int r2, rgba color) override;
    void strokeRing(int cx, int cy, int r1, int r2, rgba color, float thickness) override;

    void arc(int cx, int cy, int r1, int r2, float start, float end, rgba color) override;
    void strokeArc(int cx, int cy, int r1, int r2, float start, float end, rgba color, float thickness) override;

    void tri(int x0, int y0, int x1, int y1, int x2, int y2, rgba color) override;
    void strokeTri(int x0, int y0, int x1, int y1, int x2, int y2, rgba color, float thickness) override;

    float getTextWidth(const char* text, std::string family, float pt, FontWeight weight, FontSlant slant) override;
    void drawText(const char* text, int x, int y, std::string family, float pt, FontWeight weight, FontSlant slant, rgba color, Anchor anchor) override;

    // Image integration depends on your Image class, see stubs in .cpp
    Image createNativeImage(const rgba* pixels, int w, int h) override;
    void drawImage(const Image& img, int x, int y) override;
    void drawImageRotated(const Image& img, int x, int y, float angle, int pivotX, int pivotY) override;
    void drawImageScaled(const Image& img, int x, int y, float sx, float sy) override;
    void drawImageTransformed(const Image& img, int x, int y, float angle, float sx, float sy, int pivotX, int pivotY) override;
    void drawImageStretched(const Image& img, int x, int y, int width, int height) override;
    void drawImageRegion(const Image& img, int srcX, int srcY, int srcW, int srcH, int destX, int destY, int destW, int destH) override;

    void clip(int x, int y, int width, int height) override;
    void clearClip() override;

private:
    std::string canvasId;
    int canvasHandle = 0;
    bool hasClip = false;
    int logicalWidth = 0;
    int logicalHeight = 0;

public:
    bool resize(int w, int h) override;
};
