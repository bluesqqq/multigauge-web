#include "ApiBindings.h"

#include <multigauge/editor/Api.h>

#include <cstdint>
#include <limits>

#include <emscripten.h>

namespace {

const char* toJson(const mg::editor::Result& result) {
    return mg::bindings::storeString(result.toJson());
}

}

extern "C" {

EMSCRIPTEN_KEEPALIVE
mg::editor::EditorId mg_editor_create() {
    try {
        return mg::editor::create();
    } catch (...) {
        return std::numeric_limits<mg::editor::EditorId>::max();
    }
}

EMSCRIPTEN_KEEPALIVE
bool mg_editor_destroy(mg::editor::EditorId editorId) {
    try {
        return mg::editor::destroy(editorId);
    } catch (...) {
        return false;
    }
}

EMSCRIPTEN_KEEPALIVE
bool mg_editor_exists(mg::editor::EditorId editorId) {
    try {
        return mg::editor::exists(editorId);
    } catch (...) {
        return false;
    }
}

EMSCRIPTEN_KEEPALIVE
bool mg_editor_clear(mg::editor::EditorId editorId) {
    try {
        return mg::editor::clear(editorId);
    } catch (...) {
        return false;
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_load_document(mg::editor::EditorId editorId, const char* json) {
    try {
        return toJson(mg::editor::loadDocument(editorId, json ? std::string(json) : std::string()));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_save_document(mg::editor::EditorId editorId) {
    try {
        return toJson(mg::editor::saveDocument(editorId));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_hierarchy(mg::editor::EditorId editorId) {
    try {
        return toJson(mg::editor::getHierarchy(editorId));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_history(mg::editor::EditorId editorId) {
    try {
        return toJson(mg::editor::getHistory(editorId));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_clipboard_summary(mg::editor::EditorId editorId) {
    try {
        if (!mg::editor::exists(editorId)) {
            return mg::bindings::storeString(R"({"kind":"empty","hasValue":false})");
        }

        return mg::bindings::storeString(mg::bindings::toJson(mg::editor::getClipboardSummary(editorId)));
    } catch (...) {
        return mg::bindings::storeString(R"({"kind":"empty","hasValue":false})");
    }
}

EMSCRIPTEN_KEEPALIVE
bool mg_editor_clear_clipboard(mg::editor::EditorId editorId) {
    try {
        if (!mg::editor::exists(editorId)) return false;
        mg::editor::clearClipboard(editorId);
        return true;
    } catch (...) {
        return false;
    }
}

EMSCRIPTEN_KEEPALIVE
std::size_t mg_editor_face_count(mg::editor::EditorId editorId) {
    try {
        return mg::editor::faceCount(editorId);
    } catch (...) {
        return 0;
    }
}

EMSCRIPTEN_KEEPALIVE
mg::editor::Editor::Id mg_editor_face_at(mg::editor::EditorId editorId, std::size_t index) {
    try {
        return mg::editor::faceAt(editorId, index);
    } catch (...) {
        return 0;
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_list_element_types(mg::editor::EditorId editorId) {
    try {
        return toJson(mg::editor::listElementTypes(editorId));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_list_value_ids(mg::editor::EditorId editorId) {
    try {
        return toJson(mg::editor::listValueIDs(editorId));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
bool mg_editor_has_node(mg::editor::EditorId editorId, mg::editor::Editor::Id id) {
    try {
        return mg::editor::hasNode(editorId, id);
    } catch (...) {
        return false;
    }
}

EMSCRIPTEN_KEEPALIVE
bool mg_editor_is_face(mg::editor::EditorId editorId, mg::editor::Editor::Id id) {
    try {
        return mg::editor::isFace(editorId, id);
    } catch (...) {
        return false;
    }
}

EMSCRIPTEN_KEEPALIVE
bool mg_editor_is_element(mg::editor::EditorId editorId, mg::editor::Editor::Id id) {
    try {
        return mg::editor::isElement(editorId, id);
    } catch (...) {
        return false;
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_create_face(mg::editor::EditorId editorId, const char* json) {
    try {
        return toJson(mg::editor::createFace(editorId, json ? std::string(json) : std::string()));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_remove_face(mg::editor::EditorId editorId, mg::editor::Editor::Id faceId) {
    try {
        return toJson(mg::editor::removeFace(editorId, faceId));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_reorder_face(mg::editor::EditorId editorId, mg::editor::Editor::Id faceId, std::size_t index) {
    try {
        return toJson(mg::editor::reorderFace(editorId, faceId, index));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_create_element(mg::editor::EditorId editorId, mg::editor::Editor::Id parentId, const char* json) {
    try {
        return toJson(mg::editor::createElement(
            editorId,
            mg::editor::Editor::ElementPlacement{ parentId, mg::editor::Editor::Append },
            json ? std::string(json) : std::string()
        ));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_remove_element(mg::editor::EditorId editorId, mg::editor::Editor::Id elementId) {
    try {
        return toJson(mg::editor::removeElement(editorId, elementId));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_reorder_element(mg::editor::EditorId editorId, mg::editor::Editor::Id elementId, std::size_t index) {
    try {
        return toJson(mg::editor::reorderElement(editorId, elementId, index));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_move_element(mg::editor::EditorId editorId, mg::editor::Editor::Id elementId, mg::editor::Editor::Id parentId, std::size_t index) {
    try {
        return toJson(mg::editor::moveElement(
            editorId,
            elementId,
            mg::editor::Editor::ElementPlacement{ parentId, index }
        ));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_replace_element_from_json(mg::editor::EditorId editorId, mg::editor::Editor::Id elementId, const char* json) {
    try {
        return toJson(mg::editor::replaceElementFromJson(editorId, elementId, json ? std::string(json) : std::string()));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_set_property(mg::editor::EditorId editorId, mg::editor::Editor::Id id, const char* path, const char* json) {
    try {
        return toJson(mg::editor::setProperty(editorId, id, path ? std::string(path) : std::string(), json ? std::string(json) : std::string()));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_property(mg::editor::EditorId editorId, mg::editor::Editor::Id id, const char* path) {
    try {
        return toJson(mg::editor::getProperty(editorId, id, path ? std::string(path) : std::string()));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_properties_meta(mg::editor::EditorId editorId, mg::editor::Editor::Id id, const char* path) {
    try {
        return toJson(mg::editor::getPropertiesMeta(editorId, id, path ? std::string(path) : std::string()));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_copy_face(mg::editor::EditorId editorId, mg::editor::Editor::Id faceId) {
    try {
        return toJson(mg::editor::copyFace(editorId, faceId));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_cut_face(mg::editor::EditorId editorId, mg::editor::Editor::Id faceId) {
    try {
        return toJson(mg::editor::cutFace(editorId, faceId));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_paste_face(mg::editor::EditorId editorId, std::size_t index) {
    try {
        return toJson(mg::editor::pasteFace(editorId, mg::editor::Editor::FacePlacement{ index }));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_copy_element(mg::editor::EditorId editorId, mg::editor::Editor::Id elementId) {
    try {
        return toJson(mg::editor::copyElement(editorId, elementId));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_cut_element(mg::editor::EditorId editorId, mg::editor::Editor::Id elementId) {
    try {
        return toJson(mg::editor::cutElement(editorId, elementId));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_paste_element(mg::editor::EditorId editorId, mg::editor::Editor::Id parentId, std::size_t index) {
    try {
        return toJson(mg::editor::pasteElement(
            editorId,
            mg::editor::Editor::ElementPlacement{ parentId, index }
        ));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_paste_to_replace_element(mg::editor::EditorId editorId, mg::editor::Editor::Id elementId) {
    try {
        return toJson(mg::editor::pasteToReplaceElement(editorId, elementId));
    } catch (...) {
        return mg::bindings::storeString(R"({"ok":false,"error":"Exception"})");
    }
}

EMSCRIPTEN_KEEPALIVE
bool mg_editor_undo(mg::editor::EditorId editorId) {
    try {
        return mg::editor::undo(editorId);
    } catch (...) {
        return false;
    }
}

EMSCRIPTEN_KEEPALIVE
bool mg_editor_redo(mg::editor::EditorId editorId) {
    try {
        return mg::editor::redo(editorId);
    } catch (...) {
        return false;
    }
}

}
