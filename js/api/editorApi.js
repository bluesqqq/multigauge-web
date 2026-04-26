import { createApiHelpers } from "../apiHelpers.js";

export function createEditorApi(Module) {
    const { callJson, asInt, asUInt, asString, asNumber } = createApiHelpers(Module);

    const badResponse = { ok: false, error: "BadResponse" };

    return {
        createView(canvasId, gaugePath = "", name = "") { return callJson("mg_editor_create_view", ["string", "string", "string"], [asString(canvasId), asString(gaugePath), asString(name)], badResponse); },
        listViews() { return callJson("mg_editor_list_views", [], [], []); },
        selectView(id) { return callJson("mg_editor_select_view", ["number"], [asUInt(id)], badResponse); },
        removeView(id) { return callJson("mg_editor_remove_view", ["number"], [asUInt(id)], badResponse); },
        setViewRenderSize(id, width, height) { return callJson("mg_editor_set_view_render_size", ["number", "number", "number"], [asUInt(id), asInt(width), asInt(height)], badResponse); },
        setPreviewFace(faceId) { return callJson("mg_editor_set_preview_face", ["number"], [asUInt(faceId)], badResponse); },
        reloadView() { return callJson("mg_editor_reload_view", [], [], badResponse); },

        listTree() { return callJson("mg_editor_list_tree", [], [], []); },
        listFaces() { return callJson("mg_editor_list_faces", [], [], []); },
        listRoots(faceId) { return callJson("mg_editor_list_roots", ["number"], [asUInt(faceId)], []); },
        listChildren(elementId) { return callJson("mg_editor_list_children", ["number"], [asUInt(elementId)], []); },
        describeNode(id) { return callJson("mg_editor_describe_node", ["number"], [asUInt(id)], {}); },
        listElements() { return callJson("mg_editor_list_elements", [], [], []); },
        listValues() { return callJson("mg_editor_list_values", [], [], []); },

        getProperties(id) { return callJson("mg_editor_get_properties", ["number"], [asUInt(id)], {}); },
        getPropertyNode(id, path) { return callJson("mg_editor_get_property_node", ["number", "string"], [asUInt(id), asString(path)], {}); },
        getProperty(id, path) { return callJson("mg_editor_get_property", ["number", "string"], [asUInt(id), asString(path)], {}); },
        setProperty(id, path, jsonValueText) { return callJson("mg_editor_set_property", ["number", "string", "string"], [asUInt(id), asString(path), asString(jsonValueText)], badResponse); },

        getHistoryState() { return callJson("mg_editor_get_history_state", [], [], {}); },
        undo() { return callJson("mg_editor_undo", [], [], badResponse); },
        redo() { return callJson("mg_editor_redo", [], [], badResponse); },

        createFace(jsonText = "{}", index = -1) { return callJson("mg_editor_create_face", ["string", "number"], [asString(jsonText, "{}"), asInt(index, -1)], badResponse); },
        removeFace(faceId) { return callJson("mg_editor_remove_face", ["number"], [asUInt(faceId)], badResponse); },
        reorderFace(faceId, index) { return callJson("mg_editor_reorder_face", ["number", "number"], [asUInt(faceId), asInt(index, -1)], badResponse); },

        addElement(parentId, type) { return callJson("mg_editor_add_element", ["number", "string"], [asUInt(parentId), asString(type)], badResponse); },
        insertElement(parentId, type, index = -1) { return callJson("mg_editor_insert_element", ["number", "string", "number"], [asUInt(parentId), asString(type), asInt(index, -1)], badResponse); },
        insertElementJson(parentId, jsonText, index = -1) { return callJson("mg_editor_insert_element_json", ["number", "string", "number"], [asUInt(parentId), asString(jsonText), asInt(index, -1)], badResponse); },
        replaceElementJson(id, jsonText) { return callJson("mg_editor_replace_element_json", ["number", "string"], [asUInt(id), asString(jsonText)], badResponse); },
        removeElement(id) { return callJson("mg_editor_remove_element", ["number"], [asUInt(id)], badResponse); },
        moveElement(id, destinationId, index = -1) { return callJson("mg_editor_move_element", ["number", "number", "number"], [asUInt(id), asUInt(destinationId), asInt(index, -1)], badResponse); },
        reorderElement(id, index) { return callJson("mg_editor_reorder_element", ["number", "number"], [asUInt(id), asInt(index, -1)], badResponse); },
        bringForward(id) { return callJson("mg_editor_bring_forward", ["number"], [asUInt(id)], badResponse); },
        bringToFront(id) { return callJson("mg_editor_bring_to_front", ["number"], [asUInt(id)], badResponse); },
        sendBackward(id) { return callJson("mg_editor_send_backward", ["number"], [asUInt(id)], badResponse); },
        sendToBack(id) { return callJson("mg_editor_send_to_back", ["number"], [asUInt(id)], badResponse); },

        getClipboard() { return callJson("mg_editor_get_clipboard", [], [], {}); },
        clearClipboard() { return callJson("mg_editor_clear_clipboard", [], [], badResponse); },
        copyFace(faceId) { return callJson("mg_editor_copy_face", ["number"], [asUInt(faceId)], badResponse); },
        cutFace(faceId) { return callJson("mg_editor_cut_face", ["number"], [asUInt(faceId)], badResponse); },
        pasteFace(index = -1) { return callJson("mg_editor_paste_face", ["number"], [asInt(index, -1)], badResponse); },
        copyElement(id) { return callJson("mg_editor_copy_element", ["number"], [asUInt(id)], badResponse); },
        cutRoot(id) { return callJson("mg_editor_cut_root", ["number"], [asUInt(id)], badResponse); },
        cutElement(id) { return callJson("mg_editor_cut_element", ["number"], [asUInt(id)], badResponse); },
        pasteRoot(faceId, index = -1) { return callJson("mg_editor_paste_root", ["number", "number"], [asUInt(faceId), asInt(index, -1)], badResponse); },
        pasteIntoElement(id) { return callJson("mg_editor_paste_into_element", ["number"], [asUInt(id)], badResponse); },
        pasteChild(id, index = -1) { return callJson("mg_editor_paste_child", ["number", "number"], [asUInt(id), asInt(index, -1)], badResponse); },
        pasteToReplaceElement(id) { return callJson("mg_editor_paste_to_replace_element", ["number"], [asUInt(id)], badResponse); },
        duplicateElement(id) { return callJson("mg_editor_duplicate_element", ["number"], [asUInt(id)], badResponse); },

        getElementJson(id) { return callJson("mg_editor_get_element_json", ["number"], [asUInt(id)], {}); },
        getFaceJson(faceId) { return callJson("mg_editor_get_face_json", ["number"], [asUInt(faceId)], {}); },
        getElementBounds(id) { return callJson("mg_editor_get_element_bounds", ["number"], [asUInt(id)], badResponse); },
        listElementBounds() { return callJson("mg_editor_list_element_bounds", [], [], []); },
        hitTest(x, y, index = 0) { return callJson("mg_editor_hit_test", ["number", "number", "number"], [asNumber(x), asNumber(y), asInt(index, 0)], 0); },
        hitTestAll(x, y) { return callJson("mg_editor_hit_test_all", ["number", "number"], [asNumber(x), asNumber(y)], []); },

        getDocumentAssets() { return callJson("mg_editor_get_document_assets", [], [], []); },
        setDocumentAssets(jsonValueText) { return callJson("mg_editor_set_document_assets", ["string"], [asString(jsonValueText)], badResponse); },
        setDocumentAssetsFromFile(jsonPath) { return callJson("mg_editor_set_document_assets_from_file", ["string"], [asString(jsonPath)], badResponse); },
        exportDocument() { return callJson("mg_editor_export_document", [], [], null); },
        exportDocumentText() { return callJson("mg_editor_export_document", [], [], null); },
        exportFace() { return callJson("mg_editor_export_face", [], [], null); },
    };
}
