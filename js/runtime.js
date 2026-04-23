import { createCanvasManager } from "./canvasManager.js";

const DEFAULT_WASM_SCRIPT_URL = new URL("../wasm/multigauge.js", import.meta.url).href;
const DEFAULT_WASM_ASSET_PREFIX = new URL("../wasm/", import.meta.url).href;

let runtimePromise = null;

/**
 * Loads the generated Emscripten script once.
 * @param {string} src The script URL.
 * @returns {Promise<void>} Resolves when the script loads.
 */
function loadScriptOnce(src) {
    return new Promise((resolve, reject) => {
        const existing = document.querySelector(`script[data-multigauge-wasm="true"][src="${src}"]`);
        if (existing) {
            if (existing.dataset.loaded === "true") {
                resolve();
                return;
            }

            existing.addEventListener("load", () => resolve(), { once: true });
            existing.addEventListener("error", () => reject(new Error("Failed to load " + src)), { once: true });
            return;
        }

        const script = document.createElement("script");
        script.src = src;
        script.async = true;
        script.dataset.multigaugeWasm = "true";
        script.onload = () => {
            script.dataset.loaded = "true";
            resolve();
        };
        script.onerror = () => reject(new Error("Failed to load " + src));
        document.head.appendChild(script);
    });
}

/**
 * Builds the minimal helper API exposed by the runtime.
 * @param {any} Module The initialized Emscripten module.
 * @returns {{
 *   Module: any,
 *   trackCanvas: (target: HTMLCanvasElement | null | undefined) => void,
 *   ensureMainStarted: (args?: string[]) => Promise<void>
 * }} The runtime API.
 */
function createRuntimeTools(Module) {
    const canvasManager = createCanvasManager();

    let mainStarted = false;

    /**
     * Starts the wasm main loop once for the current page.
     * @param {string[]} [args=[]] Optional main arguments.
     * @returns {Promise<void>} Resolves after startup.
     */
    const ensureMainStarted = async (args = []) => {
        if (mainStarted) return;
        canvasManager.resizeTrackedCanvases();
        Module.callMain(Array.isArray(args) ? args : []);
        mainStarted = true;
    };

    return {
        Module,
        trackCanvas: canvasManager.trackCanvas,
        ensureMainStarted,
    };
}

/**
 * Creates the single page-level wasm runtime.
 * @param {{
 *   canvas?: HTMLCanvasElement | null,
 *   wasmScriptUrl?: string,
 *   wasmAssetPrefix?: string
 * }} [options={}] Runtime options.
 * @returns {Promise<{
 *   Module: any,
 *   trackCanvas: (target: HTMLCanvasElement | null | undefined) => void,
 *   ensureMainStarted: (args?: string[]) => Promise<void>
 * }>} The initialized runtime.
 */
async function createRuntime({canvas, wasmScriptUrl = DEFAULT_WASM_SCRIPT_URL, wasmAssetPrefix = DEFAULT_WASM_ASSET_PREFIX} = {}) {
    const Module = {
        locateFile: (path) => new URL(path, wasmAssetPrefix).href,
        noInitialRun: true,
    };

    Module.print = (...args) => console.log(...args);
    Module.printErr = (...args) => console.error(...args);

    const runtimeReady = new Promise((resolve) => { Module.onRuntimeInitialized = resolve; });

    window.Module = Module;

    await loadScriptOnce(wasmScriptUrl);
    await runtimeReady;

    const runtime = createRuntimeTools(Module);
    if (canvas) runtime.trackCanvas(canvas);
    return runtime;
}

/**
 * Returns the shared page runtime.
 * @param {{
 *   canvas?: HTMLCanvasElement | null,
 *   wasmScriptUrl?: string,
 *   wasmAssetPrefix?: string
 * }} [options={}] Runtime options.
 * @returns {Promise<{
 *   Module: any,
 *   trackCanvas: (target: HTMLCanvasElement | null | undefined) => void,
 *   ensureMainStarted: (args?: string[]) => Promise<void>
 * }>} The shared runtime.
 */
export async function getRuntime(options = {}) {
    if (!runtimePromise) runtimePromise = createRuntime(options);

    const runtime = await runtimePromise;
    if (options.canvas) runtime.trackCanvas(options.canvas);
    return runtime;
}
