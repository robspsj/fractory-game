#pragma once

struct Rect {
    float ox, oy, w, h;

    float cx() const { return ox + w * 0.5f; }
    float cy() const { return oy + h * 0.5f; }
    float halfW() const { return w * 0.5f; }
    float halfH() const { return h * 0.5f; }
    bool contains(float x, float y) const {
        return x >= ox && x <= ox + w && y >= oy && y <= oy + h;
    }
    bool outsideClip() const {
        return ox + w <= -1.0f || ox >= 1.0f || oy + h <= -1.0f || oy >= 1.0f;
    }
};
