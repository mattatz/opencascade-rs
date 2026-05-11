use std::{
    env::var,
    path::{Path, PathBuf},
};

const LIB_DIR: &str = "lib";
const INCLUDE_DIR: &str = "include";

/// Get the path to the OCCT library installation directory to be
/// used in build scripts.
///
/// Only valid during build (`cargo clean` removes these files).
pub fn occt_path() -> PathBuf {
    // moves the output into target/TARGET/OCCT
    // this way its less likely to be rebuilt without a cargo clean
    Path::new(&var("OUT_DIR").expect("missing OUT_DIR")).join("../../../../OCCT")
}

/// Build the OCCT library as static archives.
pub fn build_occt() {
    build_occt_with_type("Static");
}

/// Build the OCCT library as shared libraries (.dylib/.dll/.so).
///
/// Use this when distributing a dynamically-linked build that complies with
/// LGPL-2.1 by allowing the end user to substitute the OCCT shared libraries.
pub fn build_occt_shared() {
    build_occt_with_type("Shared");
}

fn build_occt_with_type(library_type: &str) {
    let mut cfg = cmake::Config::new(Path::new(env!("OCCT_SRC_DIR")));
    cfg.define("BUILD_PATCH", Path::new(env!("OCCT_PATCH_DIR")))
        .define("BUILD_LIBRARY_TYPE", library_type)
        .define("BUILD_MODULE_Draw", "FALSE")
        .define("USE_D3D", "FALSE")
        .define("USE_DRACO", "FALSE")
        .define("USE_EIGEN", "FALSE")
        .define("USE_FFMPEG", "FALSE")
        .define("USE_FREEIMAGE", "FALSE")
        .define("USE_FREETYPE", "FALSE")
        .define("USE_GLES2", "FALSE")
        .define("USE_OPENGL", "FALSE")
        .define("USE_OPENVR", "FALSE")
        .define("USE_RAPIDJSON", "FALSE")
        .define("USE_TBB", "FALSE")
        .define("USE_TCL", "FALSE")
        .define("USE_TK", "FALSE")
        .define("USE_VTK", "FALSE")
        .define("USE_XLIB", "FALSE")
        .define("INSTALL_DIR_LIB", LIB_DIR)
        .define("INSTALL_DIR_INCLUDE", INCLUDE_DIR);

    if library_type == "Shared" {
        let target_os = var("CARGO_CFG_TARGET_OS").unwrap_or_default();
        if target_os == "macos" {
            // Make every dylib reference its siblings via @rpath so that the
            // host binary can locate them after relocation into the app bundle.
            cfg.define("CMAKE_INSTALL_NAME_DIR", "@rpath");
            cfg.define("CMAKE_BUILD_WITH_INSTALL_NAME_DIR", "TRUE");
            cfg.define("CMAKE_MACOSX_RPATH", "TRUE");
        } else if target_os == "linux" {
            cfg.define("CMAKE_INSTALL_RPATH", "$ORIGIN");
            cfg.define("CMAKE_BUILD_WITH_INSTALL_RPATH", "TRUE");
        }
    }

    cfg.profile("Release").out_dir(occt_path()).build();
}
