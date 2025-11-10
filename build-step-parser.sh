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
cargo build --bin step_parser --target wasm32-unknown-emscripten --release

# Go back to root
cd ../..

# Ensure npm directory exists
mkdir -p crates/step-parser/npm

# Copy WASM and JS files to npm package directory
echo "Copying WASM and JS files to npm package..."
cp target/wasm32-unknown-emscripten/release/step_parser.wasm crates/step-parser/npm/step_parser.wasm
cp target/wasm32-unknown-emscripten/release/step_parser.js crates/step-parser/npm/step_parser.js

# Generate TypeScript definitions
echo "Generating TypeScript definitions..."
typeshare crates/interop --lang=typescript --output-file=crates/step-parser/npm/step.d.ts

echo ""
echo "✅ Build complete!"
echo ""
echo "Output files:"
echo "  - crates/step-parser/npm/step_parser.wasm"
echo "  - crates/step-parser/npm/step.d.ts"
echo "  - crates/step-parser/npm/index.js"
echo "  - crates/step-parser/npm/index.d.ts"
echo ""
echo "To publish to npm:"
echo "  cd crates/step-parser/npm"
echo "  npm publish"
