#include "game.hpp"
#include "game_view.hpp"
#include "../shader.hpp"
#include "../print_state.hpp"
#include <memory>
#include <cstdlib>
#include <SDL3/SDL.h>
#include <cmath> // For std::abs
#include <algorithm> // For std::min

static std::unique_ptr<GameModel> s_model;
static std::unique_ptr<GameView> s_view;

// Mouse state machine
enum class MouseState {
    NONE,
    DOWN_PENDING, // Left button down, decision pending (drag/pan)
    DRAGGING_ITEM,
    PANNING,
    // Pointer interaction states for subgrids
    HOVERING_GRID_FOR_FOCUS, // Mouse is over a GRID cell, ready to focus/unfocus
};

static MouseState mouseState = MouseState::NONE;
static int mouseDownX = 0, mouseDownY = 0;
static Uint64 mouseDownTime = 0;

static float dragMX = 0.0f, dragMY = 0.0f;

static GameModel& model() { return *s_model; }
static GameView& view() { return *s_view; }

static void logState() {
    clearScreen();
    printState(model());
}

void gameInit(unsigned int seed) {
    s_model = std::make_unique<GameModel>();
    s_model->init(seed);
    s_view = std::make_unique<GameView>(*s_model);
    s_view->initGL();
}

void gameUpdate(int mousePx, int mousePy, int winW, int winH) {
    bool changedState = false;

    // Check mouse position for focus changes if not panning or dragging
    if (mouseState != MouseState::PANNING && !s_model->hasDrag()) {
        int row, col;
        bool overGridCell = view().screenToGrid(mousePx, mousePy, winW, winH, row, col);
        
        if (overGridCell) {
            view().setHoveredCell(row, col);
            const auto& node = s_model->node(s_model->rootChild(row, col));
            if (node.type == CellType::GRID) {
                if (mouseState != MouseState::HOVERING_GRID_FOR_FOCUS) {
                    mouseState = MouseState::HOVERING_GRID_FOR_FOCUS;
                    changedState = true;
                }
            } else {
                if (mouseState == MouseState::HOVERING_GRID_FOR_FOCUS) {
                    mouseState = MouseState::NONE;
                    changedState = true;
                }
            }
        } else {
            view().clearHoveredCell();
            if (mouseState == MouseState::HOVERING_GRID_FOR_FOCUS) {
                mouseState = MouseState::NONE;
                changedState = true;
            }
        }
    }

    if (mouseState == MouseState::DOWN_PENDING) {
        int dx = mousePx - mouseDownX;
        int dy = mousePy - mouseDownY;
        // Check for significant movement or held time to decide action
        if (std::abs(dx) > 5 || std::abs(dy) > 5) {
            Uint64 heldTime = SDL_GetTicks() - mouseDownTime;
            if (heldTime >= 250) { // Long press duration threshold for panning/focusing
                mouseState = MouseState::PANNING;
                view().startPan(mousePx, mousePy);
                changedState = true;
            } else {
                // Check if we are over a grid cell to initiate drag
                int row, col;
                if (view().screenToGrid(mouseDownX, mouseDownY, winW, winH, row, col)) {
                    int idx = s_model->rootChild(row, col);
                    if (s_model->node(idx).type == CellType::ITEM) {
                        s_model->pickUp(idx, s_model->node(idx).data.item.count);
                        mouseState = MouseState::DRAGGING_ITEM;
                        changedState = true;
                        logState();
                    }
                }
            }
        }
    }

    if (view().isPanning()) {
        view().continuePan(mousePx, mousePy, winW, winH);
    }

    // Update drag position if dragging an item
    if (mouseState == MouseState::DRAGGING_ITEM || mouseState == MouseState::DOWN_PENDING) {
        float aspect = (float)winW / (float)winH;
        view().screenToWorld(mousePx, mousePy, winW, winH, dragMX, dragMY);
        view().setDragWorldPos(dragMX, dragMY);
    }

    // If state changed, maybe need to update cursor or visual feedback
    if (changedState) {
        // TODO: Update cursor based on mouseState
    }
}

void gameMouseDown(int button, int mousePx, int mousePy, int winW, int winH) {
    if (button == SDL_BUTTON_MIDDLE || button == SDL_BUTTON_RIGHT) {
        // Middle/Right click starts pan OR focus action
        int row, col;
        if (view().screenToGrid(mousePx, mousePy, winW, winH, row, col)) {
            // If over a grid cell, prepare to focus/unfocus
            mouseState = MouseState::DOWN_PENDING;
            mouseDownX = mousePx;
            mouseDownY = mousePy;
            mouseDownTime = SDL_GetTicks();
        } else {
            // If not over a grid cell, start panning
            mouseState = MouseState::PANNING;
            view().startPan(mousePx, mousePy);
        }
        return;
    }

    if (button == SDL_BUTTON_LEFT) {
        mouseState = MouseState::DOWN_PENDING;
        mouseDownX = mousePx;
        mouseDownY = mousePy;
        mouseDownTime = SDL_GetTicks();
    }
}

void gameMouseUp(int button, int mousePx, int mousePy, int winW, int winH) {
    bool stateChanged = false;

    if (button == SDL_BUTTON_MIDDLE || button == SDL_BUTTON_RIGHT) {
        if (mouseState == MouseState::PANNING) {
            view().endPan();
            stateChanged = true;
        } else if (mouseState == MouseState::DOWN_PENDING) {
            // This was a click that could have been for focus/unfocus
            int row, col;
            if (view().screenToGrid(mouseDownX, mouseDownY, winW, winH, row, col)) {
                // Click was on a grid cell
                if (button == SDL_BUTTON_RIGHT) { // Right click to enter/exit subgrid
                    if (view().isFocused()) {
                        // If focused, right-click outside a cell to unfocus
                        if (!view().screenToGrid(mousePx, mousePy, winW, winH, row, col)) {
                            view().unfocusGrid();
                            stateChanged = true;
                        }
                    } else {
                        // If not focused, right-click on a grid cell to focus
                        const auto& node = s_model->node(s_model->rootChild(row, col));
                        if (node.type == CellType::GRID) {
                            view().focusGrid(s_model->rootChild(row, col));
                            stateChanged = true;
                        }
                    }
                }
            } else {
                // Click was on empty space
                if (view().isFocused()) {
                    // Right-click on empty space to unfocus
                    view().unfocusGrid();
                    stateChanged = true;
                }
            }
        }
        mouseState = MouseState::NONE;
        stateChanged = true;
    }

    if (button == SDL_BUTTON_LEFT) {
        if (mouseState == MouseState::DOWN_PENDING) {
            int dx = mousePx - mouseDownX;
            int dy = mousePy - mouseDownY;
            bool moved = std::abs(dx) > 5 || std::abs(dy) > 5;

            if (moved) {
                // Dragged, so perform drop
                int dropRow, dropCol;
                if (view().screenToGrid(mousePx, mousePy, winW, winH, dropRow, dropCol)) {
                    s_model->drop(s_model->rootChild(dropRow, dropCol));
                } else {
                    // Dropped outside any valid cell, cancel drag
                    s_model->cancelDrag();
                }
                logState();
            } else {
                // Quick click
                int row, col;
                if (view().screenToGrid(mousePx, mousePy, winW, winH, row, col)) {
                    int idx = s_model->rootChild(row, col);
                    if (s_model->node(idx).type == CellType::ITEM) {
                        // If not dragging, pick up item. If dragging, drop.
                        if (!s_model->hasDrag()) {
                             s_model->pickUp(idx, s_model->node(idx).data.item.count);
                             logState();
                        } else {
                             s_model->drop(s_model->rootChild(row, col));
                             logState();
                        }
                    } else if (s_model->node(idx).type == CellType::GRID && !view().isFocused()) {
                        // If it's a grid and not focused, maybe focus on click too?
                        // For now, sticking to right/middle click for focus.
                    }
                } else {
                    // Clicked on empty space - if dragging, drop it here
                    if (s_model->hasDrag()) {
                        // Try to find nearest cell, or just cancel if no grid.
                        // For simplicity, if dropped on empty space outside a grid, cancel drag.
                        s_model->cancelDrag();
                        logState();
                    }
                }
            }
        } else if (mouseState == MouseState::DRAGGING_ITEM) {
            int row, col;
            if (view().screenToGrid(mousePx, mousePy, winW, winH, row, col)) {
                s_model->drop(s_model->rootChild(row, col));
            } else {
                s_model->cancelDrag();
            }
            logState();
        } else if (mouseState == MouseState::PANNING) {
            view().endPan();
        }
        stateChanged = true;
        mouseState = MouseState::NONE;
    }

    if (stateChanged) {
        // TODO: Update cursor/visual feedback
    }
}

void gameMouseWheel(float dx, float dy) {
    // Zoom is now managed by the camera, potentially relative to focus
    if (dy > 0) view().zoom(1.1f);
    else if (dy < 0) view().zoom(1.0f / 1.1f);
}

void gameRender(int winW, int winH) {
    bool showPointer = (mouseState != MouseState::NONE && mouseState != MouseState::DOWN_PENDING);
    const float* pColor = nullptr;

    if (showPointer) {
        static const float white[] = {1.0f, 1.0f, 1.0f};
        static const float yellow[] = {1.0f, 1.0f, 0.0f};
        static const float grey[] = {0.5f, 0.5f, 0.5f};

        if (mouseState == MouseState::DRAGGING_ITEM) {
            pColor = white;
        } else if (mouseState == MouseState::PANNING) {
            pColor = yellow;
        } else {
            pColor = grey;
        }
    }

    view().render(winW, winH);
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

void gamePrintState() {
    printState(model());
}

void gameGetDragState(int& outId, int& outCount) {
    model().getDragState(outId, outCount);
}
