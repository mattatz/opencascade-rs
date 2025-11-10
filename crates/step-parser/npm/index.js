/**
 * @typedef {import('./step-parser').StepInfo} StepInfo
 */

import createStepParserModule from './step-parser.js';

let Module = null;

/**
 * Initialize the WASM module
 * @param {Object} [options] - Initialization options
 * @returns {Promise<void>}
 */
export async function initStepParser(options = {}) {
    if (Module) {
        return; // Already initialized
    }

    const moduleOptions = {
        ...options,
        // Default locateFile to find the .wasm file
        locateFile: options.locateFile || ((path) => {
            if (path.endsWith('.wasm')) {
                // Use import.meta.url to resolve relative to this module
                return new URL('./step-parser.wasm', import.meta.url).href;
            }
            return path;
        })
    };

    Module = await createStepParserModule(moduleOptions);
}

/**
 * Parse STEP file from bytes
 * @param {Uint8Array} stepBytes - The STEP file bytes
 * @returns {string} JSON string with parsed geometry
 */
export function parseStep(stepBytes) {
    if (!Module) {
        throw new Error('WASM module not initialized. Call initStepParser() first.');
    }

    const malloc = Module._malloc;
    const free = Module._free;
    const parse_step = Module._parse_step;
    const free_step = Module._free_step;

    if (!malloc || !free || !parse_step || !free_step) {
        throw new Error('Required WASM exports not found');
    }

    // Allocate memory for the STEP file data
    const dataPtr = malloc(stepBytes.length);
    if (dataPtr === 0) {
        throw new Error('Failed to allocate memory');
    }

    // Copy data to WASM memory
    Module.HEAPU8.set(stepBytes, dataPtr);

    // Call the parse_step function
    const resultPtr = parse_step(dataPtr, stepBytes.length);

    // Free the input data
    free(dataPtr);

    if (resultPtr === 0) {
        throw new Error('Failed to parse STEP file');
    }

    // Read the result string using Emscripten's UTF8ToString
    const resultStr = Module.UTF8ToString(resultPtr);

    // Free the result string
    free_step(resultPtr);

    return resultStr;
}

/**
 * Parse STEP file and return parsed object
 * @param {Uint8Array} stepBytes - The STEP file bytes
 * @returns {StepInfo} Parsed geometry object
 */
export function parseStepToObject(stepBytes) {
    const jsonStr = parseStep(stepBytes);
    return JSON.parse(jsonStr);
}

/**
 * Check if the module is initialized
 * @returns {boolean}
 */
export function isInitialized() {
    return Module !== null;
}
