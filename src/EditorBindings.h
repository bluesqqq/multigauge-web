#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <rapidjson/document.h>

#include <multigauge/App.h>
#include <multigauge/AssetManager.h>
#include <multigauge/editor/Editor.h>
#include <multigauge/gauge/GaugeFace.h>

class GraphicsContextCanvas;

namespace mgweb {

struct EditorViewBinding {
    std::uint32_t id = 0;
    std::string name;
    std::string canvasId;
    std::string assetsJson = "[]";
    int renderWidth = 240;
    int renderHeight = 240;
    GraphicsContextCanvas* context = nullptr;
    std::unique_ptr<GraphicsContextCanvas> ownedContext;
    mg::ContextId contextId = 0;
    std::unique_ptr<Editor> editor;
    Editor::Id previewFaceId = 0;
    bool previewDirty = true;
    std::unique_ptr<AssetManager> previewAssets;
    std::vector<std::unique_ptr<GaugeFace>> externalFaces;
};

extern std::vector<std::unique_ptr<EditorViewBinding>> g_bindings;
extern std::uint32_t g_nextViewId;
extern std::uint32_t g_activeViewId;
extern std::string g_ret;

std::string to_json(const rapidjson::Value& value);
std::string clone_json_value(const rapidjson::Value& value);
bool parse_json(const std::string& jsonText, rapidjson::Document& document);
bool parse_object_json(const std::string& jsonText, rapidjson::Document& document);
bool parse_array_json(const std::string& jsonText, rapidjson::Document& document);

const char* set_exception_result(const char* fallback = "Exception");

EditorViewBinding* find_binding(std::uint32_t id);
EditorViewBinding* active_binding();
Editor* active_editor();

Editor::Id first_face_id(const Editor& editor);
bool has_face_id(const Editor& editor, Editor::Id faceId);
GaugeFace* find_face(Editor& editor, Editor::Id faceId);
const GaugeFace* find_face(const Editor& editor, Editor::Id faceId);

bool sync_preview_face(EditorViewBinding& binding);
void mark_preview_dirty(EditorViewBinding& binding);
bool ensure_preview_ready(EditorViewBinding& binding);
bool ensure_preview_layout(EditorViewBinding& binding);

std::string make_result(bool ok, const char* error = nullptr, const EditorViewBinding* binding = nullptr);
std::string make_editor_action_result(const EditorResult& result);
std::string make_editor_value_result(const EditorResult& result, const char* fallbackJson);

bool load_source_document_parts(const char* gaugePath, std::string& outAssetsJson, std::string& outFacesJson);
std::string build_export_document_json(const EditorViewBinding& binding);
std::string build_export_face_json(const EditorViewBinding& binding);

} // namespace mgweb
