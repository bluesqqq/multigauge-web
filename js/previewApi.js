import { createApiHelpers } from "./apiHelpers.js";

export function createPreviewApi(Module) {
    const { callJson, asUInt, asString } = createApiHelpers(Module);

    return {
        createView(canvasId, gaugePath = "", name = "") { return callJson("mg_preview_create_view", ["string", "string", "string"], [asString(canvasId), asString(gaugePath), asString(name)], { ok: false, error: "BadResponse" }); },
        removeView(id) { return callJson("mg_preview_remove_view", ["number"], [asUInt(id)], { ok: false, error: "BadResponse" }); },
    };
}
