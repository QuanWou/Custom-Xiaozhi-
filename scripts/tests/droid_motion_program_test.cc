#include "droid_motion_program.h"
#include "robot_face_ui/robot_face_animator.h"

#include <cassert>
#include <iostream>
#include <string>

const DroidGestureDefinition* Lookup(const std::string& name) {
    static const DroidMotionFrame frames[] = {{6, {0, -35, 35}, 180, 0, 0, 0, 2, 20},
                                              {6, {0, 0, 0}, 180, 0, 0, 0, 2, 20}};
    static const DroidGestureDefinition wave{"wave", frames, 2};
    return name == "wave" ? &wave : nullptr;
}

int main() {
    {
        RobotFaceAnimator eyes;
        RobotEyeFrame frame;
        eyes.Init(0);
        eyes.Update(100, frame);
        const float idle_normal = frame.left_height_factor;
        eyes.Update(5200, frame);
        assert(frame.left_height_factor < idle_normal);
        eyes.Update(8500, frame);
        assert(frame.left_bottom_cut > 0.0f && frame.right_bottom_cut > 0.0f);
        eyes.SetState(RobotFaceState::Moving, 10000);
        eyes.Update(10100, frame);
        assert(frame.left_height_factor < 1.0f);
        eyes.Update(11800, frame);
        assert(frame.left_bottom_cut > 0.0f);
        eyes.SetState(RobotFaceState::Listening, 13000);
        eyes.Update(13100, frame);
        assert(frame.left_bottom_cut == 0.0f);
    }
    {
        const RobotFaceState emotions[] = {RobotFaceState::Happy,       RobotFaceState::Sad,
                                           RobotFaceState::Angry,       RobotFaceState::Surprised,
                                           RobotFaceState::Scared,      RobotFaceState::Tired,
                                           RobotFaceState::Mischievous, RobotFaceState::Loving};
        const RobotEyeShape shapes[] = {RobotEyeShape::Happy,       RobotEyeShape::Sad,
                                        RobotEyeShape::Angry,       RobotEyeShape::Surprised,
                                        RobotEyeShape::Scared,      RobotEyeShape::Sleepy,
                                        RobotEyeShape::Mischievous, RobotEyeShape::Loving};
        for (int i = 0; i < 8; ++i) {
            RobotFaceAnimator eyes;
            RobotEyeFrame frame;
            eyes.Init(0);
            eyes.SetExpression(emotions[i]);
            eyes.SetState(RobotFaceState::Speaking, 0);
            eyes.Update(100, frame);
            assert(frame.shape == shapes[i]);
            assert(eyes.GetState() == RobotFaceState::Speaking);
            eyes.SetState(RobotFaceState::Listening, 200);
            eyes.Update(240, frame);
            assert(frame.shape == RobotEyeShape::Normal);
            eyes.SetState(RobotFaceState::Thinking, 300);
            eyes.Update(340, frame);
            assert(frame.shape == RobotEyeShape::Normal);
        }
        RobotFaceAnimator eyes;
        RobotEyeFrame frame;
        eyes.Init(0);
        bool seen[9] = {};
        for (uint32_t now = 0; now < 40000; now += 40) {
            eyes.Update(now, frame);
            seen[static_cast<int>(frame.shape)] = true;
            assert(frame.left_height_factor > 0 && frame.right_height_factor > 0);
        }
        for (bool shape : seen) {
            assert(shape);
        }
    }
    {
        DroidMotionProgram wave;
        std::string wave_error;
        assert(BuildDroidWaveProgram("both", 3, 35, "mirror", wave, wave_error));
        assert(wave.count == 2 && wave.repeat == 3);
        assert(wave.frames[0].joint_mask == 6 && wave.frames[0].joint_offset[1] == -35 &&
               wave.frames[0].joint_offset[2] == -35);
        assert(BuildDroidWaveProgram("both", 2, 35, "parallel", wave, wave_error));
        assert(wave.frames[0].joint_offset[1] == -35 && wave.frames[0].joint_offset[2] == 35);
        assert(BuildDroidWaveProgram("both", 20, 35, "alternate", wave, wave_error));
        assert(wave.count == 3 && wave.frames[0].joint_offset[1] == -35 &&
               wave.frames[0].joint_offset[2] == 0 && wave.frames[1].joint_offset[1] == 0 &&
               wave.frames[1].joint_offset[2] == -35 && wave.frames[2].joint_offset[2] == 0);
        assert(BuildDroidWaveProgram("left", 3, 35, "mirror", wave, wave_error));
        assert(wave.frames[0].joint_mask == 2 && wave.frames[0].joint_offset[2] == 0);
        assert(BuildDroidWaveProgram("right", 3, 35, "mirror", wave, wave_error));
        assert(wave.frames[0].joint_mask == 4 && wave.frames[0].joint_offset[1] == 0);
        assert(!BuildDroidWaveProgram("both", 21, 35, "mirror", wave, wave_error));
        assert(!BuildDroidWaveProgram("both", 3, 35, "unknown", wave, wave_error));
    }
    for (size_t tracks = 0; tracks <= 2; ++tracks) {
        std::array<int, 3> angles = {90, 90, 90};
        const std::array<int, 3> targets = {120, 50, 130};
        const auto changed = AdvanceDroidJoints(angles, targets, {2, 2, 2}, tracks, tracks != 0);
        int moving = static_cast<int>(tracks);
        for (int joint = 0; joint < 3; ++joint)
            moving += (changed >> joint) & 1;
        assert(moving <= 4);
        assert(angles[1] == 88 && angles[2] == 92);  // Both arms in the same tick.
        assert(angles[0] == (tracks ? 90 : 92));
    }
    std::array<int, 3> settling_angles = {90, 90, 90};
    assert(AdvanceDroidJoints(settling_angles, {120, 90, 90}, {2, 2, 2}, 0, true) == 0);
    assert(AdvanceDroidJoints(settling_angles, {120, 90, 90}, {2, 2, 2}, 0, false) == 1);
    for (int repeat = 1; repeat <= 20; ++repeat) {
        size_t frame = 0;
        int done = 0, advances = 0;
        while (!AdvanceDroidFrame(frame, done, 3, repeat)) {
            assert(++advances < 100);
            assert(frame < 3 && done < repeat);
        }
        assert(advances + 1 == 3 * repeat && done == repeat);
    }
    DroidMotionProgram program;
    std::string error;
    const auto parse = [&](const std::string& json) {
        error.clear();
        return ParseDroidProgram(json, Lookup, program, error);
    };
    assert(parse(
        R"({"repeat":5,"steps":[{"pose":{"leftArm":-35,"rightArm":35},"relative":true,"hold_ms":180},{"pose":{"leftArm":0,"rightArm":0},"relative":true}]})"));
    assert(program.count == 2 && program.repeat == 5);
    assert(program.frames[0].joint_mask == 6 && !program.frames[0].absolute);
    assert(program.frames[0].joint_offset[1] == -35 && program.frames[0].joint_offset[2] == 35);
    assert(parse(
        R"({"steps":[{"pose":{"head":120,"leftArm":40,"rightArm":140},"drive":"left","speed":40,"duration_ms":300},{"gesture":"wave"},{"hold_ms":200}]})"));
    assert(program.count == 4 && program.frames[0].absolute);
    assert(program.frames[0].left_track == -40 && program.frames[0].right_track == 40);
    assert(parse(R"({"repeat":20,"steps":[{"gesture":"wave"}]})"));
    assert(parse(R"({"steps":[{"drive":"right","duration_ms":5000,"speed":60}]})"));
    assert(program.frames[0].left_track == 60 && program.frames[0].right_track == -60);
    assert(parse(R"({"steps":[{"drive":"backward","duration_ms":100}]})"));
    assert(program.frames[0].left_track == -35 && program.frames[0].right_track == -35);
    assert(parse(R"({"steps":[{"tracks":{"left":40,"right":-25},"duration_ms":300}]})"));
    assert(program.frames[0].left_track == 40 && program.frames[0].right_track == -25);
    assert(parse(R"({"steps":[{"tracks":{"left":-30},"duration_ms":250}]})"));
    assert(program.frames[0].left_track == -30 && program.frames[0].right_track == 0);
    assert(!parse(R"({"steps":[{"tracks":{"left":0},"duration_ms":250}]})"));
    assert(!parse(R"({"steps":[{"tracks":{"left":61},"duration_ms":250}]})"));
    assert(!parse(R"({"steps":[{"drive":"left","tracks":{"right":20},"duration_ms":250}]})"));
    assert(parse(R"({"steps":[{"pose":{"head":0},"joint_speed":10}]})"));
    assert(program.frames[0].joint_step_ms == 100);

    const char* invalid[] = {"",
                             "null",
                             "[]",
                             "{}",
                             "{",
                             R"({"steps":[]})",
                             R"({"repeat":0,"steps":[{"hold_ms":1}]})",
                             R"({"repeat":21,"steps":[{"hold_ms":1}]})",
                             R"({"repeat":1.5,"steps":[{"hold_ms":1}]})",
                             R"({"repeat":1,"repeat":2,"steps":[{"hold_ms":1}]})",
                             R"({"steps":[{"pose":{"head":90,"head":91}}]})",
                             R"({"steps":[{"pose":{"head":181}}]})",
                             R"({"steps":[{"pose":{"head":-1}}]})",
                             R"({"steps":[{"pose":{"head":90.5}}]})",
                             R"({"steps":[{"pose":{"head":"90"}}]})",
                             R"({"steps":[{"pose":{"head":null}}]})",
                             R"({"steps":[{"pose":{"head":1e300}}]})",
                             R"({"steps":[{"pose":{"head":61},"relative":true}]})",
                             R"({"steps":[{"pose":{}}]})",
                             R"({"steps":[{}]})",
                             R"({"steps":[{"pose":{"leg":90}}]})",
                             R"({"steps":[{"pose":{"head":90},"relative":1}]})",
                             R"({"steps":[{"drive":"left"}]})",
                             R"({"steps":[{"drive":"left","duration_ms":5001}]})",
                             R"({"steps":[{"drive":"left","duration_ms":99}]})",
                             R"({"steps":[{"drive":"left","duration_ms":200,"speed":61}]})",
                             R"({"steps":[{"drive":"left","duration_ms":200,"speed":-40}]})",
                             R"({"steps":[{"drive":"up","duration_ms":200}]})",
                             R"({"steps":[{"gesture":"missing"}]})",
                             R"({"steps":[{"gesture":"wave","hold_ms":20}]})",
                             R"({"steps":[{"hold_ms":20,"duration_ms":100}]})",
                             R"({"steps":[{"hold_ms":20,"speed":40}]})",
                             R"({"steps":[{"hold_ms":20,"relative":true}]})",
                             R"({"steps":[{"hold_ms":20,"joint_speed":80}]})",
                             R"({"steps":[{"hold_ms":20,"code":"run"}]})",
                             R"({"steps":[{"hold_ms":20}],"forever":true})",
                             R"({"steps":[{"hold_ms":20}]} trailing)",
                             R"({"repeat":20,"steps":[{"gesture":"wave"},{"gesture":"wave"}]})",
                             R"({"steps":[[[[[[[{}]]]]]]]})"};
    for (const auto* json : invalid) {
        if (parse(json)) {
            std::cerr << "Unexpected acceptance: " << json << '\n';
            return 1;
        }
        assert(!error.empty());
    }
    assert(!parse(std::string(4097, ' ')));
    assert(!parse(std::string(R"({"steps":[{"hold_ms":1}]})") + std::string(1, '\0')));
    std::string too_many = "{\"steps\":[";
    for (int i = 0; i < 33; ++i)
        too_many += (i ? "," : "") + std::string("{\"hold_ms\":1}");
    assert(!parse(too_many + "]}"));
    // Validation must reject bad compiled frames, too (helper APIs share this path).
    assert(parse(R"({"steps":[{"hold_ms":1}]})"));
    program.frames[0].left_track = 40;
    assert(!ValidateDroidProgram(program, error));
    std::cout << "PASS: parser, ranges, simultaneous arms, repeats, direction mapping, budgets, "
                 "malformed JSON\n";
}
