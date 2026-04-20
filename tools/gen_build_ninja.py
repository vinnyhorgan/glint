#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path


PROJECT = "glint"
BUILD_DIR = "build"
GLFW_DIR = "vendor/glfw"
GLAD_DIR = "vendor/glad"

APP_SRCS = [
    "src/main.c",
]

GLFW_SRCS = [
    f"{GLFW_DIR}/src/context.c",
    f"{GLFW_DIR}/src/init.c",
    f"{GLFW_DIR}/src/input.c",
    f"{GLFW_DIR}/src/monitor.c",
    f"{GLFW_DIR}/src/platform.c",
    f"{GLFW_DIR}/src/vulkan.c",
    f"{GLFW_DIR}/src/window.c",
    f"{GLFW_DIR}/src/egl_context.c",
    f"{GLFW_DIR}/src/glx_context.c",
    f"{GLFW_DIR}/src/osmesa_context.c",
    f"{GLFW_DIR}/src/null_init.c",
    f"{GLFW_DIR}/src/null_joystick.c",
    f"{GLFW_DIR}/src/null_monitor.c",
    f"{GLFW_DIR}/src/null_window.c",
    f"{GLFW_DIR}/src/linux_joystick.c",
    f"{GLFW_DIR}/src/posix_module.c",
    f"{GLFW_DIR}/src/posix_poll.c",
    f"{GLFW_DIR}/src/posix_thread.c",
    f"{GLFW_DIR}/src/posix_time.c",
    f"{GLFW_DIR}/src/x11_init.c",
    f"{GLFW_DIR}/src/x11_monitor.c",
    f"{GLFW_DIR}/src/x11_window.c",
    f"{GLFW_DIR}/src/xkb_unicode.c",
]


def obj_path(config: str, src: str) -> str:
    return f"{BUILD_DIR}/{config}/obj/{src[:-2]}.o"


def archive_path(config: str) -> str:
    return f"{BUILD_DIR}/{config}/libglfw_x11.a"


def target_path(config: str) -> str:
    suffix = "" if config == "release" else "-debug"
    return f"{BUILD_DIR}/{PROJECT}{suffix}"


def escape(value: str) -> str:
    return value.replace("$", "$$").replace(" ", "$ ")


def join_paths(paths: list[str]) -> str:
    return " ".join(escape(path) for path in paths)


def build_ninja_text() -> str:
    common_cppflags = " ".join(
        [
            f"-I{GLFW_DIR}/include",
            f"-I{GLFW_DIR}/src",
            f"-I{GLAD_DIR}/include",
            "-D_GLFW_X11",
            "-D_DEFAULT_SOURCE",
        ]
    )
    common_cflags = "-std=c11 -ffunction-sections -fdata-sections"
    app_warnings = "-Wall -Wextra -Wpedantic -Werror"
    vendor_warnings = "-w"
    release_cflags = "-Os -DNDEBUG"
    debug_cflags = "-O0 -g3"
    release_ldflags = "-Wl,--gc-sections -Wl,-s"
    debug_ldflags = ""
    ldlibs = "-lm"

    lines = [
        "ninja_required_version = 1.7",
        "cc = cc",
        "ar = ar",
        f"common_cppflags = {common_cppflags}",
        f"common_cflags = {common_cflags}",
        f"app_warnings = {app_warnings}",
        f"vendor_warnings = {vendor_warnings}",
        f"release_cflags = {release_cflags}",
        f"debug_cflags = {debug_cflags}",
        f"release_ldflags = {release_ldflags}",
        f"debug_ldflags = {debug_ldflags}",
        f"ldlibs = {ldlibs}",
        "",
        "rule cc_app",
        "  command = mkdir -p $$(dirname \"$out\") && $cc $common_cppflags $common_cflags $mode_cflags $app_warnings -MMD -MF $out.d -c $in -o $out",
        "  depfile = $out.d",
        "  deps = gcc",
        "  description = CC $out",
        "",
        "rule cc_vendor",
        "  command = mkdir -p $$(dirname \"$out\") && $cc $common_cppflags $common_cflags $mode_cflags $vendor_warnings -MMD -MF $out.d -c $in -o $out",
        "  depfile = $out.d",
        "  deps = gcc",
        "  description = CC $out",
        "",
        "rule ar",
        "  command = mkdir -p $$(dirname \"$out\") && rm -f $out && $ar rcs $out $in",
        "  description = AR $out",
        "",
        "rule link",
        "  command = mkdir -p $$(dirname \"$out\") && $cc $ldflags $in -o $out $ldlibs",
        "  description = LINK $out",
        "",
        "rule clean",
        "  command = rm -rf build",
        "  description = CLEAN build outputs",
        "",
    ]

    for config, mode_cflags, ldflags in [
        ("release", "$release_cflags", "$release_ldflags"),
        ("debug", "$debug_cflags", "$debug_ldflags"),
    ]:
        lines.append(f"# {config}")
        app_objs = [obj_path(config, src) for src in APP_SRCS]
        glfw_objs = [obj_path(config, src) for src in GLFW_SRCS]
        glfw_lib = archive_path(config)
        target = target_path(config)

        for src, obj in zip(APP_SRCS, app_objs):
            lines.append(f"build {escape(obj)}: cc_app {escape(src)}")
            lines.append(f"  mode_cflags = {mode_cflags}")

        for src, obj in zip(GLFW_SRCS, glfw_objs):
            lines.append(f"build {escape(obj)}: cc_vendor {escape(src)}")
            lines.append(f"  mode_cflags = {mode_cflags}")

        lines.append(f"build {escape(glfw_lib)}: ar {join_paths(glfw_objs)}")
        lines.append(f"build {escape(target)}: link {join_paths(app_objs + [glfw_lib])}")
        lines.append(f"  ldflags = {ldflags}")
        lines.append("")

    lines.extend(
        [
            f"build {BUILD_DIR}/release: phony {escape(target_path('release'))}",
            f"build {BUILD_DIR}/debug: phony {escape(target_path('debug'))}",
            f"build release: phony {escape(target_path('release'))}",
            f"build debug: phony {escape(target_path('debug'))}",
            f"build all: phony {escape(target_path('release'))} {escape(target_path('debug'))}",
            "build clean: clean",
            f"default {escape(target_path('release'))}",
            "",
        ]
    )

    return "\n".join(lines) + "\n"


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    output = root / "build.ninja"
    output.write_text(build_ninja_text(), encoding="ascii")


if __name__ == "__main__":
    main()
