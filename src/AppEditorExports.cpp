#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include <emscripten/emscripten.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <multigauge/App.h>
#include <multigauge/Platform.h>
#include <multigauge/editor/GaugeEditor.h>
#include <multigauge/gauge/GaugeFace.h>


#include "platform/GraphicsContextCanvas.h"

namespace {
    struct EditorViewBinding {
        std::uint32_t id = 0;
        std::string name;
        std::string canvasId;
        std::string documentPath;
        std::string assetsJson = "[]";
        int renderWidth = 240;
        int renderHeight = 240;
        GraphicsContextCanvas* context = nullptr;
        std::unique_ptr<GraphicsContextCanvas> ownedContext;
        mg::ContextId contextId = 0;
        std::unique_ptr<GaugeFace> face;
        std::unique_ptr<GaugeEditor> editor;
    };

    std::vector<std::unique_ptr<EditorViewBinding>> g_bindings;
    std::uint32_t g_nextViewId = 1;
    std::uint32_t g_activeViewId = 0;
    std::string g_ret;
    constexpr const char* EDITOR_DOCUMENT_DIR = "/work/editor_docs";

    std::string make_document_path(std::uint32_t id);
    bool write_binding_document(EditorViewBinding& binding, const char* rootJsonOverride = nullptr);

    std::string to_json(const rapidjson::Document& document) {
        rapidjson::StringBuffer sb;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
        document.Accept(writer);
        return std::string(sb.GetString(), sb.GetSize());
    }

    const char* set_exception_result(const char* fallback = "Exception") {
        g_ret = std::string(R"({"ok":false,"error":")") + fallback + R"("})";
        return g_ret.c_str();
    }

    EditorViewBinding* find_binding(std::uint32_t id) {
        for (const auto& binding : g_bindings) {
            if (binding && binding->id == id) return binding.get();
        }
        return nullptr;
    }

    EditorViewBinding* active_binding() {
        return find_binding(g_activeViewId);
    }

    GaugeEditor* active_editor() {
        auto* binding = active_binding();
        return binding ? binding->editor.get() : nullptr;
    }

    GaugeEditor& catalog_editor() {
        static GaugeFace fallbackFace;
        static GaugeEditor fallbackEditor(fallbackFace);

        GaugeEditor* editor = active_editor();
        return editor ? *editor : fallbackEditor;
    }

    rapidjson::Value make_view_json(const EditorViewBinding& binding, rapidjson::Document::AllocatorType& allocator) {
        rapidjson::Value obj(rapidjson::kObjectType);
        obj.AddMember("id", binding.id, allocator);
        obj.AddMember("name", rapidjson::Value(binding.name.c_str(), allocator), allocator);
        obj.AddMember("canvasId", rapidjson::Value(binding.canvasId.c_str(), allocator), allocator);
        obj.AddMember("renderWidth", binding.renderWidth, allocator);
        obj.AddMember("renderHeight", binding.renderHeight, allocator);
        obj.AddMember("active", binding.id == g_activeViewId, allocator);
        return obj;
    }

    std::string make_views_json() {
        rapidjson::Document document;
        document.SetArray();
        auto& allocator = document.GetAllocator();

        for (const auto& binding : g_bindings) {
            if (!binding) continue;
            document.PushBack(make_view_json(*binding, allocator), allocator);
        }

        return to_json(document);
    }

    std::string make_result(bool ok, const char* error = nullptr, const EditorViewBinding* binding = nullptr) {
        rapidjson::Document document;
        document.SetObject();
        auto& allocator = document.GetAllocator();

        document.AddMember("ok", ok, allocator);
        if (error) {
            document.AddMember("error", rapidjson::Value(error, allocator), allocator);
        }
        if (binding) {
            document.AddMember("id", binding->id, allocator);
            document.AddMember("name", rapidjson::Value(binding->name.c_str(), allocator), allocator);
            document.AddMember("canvasId", rapidjson::Value(binding->canvasId.c_str(), allocator), allocator);
            document.AddMember("renderWidth", binding->renderWidth, allocator);
            document.AddMember("renderHeight", binding->renderHeight, allocator);
            document.AddMember("active", binding->id == g_activeViewId, allocator);
        }
        if (g_activeViewId) {
            document.AddMember("activeViewId", g_activeViewId, allocator);
        }

        return to_json(document);
    }

    std::string make_editor_action_result(const EditorResult& result) {
        rapidjson::Document document;
        document.SetObject();
        auto& allocator = document.GetAllocator();

        document.AddMember("ok", result.ok, allocator);
        if (!result.ok) {
            document.AddMember("error", rapidjson::Value(result.error.c_str(), allocator), allocator);
            return to_json(document);
        }

        if (result.data.IsObject()) {
            for (auto it = result.data.MemberBegin(); it != result.data.MemberEnd(); ++it) {
                rapidjson::Value key;
                key.CopyFrom(it->name, allocator);

                rapidjson::Value value;
                value.CopyFrom(it->value, allocator);
                document.AddMember(std::move(key), std::move(value), allocator);
            }
        } else if (!result.data.IsNull()) {
            rapidjson::Value value;
            value.CopyFrom(result.data, allocator);
            document.AddMember("data", std::move(value), allocator);
        }

        return to_json(document);
    }

    std::string make_editor_value_result(const EditorResult& result, const char* fallbackJson) {
        if (!result.ok) {
            return fallbackJson ? std::string(fallbackJson) : std::string("null");
        }

        return to_json(result.data);
    }

    std::string make_hit_test_result(std::uint32_t id) {
        rapidjson::Document document;
        document.SetUint(id);
        return to_json(document);
    }

    std::string make_default_element_json(const char* type) {
        rapidjson::Document document;
        document.SetObject();
        auto& allocator = document.GetAllocator();
        document.AddMember("type", rapidjson::Value(type ? type : "", allocator), allocator);
        return to_json(document);
    }

    std::string make_history_state_json(bool canUndo = false, bool canRedo = false, std::uint32_t undoDepth = 0, std::uint32_t redoDepth = 0) {
        rapidjson::Document document;
        document.SetObject();
        auto& allocator = document.GetAllocator();
        document.AddMember("canUndo", canUndo, allocator);
        document.AddMember("canRedo", canRedo, allocator);
        document.AddMember("undoDepth", undoDepth, allocator);
        document.AddMember("redoDepth", redoDepth, allocator);
        return to_json(document);
    }

    std::string clone_json_value(const rapidjson::Value& value) {
        rapidjson::Document document;
        document.CopyFrom(value, document.GetAllocator());
        return to_json(document);
    }

    bool parse_object_json(const std::string& jsonText, rapidjson::Document& document) {
        document.Parse(jsonText.c_str(), jsonText.size());
        return !document.HasParseError() && document.IsObject();
    }

    bool parse_array_json(const std::string& jsonText, rapidjson::Document& document) {
        document.Parse(jsonText.c_str(), jsonText.size());
        return !document.HasParseError() && document.IsArray();
    }

    void ensure_editor_document_dir() {
        FS().makeDirectory(EDITOR_DOCUMENT_DIR);
    }

    std::string make_document_path(std::uint32_t id) {
        return std::string(EDITOR_DOCUMENT_DIR) + "/view_" + std::to_string(id) + ".json";
    }

    bool load_source_document_parts(const char* gaugePath, std::string& outAssetsJson, std::string& outRootJson) {
        outAssetsJson = "[]";
        outRootJson = "{}";

        if (!gaugePath || !gaugePath[0]) {
            return true;
        }

        std::vector<std::uint8_t> bytes;
        if (!FS().readAll(gaugePath, bytes) || bytes.empty()) {
            return false;
        }

        rapidjson::Document document;
        document.Parse(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        if (document.HasParseError() || !document.IsObject()) {
            return false;
        }

        const auto root = document.GetObject();
        auto rootIt = root.FindMember("root");
        auto assetsIt = root.FindMember("assets");
        if (rootIt != root.MemberEnd() && rootIt->value.IsObject()) {
            outRootJson = clone_json_value(rootIt->value);
            if (assetsIt != root.MemberEnd() && assetsIt->value.IsArray()) {
                outAssetsJson = clone_json_value(assetsIt->value);
            }
            return true;
        }

        outRootJson = clone_json_value(document);
        return true;
    }

    bool load_text_file(const char* path, std::string& outText) {
        outText.clear();
        if (!path || !path[0]) {
            return false;
        }

        std::vector<std::uint8_t> bytes;
        if (!FS().readAll(path, bytes)) {
            return false;
        }

        outText.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return true;
    }

    std::string build_document_json_text(const EditorViewBinding& binding, const char* rootJsonOverride = nullptr) {
        rapidjson::Document document;
        document.SetObject();
        auto& allocator = document.GetAllocator();

        rapidjson::Document assetsDocument;
        if (!parse_array_json(binding.assetsJson, assetsDocument)) {
            assetsDocument.SetArray();
        }

        rapidjson::Value assetsValue;
        assetsValue.CopyFrom(assetsDocument, allocator);
        document.AddMember("assets", std::move(assetsValue), allocator);

        rapidjson::Document rootDocument;
        const std::string rootJson = rootJsonOverride
            ? std::string(rootJsonOverride)
            : (binding.editor ? binding.editor->saveFace() : std::string("{}"));
        if (!parse_object_json(rootJson, rootDocument)) {
            rootDocument.SetObject();
        }

        rapidjson::Value rootValue;
        rootValue.CopyFrom(rootDocument, allocator);
        document.AddMember("root", std::move(rootValue), allocator);

        return to_json(document);
    }

    bool write_binding_document(EditorViewBinding& binding, const char* rootJsonOverride) {
        ensure_editor_document_dir();
        if (binding.documentPath.empty()) {
            binding.documentPath = make_document_path(binding.id);
        }

        const std::string documentJson = build_document_json_text(binding, rootJsonOverride);
        return FS().writeAll(
            binding.documentPath,
            reinterpret_cast<const std::uint8_t*>(documentJson.data()),
            documentJson.size()
        );
    }

    bool reload_binding_view(EditorViewBinding& binding) {
        if (!binding.context) return false;
        if (!write_binding_document(binding)) return false;
        return mg::showGauge(binding.contextId, binding.documentPath.c_str());
    }

    const char* get_document_assets_impl() {
        EditorViewBinding* binding = active_binding();
        g_ret = binding ? binding->assetsJson : "[]";
        return g_ret.c_str();
    }

    const char* set_document_assets_text_impl(const std::string& jsonText) {
        EditorViewBinding* binding = active_binding();
        if (!binding) {
            g_ret = R"({"ok":false,"error":"ViewNotFound"})";
            return g_ret.c_str();
        }

        rapidjson::Document assetsDocument;
        if (!parse_array_json(jsonText, assetsDocument)) {
            g_ret = R"({"ok":false,"error":"BadJson"})";
            return g_ret.c_str();
        }

        binding->assetsJson = clone_json_value(assetsDocument);
        if (!reload_binding_view(*binding)) {
            g_ret = R"({"ok":false,"error":"ReloadFailed"})";
            return g_ret.c_str();
        }

        g_ret = make_result(true, nullptr, binding);
        return g_ret.c_str();
    }

    const char* set_document_assets_impl(const char* jsonValue) {
        if (!jsonValue) {
            g_ret = R"({"ok":false,"error":"BadArgs"})";
            return g_ret.c_str();
        }

        return set_document_assets_text_impl(jsonValue);
    }

    const char* set_document_assets_from_file_impl(const char* jsonPath) {
        std::string jsonText;
        if (!load_text_file(jsonPath, jsonText)) {
            g_ret = R"({"ok":false,"error":"DocumentLoadFailed"})";
            return g_ret.c_str();
        }

        return set_document_assets_text_impl(jsonText);
    }

    const char* reload_view_impl() {
        EditorViewBinding* binding = active_binding();
        if (!binding) {
            g_ret = R"({"ok":false,"error":"ViewNotFound"})";
            return g_ret.c_str();
        }

        if (!reload_binding_view(*binding)) {
            g_ret = R"({"ok":false,"error":"ReloadFailed"})";
            return g_ret.c_str();
        }

        g_ret = make_result(true, nullptr, binding);
        return g_ret.c_str();
    }

    const char* export_document_impl() {
        EditorViewBinding* binding = active_binding();
        if (!binding) {
            g_ret.clear();
            return g_ret.c_str();
        }

        g_ret = build_document_json_text(*binding);
        return g_ret.c_str();
    }

    const char* create_view_impl(const char* canvasId, const char* gaugePath, const char* name) {
        if (!canvasId || !canvasId[0]) {
            g_ret = R"({"ok":false,"error":"BadArgs"})";
            return g_ret.c_str();
        }

        auto binding = std::make_unique<EditorViewBinding>();
        binding->id = g_nextViewId++;
        binding->name = (name && name[0]) ? name : ("Face " + std::to_string(binding->id));
        binding->canvasId = canvasId;
        binding->documentPath = make_document_path(binding->id);
        binding->ownedContext = std::make_unique<GraphicsContextCanvas>(binding->canvasId);
        binding->context = binding->ownedContext.get();
        if (!binding->context->init()) {
            g_ret = R"({"ok":false,"error":"CanvasInitFailed"})";
            return g_ret.c_str();
        }
        binding->context->resize(binding->renderWidth, binding->renderHeight);

        std::string rootJson;
        if (!load_source_document_parts(gaugePath, binding->assetsJson, rootJson)) {
            g_ret = R"({"ok":false,"error":"DocumentLoadFailed"})";
            return g_ret.c_str();
        }
        if (!write_binding_document(*binding, rootJson.c_str())) {
            g_ret = R"({"ok":false,"error":"DocumentWriteFailed"})";
            return g_ret.c_str();
        }

        binding->contextId = mg::addContext(*binding->context);
        binding->face = std::make_unique<GaugeFace>();
        binding->editor = std::make_unique<GaugeEditor>(*binding->face);
        binding->editor->loadFace(rootJson);

        if (!mg::showGauge(binding->contextId, binding->documentPath.c_str())) {
            mg::removeContext(binding->contextId);
            g_ret = R"({"ok":false,"error":"DocumentLoadFailed"})";
            return g_ret.c_str();
        }

        const EditorViewBinding* bindingPtr = binding.get();
        g_bindings.push_back(std::move(binding));
        g_activeViewId = bindingPtr->id;

        g_ret = make_result(true, nullptr, bindingPtr);
        return g_ret.c_str();
    }

    const char* remove_view_impl(std::uint32_t id) {
        for (auto it = g_bindings.begin(); it != g_bindings.end(); ++it) {
            auto& binding = *it;
            if (!binding || binding->id != id) continue;

            mg::removeContext(binding->contextId);
            if (!binding->documentPath.empty()) {
                FS().remove(binding->documentPath);
            }

            const bool removedActive = (g_activeViewId == id);
            g_bindings.erase(it);

            if (removedActive) {
                g_activeViewId = g_bindings.empty() ? 0 : g_bindings.front()->id;
            }

            const EditorViewBinding* nextBinding = active_binding();
            g_ret = make_result(true, nullptr, nextBinding);
            return g_ret.c_str();
        }

        g_ret = R"({"ok":false,"error":"ViewNotFound"})";
        return g_ret.c_str();
    }

    const char* set_view_render_size_impl(std::uint32_t id, int width, int height) {
        EditorViewBinding* binding = find_binding(id);
        if (!binding || !binding->context) {
            g_ret = R"({"ok":false,"error":"ViewNotFound"})";
            return g_ret.c_str();
        }

        if (width <= 0 || height <= 0) {
            g_ret = R"({"ok":false,"error":"BadArgs"})";
            return g_ret.c_str();
        }

        binding->renderWidth = width;
        binding->renderHeight = height;
        binding->context->resize(binding->renderWidth, binding->renderHeight);

        g_ret = make_result(true, nullptr, binding);
        return g_ret.c_str();
    }
}

extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_create_view(const char* canvasId, const char* gaugePath, const char* name) {
    try {
        return create_view_impl(canvasId, gaugePath, name);
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_list_views() {
    try {
        g_ret = make_views_json();
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_select_view(std::uint32_t id) {
    try {
        EditorViewBinding* binding = find_binding(id);
        if (!binding) {
            g_ret = R"({"ok":false,"error":"ViewNotFound"})";
            return g_ret.c_str();
        }

        g_activeViewId = binding->id;
        g_ret = make_result(true, nullptr, binding);
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_remove_view(std::uint32_t id) {
    try {
        return remove_view_impl(id);
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_set_view_render_size(std::uint32_t id, int width, int height) {
    try {
        return set_view_render_size_impl(id, width, height);
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_preview_create_view(const char* canvasId, const char* gaugePath, const char* name) {
    try {
        return create_view_impl(canvasId, gaugePath, name);
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_preview_remove_view(std::uint32_t id) {
    try {
        return remove_view_impl(id);
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_list_tree() {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = "[]";
            return g_ret.c_str();
        }

        g_ret = make_editor_value_result(editor->listTreeJson(), "[]");
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_list_elements() {
    try {
        g_ret = make_editor_value_result(catalog_editor().listElements(), "[]");
        return g_ret.c_str();
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_list_values() {
    try {
        g_ret = make_editor_value_result(catalog_editor().listValues(), "[]");
        return g_ret.c_str();
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_properties(std::uint32_t id) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = "[]";
            return g_ret.c_str();
        }

        g_ret = make_editor_value_result(editor->getPropertiesMetaJson(id), "[]");
        return g_ret.c_str();
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_history_state() {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = make_history_state_json();
            return g_ret.c_str();
        }

        g_ret = make_editor_value_result(editor->getHistoryState(), "{}");
        return g_ret.c_str();
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_document_assets() {
    try {
        return get_document_assets_impl();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_set_document_assets(const char* jsonValue) {
    try {
        return set_document_assets_impl(jsonValue);
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_set_document_assets_from_file(const char* jsonPath) {
    try {
        return set_document_assets_from_file_impl(jsonPath);
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_reload_view() {
    try {
        return reload_view_impl();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_property_node(std::uint32_t id, const char* path) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor || !path) {
            g_ret = "{}";
            return g_ret.c_str();
        }

        g_ret = make_editor_value_result(editor->getPropertiesMetaJson(id, path), "{}");
        return g_ret.c_str();
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_undo() {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->undo());
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_redo() {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->redo());
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_set_property(std::uint32_t id, const char* path, const char* jsonValue) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        if (!path || !jsonValue) {
            g_ret = R"({"ok":false,"error":"BadArgs"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->setPropertyJson(id, path, jsonValue));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_add_element(std::uint32_t parentId, const char* type) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        if (!type) {
            g_ret = R"({"ok":false,"error":"BadArgs"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->addElementJson(parentId, make_default_element_json(type)));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_insert_element(std::uint32_t parentId, const char* type, int index) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        if (!type) {
            g_ret = R"({"ok":false,"error":"BadArgs"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->insertElementJson(parentId, make_default_element_json(type), index));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_move_element(std::uint32_t id, std::uint32_t newParentId, int index) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->moveElement(id, newParentId, index));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_remove_element(std::uint32_t id) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->removeElement(id));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_element_json(std::uint32_t id) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = "{}";
            return g_ret.c_str();
        }

        g_ret = make_editor_value_result(editor->getElementJson(id), "{}");
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_insert_element_json(std::uint32_t parentId, const char* jsonValue, int index) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        if (!jsonValue) {
            g_ret = R"({"ok":false,"error":"BadArgs"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->insertElementJson(parentId, jsonValue, index));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_replace_element_json(std::uint32_t id, const char* jsonValue) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        if (!jsonValue) {
            g_ret = R"({"ok":false,"error":"BadArgs"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->replaceElementJson(id, jsonValue));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_bring_forward(std::uint32_t id) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->bringForward(id));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_bring_to_front(std::uint32_t id) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->bringToFront(id));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_send_backward(std::uint32_t id) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->sendBackward(id));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_send_to_back(std::uint32_t id) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->sendToBack(id));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_reorder_element(std::uint32_t id, int index) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->reorderElement(id, index));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_copy_element(std::uint32_t id) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->copyElement(id));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_paste_into_element(std::uint32_t id) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->pasteIntoElement(id));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_paste_to_replace_element(std::uint32_t id) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->pasteToReplaceElement(id));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_duplicate_element(std::uint32_t id) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->duplicateElement(id));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_element_bounds(std::uint32_t id) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = R"({"ok":false,"error":"NoEditor"})";
            return g_ret.c_str();
        }

        g_ret = make_editor_action_result(editor->getElementBoundsJson(id));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_list_element_bounds() {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = "[]";
            return g_ret.c_str();
        }

        g_ret = make_editor_value_result(editor->listElementBoundsJson(), "[]");
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_hit_test(float x, float y, int index) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = "0";
            return g_ret.c_str();
        }

        g_ret = make_hit_test_result(editor->hitTest(x, y, index));
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_hit_test_all(float x, float y) {
    try {
        GaugeEditor* editor = active_editor();
        if (!editor) {
            g_ret = "[]";
            return g_ret.c_str();
        }

        g_ret = make_editor_value_result(editor->hitTestAll(x, y), "[]");
        return g_ret.c_str();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_export_document() {
    try {
        return export_document_impl();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_export_face() {
    try {
        return export_document_impl();
    } catch (const std::exception& error) {
        return set_exception_result(error.what());
    } catch (...) {
        return set_exception_result();
    }
}

} // extern "C"
