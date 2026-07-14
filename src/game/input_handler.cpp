#include "input_handler.hpp"
#include "game.hpp"
#include <cmath>

InputHandler::InputHandler(Game &game) : _game(game) {}

void InputHandler::processEvent(const SDL_Event &e, int winW, int winH) {
  switch (e.type) {
  case SDL_EVENT_MOUSE_MOTION:
    handleMouseMotion(e, winW, winH);
    break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP:
    handleMouseButton(e, winW, winH);
    break;
  case SDL_EVENT_MOUSE_WHEEL:
    handleMouseWheel(e, winW, winH);
    break;
  case SDL_EVENT_KEY_DOWN:
    handleKey(e, winW, winH);
    break;
  case SDL_EVENT_FINGER_DOWN:
    handleFingerDown(e, winW, winH);
    break;
  case SDL_EVENT_FINGER_MOTION:
    handleFingerMotion(e, winW, winH);
    break;
  case SDL_EVENT_FINGER_UP:
  case SDL_EVENT_FINGER_CANCELED:
    handleFingerUp(e, winW, winH);
    break;
  case SDL_EVENT_PINCH_BEGIN:
    handlePinchBegin(e, winW, winH);
    break;
  case SDL_EVENT_PINCH_UPDATE:
    handlePinchUpdate(e, winW, winH);
    break;
  case SDL_EVENT_PINCH_END:
    handlePinchEnd(e, winW, winH);
    break;
  }
}

// --- Mouse passthrough ---

void InputHandler::handleMouseMotion(const SDL_Event &e, int winW, int winH) {
  _game.update((int)e.motion.x, (int)e.motion.y, winW, winH);
}

void InputHandler::handleMouseButton(const SDL_Event &e, int winW, int winH) {
  if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    _game.mouseDown(e.button.button, (int)e.button.x, (int)e.button.y, winW,
                    winH);
  else
    _game.mouseUp(e.button.button, (int)e.button.x, (int)e.button.y, winW,
                  winH);
}

void InputHandler::handleMouseWheel(const SDL_Event &e, int winW, int winH) {
  _game.mouseWheel(e.wheel.x, e.wheel.y, (int)e.wheel.mouse_x,
                   (int)e.wheel.mouse_y, winW, winH);
}

void InputHandler::handleKey(const SDL_Event &e, int winW, int winH) {
  _game.keyDown(e.key.key, (SDL_Keymod)e.key.mod, winW, winH);
}

// --- Touch finger tracking ---

int InputHandler::findFinger(SDL_FingerID id) const {
  for (int i = 0; i < MAX_FINGERS; i++) {
    if (_fingers[i].active && _fingers[i].id == id)
      return i;
  }
  return -1;
}

int InputHandler::addFinger(SDL_FingerID id, float x, float y,
                            Uint64 timestamp) {
  for (int i = 0; i < MAX_FINGERS; i++) {
    if (!_fingers[i].active) {
      _fingers[i] = {id, x, y, x, y, timestamp, true};
      return i;
    }
  }
  return -1;
}

void InputHandler::removeFinger(SDL_FingerID id) {
  int idx = findFinger(id);
  if (idx >= 0)
    _fingers[idx].active = false;
}

int InputHandler::activeFingerCount() const {
  int count = 0;
  for (int i = 0; i < MAX_FINGERS; i++) {
    if (_fingers[i].active)
      count++;
  }
  return count;
}

void InputHandler::getFingerCenter(float &cx, float &cy) const {
  cx = 0.0f;
  cy = 0.0f;
  int count = 0;
  for (int i = 0; i < MAX_FINGERS; i++) {
    if (_fingers[i].active) {
      cx += _fingers[i].currentX;
      cy += _fingers[i].currentY;
      count++;
    }
  }
  if (count > 0) {
    cx /= (float)count;
    cy /= (float)count;
  }
}

void InputHandler::getFingerCenterPixels(int &cx, int &cy, int winW,
                                         int winH) const {
  float fcx, fcy;
  getFingerCenter(fcx, fcy);
  cx = (int)(fcx * winW);
  cy = (int)(fcy * winH);
}

// --- Touch helpers ---

void InputHandler::startOneFingerPan(int pixelX, int pixelY) {
  _touchPanning = true;
  _lastPanPixelX = pixelX;
  _lastPanPixelY = pixelY;
  _game.mouseDown(SDL_BUTTON_MIDDLE, pixelX, pixelY, 0, 0);
}

void InputHandler::endOneFingerPan() {
  if (_touchPanning) {
    _game.mouseUp(SDL_BUTTON_MIDDLE, 0, 0, 0, 0);
    _touchPanning = false;
  }
}

void InputHandler::emitTap(int pixelX, int pixelY, int winW, int winH) {
  _game.mouseDown(SDL_BUTTON_LEFT, pixelX, pixelY, winW, winH);
  _game.mouseUp(SDL_BUTTON_LEFT, pixelX, pixelY, winW, winH);
}

// --- Touch finger events ---

void InputHandler::handleFingerDown(const SDL_Event &e, int winW, int winH) {
  (void)winW;
  (void)winH;
  const SDL_TouchFingerEvent &tf = e.tfinger;
  addFinger(tf.fingerID, tf.x, tf.y, tf.timestamp);

  int count = activeFingerCount();

  if (count == 1) {
    // Start tracking for potential tap or pan
  } else if (count == 2 && _touchPanning) {
    // Second finger arrived while one-finger panning: end pan cleanly
    endOneFingerPan();
  }
  // More than 2 fingers: ignore for now
}

void InputHandler::handleFingerMotion(const SDL_Event &e, int winW, int winH) {
  const SDL_TouchFingerEvent &tf = e.tfinger;
  int idx = findFinger(tf.fingerID);
  if (idx < 0)
    return;

  _fingers[idx].currentX = tf.x;
  _fingers[idx].currentY = tf.y;

  int count = activeFingerCount();

  // Only handle one-finger pan; two-finger handled by pinch events
  if (count != 1 || _pinchActive)
    return;

  Finger &f = _fingers[idx];
  float dx = f.currentX - f.startX;
  float dy = f.currentY - f.startY;
  float distSq = dx * dx + dy * dy;

  if (!_touchPanning) {
    // Check if moved far enough to start panning
    if (distSq > TAP_MAX_DISTANCE * TAP_MAX_DISTANCE) {
      int pixelX = (int)(f.startX * winW);
      int pixelY = (int)(f.startY * winH);
      startOneFingerPan(pixelX, pixelY);
    }
  }

  if (_touchPanning) {
    int pixelX = (int)(tf.x * winW);
    int pixelY = (int)(tf.y * winH);
    _game.update(pixelX, pixelY, winW, winH);
    _lastPanPixelX = pixelX;
    _lastPanPixelY = pixelY;
  }
}

void InputHandler::handleFingerUp(const SDL_Event &e, int winW, int winH) {
  const SDL_TouchFingerEvent &tf = e.tfinger;
  int idx = findFinger(tf.fingerID);
  if (idx < 0)
    return;

  Finger &f = _fingers[idx];

  if (_touchPanning && activeFingerCount() == 1) {
    // Was panning with this finger: end pan
    endOneFingerPan();
  } else if (!_touchPanning && !_pinchActive && activeFingerCount() == 1) {
    // Was not panning and not pinching: check for tap
    float dx = f.currentX - f.startX;
    float dy = f.currentY - f.startY;
    float distSq = dx * dx + dy * dy;
    Uint64 duration = tf.timestamp - f.startTime;

    if (distSq <= TAP_MAX_DISTANCE * TAP_MAX_DISTANCE &&
        duration <= TAP_MAX_DURATION_NS) {
      int pixelX = (int)(f.currentX * winW);
      int pixelY = (int)(f.currentY * winH);
      emitTap(pixelX, pixelY, winW, winH);
    }
  }

  removeFinger(tf.fingerID);
}

// --- Pinch gesture events ---

void InputHandler::handlePinchBegin(const SDL_Event &e, int winW, int winH) {
  (void)e;
  (void)winW;
  (void)winH;
  _pinchActive = true;
  // End any one-finger pan that might still be active
  endOneFingerPan();
}

void InputHandler::handlePinchUpdate(const SDL_Event &e, int winW, int winH) {
  if (!_pinchActive)
    return;

  float scale = e.pinch.scale;
  if (scale <= 0.0f)
    return;

  // Convert multiplicative scale to discrete zoom steps (each step = 1.1x)
  float steps = std::log(scale) / std::log(1.1f);
  int discreteSteps = (int)std::round(steps);
  if (discreteSteps == 0)
    return;

  // Get pinch center from tracked fingers (SDL3 doesn't provide focus_x/y)
  int centerPX, centerPY;
  getFingerCenterPixels(centerPX, centerPY, winW, winH);

  float dy = (discreteSteps > 0) ? 1.0f : -1.0f;
  for (int i = 0; i < std::abs(discreteSteps); i++) {
    _game.mouseWheel(0.0f, dy, centerPX, centerPY, winW, winH);
  }
}

void InputHandler::handlePinchEnd(const SDL_Event &e, int winW, int winH) {
  (void)e;
  (void)winW;
  (void)winH;
  _pinchActive = false;
}
