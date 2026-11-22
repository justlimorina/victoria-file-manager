#!/usr/bin/env python3
"""Bundle Victoria Files and its MSYS2 UCRT64 runtime dependencies for Windows."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
from pathlib import Path


def find_dll_deps(entry_files: list[Path], search_dirs: list[Path], objdump: str) -> list[Path]:
    """Recursively collect non-Windows DLL dependencies via objdump."""
    seen: set[str] = set()
    queue = [str(p) for p in entry_files]
    collected: list[Path] = []

    while queue:
        current = queue.pop(0)
        out = subprocess.check_output([objdump, "-p", current], text=True, errors="ignore")
        for line in out.splitlines():
            if "DLL Name:" not in line:
                continue
            dll = line.split("DLL Name:", 1)[1].strip()
            low = dll.lower()
            if low in seen or low.startswith(("api-ms-win", "ext-ms-win")):
                continue
            seen.add(low)

            for directory in search_dirs:
                candidate = directory / dll
                if candidate.is_file():
                    if not str(candidate).lower().startswith(r"c:\windows"):
                        collected.append(candidate)
                        queue.append(str(candidate))
                    break
    return collected


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", type=Path, default=Path("."))
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--output-dir", type=Path, default=Path("dist/Victoria"))
    parser.add_argument("--msys-prefix", type=Path, default=Path("C:/msys64/ucrt64"))
    args = parser.parse_args()

    src = args.source_dir.resolve()
    bld = args.build_dir.resolve()
    out = args.output_dir.resolve()
    msys = args.msys_prefix.resolve()

    if out.exists():
        shutil.rmtree(out)

    bin_dir = out / "bin"
    lib_dir = out / "lib"
    share_dir = out / "share"
    bin_dir.mkdir(parents=True, exist_ok=True)
    lib_dir.mkdir(parents=True, exist_ok=True)
    share_dir.mkdir(parents=True, exist_ok=True)

    exe = bld / "src" / "victoria-files.exe"
    ext_dll = bld / "libnautilus-extension" / "libnautilus-extension-4.dll"
    autoar_dll = bld / "subprojects" / "gnome-autoar" / "gnome-autoar" / "libgnome-autoar-0-0.dll"

    for f in (exe, ext_dll, autoar_dll):
        if not f.is_file():
            raise FileNotFoundError(f"Expected build artifact missing: {f}")
        shutil.copy2(f, bin_dir / f.name)

    search_dirs = [
        ext_dll.parent,
        autoar_dll.parent,
        msys / "bin",
        Path("C:/Windows/System32"),
    ]
    objdump = str(msys / "bin" / "objdump.exe")

    # Also include gdk-pixbuf loaders when scanning DLL deps
    entry_files = [exe, ext_dll, autoar_dll]
    pixbuf_src = msys / "lib" / "gdk-pixbuf-2.0"
    if pixbuf_src.is_dir():
        shutil.copytree(pixbuf_src, lib_dir / "gdk-pixbuf-2.0", dirs_exist_ok=True)
        entry_files.extend(pixbuf_src.rglob("*.dll"))

    for dll in find_dll_deps(entry_files, search_dirs, objdump):
        target = bin_dir / dll.name
        if not target.exists():
            shutil.copy2(dll, target)

    # Schemas: merge MSYS2 schemas + Victoria schemas and compile
    schemas_dir = share_dir / "glib-2.0" / "schemas"
    schemas_dir.mkdir(parents=True, exist_ok=True)
    msys_schemas = msys / "share" / "glib-2.0" / "schemas"
    if msys_schemas.is_dir():
        for xml in msys_schemas.glob("*.xml"):
            shutil.copy2(xml, schemas_dir / xml.name)
    for xml in (src / "data").glob("*.gschema.xml"):
        shutil.copy2(xml, schemas_dir / xml.name)
    for xml in (bld / "data").glob("*.gschema.xml"):
        shutil.copy2(xml, schemas_dir / xml.name)
    subprocess.check_call([str(msys / "bin" / "glib-compile-schemas.exe"), str(schemas_dir)])

    # Icons: Adwaita + hicolor + Victoria app icons
    icons_out = share_dir / "icons"
    for theme in ("Adwaita", "hicolor"):
        theme_src = msys / "share" / "icons" / theme
        if theme_src.is_dir():
            shutil.copytree(theme_src, icons_out / theme, dirs_exist_ok=True)

    apps_icons = icons_out / "hicolor" / "scalable" / "apps"
    apps_icons.mkdir(parents=True, exist_ok=True)
    for svg in (src / "data" / "icons" / "hicolor" / "scalable" / "apps").glob("org.victoria*.svg"):
        shutil.copy2(svg, apps_icons / svg.name)

    updater = msys / "bin" / "gtk4-update-icon-cache.exe"
    if updater.is_file():
        for theme_dir in icons_out.iterdir():
            if (theme_dir / "index.theme").is_file():
                subprocess.call([str(updater), "-f", "-t", str(theme_dir)])

    # Portable launcher at root
    (out / "Victoria Files.bat").write_text(
        '@echo off\r\nstart "" "%~dp0bin\\victoria-files.exe" %*\r\n',
        encoding="utf-8",
    )

    print(f"Bundled Victoria Files successfully into {out}")


if __name__ == "__main__":
    main()
