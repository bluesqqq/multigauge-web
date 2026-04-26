#include "EditorBindings.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include <emscripten/emscripten.h>

#include <multigauge/Platform.h>
#include <multigauge/geometry/Point.h>
#include <multigauge/gauge/Element.h>
#include <multigauge/values/Value.h>

namespace mgweb {
namespace {

rapidjson::Value copy_json(const rapidjson::Value& value, rapidjson::Document::AllocatorType& allocator) {
    rapidjson::Value copy;
    copy.CopyFrom(value, allocator);
    return copy;
}

rapidjson::Value make_element_bounds_json(Editor::Id id, const Element& element, rapidjson::Document::AllocatorType& allocator) {
    const Rect<float>& bounds = element.getBounds();

    rapidjson::Value object(rapidjson::kObjectType);
    object.AddMember("id", id, allocator);
    object.AddMember("x", bounds.x, allocator);
    object.AddMember("y", bounds.y, allocator);
    object.AddMember("width", bounds.width, allocator);
    object.AddMember("height", bounds.height, allocator);
    return object;
}

void append_tree_children(const Editor& editor,
                          Editor::Id parentId,
                          rapidjson::Value& output,
                          rapidjson::Document::AllocatorType& allocator);

rapidjson::Value make_tree_node(const Editor& editor,
                                Editor::Id id,
                                rapidjson::Document::AllocatorType& allocator) {
    EditorResult described = editor.describeNode(id);
    if (!described.ok || !described.data.IsObject()) {
        return rapidjson::Value(rapidjson::kObjectType);
    }

    rapidjson::Value node(rapidjson::kObjectType);
    for (auto it = described.data.MemberBegin(); it != described.data.MemberEnd(); ++it) {
        node.AddMember(copy_json(it->name, allocator), copy_json(it->value, allocator), allocator);
    }

    rapidjson::Value children(rapidjson::kArrayType);
    append_tree_children(editor, id, children, allocator);
    node.AddMember("children", std::move(children), allocator);
    return node;
}

void append_tree_children(const Editor& editor,
                          Editor::Id parentId,
                          rapidjson::Value& output,
                          rapidjson::Document::AllocatorType& allocator) {
    EditorResult described = editor.describeNode(parentId);
    if (!described.ok || !described.data.IsObject()) return;

    EditorResult listed = Error("Invalid id");
    const char* kind = described.data.HasMember("kind") && described.data["kind"].IsString()
        ? described.data["kind"].GetString()
        : "";

    if (std::strcmp(kind, "face") == 0) {
        listed = editor.listRoots(parentId);
    } else {
        listed = editor.listChildren(parentId);
    }

    if (!listed.ok || !listed.data.IsArray()) return;

    for (const auto& item : listed.data.GetArray()) {
        if (!item.IsObject()) continue;
        auto idIt = item.FindMember("id");
        if (idIt == item.MemberEnd() || !idIt->value.IsUint()) continue;
        output.PushBack(make_tree_node(editor, idIt->value.GetUint(), allocator), allocator);
    }
}

rapidjson::Value build_tree_json(const Editor& editor) {
    rapidjson::Document document;
    document.SetArray();
    auto& allocator = document.GetAllocator();

    EditorResult faces = editor.listFaces();
    if (!faces.ok || !faces.data.IsArray()) {
        return rapidjson::Value(rapidjson::kArrayType);
    }

    for (const auto& item : faces.data.GetArray()) {
        if (!item.IsObject()) continue;
        auto idIt = item.FindMember("id");
        if (idIt == item.MemberEnd() || !idIt->value.IsUint()) continue;
        document.PushBack(make_tree_node(editor, idIt->value.GetUint(), allocator), allocator);
    }

    rapidjson::Value output;
    output.CopyFrom(document, allocator);
    return output;
}

void collect_bounds_recursive(const Editor& editor,
                              Editor::Id faceId,
                              const Element& element,
                              rapidjson::Value& output,
                              rapidjson::Document::AllocatorType& allocator) {
    rapidjson::Value item = make_element_bounds_json(editor.idOf(&element), element, allocator);
    item.AddMember("faceId", faceId, allocator);
    if (const Element* parent = element.getParent()) {
        item.AddMember("parentElementId", editor.idOf(parent), allocator);
    }
    output.PushBack(std::move(item), allocator);

    for (std::size_t i = 0; i < element.childCount(); ++i) {
        const Element* child = element.childAt(i);
        if (child) {
            collect_bounds_recursive(editor, faceId, *child, output, allocator);
        }
    }
}

void hit_test_recursive(const Editor& editor,
                        const Element& element,
                        const Point<float>& point,
                        std::vector<Editor::Id>& hits) {
    for (std::size_t i = element.childCount(); i > 0; --i) {
        const Element* child = element.childAt(i - 1);
        if (child) {
            hit_test_recursive(editor, *child, point, hits);
        }
    }

    if (element.getBounds().contains(point)) {
        hits.push_back(editor.idOf(&element));
    }
}

EditorResult list_values_result() {
    EditorResult result = OkArray();
    auto& allocator = result.data.GetAllocator();

    const std::vector<const Value*> values = Value::list();
    for (const Value* value : values) {
        if (!value) continue;

        rapidjson::Value item(rapidjson::kObjectType);
        item.AddMember("id", rapidjson::Value(value->getId(), allocator), allocator);
        item.AddMember("name", rapidjson::Value(value->getName(), allocator), allocator);
        item.AddMember("minimum", value->getMinimumBase(), allocator);
        item.AddMember("maximum", value->getMaximumBase(), allocator);

        rapidjson::Value units(rapidjson::kArrayType);
        for (const Unit& unit : value->getUnitType().getUnits()) {
            rapidjson::Value unitValue(rapidjson::kObjectType);
            unitValue.AddMember("name", rapidjson::Value(unit.name, allocator), allocator);
            unitValue.AddMember("abbreviation", rapidjson::Value(unit.abbreviation, allocator), allocator);
            unitValue.AddMember("decimalPlaces", unit.decimalPlaces, allocator);
            units.PushBack(std::move(unitValue), allocator);
        }
        item.AddMember("units", std::move(units), allocator);

        result.data.PushBack(std::move(item), allocator);
    }

    return result;
}

EditorResult list_element_types_result() {
    EditorResult result = OkArray();
    auto& allocator = result.data.GetAllocator();
    rapidjson::Value meta = Element::registry().getTypesMeta(allocator);
    result.data.CopyFrom(meta, allocator);
    return result;
}

EditorResult ok_with_face(Editor::Id faceId) {
    EditorResult result = OkObject();
    result.data.AddMember("faceId", faceId, result.data.GetAllocator());
    return result;
}

EditorResult result_for_insert(EditorResult result, EditorViewBinding* binding) {
    if (!result.ok || !binding) return result;
    sync_preview_face(*binding);
    mark_preview_dirty(*binding);
    return result;
}

EditorResult load_face_json(EditorViewBinding& binding, const std::string& jsonText, std::size_t index) {
    rapidjson::Document document;
    if (!parse_object_json(jsonText, document)) {
        return Error("Invalid JSON");
    }

    auto face = std::make_unique<GaugeFace>();
    face->load(document);

    GaugeFace* raw = face.get();
    binding.externalFaces.push_back(std::move(face));
    EditorResult result = binding.editor->insertFace(*raw, Editor::FacePlacement{ index });
    if (result.ok && binding.previewFaceId == 0) {
        binding.previewFaceId = result.data.HasMember("id") && result.data["id"].IsUint() ? result.data["id"].GetUint() : first_face_id(*binding.editor);
    }
    return result_for_insert(std::move(result), &binding);
}

EditorResult create_element_json(Editor& editor, Editor::Id parentId, const std::string& jsonText, std::size_t index) {
    EditorResult described = editor.describeNode(parentId);
    if (!described.ok || !described.data.IsObject()) {
        return Error("Invalid parent id");
    }

    const bool isFace = described.data.HasMember("kind")
        && described.data["kind"].IsString()
        && std::strcmp(described.data["kind"].GetString(), "face") == 0;

    if (isFace) {
        return editor.createRoot(Editor::RootPlacement{ parentId, index }, jsonText);
    }

    return editor.createChild(Editor::ChildPlacement{ parentId, index }, jsonText);
}

EditorResult remove_node(Editor& editor, Editor::Id id) {
    EditorResult described = editor.describeNode(id);
    if (!described.ok || !described.data.IsObject()) {
        return Error("Invalid id");
    }

    const bool isFace = described.data.HasMember("kind")
        && described.data["kind"].IsString()
        && std::strcmp(described.data["kind"].GetString(), "face") == 0;
    if (isFace) {
        return editor.removeFace(id);
    }

    if (described.data.HasMember("faceId")) {
        return editor.removeRoot(id);
    }

    return editor.removeElement(id);
}

EditorResult reorder_node(Editor& editor, Editor::Id id, std::size_t index) {
    EditorResult described = editor.describeNode(id);
    if (!described.ok || !described.data.IsObject()) {
        return Error("Invalid id");
    }

    const bool isFace = described.data.HasMember("kind")
        && described.data["kind"].IsString()
        && std::strcmp(described.data["kind"].GetString(), "face") == 0;
    if (isFace) {
        return editor.reorderFace(id, index);
    }

    if (described.data.HasMember("faceId")) {
        return editor.reorderRoot(id, index);
    }

    return editor.reorderChild(id, index);
}

EditorResult move_node(Editor& editor, Editor::Id id, Editor::Id destinationId, std::size_t index) {
    EditorResult described = editor.describeNode(id);
    if (!described.ok || !described.data.IsObject()) {
        return Error("Invalid id");
    }

    EditorResult destination = editor.describeNode(destinationId);
    if (!destination.ok || !destination.data.IsObject()) {
        return Error("Invalid destination id");
    }

    const bool sourceIsRoot = described.data.HasMember("faceId");
    const bool sourceIsChild = described.data.HasMember("parentElementId");
    const bool destinationIsFace = destination.data.HasMember("kind")
        && destination.data["kind"].IsString()
        && std::strcmp(destination.data["kind"].GetString(), "face") == 0;
    const bool destinationIsElement = destination.data.HasMember("kind")
        && destination.data["kind"].IsString()
        && std::strcmp(destination.data["kind"].GetString(), "element") == 0;

    if (sourceIsRoot && destinationIsFace) {
        return editor.moveRootToFace(id, Editor::RootPlacement{ destinationId, index });
    }
    if (sourceIsChild && destinationIsElement) {
        return editor.moveElementToParent(id, Editor::ChildPlacement{ destinationId, index });
    }

    return Error("Unsupported move for current core API");
}

std::size_t normalized_index(int index) {
    return index < 0 ? Editor::Append : static_cast<std::size_t>(index);
}

EditorResult list_current_preview_bounds(EditorViewBinding& binding) {
    if (!binding.editor) return OkArray();
    if (!ensure_preview_layout(binding)) return Error("PreviewUnavailable");

    GaugeFace* face = find_face(*binding.editor, binding.previewFaceId);
    if (!face) return OkArray();

    EditorResult result = OkArray();
    for (std::size_t i = 0; i < face->childCount(); ++i) {
        Element* root = face->childAt(i);
        if (root) {
            collect_bounds_recursive(*binding.editor, binding.previewFaceId, *root, result.data, result.data.GetAllocator());
        }
    }
    return result;
}

} // namespace
} // namespace mgweb

extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_list_tree() {
    try {
        Editor* editor = mgweb::active_editor();
        if (!editor) {
            mgweb::g_ret = "[]";
            return mgweb::g_ret.c_str();
        }

        rapidjson::Value tree = mgweb::build_tree_json(*editor);
        mgweb::g_ret = mgweb::to_json(tree);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_list_faces() {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::g_ret = mgweb::make_editor_value_result(editor ? editor->listFaces() : OkArray(), "[]");
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_list_roots(std::uint32_t faceId) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::g_ret = mgweb::make_editor_value_result(editor ? editor->listRoots(faceId) : OkArray(), "[]");
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_list_children(std::uint32_t elementId) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::g_ret = mgweb::make_editor_value_result(editor ? editor->listChildren(elementId) : OkArray(), "[]");
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_describe_node(std::uint32_t id) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::g_ret = mgweb::make_editor_value_result(editor ? editor->describeNode(id) : OkObject(), "{}");
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_list_elements() {
    try {
        mgweb::g_ret = mgweb::make_editor_value_result(mgweb::list_element_types_result(), "[]");
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_list_values() {
    try {
        mgweb::g_ret = mgweb::make_editor_value_result(mgweb::list_values_result(), "[]");
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_properties(std::uint32_t id) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::g_ret = mgweb::make_editor_value_result(editor ? editor->getPropertiesMeta(id) : OkObject(), "{}");
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_property_node(std::uint32_t id, const char* path) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::g_ret = mgweb::make_editor_value_result(editor ? editor->getPropertiesMeta(id, path ? path : "") : OkObject(), "{}");
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_property(std::uint32_t id, const char* path) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::g_ret = mgweb::make_editor_value_result((editor && path) ? editor->getProperty(id, path) : Error("BadArgs"), "{}");
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_set_property(std::uint32_t id, const char* path, const char* jsonValue) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = (editor && path && jsonValue) ? editor->setProperty(id, path, jsonValue) : Error("BadArgs");
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_history_state() {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::g_ret = mgweb::make_editor_value_result(editor ? editor->getHistory() : OkObject(), "{}");
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_undo() {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = OkObject();
        if (!editor) {
            result = Error("NoEditor");
        } else if (!editor->undo()) {
            result = Error("CannotUndo");
        }
        if (result.ok && binding) {
            mgweb::sync_preview_face(*binding);
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_redo() {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = OkObject();
        if (!editor) {
            result = Error("NoEditor");
        } else if (!editor->redo()) {
            result = Error("CannotRedo");
        }
        if (result.ok && binding) {
            mgweb::sync_preview_face(*binding);
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_create_face(const char* jsonValue, int index) {
    try {
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = (binding && binding->editor && jsonValue)
            ? mgweb::load_face_json(*binding, jsonValue, mgweb::normalized_index(index))
            : Error("BadArgs");
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_remove_face(std::uint32_t faceId) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = editor ? editor->removeFace(faceId) : Error("NoEditor");
        if (result.ok && binding) {
            mgweb::sync_preview_face(*binding);
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_reorder_face(std::uint32_t faceId, int index) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = editor ? editor->reorderFace(faceId, mgweb::normalized_index(index)) : Error("NoEditor");
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_add_element(std::uint32_t parentId, const char* type) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        const std::string json = std::string(R"({"type":")") + (type ? type : "") + R"("})";
        EditorResult result = (editor && type) ? mgweb::create_element_json(*editor, parentId, json, Editor::Append) : Error("BadArgs");
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_insert_element(std::uint32_t parentId, const char* type, int index) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        const std::string json = std::string(R"({"type":")") + (type ? type : "") + R"("})";
        EditorResult result = (editor && type) ? mgweb::create_element_json(*editor, parentId, json, mgweb::normalized_index(index)) : Error("BadArgs");
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_insert_element_json(std::uint32_t parentId, const char* jsonValue, int index) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = (editor && jsonValue) ? mgweb::create_element_json(*editor, parentId, jsonValue, mgweb::normalized_index(index)) : Error("BadArgs");
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_replace_element_json(std::uint32_t id, const char* jsonValue) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = (editor && jsonValue) ? editor->replaceElementFromJson(id, jsonValue) : Error("BadArgs");
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_remove_element(std::uint32_t id) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = editor ? mgweb::remove_node(*editor, id) : Error("NoEditor");
        if (result.ok && binding) {
            mgweb::sync_preview_face(*binding);
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_move_element(std::uint32_t id, std::uint32_t destinationId, int index) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = editor ? mgweb::move_node(*editor, id, destinationId, mgweb::normalized_index(index)) : Error("NoEditor");
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_reorder_element(std::uint32_t id, int index) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = editor ? mgweb::reorder_node(*editor, id, mgweb::normalized_index(index)) : Error("NoEditor");
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_bring_forward(std::uint32_t id) {
    try {
        Editor* editor = mgweb::active_editor();
        EditorResult described = editor ? editor->describeNode(id) : Error("NoEditor");
        EditorResult result = std::move(described);
        if (described.ok && described.data.IsObject() && described.data.HasMember("index") && described.data["index"].IsUint()) {
            result = mgweb::reorder_node(*editor, id, static_cast<std::size_t>(described.data["index"].GetUint() + 1));
        }
        if (result.ok && mgweb::active_binding()) {
            mgweb::mark_preview_dirty(*mgweb::active_binding());
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_bring_to_front(std::uint32_t id) {
    try {
        Editor* editor = mgweb::active_editor();
        if (!editor) {
            mgweb::g_ret = mgweb::make_editor_action_result(Error("NoEditor"));
            return mgweb::g_ret.c_str();
        }

        EditorResult described = editor->describeNode(id);
        EditorResult result = std::move(described);
        if (described.ok && described.data.IsObject()) {
            if (described.data.HasMember("faceId") && described.data["faceId"].IsUint()) {
                EditorResult roots = editor->listRoots(described.data["faceId"].GetUint());
                result = roots.ok ? editor->reorderRoot(id, roots.data.Size() - 1) : std::move(roots);
            } else if (described.data.HasMember("parentElementId") && described.data["parentElementId"].IsUint()) {
                EditorResult children = editor->listChildren(described.data["parentElementId"].GetUint());
                result = children.ok ? editor->reorderChild(id, children.data.Size() - 1) : std::move(children);
            }
        }
        if (result.ok && mgweb::active_binding()) {
            mgweb::mark_preview_dirty(*mgweb::active_binding());
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_send_backward(std::uint32_t id) {
    try {
        Editor* editor = mgweb::active_editor();
        EditorResult described = editor ? editor->describeNode(id) : Error("NoEditor");
        EditorResult result = std::move(described);
        if (described.ok && described.data.IsObject() && described.data.HasMember("index") && described.data["index"].IsUint()) {
            const unsigned current = described.data["index"].GetUint();
            result = mgweb::reorder_node(*editor, id, current == 0 ? 0 : static_cast<std::size_t>(current - 1));
        }
        if (result.ok && mgweb::active_binding()) {
            mgweb::mark_preview_dirty(*mgweb::active_binding());
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_send_to_back(std::uint32_t id) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = editor ? mgweb::reorder_node(*editor, id, 0) : Error("NoEditor");
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_copy_face(std::uint32_t faceId) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::g_ret = mgweb::make_editor_action_result(editor ? editor->copyFace(faceId) : Error("NoEditor"));
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_cut_face(std::uint32_t faceId) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = editor ? editor->cutFace(faceId) : Error("NoEditor");
        if (result.ok && binding) {
            mgweb::sync_preview_face(*binding);
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_paste_face(int index) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = editor ? editor->pasteFace(Editor::FacePlacement{ mgweb::normalized_index(index) }) : Error("NoEditor");
        if (result.ok && binding) {
            mgweb::sync_preview_face(*binding);
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_clipboard() {
    try {
        Editor* editor = mgweb::active_editor();
        EditorResult result = OkObject();
        auto& allocator = result.data.GetAllocator();
        const ClipboardSummary summary = editor ? editor->getClipboardSummary() : ClipboardSummary{};
        const char* kind = "empty";
        if (summary.kind == ClipboardState::Kind::Face) kind = "face";
        if (summary.kind == ClipboardState::Kind::Element) kind = "element";
        result.data.AddMember("kind", rapidjson::Value(kind, allocator), allocator);
        result.data.AddMember("hasValue", summary.hasValue(), allocator);
        mgweb::g_ret = mgweb::make_editor_value_result(result, "{}");
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_clear_clipboard() {
    try {
        Editor* editor = mgweb::active_editor();
        if (!editor) {
            mgweb::g_ret = mgweb::make_editor_action_result(Error("NoEditor"));
            return mgweb::g_ret.c_str();
        }
        editor->clearClipboard();
        mgweb::g_ret = mgweb::make_editor_action_result(OkObject());
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_copy_element(std::uint32_t id) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::g_ret = mgweb::make_editor_action_result(editor ? editor->copyElement(id) : Error("NoEditor"));
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_cut_root(std::uint32_t id) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = editor ? editor->cutRoot(id) : Error("NoEditor");
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_cut_element(std::uint32_t id) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = editor ? editor->cutElement(id) : Error("NoEditor");
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_paste_root(std::uint32_t faceId, int index) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = editor ? editor->pasteRoot(Editor::RootPlacement{ faceId, mgweb::normalized_index(index) }) : Error("NoEditor");
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_paste_into_element(std::uint32_t id) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = editor ? editor->pasteChild(Editor::ChildPlacement{ id, Editor::Append }) : Error("NoEditor");
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_paste_child(std::uint32_t id, int index) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = editor ? editor->pasteChild(Editor::ChildPlacement{ id, mgweb::normalized_index(index) }) : Error("NoEditor");
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_paste_to_replace_element(std::uint32_t id) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = editor ? editor->pasteToReplaceElement(id) : Error("NoEditor");
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_duplicate_element(std::uint32_t id) {
    try {
        Editor* editor = mgweb::active_editor();
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult described = editor ? editor->describeNode(id) : Error("NoEditor");
        EditorResult result = std::move(described);
        if (editor && described.ok && described.data.IsObject()) {
            result = editor->copyElement(id);
            if (result.ok) {
                if (described.data.HasMember("faceId") && described.data["faceId"].IsUint() && described.data.HasMember("index") && described.data["index"].IsUint()) {
                    result = editor->pasteRoot(Editor::RootPlacement{
                        described.data["faceId"].GetUint(),
                        static_cast<std::size_t>(described.data["index"].GetUint() + 1)
                    });
                } else if (described.data.HasMember("parentElementId") && described.data["parentElementId"].IsUint() && described.data.HasMember("index") && described.data["index"].IsUint()) {
                    result = editor->pasteChild(Editor::ChildPlacement{
                        described.data["parentElementId"].GetUint(),
                        static_cast<std::size_t>(described.data["index"].GetUint() + 1)
                    });
                }
            }
        }
        if (result.ok && binding) {
            mgweb::mark_preview_dirty(*binding);
        }
        mgweb::g_ret = mgweb::make_editor_action_result(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_element_json(std::uint32_t id) {
    try {
        Editor* editor = mgweb::active_editor();
        if (!editor) {
            mgweb::g_ret = "{}";
            return mgweb::g_ret.c_str();
        }

        EditorResult described = editor->describeNode(id);
        if (!described.ok || !described.data.IsObject() || !described.data.HasMember("kind") || !described.data["kind"].IsString() || std::strcmp(described.data["kind"].GetString(), "element") != 0) {
            mgweb::g_ret = "{}";
            return mgweb::g_ret.c_str();
        }

        rapidjson::Document document;
        document.SetObject();
        if (const Element* element = [&]() -> const Element* {
            for (std::size_t faceIndex = 0; faceIndex < editor->faceCount(); ++faceIndex) {
                const GaugeFace* face = editor->faceAt(faceIndex);
                for (std::size_t rootIndex = 0; face && rootIndex < face->childCount(); ++rootIndex) {
                    std::vector<const Element*> stack{ face->childAt(rootIndex) };
                    while (!stack.empty()) {
                        const Element* current = stack.back();
                        stack.pop_back();
                        if (current && editor->idOf(current) == id) {
                            return current;
                        }
                        if (!current) continue;
                        for (std::size_t childIndex = 0; childIndex < current->childCount(); ++childIndex) {
                            stack.push_back(current->childAt(childIndex));
                        }
                    }
                }
            }
            return nullptr;
        }()) {
            element->saveToJson(document, document.GetAllocator());
        }

        mgweb::g_ret = mgweb::to_json(document);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_face_json(std::uint32_t faceId) {
    try {
        Editor* editor = mgweb::active_editor();
        const GaugeFace* face = editor ? mgweb::find_face(*editor, faceId) : nullptr;
        if (!face) {
            mgweb::g_ret = "{}";
            return mgweb::g_ret.c_str();
        }

        rapidjson::Document document = face->save();
        mgweb::g_ret = mgweb::to_json(document);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_element_bounds(std::uint32_t id) {
    try {
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        Editor* editor = binding ? binding->editor.get() : nullptr;
        if (!binding || !editor || !mgweb::ensure_preview_layout(*binding)) {
            mgweb::g_ret = R"({"ok":false,"error":"PreviewUnavailable"})";
            return mgweb::g_ret.c_str();
        }

        rapidjson::Document document;
        document.SetObject();
        auto& allocator = document.GetAllocator();

        bool found = false;
        const GaugeFace* face = mgweb::find_face(*editor, binding->previewFaceId);
        if (face) {
            for (std::size_t i = 0; i < face->childCount() && !found; ++i) {
                std::vector<const Element*> stack{ face->childAt(i) };
                while (!stack.empty() && !found) {
                    const Element* current = stack.back();
                    stack.pop_back();
                    if (!current) continue;
                    if (editor->idOf(current) == id) {
                        document = rapidjson::Document();
                        document.CopyFrom(mgweb::make_element_bounds_json(id, *current, allocator), allocator);
                        found = true;
                        break;
                    }
                    for (std::size_t childIndex = 0; childIndex < current->childCount(); ++childIndex) {
                        stack.push_back(current->childAt(childIndex));
                    }
                }
            }
        }

        if (!found) {
            mgweb::g_ret = R"({"ok":false,"error":"ElementNotVisible"})";
            return mgweb::g_ret.c_str();
        }

        mgweb::g_ret = mgweb::to_json(document);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_list_element_bounds() {
    try {
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        EditorResult result = binding ? mgweb::list_current_preview_bounds(*binding) : OkArray();
        mgweb::g_ret = mgweb::make_editor_value_result(result, "[]");
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_hit_test_all(float x, float y) {
    try {
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        Editor* editor = binding ? binding->editor.get() : nullptr;
        if (!binding || !editor || !mgweb::ensure_preview_layout(*binding)) {
            mgweb::g_ret = "[]";
            return mgweb::g_ret.c_str();
        }

        const GaugeFace* face = mgweb::find_face(*editor, binding->previewFaceId);
        std::vector<Editor::Id> hits;
        if (face) {
            const Point<float> point(x, y);
            for (std::size_t i = face->childCount(); i > 0; --i) {
                const Element* root = face->childAt(i - 1);
                if (root) {
                    mgweb::hit_test_recursive(*editor, *root, point, hits);
                }
            }
        }

        rapidjson::Document document;
        document.SetArray();
        for (Editor::Id hitId : hits) {
            document.PushBack(hitId, document.GetAllocator());
        }
        mgweb::g_ret = mgweb::to_json(document);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_hit_test(float x, float y, int index) {
    try {
        const char* all = mg_editor_hit_test_all(x, y);
        rapidjson::Document document;
        if (!mgweb::parse_array_json(all ? all : "[]", document)) {
            mgweb::g_ret = "0";
            return mgweb::g_ret.c_str();
        }

        const int safeIndex = index < 0 ? 0 : index;
        const std::uint32_t value = (safeIndex < static_cast<int>(document.Size()) && document[safeIndex].IsUint())
            ? document[safeIndex].GetUint()
            : 0;

        rapidjson::Document result;
        result.SetUint(value);
        mgweb::g_ret = mgweb::to_json(result);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

} // extern "C"
