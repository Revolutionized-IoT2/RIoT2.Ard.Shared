"""
PlatformIO pre-build script: generates this build's manifest in two forms -

1. $BUILD_DIR/manifest.json - an external artifact (e.g. for an OTA host to
   serve alongside firmware.bin) describing the build.
2. include/Manifest.h + src/Manifest.cpp - the same JSON embedded as a C
   string constant, compiled into the firmware itself so the device can
   report its own build info at runtime (see MqttConnection::publishOnline(),
   which embeds Manifest::json into the retained online announcement).

Both are auto-generated - do not edit include/Manifest.h or src/Manifest.cpp
by hand, they're overwritten on every build.

Shared by every RIoT2 Ard node project via
`extra_scripts = pre:../RIoT2.Ard.Shared/scripts/generate_manifest.py` in each
project's platformio.ini. The build name comes from a MANIFEST_NAME file at
the consuming project's root (one line, e.g. "RIoT2 M5Dial Node") so this one
script produces the right manifest for whichever project invokes it. The
version string comes from the VERSION file at the project root - bump that
file to release a new version, no code changes needed.
"""

import datetime
import json
import os

Import("env")  # noqa: F821  (SCons injects this at exec time)


def read_project_file(env, filename, default):
    project_dir = env.subst("$PROJECT_DIR")
    path = os.path.join(project_dir, filename)
    try:
        with open(path, "r", encoding="utf-8") as f:
            return f.read().strip()
    except OSError:
        return default


def build_manifest_data(env):
    name = read_project_file(env, "MANIFEST_NAME", "RIoT2 Node")
    version = read_project_file(env, "VERSION", "0.0.0")

    return {
        "name": name,
        "version": version,
        "date": datetime.datetime.now().replace(microsecond=0).isoformat(),
        "installedPackageFilename": "firmware.bin",
    }


def write_manifest_json(env, manifest):
    build_dir = env.subst("$BUILD_DIR")
    os.makedirs(build_dir, exist_ok=True)

    manifest_path = os.path.join(build_dir, "manifest.json")
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")

    print("[manifest] wrote {}".format(manifest_path))


def write_manifest_source(env, manifest):
    project_dir = env.subst("$PROJECT_DIR")

    manifest_json = json.dumps(manifest, separators=(",", ":"))
    escaped_json = manifest_json.replace("\\", "\\\\").replace('"', '\\"')

    header_path = os.path.join(project_dir, "include", "Manifest.h")
    os.makedirs(os.path.dirname(header_path), exist_ok=True)
    with open(header_path, "w", encoding="utf-8") as f:
        f.write(
            "#pragma once\n\n"
            "// Auto-generated at build time by generate_manifest.py - do not edit\n"
            "// by hand. Mirrors the manifest.json build artifact ($BUILD_DIR/manifest.json)\n"
            "// so the firmware can report its own build info at runtime, e.g. embedded into\n"
            "// the retained MQTT online announcement (see MqttConnection::publishOnline()).\n"
            "namespace Manifest {\n\n"
            "extern const char* const json;\n\n"
            "}  // namespace Manifest\n"
        )

    source_path = os.path.join(project_dir, "src", "Manifest.cpp")
    os.makedirs(os.path.dirname(source_path), exist_ok=True)
    with open(source_path, "w", encoding="utf-8") as f:
        f.write(
            "#include \"Manifest.h\"\n\n"
            "// Auto-generated at build time by generate_manifest.py - do not edit by hand.\n"
            "namespace Manifest {\n\n"
            "const char* const json = \"" + escaped_json + "\";\n\n"
            "}  // namespace Manifest\n"
        )

    print("[manifest] wrote {} and {}".format(header_path, source_path))


_manifest = build_manifest_data(env)  # noqa: F821
write_manifest_json(env, _manifest)  # noqa: F821
write_manifest_source(env, _manifest)  # noqa: F821
