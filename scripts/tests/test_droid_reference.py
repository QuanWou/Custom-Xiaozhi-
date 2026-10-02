"""Reference-fidelity checks; these do not replace physical robot validation."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "main/boards/custom-s3-voice"


class DroidReferenceTests(unittest.TestCase):
    def test_original_ui_styles_and_icons_preserved(self):
        original = (ROOT / "TEST_5/DroidE3D_AP_Control/RobotUI.h").read_text(encoding="utf-8")
        integrated = (BOARD / "droid_web_ui.h").read_text(encoding="utf-8")
        for pattern in (r"<style>(.*?)</style>", r'<svg class="icons"(.*?)</defs></svg>'):
            self.assertEqual(re.search(pattern, original, re.S).group(1),
                             re.search(pattern, integrated, re.S).group(1))

    def test_all_original_gesture_frames_preserved(self):
        original = (ROOT / "TEST_5/DroidE3D_AP_Control/DroidE3D_AP_Control.ino").read_text(encoding="utf-8")
        integrated = (BOARD / "droid_robot_controller.cc").read_text(encoding="utf-8")
        pairs = {"LOOK_LEFT": "LookLeft", "LOOK_RIGHT": "LookRight", "LOOK_AROUND": "LookAround",
                 "SHAKE_HEAD": "ShakeHead", "WAVE_LEFT": "WaveLeft", "WAVE_RIGHT": "WaveRight",
                 "CHEER": "Cheer", "SAD": "Sad", "CURIOUS": "Curious", "GREET": "Greet",
                 "SHOW": "Show", "HOME_POSE": "Home"}
        for old, new in pairs.items():
            with self.subTest(gesture=old):
                a = re.search(r"MotionFrame " + old + r"\[\] = \{(.*?)\n\};", original, re.S)
                b = re.search(r"DroidMotionFrame k" + new + r"\[\] = \{(.*?)\n\};", integrated, re.S)
                self.assertEqual(re.findall(r"-?\d+", a.group(1)), re.findall(r"-?\d+", b.group(1)))

    def test_single_display_offset_owner(self):
        source = (BOARD / "custom_s3_voice.cc").read_text(encoding="utf-8")
        self.assertEqual(source.count("esp_lcd_panel_set_gap("), 1)
        self.assertRegex(source, r"new RobotFaceDisplay\([^;]*DISPLAY_HEIGHT,\s*0, 0,")
        self.assertIn('GetInt("rotation", 2)', source)

    def test_calibrated_audio_configuration(self):
        source = (BOARD / "droid_audio_codec.cc").read_text(encoding="utf-8")
        config = (BOARD / "config.h").read_text(encoding="utf-8")
        self.assertIn("#define AUDIO_INPUT_SAMPLE_RATE 16000", config)
        self.assertIn("#define AUDIO_OUTPUT_SAMPLE_RATE 16000", config)
        self.assertIn("#define AUDIO_MIC_GAIN_DEFAULT 9", config)
        self.assertRegex(source, r"rx = I2S_CHANNEL_DEFAULT_CONFIG\(XIAOZHI_I2S_PORT\(0\)")
        self.assertRegex(source, r"tx = I2S_CHANNEL_DEFAULT_CONFIG\(XIAOZHI_I2S_PORT\(1\)")
        self.assertIn("(input_buffer_[i * 2] >> 16) * mic_gain_", source)
        self.assertIn("output_buffer_[i * 2 + 1] = value", source)


if __name__ == "__main__":
    unittest.main()
