export function createApiHelpers(Module) {
    function callVoid(name, argTypes = [], args = []) {
        Module.ccall(name, "void", argTypes, args);
    }

    function callJson(name, argTypes = [], args = [], fallback = null) {
        const result = Module.ccall(name, "string", argTypes, args);

        if (typeof result !== "string" || result.length === 0) {
            return fallback;
        }

        try {
            return JSON.parse(result);
        } catch {
            return fallback;
        }
    }

    function asInt(value, fallback = 0) {
        return Number.isFinite(value) ? (value | 0) : fallback;
    }

    function asUInt(value, fallback = 0) {
        return Number.isFinite(value) ? (value >>> 0) : fallback;
    }

    function asNumber(value, fallback = 0) {
        return Number.isFinite(value) ? value : fallback;
    }

    function asString(value, fallback = "") {
        return typeof value === "string" ? value : fallback;
    }

    return { callVoid, callJson, asInt, asUInt, asNumber, asString };
}
