#include "droid_motion_program.h"

#include <cJSON.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <memory>

uint8_t AdvanceDroidJoints(std::array<int, 3>& angles, const std::array<int, 3>& targets,
                           const std::array<int, 3>& max_steps, size_t moving_tracks,
                           bool defer_head) {
    size_t slots = 4 - std::min<size_t>(moving_tracks, 4);
    uint8_t changed = 0;
    constexpr std::array<size_t, 3> priority = {1, 2, 0};
    for (size_t joint : priority) {
        if (slots == 0 || (joint == 0 && defer_head))
            continue;
        const int delta = targets[joint] - angles[joint];
        if (delta == 0 || max_steps[joint] <= 0)
            continue;
        angles[joint] += std::clamp(delta, -max_steps[joint], max_steps[joint]);
        changed |= 1U << joint;
        --slots;
    }
    return changed;
}

bool AdvanceDroidFrame(size_t& frame, int& cycles_done, size_t frame_count, int repeat) {
    if (++frame < frame_count)
        return false;
    ++cycles_done;
    if (cycles_done >= repeat)
        return true;
    frame = 0;
    return false;
}

bool BuildDroidWaveProgram(const std::string& arms, int cycles, int amplitude,
                           const std::string& pattern, DroidMotionProgram& program,
                           std::string& error) {
    if ((arms != "both" && arms != "left" && arms != "right") || cycles < 1 || cycles > 20 ||
        amplitude < 10 || amplitude > 45 ||
        (pattern != "mirror" && pattern != "parallel" && pattern != "alternate")) {
        error =
            "arms=both/left/right, cycles=1..20, amplitude=10..45, "
            "pattern=mirror/parallel/alternate";
        return false;
    }
    program = DroidMotionProgram{};
    program.repeat = cycles;
    const uint8_t mask = arms == "both" ? 6 : arms == "left" ? 2 : 4;
    const int16_t left_out = arms == "right" ? 0 : static_cast<int16_t>(-amplitude);
    // On the assembled robot, opposite servo signs move the arms physically together.
    // Equal signs move them in opposite physical directions.
    const int16_t right_out =
        arms == "left" ? 0 : static_cast<int16_t>(pattern == "parallel" ? amplitude : -amplitude);
    const auto frame = [mask](int16_t left, int16_t right) -> DroidMotionFrame {
        return {mask, {0, left, right}, 150, 0, 0, 0, 3, 20};
    };
    if (arms == "both" && pattern == "alternate") {
        program.frames[0] = frame(left_out, 0);
        program.frames[1] = frame(0, right_out);
        program.frames[2] = frame(0, 0);
        program.count = 3;
    } else {
        program.frames[0] = frame(left_out, right_out);
        program.frames[1] = frame(0, 0);
        program.count = 2;
    }
    return ValidateDroidProgram(program, error);
}

namespace {
bool Fields(const cJSON* object, std::initializer_list<const char*> allowed) {
    if (!cJSON_IsObject(object))
        return false;
    for (const cJSON* item = object->child; item; item = item->next) {
        bool known = false;
        for (const char* key : allowed) {
            if (item->string && std::strcmp(item->string, key) == 0)
                known = true;
        }
        if (!known)
            return false;
        for (const cJSON* prior = object->child; prior != item; prior = prior->next) {
            if (std::strcmp(prior->string, item->string) == 0)
                return false;
        }
    }
    return true;
}

bool Integer(const cJSON* object, const char* key, int fallback, int low, int high, int& out) {
    const auto* value = cJSON_GetObjectItemCaseSensitive(object, key);
    out = fallback;
    if (!value)
        return true;
    if (!cJSON_IsNumber(value) || !std::isfinite(value->valuedouble) || value->valuedouble < low ||
        value->valuedouble > high || std::floor(value->valuedouble) != value->valuedouble)
        return false;
    out = static_cast<int>(value->valuedouble);
    return true;
}

// Bound parser recursion before handing untrusted JSON to cJSON.
bool ShallowJson(const std::string& json) {
    bool quoted = false, escaped = false;
    int depth = 0;
    for (char ch : json) {
        if (ch == '\0')
            return false;
        if (quoted) {
            if (escaped)
                escaped = false;
            else if (ch == '\\')
                escaped = true;
            else if (ch == '"')
                quoted = false;
        } else if (ch == '"')
            quoted = true;
        else if (ch == '{' || ch == '[') {
            if (++depth > 5)
                return false;
        } else if (ch == '}' || ch == ']') {
            if (--depth < 0)
                return false;
        }
    }
    return !quoted && depth == 0;
}
}  // namespace

bool ValidateDroidProgram(const DroidMotionProgram& program, std::string& error) {
    if (program.count == 0 || program.count > program.kMaxFrames || program.repeat < 1 ||
        program.repeat > 20) {
        error = "Need 1-64 frames and repeat 1-20";
        return false;
    }
    uint64_t budget = 0;
    for (size_t i = 0; i < program.count; ++i) {
        const auto& f = program.frames[i];
        if (f.joint_mask > 7 || f.joint_step_degrees < 1 || f.joint_step_degrees > 4 ||
            f.joint_step_ms < 20 || f.joint_step_ms > 100 || f.hold_ms > 5000 ||
            f.track_ms > 5000 || (f.track_ms != 0 && f.track_ms < 100) ||
            std::abs(f.left_track) > 60 || std::abs(f.right_track) > 60 ||
            ((f.left_track != 0 || f.right_track != 0) && f.track_ms == 0)) {
            error = "Unsafe frame parameters";
            return false;
        }
        for (size_t j = 0; j < 3; ++j) {
            if (!(f.joint_mask & (1U << j)))
                continue;
            if (f.joint_offset[j] < (f.absolute ? 0 : -90) ||
                f.joint_offset[j] > (f.absolute ? 180 : 90)) {
                error = "Joint target outside allowed range";
                return false;
            }
        }
        // Worst-case full joint travel, including quantization by the 20 ms control tick.
        const uint64_t travel = f.joint_mask == 0
                                    ? 0
                                    : ((180 + f.joint_step_degrees - 1) / f.joint_step_degrees) *
                                          ((f.joint_step_ms + 19) / 20 * 20);
        budget += travel + f.track_ms + f.hold_ms + 180;
    }
    if (budget * program.repeat > program.kMaxRunMs) {
        error = "Sequence exceeds the 120-second safety budget; shorten it";
        return false;
    }
    return true;
}

bool ParseDroidProgram(const std::string& json, DroidGestureLookup lookup,
                       DroidMotionProgram& result, std::string& error) {
    result.count = 0;
    result.repeat = 1;
    if (json.empty() || json.size() > result.kMaxJsonBytes || !ShallowJson(json)) {
        error = "Program JSON is empty, too large, or too deeply nested";
        return false;
    }
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(
        cJSON_ParseWithLengthOpts(json.c_str(), json.size() + 1, nullptr, true), cJSON_Delete);
    if (!root || !Fields(root.get(), {"steps", "repeat"}) ||
        !Integer(root.get(), "repeat", 1, 1, 20, result.repeat)) {
        error = "Expected {steps:[...],repeat:1..20}; unknown/duplicate fields are rejected";
        return false;
    }
    const auto* steps = cJSON_GetObjectItemCaseSensitive(root.get(), "steps");
    if (!cJSON_IsArray(steps) || cJSON_GetArraySize(steps) < 1 || cJSON_GetArraySize(steps) > 32) {
        error = "Need 1-32 steps";
        return false;
    }
    for (const cJSON* step = steps->child; step; step = step->next) {
        const auto* gesture = cJSON_GetObjectItemCaseSensitive(step, "gesture");
        if (gesture) {
            const auto* definition =
                cJSON_IsString(gesture) && lookup ? lookup(gesture->valuestring) : nullptr;
            if (!Fields(step, {"gesture"}) || !definition ||
                result.count + definition->frame_count > result.kMaxFrames) {
                error = "Unknown gesture, mixed gesture fields, or too many expanded frames";
                return false;
            }
            for (size_t i = 0; i < definition->frame_count; ++i)
                result.frames[result.count++] = definition->frames[i];
            continue;
        }
        if (!Fields(step, {"pose", "relative", "drive", "tracks", "speed", "duration_ms", "hold_ms",
                           "joint_speed"}) ||
            result.count >= result.kMaxFrames) {
            error = "Unknown/duplicate step fields or too many frames";
            return false;
        }
        DroidMotionFrame frame{};
        const auto* relative = cJSON_GetObjectItemCaseSensitive(step, "relative");
        if (relative && !cJSON_IsBool(relative)) {
            error = "relative must be boolean";
            return false;
        }
        frame.absolute = !cJSON_IsTrue(relative);
        const auto* pose = cJSON_GetObjectItemCaseSensitive(step, "pose");
        if (pose) {
            if (!Fields(pose, {"head", "leftArm", "rightArm"}) || !pose->child) {
                error = "pose requires head, leftArm and/or rightArm";
                return false;
            }
            const char* names[] = {"head", "leftArm", "rightArm"};
            for (size_t j = 0; j < 3; ++j) {
                if (!cJSON_GetObjectItemCaseSensitive(pose, names[j]))
                    continue;
                int angle;
                if (!Integer(pose, names[j], 0, frame.absolute ? 0 : -60, frame.absolute ? 180 : 60,
                             angle)) {
                    error = "Pose angles must be integer 0-180, or relative -60..60";
                    return false;
                }
                frame.joint_mask |= 1U << j;
                frame.joint_offset[j] = angle;
            }
        }
        int speed, duration, hold, joint_speed;
        if (!Integer(step, "speed", 35, 10, 60, speed) ||
            !Integer(step, "duration_ms", 0, 100, 5000, duration) ||
            !Integer(step, "hold_ms", 0, 0, 5000, hold) ||
            !Integer(step, "joint_speed", 100, 10, 100, joint_speed)) {
            error = "Invalid speed/duration/hold/joint_speed";
            return false;
        }
        const auto* drive = cJSON_GetObjectItemCaseSensitive(step, "drive");
        const auto* tracks = cJSON_GetObjectItemCaseSensitive(step, "tracks");
        if (drive && tracks) {
            error = "Use drive or tracks, not both";
            return false;
        }
        if (drive) {
            if (!cJSON_IsString(drive) || duration == 0) {
                error = "drive requires direction and duration_ms 100-5000";
                return false;
            }
            const std::string direction(drive->valuestring);
            if (direction == "forward") {
                frame.left_track = speed;
                frame.right_track = speed;
            } else if (direction == "backward") {
                frame.left_track = -speed;
                frame.right_track = -speed;
            } else if (direction == "left") {
                frame.left_track = -speed;
                frame.right_track = speed;
            } else if (direction == "right") {
                frame.left_track = speed;
                frame.right_track = -speed;
            } else {
                error = "drive must be forward/backward/left/right";
                return false;
            }
            frame.track_ms = duration;
        } else if (tracks) {
            int left = 0, right = 0;
            if (!Fields(tracks, {"left", "right"}) || !tracks->child || duration == 0 ||
                cJSON_GetObjectItemCaseSensitive(step, "speed") ||
                !Integer(tracks, "left", 0, -60, 60, left) ||
                !Integer(tracks, "right", 0, -60, 60, right) || (left == 0 && right == 0)) {
                error = "tracks need signed left/right -60..60 and duration_ms 100..5000";
                return false;
            }
            frame.left_track = left;
            frame.right_track = right;
            frame.track_ms = duration;
        } else if (duration || cJSON_GetObjectItemCaseSensitive(step, "speed")) {
            error = "duration_ms requires drive/tracks; speed requires drive";
            return false;
        }
        if (!pose && (relative || cJSON_GetObjectItemCaseSensitive(step, "joint_speed"))) {
            error = "relative/joint_speed require pose";
            return false;
        }
        if (!pose && !drive && !tracks && hold == 0) {
            error = "Empty step";
            return false;
        }
        frame.hold_ms = hold;
        frame.joint_step_degrees = joint_speed >= 50 ? 2 : 1;
        frame.joint_step_ms = (1000 * frame.joint_step_degrees + joint_speed - 1) / joint_speed;
        result.frames[result.count++] = frame;
    }
    return ValidateDroidProgram(result, error);
}
