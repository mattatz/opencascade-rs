// Re-export OpenCascade types
export * from './opencascade';
// Re-export step-parser types
export * from './step_parser';

/**
 * Initialize the WASM module
 * @param wasmSource - Path/URL to the wasm file or raw bytes
 */
export function initStepParser(options: {
  locateFile: (path: string) => string;
}): Promise<void>;

/**
 * Parse STEP file from bytes
 * @param stepBytes - The STEP file bytes
 * @returns JSON string with parsed geometry
 */
export function parseStep(stepBytes: Uint8Array): StepInfo;

/**
 * Check if the module is initialized
 */
export function isInitialized(): boolean;
