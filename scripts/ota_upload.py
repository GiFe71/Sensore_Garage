"""Pass the local OTA password directly to espota, without shell expansion."""

import json
from pathlib import Path
import re
import subprocess

Import("env")


def read_ota_password(path):
    text = Path(path).read_text(encoding="utf-8")
    # Keep this declaration as one double-quoted string in secrets.h.
    match = re.search(
        r'^\s*constexpr\s+char\s+OTA_PASSWORD\s*\[\s*\]\s*=\s*'
        r'("(?:[^"\\\r\n]|\\.)*")\s*;',
        text,
        re.MULTILINE,
    )
    if not match:
        raise ValueError("Missing constexpr char OTA_PASSWORD[] in lib/secrets.h")
    password = json.loads(match.group(1))
    if not password or any(char in password for char in "\x00\r\n"):
        raise ValueError("OTA_PASSWORD must be nonempty and contain no line breaks or NUL")
    return password


def upload_with_secrets(source, target, env):
    password = read_ota_password(Path(env.subst("$PROJECT_DIR")) / "lib/secrets.h")
    # SCons may return CmdStringHolder objects; subprocess requires strings.
    flags = [str(flag) for flag in env.subst_list("$UPLOADERFLAGS")[0]]
    # espota debug mode prints its parsed options, including the password.
    flags = [flag for flag in flags if flag not in ("--debug", "-d")]
    command = [
        env.subst("$PYTHONEXE").strip('"'),
        env.subst("$UPLOADER"),
        *flags,
        "--auth=" + password,
        "-f",
        str(source[0]),
    ]
    return subprocess.call(command)


if env.GetProjectOption("upload_protocol") == "espota":
    env.Replace(UPLOADCMD=upload_with_secrets)
