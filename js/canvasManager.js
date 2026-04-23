/**
 * Matches a canvas backing store to its displayed size.
 * @param {HTMLCanvasElement | null | undefined} target The canvas to resize.
 * @returns {boolean} True if the backing size changed.
 */
function resizeCanvasToDisplaySize(target) {
    if (!target) return false;
    if (target.dataset?.mgManagedLogicalSize === "true") {
        return false;
    }

    const dpr = window.devicePixelRatio || 1;
    const rect = target.getBoundingClientRect();
    const cssWidth = Math.max(1, rect.width);
    const cssHeight = Math.max(1, rect.height);
    const pixelWidth = Math.max(1, Math.round(cssWidth * dpr));
    const pixelHeight = Math.max(1, Math.round(cssHeight * dpr));

    if (target.width !== pixelWidth || target.height !== pixelHeight) {
        target.width = pixelWidth;
        target.height = pixelHeight;
        console.log("[canvas] backing store:", pixelWidth, pixelHeight, "(css:", cssWidth, cssHeight, "dpr:", dpr, ")");
        return true;
    }

    return false;
}

/**
 * Creates a page-level canvas tracker.
 * @returns {{
 *   trackCanvas: (target: HTMLCanvasElement | null | undefined) => void,
 *   resizeTrackedCanvases: () => void
 * }} The canvas manager API.
 */
export function createCanvasManager() {
    const trackedCanvases = new Set();

    /**
     * Resizes every tracked canvas.
     * @returns {void}
     */
    const resizeTrackedCanvases = () => {
        for (const target of trackedCanvases) {
            resizeCanvasToDisplaySize(target);
        }
    };

    const resizeObserver =
        typeof ResizeObserver !== "undefined"
            ? new ResizeObserver((entries) => {
                for (const entry of entries) {
                    resizeCanvasToDisplaySize(entry.target);
                }
            })
            : null;

    /**
     * Registers a canvas for automatic resize tracking.
     * @param {HTMLCanvasElement | null | undefined} target The canvas to track.
     * @returns {void}
     */
    const trackCanvas = (target) => {
        if (!target || trackedCanvases.has(target)) return;
        trackedCanvases.add(target);
        resizeCanvasToDisplaySize(target);
        resizeObserver?.observe(target);
    };

    window.addEventListener("resize", resizeTrackedCanvases);

    return {
        trackCanvas,
        resizeTrackedCanvases,
    };
}
