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
echo "Building step-parser for wasm32-unknown-emscripten target..."
cd crates/step-parser
cargo build --target wasm32-unknown-emscripten --release
cd ../..

OUTPUT_DIR="target/wasm32-unknown-emscripten/release"

echo ""
echo "Build complete! WASM binary is located at:"
echo "  - $OUTPUT_DIR/step_parser.wasm"
echo ""
echo "To use the WASM module in a browser:"
echo "  1. See examples/step_parser_usage.md for detailed instructions"
echo "  2. Open examples/wasm_test.html in a web browser with a local server"
echo "  3. Example: python3 -m http.server 8000"
