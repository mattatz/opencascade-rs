# @opencascade-rs/step-parser

A high-performance STEP file parser for JavaScript/TypeScript, powered by OpenCascade and Rust/WebAssembly.

## Features

- 🚀 Fast STEP file parsing using OpenCascade
- 📦 Compiled to WebAssembly for near-native performance
- 🔒 Type-safe API with TypeScript definitions
- 🌐 Works in browsers and Node.js
- 📐 Extracts complete B-Rep geometry (Solids, Faces, Wires, Edges)

## Installation

```bash
npm install @opencascade-rs/step-parser
```

## Usage

### Browser Example

```typescript
import { initStepParser, parseStepToObject } from '@opencascade-rs/step-parser';
import wasmUrl from '@opencascade-rs/step-parser/step-parser.wasm?url';

// Initialize the WASM module (do this once)
await initStepParser(wasmUrl);

// Load STEP file
const response = await fetch('model.step');
const stepBytes = new Uint8Array(await response.arrayBuffer());

// Parse STEP file
const geometry = parseStepToObject(stepBytes);

console.log(`Parsed ${geometry.solids.length} solids`);
for (const solid of geometry.solids) {
  console.log(`  Solid has ${solid.faces.length} faces`);
}
```

### Node.js Example

```typescript
import { readFileSync } from 'fs';
import { initStepParser, parseStepToObject } from '@opencascade-rs/step-parser';

// Load WASM module from file
const wasmBytes = readFileSync('./node_modules/@opencascade-rs/step-parser/step-parser.wasm');
await initStepParser(wasmBytes);

// Load and parse STEP file
const stepBytes = readFileSync('model.step');
const geometry = parseStepToObject(stepBytes);
```

## API

### `initStepParser(wasmSource: string | URL | Uint8Array): Promise<void>`

Initialize the WASM module. Must be called before parsing any STEP files.

### `parseStepToObject(stepBytes: Uint8Array): StepInfo`

Parse a STEP file and return the result as a typed object.

### `isInitialized(): boolean`

Check if the WASM module has been initialized.

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

