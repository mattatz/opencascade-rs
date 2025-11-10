# step-parser

A high-performance STEP file parser for JavaScript/TypeScript, powered by OpenCascade and Rust/WebAssembly.

## Features

- 🚀 Fast STEP file parsing using OpenCascade
- 📦 Compiled to WebAssembly for near-native performance
- 🔒 Type-safe API with TypeScript definitions
- 🌐 Works in browsers and Node.js
- 📐 Extracts complete B-Rep geometry (Solids, Faces, Wires, Edges)

## Installation

```bash
npm install step-parser
```

## Usage

### Browser Example

```typescript
import { initStepParser, parseStep } from 'step-parser';

// Initialize the WASM module (do this once)
await initStepParser();

// Load STEP file
const response = await fetch('model.step');
const stepBytes = new Uint8Array(await response.arrayBuffer());

// Parse STEP file
const geometry = parseStep(stepBytes);

console.log(`Parsed ${geometry.solids.length} solids`);
for (const solid of geometry.solids) {
  console.log(`  Solid has ${solid.faces.length} faces`);
}
```

### Node.js Example

```typescript
import { readFileSync } from 'fs';
import { initStepParser, parseStep } from 'step-parser';

// Initialize WASM module
await initStepParser();

// Load and parse STEP file
const stepBytes = readFileSync('model.step');
const geometry = parseStep(stepBytes);
```

## API

### `initStepParser(options?: Object): Promise<void>`

Initialize the WASM module. Must be called before parsing any STEP files.

Options:
- `locateFile?: (path: string) => string` - Custom function to locate WASM file (optional)

### `parseStep(stepBytes: Uint8Array): StepInfo`

Parse a STEP file and return the result as a typed object.

### `isInitialized(): boolean`

Check if the WASM module has been initialized.
