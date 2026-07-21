"""Pre-build script: ensure main IDF component REQUIRES mbedtls.

Mumble crypto (GCM, AES/OCB2) uses mbedtls directly. PlatformIO's default
src/CMakeLists.txt has no REQUIRES, so mbedtls is not linked. On clean builds
PlatformIO creates that file after pre-scripts would otherwise no-op — so we
create a CMakeLists with REQUIRES when missing, and patch when present.
"""

Import("env")  # noqa: F821

import os

CMAKE_CONTENTS = """\
# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.
# Patched by esp32-mumble scripts/patch_mbedtls_requires.py for mbedtls.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS ${app_sources} REQUIRES mbedtls)
"""


def _is_espidf() -> bool:
    fw = env.get("PIOFRAMEWORK", [])  # noqa: F821
    if isinstance(fw, list):
        return "espidf" in fw
    return fw == "espidf"


def patch_cmake(*_args, **_kwargs) -> None:
    if not _is_espidf():
        return
    proj_dir = env.get("PROJECT_DIR")  # noqa: F821
    if not proj_dir:
        return
    src_dir = os.path.join(proj_dir, "src")
    cmake_path = os.path.join(src_dir, "CMakeLists.txt")
    if not os.path.exists(cmake_path):
        os.makedirs(src_dir, exist_ok=True)
        with open(cmake_path, "w") as f:
            f.write(CMAKE_CONTENTS)
        return
    with open(cmake_path, "r") as f:
        content = f.read()
    if "REQUIRES mbedtls" in content:
        return
    old = "idf_component_register(SRCS ${app_sources})"
    new = "idf_component_register(SRCS ${app_sources} REQUIRES mbedtls)"
    if old not in content:
        return
    with open(cmake_path, "w") as f:
        f.write(content.replace(old, new))


patch_cmake()
