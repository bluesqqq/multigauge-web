import { getRuntime } from "./runtime.js";
import { createPreviewApi } from "./api/previewApi.js";

let pageRendererPromise = null;

/**
 * Parses embedded gauge content into plain gauge text.
 * @param {string | unknown} rawValue The embedded gauge value.
 * @returns {string} The gauge document text.
 */
function parseEmbeddedGaugeText(rawValue) {
    if (typeof rawValue !== "string") {
        return JSON.stringify(rawValue);
    }

    try {
        const parsed = JSON.parse(rawValue);
        return typeof parsed === "string" ? parsed : JSON.stringify(parsed);
    } catch {
        return rawValue;
    }
}

/**
 * Ensures a directory tree exists in the wasm filesystem.
 * @param {any} Module The initialized Emscripten module.
 * @param {string} path The directory path.
 * @returns {void}
 */
function mkdirTree(Module, path) {
    const parts = String(path || "").split("/").filter(Boolean);
    let current = "";

    for (const part of parts) {
        current += "/" + part;
        try {
            Module.FS.mkdir(current);
        } catch {}
    }
}

/**
 * Writes gauge text into the wasm filesystem.
 * @param {any} Module The initialized Emscripten module.
 * @param {string} path The wasm file path.
 * @param {string} text The gauge text.
 * @returns {string} The staged wasm path.
 */
function stageGaugeText(Module, path, text) {
    if (!path) throw new Error("Missing wasmPath");
    if (typeof text !== "string") throw new Error("Missing gauge text");

    const dirPath = String(path).split("/").slice(0, -1).join("/") || "/";
    mkdirTree(Module, dirPath);

    try {
        Module.FS.unlink(path);
    } catch {}

    Module.FS.writeFile(path, text);
    return path;
}

/**
 * Creates the shared page renderer.
 * @returns {Promise<{
 *   runtime: any,
 *   previewApi: ReturnType<typeof createPreviewApi>,
 *   renderGaugeText: (options: { canvas: HTMLCanvasElement, wasmPath: string, gaugeText: string, name?: string }) => Promise<any>,
 *   renderGaugeJson: (options: { canvas: HTMLCanvasElement, wasmPath: string, gaugeJson: unknown, name?: string }) => Promise<any>,
 *   removeView: (id: number) => any,
 *   disposeAll: () => void
 * }>} The page renderer.
 */
async function createPageRenderer() {
    const runtime = await getRuntime();
    await runtime.ensureMainStarted();

    const previewApi = createPreviewApi(runtime.Module);
    const activeViewIds = new Set();

    const renderGaugeText = async ({ canvas, wasmPath, gaugeText, name }) => {
        if (!canvas) throw new Error("Missing canvas");
        if (typeof gaugeText !== "string" || !gaugeText.trim()) throw new Error("Missing gauge text");

        runtime.trackCanvas(canvas);
        stageGaugeText(runtime.Module, wasmPath, gaugeText);

        const result = previewApi.createView(canvas.id, wasmPath, name || "");
        if (!result || result.ok === false) {
            throw new Error(result?.error || "PreviewCreateFailed");
        }

        activeViewIds.add(result.id >>> 0);
        return result;
    };

    const renderGaugeJson = async ({ canvas, wasmPath, gaugeJson, name }) =>
        renderGaugeText({
            canvas,
            wasmPath,
            gaugeText: parseEmbeddedGaugeText(gaugeJson),
            name,
        });

    const removeView = (id) => {
        if (!id) return null;
        const numericId = id >>> 0;
        const result = previewApi.removeView(numericId);
        activeViewIds.delete(numericId);
        return result;
    };

    const disposeAll = () => {
        for (const id of [...activeViewIds]) {
            try {
                removeView(id);
            } catch (error) {
                console.warn("[pageRenderer][dispose]", error);
            }
        }
    };

    window.addEventListener("pagehide", (event) => {
        // When the page is entering the browser's back/forward cache, keep
        // the preview views alive so canvases still render after Back.
        if (event?.persisted) {
            return;
        }
        disposeAll();
    }, { once: true });

    return {
        runtime,
        previewApi,
        renderGaugeText,
        renderGaugeJson,
        removeView,
        disposeAll,
    };
}

/**
 * Returns the shared page renderer.
 * @returns {ReturnType<typeof createPageRenderer>} The shared renderer.
 */
export function getPageRenderer() {
    if (!pageRendererPromise) {
        pageRendererPromise = createPageRenderer();
    }

    return pageRendererPromise;
}
