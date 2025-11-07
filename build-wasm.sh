#!/bin/bash
set -e

echo "Building step-parser to WASM with emscripten..."

# Check if emscripten is installed
if ! command -v emcc &> /dev/null; then
    echo "Error: Emscripten is not installed or not in PATH"
    echo "Please install Emscripten from https://emscripten.org/docs/getting_started/downloads.html"
    exit 1
fi

# Add wasm32-unknown-emscripten target if not already added
rustup target add wasm32-unknown-emscripten

# Build the step-parser crate
cd crates/step-parser
echo "Building step-parser for wasm32-unknown-emscripten target..."
cargo build --target wasm32-unknown-emscripten --release

echo ""
echo "Build complete! WASM artifacts are located at:"
echo "  - target/wasm32-unknown-emscripten/release/step_parser.wasm"
echo "  - target/wasm32-unknown-emscripten/release/step_parser.js"
