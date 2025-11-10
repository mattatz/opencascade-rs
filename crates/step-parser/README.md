# step-parser

A high-performance STEP file parser for JavaScript/TypeScript, powered by OpenCascade and Rust/WebAssembly.

## Development

### Building from Source

```bash
# From repository root
./build-step-parser.sh
```

This will:
1. Build the Rust WASM module
2. Generate TypeScript type definitions
3. Copy outputs to `npm/` directory

The npm package files are located in `crates/step-parser/npm/`.

### Publishing to npm

```bash
cd crates/step-parser/npm
npm publish
```

### Native Rust Usage

```rust
use step_parser::{parse_step_from_bytes, step_bytes_to_json};

let step_bytes = std::fs::read("model.step")?;
let geometry = parse_step_from_bytes(&step_bytes)?;
let json = step_bytes_to_json(&step_bytes)?;
```

