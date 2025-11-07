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

### WebAssembly

```javascript
import init, { parse_step_wasm, init_panic_hook } from './step_parser.js';

async function parseSTEP(fileBytes) {
  await init();
  init_panic_hook();

  const json = parse_step_wasm(fileBytes);
  return JSON.parse(json);
}
```

## Building

### Native Build

```bash
cargo build --release
```

### WASM Build with Emscripten

#### Using the build script (from project root):

```bash
./build-wasm.sh
```

#### Manual build:

```bash
# Install emscripten target
rustup target add wasm32-unknown-emscripten

# Build
cd crates/step-parser
cargo build --target wasm32-unknown-emscripten --release
```

The WASM artifacts will be located at:
- `target/wasm32-unknown-emscripten/release/step_parser.wasm`
- `target/wasm32-unknown-emscripten/release/step_parser.js`

## Output Format

The parser extracts geometry into the following JSON structure:

```json
{
  "edges": [
    {
      "curve_details": {
        "Line": {
          "origin": { "x": 0.0, "y": 0.0, "z": 0.0 },
          "direction": { "x": 1.0, "y": 0.0, "z": 0.0 }
        }
      },
      "orientation": "Forward"
    }
  ],
  "faces": [
    {
      "surface_details": {
        "Plane": {
          "origin": { "x": 0.0, "y": 0.0, "z": 0.0 },
          "normal": { "x": 0.0, "y": 0.0, "z": 1.0 }
        }
      },
      "surface_type": "Plane"
    }
  ]
}
```

## Dependencies

- `opencascade` - OpenCASCADE wrapper for Rust
- `serde` - Serialization framework
- `serde_json` - JSON serialization
- `glam` - Mathematics library for 3D vectors
- `wasm-bindgen` - WebAssembly bindings (WASM target only)
- `console_error_panic_hook` - Better error messages in browser console (WASM target only)
