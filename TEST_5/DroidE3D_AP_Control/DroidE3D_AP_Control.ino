#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_arduino_version.h>
#include <set>
#include "RobotUI.h"

// Droid E3D v4.1: ESP32-S3, Arduino-ESP32 2.x / 3.x.
// External library: WebSockets by Markus Sattler (Links2004).
// All servo outputs use explicit LEDC channels. No ESP32Servo dependency.
// GPIO48, audio, display and sensors are not initialized by this sketch.

const char AP_SSID[] = "Droid-WALLE";
const char AP_PASSWORD[] = "33333333";
constexpr uint8_t SERVO_COUNT = 5;
// Output order: left track, right track, head, left arm, right arm.
constexpr uint8_t SERVO_PINS[SERVO_COUNT] = {4, 5, 6, 7, 15};
constexpr uint8_t PWM_CHANNELS[SERVO_COUNT] = {0, 1, 2, 3, 4};
const char* const SERVO_NAMES[SERVO_COUNT] = {
  "leftTrack", "rightTrack", "head", "leftArm", "rightArm"
};
constexpr uint32_t PWM_HZ = 50;
constexpr uint8_t PWM_BITS = 14; // ESP32-S3 LEDC supports up to 14 bits.
constexpr uint32_t PWM_PERIOD_US = 1000000UL / PWM_HZ;
constexpr uint32_t PWM_COUNTS = 1UL << PWM_BITS;
constexpr uint32_t MOTOR_TIMEOUT_MS = 700;
constexpr uint32_t JOINT_STEP_MS = 20;
constexpr int JOINT_STEP_DEG = 2;
// Gesture load scheduling: tracks + two arms first; head follows after tracks stop.
// This limits changing motion commands, not holding current or measured motion.
constexpr uint32_t HEAD_RESUME_GAP_MS = 120;
constexpr int TRACK_RANGE_US = 420;
constexpr uint32_t GESTURE_TIMEOUT_MS = 1200;
constexpr int DEFAULT_HOME[3] = {90, 90, 90};
const char* const HOME_KEYS[3] = {"headHome", "leftHome", "rightHome"};
constexpr int DEFAULT_STOP_US[2] = {1505, 1500};
constexpr int DEFAULT_FORWARD_SIGN[2] = {-1, -1};
// Match the successful standalone tests: head attach(6,1000,2000),
// arms attach(pin) with ESP32Servo defaults 544..2400 us.
// These endpoints are electrical ranges; physical angles depend on the servo.
constexpr int JOINT_MIN_US[3] = {1000, 544, 544};
constexpr int JOINT_MAX_US[3] = {2000, 2400, 2400};

WebServer http(80);
WebSocketsServer socketServer(81);
Preferences preferences;
std::set<uint8_t> clients;
int owner = -1;
bool prefsReady = false;
bool pwmReady = false;
bool attached[SERVO_COUNT] = {};
int outputPulseUs[SERVO_COUNT] = {};
int trackSpeed[2] = {};
int stopUs[2] = {1505, 1500};
int forwardSign[2] = {-1, -1};
int jointHome[3] = {90, 90, 90};
int jointAngle[3] = {90, 90, 90};
int jointTarget[3] = {90, 90, 90};
uint32_t lastControlHeartbeat = 0;
// All choreography belongs to the firmware. Bit masks: head=1,left arm=2,right arm=4.
struct MotionFrame {
  uint8_t mask;
  int offset[3]; // Relative to the saved home angles: head, left arm, right arm.
  uint16_t holdMs; // Hold after joints reach the commanded angles.
  int8_t leftSpeed, rightSpeed; // Logical track speeds, -100..100.
  uint16_t trackMs; // Independent burst deadline; never waits for joint completion.
  uint8_t stepDeg;
  uint16_t stepMs;
};
// Track bursts are timed, not measured distance/rotation. Positive = forward.
const MotionFrame LOOK_LEFT[] = {
  {1, {-30,0,0}, 180, 0,0,0, 2,20},
};
const MotionFrame LOOK_RIGHT[] = {
  {1, {30,0,0}, 180, 0,0,0, 2,20},
};
const MotionFrame LOOK_AROUND[] = {
  {1, {-30,0,0}, 400, 0,0,0, 2,20},
  {1, {30,0,0}, 400, 0,0,0, 2,20},
  {1, {0,0,0}, 200, 0,0,0, 2,20},
};
const MotionFrame SHAKE_HEAD[] = {
  {1, {-25,0,0}, 120, 0,0,0, 2,20},
  {1, {25,0,0}, 120, 0,0,0, 2,20},
  {1, {-25,0,0}, 120, 0,0,0, 2,20},
  {1, {25,0,0}, 120, 0,0,0, 2,20},
  {1, {0,0,0}, 120, 0,0,0, 2,20},
};
const MotionFrame WAVE_LEFT[] = {
  {2, {0,-40,0}, 140, 0,0,0, 2,20},
  {2, {0,-15,0}, 140, 0,0,0, 2,20},
  {2, {0,-40,0}, 140, 0,0,0, 2,20},
  {2, {0,-15,0}, 140, 0,0,0, 2,20},
  {2, {0,-40,0}, 140, 0,0,0, 2,20},
  {2, {0,0,0}, 140, 0,0,0, 2,20},
};
const MotionFrame WAVE_RIGHT[] = {
  {4, {0,0,40}, 140, 0,0,0, 2,20},
  {4, {0,0,15}, 140, 0,0,0, 2,20},
  {4, {0,0,40}, 140, 0,0,0, 2,20},
  {4, {0,0,15}, 140, 0,0,0, 2,20},
  {4, {0,0,40}, 140, 0,0,0, 2,20},
  {4, {0,0,0}, 140, 0,0,0, 2,20},
};
const MotionFrame CHEER[] = {
  {7, {0,-40,40}, 160, 30,30,240, 2,20},
  {7, {-20,-15,15}, 140, -28,28,200, 2,20},
  {7, {20,-40,40}, 140, 28,-28,200, 2,20},
  {7, {-20,-15,15}, 140, -28,28,200, 2,20},
  {7, {20,-40,40}, 160, 28,-28,200, 2,20},
  {7, {0,0,0}, 200, -30,-30,240, 2,20},
};
const MotionFrame SAD[] = {
  {7, {-15,25,-25}, 650, -22,-22,280, 1,40},
  {7, {15,18,-18}, 500, 0,0,0, 1,40},
  {7, {-10,25,-25}, 650, 0,0,0, 1,40},
  {7, {0,0,0}, 250, 0,0,0, 1,40},
};
const MotionFrame CURIOUS[] = {
  {7, {-25,-20,10}, 450, -24,24,200, 1,25},
  {7, {25,-10,25}, 450, 24,-24,200, 1,25},
  {7, {0,-15,15}, 300, 22,22,200, 1,25},
  {7, {0,0,0}, 200, -22,-22,200, 1,25},
};
const MotionFrame GREET[] = {
  {7, {0,-35,35}, 200, 25,25,220, 2,20},
  {7, {-12,-10,35}, 150, 0,0,0, 2,20},
  {7, {12,-35,10}, 150, 0,0,0, 2,20},
  {7, {-12,-10,35}, 150, 0,0,0, 2,20},
  {7, {12,-35,10}, 150, 0,0,0, 2,20},
  {7, {0,0,0}, 200, -25,-25,220, 2,20},
};
const MotionFrame SHOW[] = {
  {7, {-25,-35,0}, 180, -32,32,260, 2,20},
  {7, {25,0,35}, 180, 32,-32,260, 2,20},
  {7, {-25,-35,0}, 180, -32,32,260, 2,20},
  {7, {25,0,35}, 180, 32,-32,260, 2,20},
  {7, {0,-40,40}, 250, 30,30,280, 2,20},
  {7, {0,-10,10}, 180, -30,-30,280, 2,20},
  {7, {-20,-35,15}, 150, -28,28,200, 2,20},
  {7, {20,-15,35}, 150, 28,-28,200, 2,20},
  {7, {0,0,0}, 250, 0,0,0, 2,20},
};
const MotionFrame HOME_POSE[] = {
  {7, {0,0,0}, 150, 0,0,0, 2,20},
};
struct GestureDefinition { const char* name; const MotionFrame* frames; uint8_t count; };
#define FRAME_COUNT(frames) (sizeof(frames) / sizeof((frames)[0]))
const GestureDefinition GESTURES[] = {
  {"lookLeft",LOOK_LEFT,FRAME_COUNT(LOOK_LEFT)},
  {"lookRight",LOOK_RIGHT,FRAME_COUNT(LOOK_RIGHT)},
  {"lookAround",LOOK_AROUND,FRAME_COUNT(LOOK_AROUND)},
  {"shakeHead",SHAKE_HEAD,FRAME_COUNT(SHAKE_HEAD)},
  {"waveLeft",WAVE_LEFT,FRAME_COUNT(WAVE_LEFT)},
  {"waveRight",WAVE_RIGHT,FRAME_COUNT(WAVE_RIGHT)},
  {"cheer",CHEER,FRAME_COUNT(CHEER)},
  {"show",SHOW,FRAME_COUNT(SHOW)},
  {"sad",SAD,FRAME_COUNT(SAD)},
  {"curious",CURIOUS,FRAME_COUNT(CURIOUS)},
  {"greet",GREET,FRAME_COUNT(GREET)},
  {"home",HOME_POSE,FRAME_COUNT(HOME_POSE)}
};
const GestureDefinition* activeGesture = nullptr;
uint8_t gestureFrame = 0;
bool frameReached = false;
uint32_t frameReachedAt = 0;
uint32_t frameStartedAt = 0;
bool frameTracksRunning = false;
bool headWaitingForTracks = false;
uint32_t lastTracksStoppedAt = 0;
void stopTracks();
void cancelGesture(bool freeze);
uint32_t lastMotorCommand = 0;
uint32_t lastJointStep = 0;

// Disable outputs only on initialization/driver failure, never for normal STOP.
void disableOutputs() {
  pwmReady = false;
  cancelGesture(true);
  trackSpeed[0] = trackSpeed[1] = 0;
  for (uint8_t i = 0; i < SERVO_COUNT; ++i) {
    if (attached[i]) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcDetach(SERVO_PINS[i]);
#else
      ledcDetachPin(SERVO_PINS[i]);
#endif
    }
    attached[i] = false;
    pinMode(SERVO_PINS[i], OUTPUT);
    digitalWrite(SERVO_PINS[i], LOW);
    outputPulseUs[i] = 0;
  }
}

bool writeOutput(uint8_t output, int pulseUs) {
  if (output >= SERVO_COUNT || !attached[output]) return false;
  if (pulseUs < 500 || pulseUs > 2500) return false;
  if (outputPulseUs[output] == pulseUs) return true;
  // Duty uses 2^resolution counts per PWM period, rounded to nearest count.
  const uint32_t duty = ((uint32_t)pulseUs * PWM_COUNTS + PWM_PERIOD_US / 2) / PWM_PERIOD_US;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (!ledcWriteChannel(PWM_CHANNELS[output], duty)) {
    Serial.printf("PWM ERROR: gpio=%u channel=%u\n", SERVO_PINS[output], PWM_CHANNELS[output]);
    disableOutputs();
    return false;
  }
#else
  ledcWrite(PWM_CHANNELS[output], duty);
#endif
  outputPulseUs[output] = pulseUs;
  return true;
}

int angleToPulse(uint8_t joint, int angle) {
  // User-facing joint commands are degrees, 0..180.
  return JOINT_MIN_US[joint] + (JOINT_MAX_US[joint] - JOINT_MIN_US[joint]) * angle / 180;
}

bool initOutputs() {
  for (uint8_t i = 0; i < SERVO_COUNT; ++i) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    attached[i] = ledcAttachChannel(SERVO_PINS[i], PWM_HZ, PWM_BITS, PWM_CHANNELS[i]);
#else
    attached[i] = ledcSetup(PWM_CHANNELS[i], PWM_HZ, PWM_BITS) > 0;
    if (attached[i]) ledcAttachPin(SERVO_PINS[i], PWM_CHANNELS[i]);
#endif
    if (!attached[i]) {
      Serial.printf("ATTACH ERROR: gpio=%u channel=%u\n", SERVO_PINS[i], PWM_CHANNELS[i]);
      disableOutputs();
      return false;
    }
    const int initialUs = i < 2 ? stopUs[i] : angleToPulse(i - 2, jointAngle[i - 2]);
    if (!writeOutput(i, initialUs)) {
      disableOutputs();
      return false;
    }
    Serial.printf("PWM %s gpio=%u channel=%u initial=%d us\n",
                  SERVO_NAMES[i], SERVO_PINS[i], PWM_CHANNELS[i], initialUs);
  }
  pwmReady = true;
  return true;
}

void applyTrack(uint8_t side) {
  if (!pwmReady || side > 1) return;
  const int pulse = stopUs[side] + forwardSign[side] * trackSpeed[side] * TRACK_RANGE_US / 100;
  writeOutput(side, constrain(pulse, 1000, 2000));
}

void stopTracks() {
  const bool wasMoving = trackSpeed[0] != 0 || trackSpeed[1] != 0;
  if (wasMoving) lastTracksStoppedAt = millis();
  trackSpeed[0] = trackSpeed[1] = 0;
  applyTrack(0);
  applyTrack(1);
}

void setTracks(int left, int right) {
  if (!pwmReady) return;
  const bool changed = trackSpeed[0] != left || trackSpeed[1] != right;
  trackSpeed[0] = constrain(left, -100, 100);
  trackSpeed[1] = constrain(right, -100, 100);
  lastMotorCommand = millis();
  applyTrack(0);
  applyTrack(1);
  if (changed) Serial.printf("TRACK left=%d right=%d | gpio4=%d us gpio5=%d us\n",
    trackSpeed[0], trackSpeed[1], outputPulseUs[0], outputPulseUs[1]);
}

void updateJoints() {
  // Never step the head while a gesture drives either track. Two tracks plus
  // two arms = at most four commanded moving servos. PWM holding stays active.
  if (activeGesture && headWaitingForTracks &&
      trackSpeed[0] == 0 && trackSpeed[1] == 0 &&
      (uint32_t)(millis() - lastTracksStoppedAt) >= HEAD_RESUME_GAP_MS) {
    headWaitingForTracks = false;
  }
  const uint32_t stepMs = activeGesture ? activeGesture->frames[gestureFrame].stepMs : JOINT_STEP_MS;
  const int stepDeg = activeGesture ? activeGesture->frames[gestureFrame].stepDeg : JOINT_STEP_DEG;
  if (!pwmReady || (uint32_t)(millis() - lastJointStep) < stepMs) return;
  lastJointStep = millis();
  for (uint8_t joint = 0; joint < 3; ++joint) {
    if (joint == 0 && activeGesture &&
        (headWaitingForTracks || trackSpeed[0] != 0 || trackSpeed[1] != 0)) continue;
    int delta = jointTarget[joint] - jointAngle[joint];
    if (delta == 0) continue;
    jointAngle[joint] += constrain(delta, -stepDeg, stepDeg);
    if (!writeOutput(joint + 2, angleToPulse(joint, jointAngle[joint]))) return;
  }
}

String field(const String& message, int index) {
  int begin = 0;
  for (int i = 0; i < index; ++i) {
    const int split = message.indexOf('|', begin);
    if (split < 0) return "";
    begin = split + 1;
  }
  const int end = message.indexOf('|', begin);
  return end < 0 ? message.substring(begin) : message.substring(begin, end);
}

int fieldCount(const String& message) {
  int count = 1;
  for (size_t i = 0; i < message.length(); ++i) if (message[i] == '|') ++count;
  return count;
}

bool parseNumber(const String& text, int low, int high, int& value) {
  if (text.length() == 0 || text.length() > 5) return false;
  int start = text[0] == '-' ? 1 : 0;
  if (start == (int)text.length()) return false;
  for (int i = start; i < (int)text.length(); ++i) if (!isDigit(text[i])) return false;
  const long parsed = text.toInt();
  if (parsed < low || parsed > high) return false;
  value = (int)parsed;
  return true;
}

int jointIndex(const String& name) {
  if (name == "head") return 0;
  if (name == "leftArm") return 1;
  if (name == "rightArm") return 2;
  return -1;
}

int sideIndex(const String& name) {
  if (name == "left") return 0;
  if (name == "right") return 1;
  return -1;
}

String stateMessage() {
  return String("STATE|") + trackSpeed[0] + "|" + trackSpeed[1] + "|" +
    jointTarget[0] + "|" + jointTarget[1] + "|" + jointTarget[2] + "|" +
    stopUs[0] + "|" + stopUs[1] + "|" + forwardSign[0] + "|" + forwardSign[1] + "|" + (pwmReady ? 1 : 0) + "|" +
    jointHome[0] + "|" + jointHome[1] + "|" + jointHome[2] + "|" + (activeGesture ? activeGesture->name : "idle");
}

void sendText(uint8_t client, const String& message) {
  // Works with WebSockets versions whose String overload takes a non-const reference.
  String copy = message;
  socketServer.sendTXT(client, copy);
}

void broadcastState() {
  String state = stateMessage();
  socketServer.broadcastTXT(state);
}

// Cancel holds each joint at its last commanded step. No feedback sensor is assumed.
void cancelGesture(bool freeze) {
  const bool ownedTracks = activeGesture || frameTracksRunning;
  activeGesture = nullptr;
  frameTracksRunning = false;
  headWaitingForTracks = false;
  if (ownedTracks) stopTracks();
  frameReached = false;
  if (freeze) for (uint8_t joint = 0; joint < 3; ++joint) jointTarget[joint] = jointAngle[joint];
}

void applyGestureFrame() {
  if (!activeGesture) return;
  const MotionFrame& frame = activeGesture->frames[gestureFrame];
  for (uint8_t joint = 0; joint < 3; ++joint) {
    if (frame.mask & (1 << joint)) jointTarget[joint] = constrain(jointHome[joint] + frame.offset[joint], 0, 180);
  }
  frameReached = false;
  frameStartedAt = millis();
  frameTracksRunning = frame.trackMs > 0 && (frame.leftSpeed || frame.rightSpeed);
  headWaitingForTracks = frameTracksRunning;
  Serial.printf("FRAME %s %u/%u head=%d leftArm=%d rightArm=%d headDeferred=%d\n",
                activeGesture->name, gestureFrame + 1, activeGesture->count,
                jointTarget[0], jointTarget[1], jointTarget[2], headWaitingForTracks);
  if (frameTracksRunning) setTracks(frame.leftSpeed, frame.rightSpeed);
  else stopTracks();
}

bool startGesture(const String& name) {
  const GestureDefinition* selected = nullptr;
  for (const auto& gesture : GESTURES) if (name == gesture.name) { selected = &gesture; break; }
  if (!selected) return false;
  cancelGesture(true);
  stopTracks();
  activeGesture = selected;
  gestureFrame = 0;
  lastControlHeartbeat = millis();
  applyGestureFrame();
  Serial.printf("GESTURE %s\n", selected->name);
  return true;
}

void updateGesture() {
  if (!pwmReady || !activeGesture) return;
  if ((uint32_t)(millis() - lastControlHeartbeat) >= GESTURE_TIMEOUT_MS) {
    cancelGesture(true);
    broadcastState();
    Serial.println("GESTURE STOP: heartbeat timeout");
    return;
  }
  const MotionFrame& frame = activeGesture->frames[gestureFrame];
  if (frameTracksRunning && (uint32_t)(millis() - frameStartedAt) >= frame.trackMs) {
    frameTracksRunning = false;
    stopTracks();
    broadcastState();
    if (!pwmReady || !activeGesture) return;
  }
  for (uint8_t joint = 0; joint < 3; ++joint) {
    if ((frame.mask & (1 << joint)) && jointAngle[joint] != jointTarget[joint]) return;
  }
  if (!frameReached) { frameReached = true; frameReachedAt = millis(); }
  if (frameTracksRunning || (uint32_t)(millis() - frameReachedAt) < frame.holdMs) return;
  if (++gestureFrame >= activeGesture->count) cancelGesture(false);
  else applyGestureFrame();
  broadcastState();
}

void errorReply(uint8_t client, const char* reason) {
  sendText(client, String("ERROR|") + reason);
}

void handleCommand(uint8_t client, const String& message) {
  const String command = field(message, 0);
  const int count = fieldCount(message);
  if (command == "STATE" && count == 1) { sendText(client, stateMessage()); return; }
  if (owner != client) { errorReply(client, "Thiet bi khac dang dieu khien."); return; }
  if (!pwmReady) { errorReply(client, "Loi khoi tao PWM. Xem Serial Monitor 115200."); return; }

  if (command == "PING" && count == 1) {
    lastControlHeartbeat = millis();
    return;
  } else if (command == "HALT" && count == 1) {
    stopTracks();
    cancelGesture(true);
  } else if (command == "CANCEL" && count == 1) {
    cancelGesture(true);
  } else if (command == "GESTURE" && count == 2) {
    if (!startGesture(field(message, 1))) { errorReply(client, "Dong tac khong hop le."); return; }
  } else if (command == "STOP" && count == 1) {
    if (activeGesture) cancelGesture(true);
    stopTracks();
  } else if (command == "DRIVE" && count == 3) {
    int speed;
    if (!parseNumber(field(message, 2), 0, 100, speed)) { errorReply(client, "Toc do 0-100."); return; }
    const String direction = field(message, 1);
    if (!(direction == "forward" || direction == "backward" || direction == "left" || direction == "right")) {
      errorReply(client, "Huong khong hop le."); return;
    }
    if (activeGesture) cancelGesture(true);
    if (direction == "forward") setTracks(speed, speed);
    else if (direction == "backward") setTracks(-speed, -speed);
    else if (direction == "left") setTracks(-speed, speed);
    else if (direction == "right") setTracks(speed, -speed);
    else { errorReply(client, "Huong khong hop le."); return; }
  } else if (command == "WHEELS" && count == 3) {
    int left, right;
    if (!parseNumber(field(message, 1), -100, 100, left) || !parseNumber(field(message, 2), -100, 100, right)) {
      errorReply(client, "Toc do xich phai tu -100 den 100."); return;
    }
    if (activeGesture) cancelGesture(true);
    setTracks(left, right);
  } else if (command == "ANGLE" && count == 3) {
    const int joint = jointIndex(field(message, 1));
    int angle;
    if (joint < 0 || !parseNumber(field(message, 2), 0, 180, angle)) {
      errorReply(client, "Khop hoac goc 0-180 khong hop le."); return;
    }
    cancelGesture(true);
    jointTarget[joint] = angle;
    Serial.printf("ANGLE %s gpio=%u target=%d deg\n", SERVO_NAMES[joint + 2], SERVO_PINS[joint + 2], angle);
  } else if (command == "CENTER" && count == 1) {
    startGesture("home");
  } else if (command == "HOMESET" && count == 3) {
    const int joint = jointIndex(field(message, 1));
    int angle;
    if (joint < 0 || !parseNumber(field(message, 2), 0, 180, angle)) {
      errorReply(client, "Goc mac dinh phai tu 0 den 180."); return;
    }
    if (trackSpeed[0] != 0 || trackSpeed[1] != 0 || activeGesture) {
      errorReply(client, "Dung robot truoc khi luu tu the."); return;
    }
    if (!prefsReady || preferences.putInt(HOME_KEYS[joint], angle) != sizeof(int32_t)) {
      errorReply(client, "Khong luu duoc tu the."); return;
    }
    jointHome[joint] = angle;
    sendText(client, String("HOMEOK|") + field(message, 1) + "|" + angle);
  } else if ((command == "CAL" && count == 3) || (command == "FLIP" && count == 2)) {
    if (trackSpeed[0] != 0 || trackSpeed[1] != 0 || activeGesture) { errorReply(client, "Dung dong tac va hai xich truoc khi hieu chinh."); return; }
    const int side = sideIndex(field(message, 1));
    if (side < 0) { errorReply(client, "Ten xich khong hop le."); return; }
    if (!prefsReady) { errorReply(client, "Bo nho luu hieu chinh chua san sang."); return; }
    if (command == "CAL") {
      int pulse;
      if (!parseNumber(field(message, 2), 1400, 1600, pulse)) { errorReply(client, "Xung dung phai tu 1400 den 1600 us."); return; }
      if (preferences.putInt(side == 0 ? "leftStop" : "rightStop", pulse) != sizeof(int32_t)) {
        errorReply(client, "Khong luu duoc hieu chinh."); return;
      }
      stopUs[side] = pulse;
      applyTrack(side); // ONLY the chosen track; never head or arms.
      Serial.printf("CAL %s gpio=%u neutral=%d us\n", SERVO_NAMES[side], SERVO_PINS[side], pulse);
      sendText(client, String("CALOK|") + field(message, 1) + "|" + pulse);
    } else {
      const int sign = -forwardSign[side];
      if (preferences.putBool(side == 0 ? "leftFlip" : "rightFlip", sign != DEFAULT_FORWARD_SIGN[side]) != 1) {
        errorReply(client, "Khong luu duoc chieu quay."); return;
      }
      forwardSign[side] = sign;
      sendText(client, "OK|Da luu chieu quay.");
    }
  } else {
    errorReply(client, "Lenh khong hop le."); return;
  }
  broadcastState();
}

void onSocket(uint8_t client, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    clients.insert(client);
    if (owner < 0) owner = client;
    sendText(client, owner == client ? "OWNER|1" : "OWNER|0");
    sendText(client, stateMessage());
  } else if (type == WStype_DISCONNECTED) {
    clients.erase(client);
    if (owner == client) {
      stopTracks();
      cancelGesture(true);
      owner = -1;
      if (!clients.empty()) {
        owner = *clients.begin();
        sendText((uint8_t)owner, "OWNER|1");
      }
      broadcastState();
    }
  } else if (type == WStype_TEXT) {
    if (length == 0 || length > 80) { errorReply(client, "Do dai lenh khong hop le."); return; }
    String message;
    message.reserve(length + 1);
    for (size_t i = 0; i < length; ++i) {
      if (payload[i] < 32 || payload[i] > 126) { errorReply(client, "Ky tu lenh khong hop le."); return; }
      message += (char)payload[i];
    }
    handleCommand(client, message);
  }
}

String diagnosticsJson() {
  String json = String("{\"firmware\":\"4.1-max4-motion\",\"pwmReady\":") + (pwmReady ? "true" : "false") + ",\"outputs\":[";
  for (uint8_t i = 0; i < SERVO_COUNT; ++i) {
    if (i) json += ',';
    json += String("{\"name\":\"") + SERVO_NAMES[i] + "\",\"gpio\":" + SERVO_PINS[i] +
      ",\"channel\":" + PWM_CHANNELS[i] + ",\"commandedPulseUs\":" + outputPulseUs[i] + "}";
  }
  return json + "]}";
}

void loadCalibration() {
  prefsReady = preferences.begin("droid-tracks", false);
  if (!prefsReady) { Serial.println("NVS unavailable: using 1505/1500 defaults."); return; }
  for (uint8_t side = 0; side < 2; ++side) {
    stopUs[side] = preferences.getInt(side == 0 ? "leftStop" : "rightStop", DEFAULT_STOP_US[side]);
    if (stopUs[side] < 1400 || stopUs[side] > 1600) stopUs[side] = DEFAULT_STOP_US[side];
    forwardSign[side] = preferences.getBool(side == 0 ? "leftFlip" : "rightFlip", false)
      ? -DEFAULT_FORWARD_SIGN[side] : DEFAULT_FORWARD_SIGN[side];
  }
  // One-time update after the user physically re-centered the head at 90 degrees.
  const bool migrateHead = !preferences.getBool("v4Head90", false);
  if (migrateHead && preferences.putInt("headHome", 90) == sizeof(int32_t))
    preferences.putBool("v4Head90", true);
  for (uint8_t joint = 0; joint < 3; ++joint) {
    int home = (joint == 0 && migrateHead) ? 90 : preferences.getInt(HOME_KEYS[joint], DEFAULT_HOME[joint]);
    jointHome[joint] = (home >= 0 && home <= 180) ? home : DEFAULT_HOME[joint];
    jointAngle[joint] = jointTarget[joint] = jointHome[joint];
  }
  // One-time migration: use the user's confirmed LEFT neutral of 1505 us.
  // Later CAL adjustments remain stored across resets and subsequent flashes.
  if (!preferences.getBool("v2Neutral", false)) {
    stopUs[0] = 1505;
    if (preferences.putInt("leftStop", 1505) == sizeof(int32_t)) preferences.putBool("v2Neutral", true);
  }
}

void setup() {
  Serial.begin(115200); // Never wait for a computer/Serial connection.
  Serial.printf("BOOT Droid-E3D v4.1 max4-motion core=%d reset_reason=%d\n", ESP_ARDUINO_VERSION_MAJOR, (int)esp_reset_reason());
  loadCalibration();
  initOutputs();
  WiFi.mode(WIFI_AP);
  const bool apReady = WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.printf("AP %s: %s\n", AP_SSID, apReady ? "ready" : "FAILED");
  Serial.print("Open http://");
  Serial.println(WiFi.softAPIP());
  http.on("/", HTTP_GET, []() {
    http.sendHeader("Cache-Control", "no-store");
    http.send_P(200, "text/html; charset=utf-8", ROBOT_UI_HTML);
  });
  http.on("/status", HTTP_GET, []() {
    http.sendHeader("Cache-Control", "no-store");
    http.send(200, "application/json", diagnosticsJson());
  });
  http.onNotFound([]() { http.send(404, "text/plain", "Not found"); });
  http.begin();
  socketServer.begin();
  socketServer.onEvent(onSocket);
  socketServer.enableHeartbeat(10000, 3000, 2);
}

void loop() {
  http.handleClient();
  socketServer.loop();
  updateJoints();
  updateGesture();
  if ((trackSpeed[0] != 0 || trackSpeed[1] != 0) &&
      (uint32_t)(millis() - lastMotorCommand) >= MOTOR_TIMEOUT_MS) {
    stopTracks();
    if (activeGesture) cancelGesture(true);
    Serial.println("TRACK STOP: command timeout");
    broadcastState();
  }
  delay(1);
}
