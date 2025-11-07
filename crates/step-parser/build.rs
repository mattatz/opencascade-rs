fn main() {
    let target = std::env::var("TARGET").unwrap_or_default();

    // Only build embind wrapper for emscripten target
    if target.contains("emscripten") {
        println!("cargo:rerun-if-changed=cpp/embind_wrapper.cpp");

        cc::Build::new()
            .cpp(true)
            .file("cpp/embind_wrapper.cpp")
            .flag("-std=c++11")
            .flag("-fexceptions")
            .flag("--bind")
            .compile("embind_wrapper");
    }
}
