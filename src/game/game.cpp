#include "game.hpp"
#include "game_view.hpp"
#include <memory>
#include <cstdlib>
#include <SDL3/SDL.h>

static std::unique_ptr<GameModel> s_model;
static std::unique_ptr<GameView> s_view;

enum class MouseState {
    NONE,
    DOWN_PENDING,
    DRAGGING_ITEM,
    PANNING
};

static MouseState mouseState = MouseState::NONE;
static int mouseDownX = 0, mouseDownY = 0;
static Uint64 mouseDownTime = 0;

static float dragMX = 0.0f, dragMY = 0.0f;

static GameModel& model() { return *s_model; }
static GameView& view() { return *s_view; }

void gameInit(unsigned int seed) {
    s_model = std::make_unique<GameModel>();
    if (seed == 42) {
        s_model->setTestMode(true);
    }
    s_model->init(seed);
    s_view = std::make_unique<GameView>(*s_model);
    s_view->initGL();
}

void gameUpdate(int mousePx, int mousePy, int winW, int winH) {
    if (mouseState == MouseState::DOWN_PENDING) {
        int dx = mousePx - mouseDownX;
        int dy = mousePy - mouseDownY;
        if (std::abs(dx) > 5 || std::abs(dy) > 5) {
            Uint64 heldTime = SDL_GetTicks() - mouseDownTime;
            if (heldTime >= 250) {
                mouseState = MouseState::PANNING;
                view().startPan(mousePx, mousePy);
            } else {
                mouseState = MouseState::DRAGGING_ITEM;
                int row, col;
                if (view().screenToGrid(mouseDownX, mouseDownY, winW, winH, row, col)) {
                    if (model().cell(row, col).type == CellType::ITEM) {
                        model().pickUp(row, col, model().cell(row, col).data.item.count);
                    }
                }
            }
        }
    }

    if (view().isPanning()) {
        view().continuePan(mousePx, mousePy, winW, winH);
    }

    float aspect = (float)winW / (float)winH;
    dragMX = ((2.0f * mousePx / (float)winW) - 1.0f - view().panPtr()[0]) / view().zoomFactor();
    dragMY = ((1.0f - (2.0f * mousePy / (float)winH)) - view().panPtr()[1]) / aspect / view().zoomFactor();
}

void gameMouseDown(int button, int mousePx, int mousePy, int winW, int winH) {
    if (button == SDL_BUTTON_MIDDLE || button == SDL_BUTTON_RIGHT) {
        view().startPan(mousePx, mousePy);
        return;
    }

    if (button == SDL_BUTTON_LEFT) {
        if (model().isTestMode()) {
            int row, col;
            if (view().screenToGrid(mousePx, mousePy, winW, winH, row, col)) {
                if (model().cell(row, col).type == CellType::ITEM) {
                    model().pickUp(row, col, model().cell(row, col).data.item.count);
                }
            }
        } else {
            mouseState = MouseState::DOWN_PENDING;
            mouseDownX = mousePx;
            mouseDownY = mousePy;
            mouseDownTime = SDL_GetTicks();
        }
    }
}

void gameMouseUp(int button, int mousePx, int mousePy, int winW, int winH) {
    if (button == SDL_BUTTON_MIDDLE || button == SDL_BUTTON_RIGHT) {
        view().endPan();
        return;
    }

    if (button == SDL_BUTTON_LEFT) {
        if (model().isTestMode()) {
            int row, col;
            view().screenToGrid(mousePx, mousePy, winW, winH, row, col);
            model().drop(row, col);
        } else {
            if (mouseState == MouseState::DOWN_PENDING) {
                int dx = mousePx - mouseDownX;
                int dy = mousePy - mouseDownY;
                bool moved = std::abs(dx) > 5 || std::abs(dy) > 5;

                if (moved) {
                    int row, col;
                    if (view().screenToGrid(mouseDownX, mouseDownY, winW, winH, row, col)) {
                        if (model().cell(row, col).type == CellType::ITEM) {
                            model().pickUp(row, col, model().cell(row, col).data.item.count);
                        }
                    }
                    int dropRow, dropCol;
                    view().screenToGrid(mousePx, mousePy, winW, winH, dropRow, dropCol);
                    model().drop(dropRow, dropCol);
                } else {
                    if (!model().hasDrag()) {
                        int row, col;
                        if (view().screenToGrid(mousePx, mousePy, winW, winH, row, col)) {
                            if (model().cell(row, col).type == CellType::ITEM) {
                                model().pickUp(row, col, model().cell(row, col).data.item.count);
                            }
                        }
                    } else {
                        int row, col;
                        view().screenToGrid(mousePx, mousePy, winW, winH, row, col);
                        model().drop(row, col);
                    }
                }
            } else if (mouseState == MouseState::DRAGGING_ITEM) {
                int row, col;
                view().screenToGrid(mousePx, mousePy, winW, winH, row, col);
                model().drop(row, col);
            } else if (mouseState == MouseState::PANNING) {
                view().endPan();
            }
            mouseState = MouseState::NONE;
        }
    }
}

void gameMouseWheel(float dx, float dy) {
    if (dy > 0) view().zoom(1.1f);
    else if (dy < 0) view().zoom(1.0f / 1.1f);
}

void gameRender(int winW, int winH) {
    bool showPointer = mouseState != MouseState::NONE;
    const float* pColor = nullptr;
    if (showPointer) {
        if (mouseState == MouseState::DRAGGING_ITEM) {
            static const float white[] = {1.0f, 1.0f, 1.0f};
            pColor = white;
        } else if (mouseState == MouseState::PANNING) {
            static const float yellow[] = {1.0f, 1.0f, 0.0f};
            pColor = yellow;
        } else {
            static const float grey[] = {0.5f, 0.5f, 0.5f};
            pColor = grey;
        }
    }
    view().render(winW, winH, dragMX, dragMY, showPointer, pColor);
}

GLuint gameProgram() {
    return view().program();
}

void gameSetFullState(int* inData) {
    model().setFullState(inData);
}

void gameGetFullState(int* outData) {
    model().getFullState(outData);
}

void gameGetDragState(int& outId, int& outCount) {
    model().getDragState(outId, outCount);
}
