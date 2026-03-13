"""Pre-build script: add mbedtls to main component REQUIRES for ESP-IDF builds.

Mumble crypto (GCM, AES/OCB2) uses mbedtls directly. The main component's
idf_component_register has no REQUIRES, so mbedtls isn't linked. We patch
src/CMakeLists.txt to add REQUIRES mbedtls.
"""
Import("env")  # noqa: F821

import os


def patch_cmake():
    # Only for ESP-IDF
    if env.get("PIOFRAMEWORK", []) != ["espidf"]:
        return
    proj_dir = env.get("PROJECT_DIR")
    if not proj_dir:
        return
    cmake_path = os.path.join(proj_dir, "src", "CMakeLists.txt")
    if not os.path.exists(cmake_path):
        return
    with open(cmake_path, "r") as f:
        content = f.read()
    if "REQUIRES mbedtls" in content:
        return
    old = "idf_component_register(SRCS ${app_sources})"
    new = "idf_component_register(SRCS ${app_sources} REQUIRES mbedtls)"
    if old not in content:
        return
    content = content.replace(old, new)
    with open(cmake_path, "w") as f:
        f.write(content)


patch_cmake()
