#include "EditorBindings.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include <emscripten/emscripten.h>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <multigauge/Platform.h>
#include <multigauge/screens/Screen.h>

#include "platform/GraphicsContextCanvas.h"

namespace mgweb {

std::vector<std::unique_ptr<EditorViewBinding>> g_bindings;
std::uint32_t g_nextViewId = 1;
std::uint32_t g_activeViewId = 0;
std::string g_ret;

namespace {

struct PreviewViewBinding {
    std::uint32_t id = 0;
    std::string canvasId;
    std::string gaugePath;
    int renderWidth = 240;
    int renderHeight = 240;
    GraphicsContextCanvas* context = nullptr;
    std::unique_ptr<GraphicsContextCanvas> ownedContext;
    mg::ContextId contextId = 0;
};

std::vector<std::unique_ptr<PreviewViewBinding>> g_previewBindings;
std::uint32_t g_nextPreviewId = 1;

class EditorPreviewScreen : public mg::Screen {
public:
    explicit EditorPreviewScreen(EditorViewBinding* owner) : binding(owner) {}

    void onShow(mg::RuntimeContext&) override {
        if (binding) {
            mark_preview_dirty(*binding);
        }
    }

    void update(mg::RuntimeContext&, uint64_t deltaUs) override {
        if (!binding || !binding->editor) return;
        if (!ensure_preview_ready(*binding)) return;

        if (GaugeFace* face = find_face(*binding->editor, binding->previewFaceId)) {
            face->update(static_cast<int>(deltaUs));
        }
    }

    void draw(mg::RuntimeContext&, Graphics& g) override {
        if (!binding || !binding->editor) return;
        if (!ensure_preview_layout(*binding)) return;

        if (GaugeFace* face = find_face(*binding->editor, binding->previewFaceId)) {
            face->draw(g);
        }
    }

private:
    EditorViewBinding* binding = nullptr;
};

PreviewViewBinding* find_preview_binding(std::uint32_t id) {
    for (const auto& binding : g_previewBindings) {
        if (binding && binding->id == id) return binding.get();
    }
    return nullptr;
}

std::string read_file_text(const char* path) {
    std::vector<std::uint8_t> bytes;
    if (!path || !path[0] || !FS().readAll(path, bytes)) {
        return {};
    }

    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

rapidjson::Value copy_json(const rapidjson::Value& value, rapidjson::Document::AllocatorType& allocator) {
    rapidjson::Value copy;
    copy.CopyFrom(value, allocator);
    return copy;
}

std::string make_preview_result(bool ok, const char* error = nullptr, const PreviewViewBinding* binding = nullptr) {
    rapidjson::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();

    document.AddMember("ok", ok, allocator);
    if (error) {
        document.AddMember("error", rapidjson::Value(error, allocator), allocator);
    }
    if (binding) {
        document.AddMember("id", binding->id, allocator);
        document.AddMember("canvasId", rapidjson::Value(binding->canvasId.c_str(), allocator), allocator);
        document.AddMember("renderWidth", binding->renderWidth, allocator);
        document.AddMember("renderHeight", binding->renderHeight, allocator);
    }

    return to_json(document);
}

bool show_editor_binding(EditorViewBinding& binding) {
    binding.previewAssets.reset();
    mark_preview_dirty(binding);
    return mg::setScreen(binding.contextId, std::make_unique<EditorPreviewScreen>(&binding));
}

const char* create_preview_view_impl(const char* canvasId, const char* gaugePath) {
    if (!canvasId || !canvasId[0] || !gaugePath || !gaugePath[0]) {
        g_ret = R"({"ok":false,"error":"BadArgs"})";
        return g_ret.c_str();
    }

    auto binding = std::make_unique<PreviewViewBinding>();
    binding->id = g_nextPreviewId++;
    binding->canvasId = canvasId;
    binding->gaugePath = gaugePath;
    binding->ownedContext = std::make_unique<GraphicsContextCanvas>(binding->canvasId);
    binding->context = binding->ownedContext.get();

    if (!binding->context->init()) {
        g_ret = R"({"ok":false,"error":"CanvasInitFailed"})";
        return g_ret.c_str();
    }

    binding->context->resize(binding->renderWidth, binding->renderHeight);
    binding->contextId = mg::addContext(*binding->context);

    if (!mg::showGauge(binding->contextId, binding->gaugePath.c_str())) {
        mg::removeContext(binding->contextId);
        g_ret = R"({"ok":false,"error":"DocumentLoadFailed"})";
        return g_ret.c_str();
    }

    const PreviewViewBinding* resultBinding = binding.get();
    g_previewBindings.push_back(std::move(binding));
    g_ret = make_preview_result(true, nullptr, resultBinding);
    return g_ret.c_str();
}

const char* remove_preview_view_impl(std::uint32_t id) {
    for (auto it = g_previewBindings.begin(); it != g_previewBindings.end(); ++it) {
        auto& binding = *it;
        if (!binding || binding->id != id) continue;

        mg::removeContext(binding->contextId);
        g_previewBindings.erase(it);
        g_ret = make_preview_result(true);
        return g_ret.c_str();
    }

    g_ret = R"({"ok":false,"error":"ViewNotFound"})";
    return g_ret.c_str();
}

} // namespace

std::string to_json(const rapidjson::Value& value) {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    value.Accept(writer);
    return std::string(sb.GetString(), sb.GetSize());
}

std::string clone_json_value(const rapidjson::Value& value) {
    rapidjson::Document document;
    document.CopyFrom(value, document.GetAllocator());
    return to_json(document);
}

bool parse_json(const std::string& jsonText, rapidjson::Document& document) {
    document.Parse(jsonText.c_str(), jsonText.size());
    return !document.HasParseError();
}

bool parse_object_json(const std::string& jsonText, rapidjson::Document& document) {
    return parse_json(jsonText, document) && document.IsObject();
}

bool parse_array_json(const std::string& jsonText, rapidjson::Document& document) {
    return parse_json(jsonText, document) && document.IsArray();
}

const char* set_exception_result(const char* fallback) {
    g_ret = std::string(R"({"ok":false,"error":")") + (fallback ? fallback : "Exception") + R"("})";
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

Editor* active_editor() {
    EditorViewBinding* binding = active_binding();
    return binding ? binding->editor.get() : nullptr;
}

Editor::Id first_face_id(const Editor& editor) {
    if (editor.faceCount() == 0) return 0;
    return editor.idOf(editor.faceAt(0));
}

bool has_face_id(const Editor& editor, Editor::Id faceId) {
    return find_face(editor, faceId) != nullptr;
}

GaugeFace* find_face(Editor& editor, Editor::Id faceId) {
    for (std::size_t i = 0; i < editor.faceCount(); ++i) {
        GaugeFace* face = editor.faceAt(i);
        if (face && editor.idOf(face) == faceId) {
            return face;
        }
    }
    return nullptr;
}

const GaugeFace* find_face(const Editor& editor, Editor::Id faceId) {
    for (std::size_t i = 0; i < editor.faceCount(); ++i) {
        const GaugeFace* face = editor.faceAt(i);
        if (face && editor.idOf(face) == faceId) {
            return face;
        }
    }
    return nullptr;
}

bool sync_preview_face(EditorViewBinding& binding) {
    if (!binding.editor) {
        binding.previewFaceId = 0;
        return false;
    }

    if (binding.previewFaceId != 0 && has_face_id(*binding.editor, binding.previewFaceId)) {
        return true;
    }

    const Editor::Id nextFaceId = first_face_id(*binding.editor);
    const bool changed = nextFaceId != binding.previewFaceId;
    binding.previewFaceId = nextFaceId;
    if (changed) {
        binding.previewDirty = true;
        binding.previewAssets.reset();
    }

    return binding.previewFaceId != 0;
}

void mark_preview_dirty(EditorViewBinding& binding) {
    binding.previewDirty = true;
    binding.previewAssets.reset();
}

bool ensure_preview_ready(EditorViewBinding& binding) {
    if (!binding.editor || !sync_preview_face(binding)) return false;
    if (!binding.context) return false;

    mg::RuntimeContext* runtime = mg::getContext(binding.contextId);
    if (!runtime) return false;

    GaugeFace* face = find_face(*binding.editor, binding.previewFaceId);
    if (!face) return false;

    if (!binding.previewDirty && binding.previewAssets) {
        return true;
    }

    auto assets = std::make_unique<AssetManager>(FS(), runtime->getGraphicsContext());

    rapidjson::Document assetsDocument;
    if (parse_array_json(binding.assetsJson, assetsDocument)) {
        const rapidjson::Document& doc = assetsDocument;
        if (!assets->loadDocumentAssets(doc.GetArray())) {
            return false;
        }
    }

    if (!face->init(*assets)) {
        return false;
    }

    binding.previewAssets = std::move(assets);
    binding.previewDirty = false;
    return true;
}

bool ensure_preview_layout(EditorViewBinding& binding) {
    if (!ensure_preview_ready(binding)) return false;

    mg::RuntimeContext* runtime = mg::getContext(binding.contextId);
    if (!runtime || !binding.editor) return false;

    GaugeFace* face = find_face(*binding.editor, binding.previewFaceId);
    if (!face) return false;

    face->layout(runtime->getGraphics());
    return true;
}

std::string make_result(bool ok, const char* error, const EditorViewBinding* binding) {
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
        document.AddMember("previewFaceId", binding->previewFaceId, allocator);
        document.AddMember("active", binding->id == g_activeViewId, allocator);
    }
    if (g_activeViewId != 0) {
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
            document.AddMember(copy_json(it->name, allocator), copy_json(it->value, allocator), allocator);
        }
    } else if (!result.data.IsNull()) {
        document.AddMember("data", copy_json(result.data, allocator), allocator);
    }

    return to_json(document);
}

std::string make_editor_value_result(const EditorResult& result, const char* fallbackJson) {
    if (!result.ok) {
        return fallbackJson ? std::string(fallbackJson) : std::string("null");
    }
    return to_json(result.data);
}

bool load_source_document_parts(const char* gaugePath, std::string& outAssetsJson, std::string& outFacesJson) {
    outAssetsJson = "[]";
    outFacesJson = "[]";

    if (!gaugePath || !gaugePath[0]) {
        return true;
    }

    const std::string text = read_file_text(gaugePath);
    if (text.empty()) {
        return false;
    }

    rapidjson::Document document;
    if (!parse_json(text, document)) {
        return false;
    }

    if (document.IsArray()) {
        outFacesJson = to_json(document);
        return true;
    }

    if (!document.IsObject()) {
        return false;
    }

    const auto object = document.GetObject();
    auto assetsIt = object.FindMember("assets");
    if (assetsIt != object.MemberEnd() && assetsIt->value.IsArray()) {
        outAssetsJson = clone_json_value(assetsIt->value);
    }

    auto facesIt = object.FindMember("faces");
    if (facesIt != object.MemberEnd() && facesIt->value.IsArray()) {
        outFacesJson = clone_json_value(facesIt->value);
        return true;
    }

    auto rootIt = object.FindMember("root");
    if (rootIt != object.MemberEnd() && rootIt->value.IsObject()) {
        rapidjson::Document facesDocument;
        facesDocument.SetArray();
        facesDocument.PushBack(copy_json(rootIt->value, facesDocument.GetAllocator()), facesDocument.GetAllocator());
        outFacesJson = to_json(facesDocument);
        return true;
    }

    rapidjson::Document facesDocument;
    facesDocument.SetArray();
    facesDocument.PushBack(copy_json(document, facesDocument.GetAllocator()), facesDocument.GetAllocator());
    outFacesJson = to_json(facesDocument);
    return true;
}

std::string build_export_document_json(const EditorViewBinding& binding) {
    rapidjson::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();

    rapidjson::Document assetsDocument;
    if (!parse_array_json(binding.assetsJson, assetsDocument)) {
        assetsDocument.SetArray();
    }
    document.AddMember("assets", copy_json(assetsDocument, allocator), allocator);

    rapidjson::Document facesDocument;
    const std::string facesJson = binding.editor ? binding.editor->saveDocument() : std::string("[]");
    if (!parse_array_json(facesJson, facesDocument)) {
        facesDocument.SetArray();
    }
    document.AddMember("faces", copy_json(facesDocument, allocator), allocator);

    document.AddMember("previewFaceId", binding.previewFaceId, allocator);
    return to_json(document);
}

std::string build_export_face_json(const EditorViewBinding& binding) {
    if (!binding.editor) {
        return "{}";
    }

    const GaugeFace* face = find_face(*binding.editor, binding.previewFaceId);
    if (!face) {
        return "{}";
    }

    rapidjson::Document saved = face->save();
    return to_json(saved);
}

namespace {

std::string make_views_json() {
    rapidjson::Document document;
    document.SetArray();
    auto& allocator = document.GetAllocator();

    for (const auto& binding : g_bindings) {
        if (!binding) continue;

        rapidjson::Value view(rapidjson::kObjectType);
        view.AddMember("id", binding->id, allocator);
        view.AddMember("name", rapidjson::Value(binding->name.c_str(), allocator), allocator);
        view.AddMember("canvasId", rapidjson::Value(binding->canvasId.c_str(), allocator), allocator);
        view.AddMember("renderWidth", binding->renderWidth, allocator);
        view.AddMember("renderHeight", binding->renderHeight, allocator);
        view.AddMember("previewFaceId", binding->previewFaceId, allocator);
        view.AddMember("active", binding->id == g_activeViewId, allocator);
        document.PushBack(std::move(view), allocator);
    }

    return to_json(document);
}

const char* create_view_impl(const char* canvasId, const char* gaugePath, const char* name) {
    if (!canvasId || !canvasId[0]) {
        g_ret = R"({"ok":false,"error":"BadArgs"})";
        return g_ret.c_str();
    }

    auto binding = std::make_unique<EditorViewBinding>();
    binding->id = g_nextViewId++;
    binding->name = (name && name[0]) ? name : ("Document " + std::to_string(binding->id));
    binding->canvasId = canvasId;
    binding->ownedContext = std::make_unique<GraphicsContextCanvas>(binding->canvasId);
    binding->context = binding->ownedContext.get();

    if (!binding->context->init()) {
        g_ret = R"({"ok":false,"error":"CanvasInitFailed"})";
        return g_ret.c_str();
    }

    binding->context->resize(binding->renderWidth, binding->renderHeight);

    std::string facesJson;
    if (!load_source_document_parts(gaugePath, binding->assetsJson, facesJson)) {
        g_ret = R"({"ok":false,"error":"DocumentLoadFailed"})";
        return g_ret.c_str();
    }

    binding->editor = std::make_unique<Editor>();
    binding->editor->loadDocument(facesJson);
    sync_preview_face(*binding);

    binding->contextId = mg::addContext(*binding->context);
    if (!show_editor_binding(*binding)) {
        mg::removeContext(binding->contextId);
        g_ret = R"({"ok":false,"error":"ContextInitFailed"})";
        return g_ret.c_str();
    }

    const EditorViewBinding* resultBinding = binding.get();
    g_bindings.push_back(std::move(binding));
    g_activeViewId = resultBinding->id;
    g_ret = make_result(true, nullptr, resultBinding);
    return g_ret.c_str();
}

const char* remove_view_impl(std::uint32_t id) {
    for (auto it = g_bindings.begin(); it != g_bindings.end(); ++it) {
        auto& binding = *it;
        if (!binding || binding->id != id) continue;

        mg::removeContext(binding->contextId);

        const bool removedActive = (g_activeViewId == id);
        g_bindings.erase(it);

        if (removedActive) {
            g_activeViewId = g_bindings.empty() ? 0 : g_bindings.front()->id;
        }

        g_ret = make_result(true, nullptr, active_binding());
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
    binding->context->resize(width, height);
    mark_preview_dirty(*binding);
    g_ret = make_result(true, nullptr, binding);
    return g_ret.c_str();
}

const char* set_preview_face_impl(std::uint32_t faceId) {
    EditorViewBinding* binding = active_binding();
    if (!binding || !binding->editor) {
        g_ret = R"({"ok":false,"error":"ViewNotFound"})";
        return g_ret.c_str();
    }

    if (faceId != 0 && !has_face_id(*binding->editor, faceId)) {
        g_ret = R"({"ok":false,"error":"FaceNotFound"})";
        return g_ret.c_str();
    }

    binding->previewFaceId = faceId == 0 ? first_face_id(*binding->editor) : faceId;
    mark_preview_dirty(*binding);
    g_ret = make_result(true, nullptr, binding);
    return g_ret.c_str();
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
    mark_preview_dirty(*binding);
    g_ret = make_result(true, nullptr, binding);
    return g_ret.c_str();
}

const char* export_document_impl() {
    EditorViewBinding* binding = active_binding();
    if (!binding) {
        g_ret.clear();
        return g_ret.c_str();
    }

    g_ret = build_export_document_json(*binding);
    return g_ret.c_str();
}

const char* export_face_impl() {
    EditorViewBinding* binding = active_binding();
    if (!binding) {
        g_ret = "{}";
        return g_ret.c_str();
    }

    g_ret = build_export_face_json(*binding);
    return g_ret.c_str();
}

} // namespace

} // namespace mgweb

extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_create_view(const char* canvasId, const char* gaugePath, const char* name) {
    try {
        return mgweb::create_view_impl(canvasId, gaugePath, name);
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_list_views() {
    try {
        mgweb::g_ret = mgweb::make_views_json();
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_select_view(std::uint32_t id) {
    try {
        mgweb::EditorViewBinding* binding = mgweb::find_binding(id);
        if (!binding) {
            mgweb::g_ret = R"({"ok":false,"error":"ViewNotFound"})";
            return mgweb::g_ret.c_str();
        }

        mgweb::g_activeViewId = id;
        mgweb::g_ret = mgweb::make_result(true, nullptr, binding);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_remove_view(std::uint32_t id) {
    try {
        return mgweb::remove_view_impl(id);
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_set_view_render_size(std::uint32_t id, int width, int height) {
    try {
        return mgweb::set_view_render_size_impl(id, width, height);
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_set_preview_face(std::uint32_t faceId) {
    try {
        return mgweb::set_preview_face_impl(faceId);
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_get_document_assets() {
    try {
        return mgweb::get_document_assets_impl();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_set_document_assets(const char* jsonValue) {
    try {
        if (!jsonValue) {
            mgweb::g_ret = R"({"ok":false,"error":"BadArgs"})";
            return mgweb::g_ret.c_str();
        }
        return mgweb::set_document_assets_text_impl(jsonValue);
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_set_document_assets_from_file(const char* jsonPath) {
    try {
        const std::string text = mgweb::read_file_text(jsonPath);
        if (text.empty()) {
            mgweb::g_ret = R"({"ok":false,"error":"DocumentLoadFailed"})";
            return mgweb::g_ret.c_str();
        }
        return mgweb::set_document_assets_text_impl(text);
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_reload_view() {
    try {
        mgweb::EditorViewBinding* binding = mgweb::active_binding();
        if (!binding) {
            mgweb::g_ret = R"({"ok":false,"error":"ViewNotFound"})";
            return mgweb::g_ret.c_str();
        }

        mgweb::mark_preview_dirty(*binding);
        if (!mgweb::show_editor_binding(*binding)) {
            mgweb::g_ret = R"({"ok":false,"error":"ReloadFailed"})";
            return mgweb::g_ret.c_str();
        }

        mgweb::g_ret = mgweb::make_result(true, nullptr, binding);
        return mgweb::g_ret.c_str();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_export_document() {
    try {
        return mgweb::export_document_impl();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_editor_export_face() {
    try {
        return mgweb::export_face_impl();
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_preview_create_view(const char* canvasId, const char* gaugePath, const char* name) {
    (void)name;
    try {
        return mgweb::create_preview_view_impl(canvasId, gaugePath);
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

EMSCRIPTEN_KEEPALIVE
const char* mg_preview_remove_view(std::uint32_t id) {
    try {
        return mgweb::remove_preview_view_impl(id);
    } catch (const std::exception& error) {
        return mgweb::set_exception_result(error.what());
    } catch (...) {
        return mgweb::set_exception_result();
    }
}

} // extern "C"
