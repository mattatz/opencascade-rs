# step-parser

A Rust crate for parsing STEP files and extracting geometry information in JSON format. Supports both native Rust and WebAssembly (WASM) targets.

## Features

- Parse STEP files from byte arrays
- Extract edge curve details (lines, circles, B-splines, etc.)
- Extract face surface details (planes, cylinders, spheres, etc.)
- Serialize geometry to JSON
- WebAssembly support via emscripten

## Usage

### Native Rust

```rust
use step_parser::{parse_step_from_bytes, step_bytes_to_json};

// Parse STEP file from bytes
let step_bytes = std::fs::read("model.step")?;
let geometry = parse_step_from_bytes(&step_bytes)?;

// Convert to JSON
let json = step_bytes_to_json(&step_bytes)?;
println!("{}", json);

// Or use pretty JSON
let pretty_json = step_bytes_to_json_pretty(&step_bytes)?;
```

#### Manual build:

```bash
# Install emscripten target
rustup target add wasm32-unknown-emscripten

# Build
cd crates/step-parser
cargo build --bin step-parser --target wasm32-unknown-emscripten --release
```

The WASM artifacts will be located at:
- `target/wasm32-unknown-emscripten/release/step_parser.wasm`
- `target/wasm32-unknown-emscripten/release/step_parser.js`

