"""Offline checks: never contact an ESP or run espota."""

import json
from pathlib import Path
import runpy
import subprocess
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]


class FakeEnvironment:
    def __init__(self, project, protocol="espota"):
        self.project = project
        self.protocol = protocol
        self.values = {}

    def GetProjectOption(self, name):
        return self.protocol

    def Replace(self, **values):
        self.values.update(values)

    def subst(self, value):
        return {
            "$PROJECT_DIR": str(self.project),
            "$PYTHONEXE": '"python.exe"',
            "$UPLOADER": "tool directory/espota.py",
        }[value]

    def subst_list(self, value):
        return [["--debug", "--progress", "-i", "192.0.2.1"]]


def load_script(env):
    return runpy.run_path(
        str(ROOT / "scripts/ota_upload.py"),
        init_globals={"env": env, "Import": lambda name: None},
    )


class OtaUploadTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.project = Path(self.directory.name)
        (self.project / "lib").mkdir()
        self.header = self.project / "lib/secrets.h"
        self.env = FakeEnvironment(self.project)
        self.script = load_script(self.env)

    def write_password(self, password):
        self.header.write_text(
            "namespace secrets {\nconstexpr char OTA_PASSWORD[] = "
            + json.dumps(password)
            + ";\n}\n",
            encoding="utf-8",
        )

    def test_upload_passes_special_characters_without_shell_and_hides_debug(self):
        password = 'test $value & space "quote" \\ backtick `'
        self.write_password(password)
        with patch("subprocess.call", return_value=0) as call:
            result = self.env.values["UPLOADCMD"](
                ["firmware directory/firmware.bin"], [], self.env
            )
        self.assertEqual(result, 0)
        command = call.call_args.args[0]
        self.assertIn("--auth=" + password, command)
        self.assertNotIn("--debug", command)
        self.assertEqual(command[-2:], ["-f", "firmware directory/firmware.bin"])
        self.assertFalse(call.call_args.kwargs.get("shell", False))

    def test_upload_propagates_failure(self):
        self.write_password("test-password")
        with patch("subprocess.call", return_value=1):
            self.assertEqual(self.script["upload_with_secrets"](["fw.bin"], [], self.env), 1)

    def test_scons_argument_wrappers_are_converted_before_subprocess(self):
        class CmdStringHolder:
            def __init__(self, value):
                self.value = value

            def __str__(self):
                return self.value

        self.write_password("test-password")
        flags = [CmdStringHolder(value) for value in self.env.subst_list("")[0]]
        with patch.object(self.env, "subst_list", return_value=[flags]):
            with patch("subprocess.call", return_value=0) as call:
                self.script["upload_with_secrets"](["fw.bin"], [], self.env)
        command = call.call_args.args[0]
        self.assertTrue(all(isinstance(arg, str) for arg in command))
        self.assertNotIn("--debug", command)
        self.assertIn("192.0.2.1", command)
        # Exercise the Windows conversion that failed in the reported traceback.
        subprocess.list2cmdline(command)

    def test_invalid_password_never_starts_upload(self):
        for password in ("", "line\nbreak", "zero\x00byte", "return\rbyte"):
            with self.subTest(password=repr(password)):
                self.write_password(password)
                with patch("subprocess.call") as call:
                    with self.assertRaises(ValueError):
                        self.script["upload_with_secrets"](["fw.bin"], [], self.env)
                    call.assert_not_called()

    def test_missing_declaration(self):
        self.header.write_text("#pragma once\n", encoding="utf-8")
        with self.assertRaises(ValueError):
            self.script["read_ota_password"](self.header)

    def test_serial_upload_unchanged(self):
        env = FakeEnvironment(self.project, "esptool")
        load_script(env)
        self.assertNotIn("UPLOADCMD", env.values)


if __name__ == "__main__":
    unittest.main()
