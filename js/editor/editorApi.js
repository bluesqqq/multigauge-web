import { createApiHelpers } from "../apiHelpers.js";

export function createEditorApi(Module) {
    const { callVoid, callJson, asInt, asUInt, asNumber, asString } = createApiHelpers(Module);

    return {
        attach() { callVoid("mg_editor_attach"); },

        // VIEWS
        createView(canvasId, gaugePath = "", name = "") { return callJson("mg_editor_create_view", ["string", "string", "string"], [asString(canvasId), asString(gaugePath), asString(name)], { ok: false, error: "BadResponse" }); },
        listViews() { return callJson("mg_editor_list_views", [], [], []); },
        selectView(id) { return callJson("mg_editor_select_view", ["number"], [asUInt(id)], { ok: false, error: "BadResponse" }); },
        removeView(id) { return callJson("mg_editor_remove_view", ["number"], [asUInt(id)], { ok: false, error: "BadResponse" }); },
        setViewRenderSize(id, width, height) { return callJson("mg_editor_set_view_render_size", ["number", "number", "number"], [asUInt(id), asInt(width), asInt(height)], { ok: false, error: "BadResponse" }); },
        reloadView() { return callJson("mg_editor_reload_view", [], [], { ok: false, error: "BadResponse" }); },

        // LISTS
        listTree() { return callJson("mg_editor_list_tree", [], [], []); },
        listElements() { return callJson("mg_editor_list_elements", [], [], []); },
        listValues() { return callJson("mg_editor_list_values", [], [], []); },

        // PROPERTIES
        getProperties(id) { return callJson("mg_editor_get_properties", ["number"], [asUInt(id)], []); },
        getPropertyNode(id, path) { return callJson("mg_editor_get_property_node", ["number", "string"], [asUInt(id), asString(path)], {}); },
        setProperty(id, path, jsonValueText) { return callJson("mg_editor_set_property", ["number", "string", "string"], [asUInt(id), asString(path), asString(jsonValueText)], { ok: false, error: "BadResponse" }); },

        // INSERTION + DELETION
        addElement(parentId, type) { return callJson("mg_editor_add_element", ["number", "string"], [asUInt(parentId), asString(type)], { ok: false, error: "BadResponse" }); },
        insertElement(parentId, type, index) { return callJson("mg_editor_insert_element", ["number", "string", "number"], [asUInt(parentId), asString(type), Number.isFinite(index) ? index : -1], { ok: false, error: "BadResponse" }); },
        removeElement(id) { return callJson("mg_editor_remove_element", ["number"], [asUInt(id)], { ok: false, error: "BadResponse" }); },
        getElementJson(id) { return callJson("mg_editor_get_element_json", ["number"], [asUInt(id)], {}); },
        insertElementJson(parentId, jsonText, index = -1) { return callJson("mg_editor_insert_element_json", ["number", "string", "number"], [asUInt(parentId), asString(jsonText), Number.isFinite(index) ? index : -1], { ok: false, error: "BadResponse" }); },
        replaceElementJson(id, jsonText) { return callJson("mg_editor_replace_element_json", ["number", "string"], [asUInt(id), asString(jsonText)], { ok: false, error: "BadResponse" }); },

        // ORDERING
        moveElement(id, newParentId, index) { return callJson("mg_editor_move_element", ["number", "number", "number"], [asUInt(id), asUInt(newParentId), Number.isFinite(index) ? index : -1], { ok: false, error: "BadResponse" }); },
        bringForward(id) { return callJson("mg_editor_bring_forward", ["number"], [asUInt(id)], { ok: false, error: "BadResponse" }); },
        bringToFront(id) { return callJson("mg_editor_bring_to_front", ["number"], [asUInt(id)], { ok: false, error: "BadResponse" }); },
        sendBackward(id) { return callJson("mg_editor_send_backward", ["number"], [asUInt(id)], { ok: false, error: "BadResponse" }); },
        sendToBack(id) { return callJson("mg_editor_send_to_back", ["number"], [asUInt(id)], { ok: false, error: "BadResponse" }); },
        reorderElement(id, index) { return callJson("mg_editor_reorder_element", ["number", "number"], [asUInt(id), Number.isFinite(index) ? index : -1], { ok: false, error: "BadResponse" }); },

        // COPYING + PASTING
        copyElement(id) { return callJson("mg_editor_copy_element", ["number"], [asUInt(id)], { ok: false, error: "BadResponse" }); },
        pasteIntoElement(id) { return callJson("mg_editor_paste_into_element", ["number"], [asUInt(id)], { ok: false, error: "BadResponse" }); },
        pasteToReplaceElement(id) { return callJson("mg_editor_paste_to_replace_element", ["number"], [asUInt(id)], { ok: false, error: "BadResponse" }); },
        duplicateElement(id) { return callJson("mg_editor_duplicate_element", ["number"], [asUInt(id)], { ok: false, error: "BadResponse" }); },

        // UNDO + REDO
        undo() { return callJson("mg_editor_undo", [], [], { ok: false, error: "BadResponse" }); },
        redo() { return callJson("mg_editor_redo", [], [], { ok: false, error: "BadResponse" }); },
        getHistoryState() { return callJson("mg_editor_get_history_state", [], [], {}); },

        // BOUNDS
        getElementBounds(id) { return callJson("mg_editor_get_element_bounds", ["number"], [asUInt(id)], { ok: false, error: "BadResponse" }); },
        listElementBounds() { return callJson("mg_editor_list_element_bounds", [], [], []); },
        hitTest(x, y, index = 0) { return callJson("mg_editor_hit_test", ["number", "number", "number"], [asNumber(x), asNumber(y), asInt(index, 0)], 0); },
        hitTestAll(x, y) { return callJson("mg_editor_hit_test_all", ["number", "number"], [asNumber(x), asNumber(y)], []); },

        // DOCUMENT
        getDocumentAssets() { return callJson("mg_editor_get_document_assets", [], [], []); },
        setDocumentAssets(jsonValueText) { return callJson("mg_editor_set_document_assets", ["string"], [asString(jsonValueText)], { ok: false, error: "BadResponse" }); },
        setDocumentAssetsFromFile(jsonPath) { return callJson("mg_editor_set_document_assets_from_file", ["string"], [asString(jsonPath)], { ok: false, error: "BadResponse" }); },
        exportDocument() { return callJson("mg_editor_export_document", [], [], null); },
        exportDocumentText() { return callJson("mg_editor_export_document", [], [], null); },
        exportFace() { return callJson("mg_editor_export_face", [], [], null); },
    };
}
