#include "hocloth/inspector/inspector_ui.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"

namespace hocloth::inspector {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view kBridgeRoot = "inspector_bridge";
constexpr std::string_view kStateFile = "inspector_bridge/state.txt";
constexpr std::string_view kCommandsDir = "inspector_bridge/commands";
constexpr float kStatePollIntervalSeconds = 0.20f;

enum class ComponentKind {
    MagicaCloth,
    SphereCollider,
    CapsuleCollider,
    PlaneCollider,
    CacheOutput,
    Unknown,
};

struct CurveField {
    float value = 0.0f;
    bool use_curve = false;
    std::vector<std::pair<float, float>> points;
};

struct ComponentData {
    std::string type;
    ComponentKind kind = ComponentKind::Unknown;
    std::string id;
    std::string display_name;
    std::string object_name;
    bool enabled = true;
    std::map<std::string, std::string> fields;
    std::unordered_map<std::string, CurveField> curves;
    std::unordered_map<std::string, std::vector<std::string>> lists;
};

struct RuntimeData {
    std::string status;
    std::string backend;
    int step_count = 0;
    int transform_count = 0;
    bool apply_pose_on_step = true;
    float dt = 1.0f / 60.0f;
    int simulation_frequency = 60;
    bool live_running = false;
};

struct BridgeState {
    bool loaded = false;
    std::string scene_name;
    RuntimeData runtime;
    std::vector<ComponentData> components;
    std::unordered_map<std::string, int> component_index_by_id;
};

struct InspectorUiState {
    int selected_component_index = -1;
    bool show_advanced = false;
    float last_state_poll_time = -100.0f;
    BridgeState bridge;
};

InspectorUiState& GetUiState()
{
    static InspectorUiState state;
    return state;
}

std::string TrimCopy(std::string value)
{
    const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char c) { return !is_space(static_cast<unsigned char>(c)); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char c) { return !is_space(static_cast<unsigned char>(c)); }).base(), value.end());
    return value;
}

std::vector<std::string> Split(std::string_view text, char delimiter)
{
    std::vector<std::string> parts;
    std::string current;
    for (const char character : text) {
        if (character == delimiter) {
            parts.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(character);
    }
    parts.push_back(current);
    return parts;
}

std::string SanitizeFileToken(std::string_view value)
{
    std::string text(value);
    for (char& character : text) {
        if (!(std::isalnum(static_cast<unsigned char>(character)) != 0) && character != '_' && character != '-') {
            character = '_';
        }
    }
    return text;
}

std::string MakeWidgetLabel(const char* label, std::string_view unique_id)
{
    std::string result(label != nullptr ? label : "");
    result += "##";
    result += std::string(unique_id);
    return result;
}

bool ParseBool(std::string_view value)
{
    return value == "1" || value == "true" || value == "True" || value == "on" || value == "yes";
}

int ParseInt(std::string_view value, int fallback = 0)
{
    try {
        return std::stoi(std::string(value));
    } catch (...) {
        return fallback;
    }
}

float ParseFloat(std::string_view value, float fallback = 0.0f)
{
    try {
        return std::stof(std::string(value));
    } catch (...) {
        return fallback;
    }
}

bool EndsWith(std::string_view value, std::string_view suffix)
{
    return value.size() >= suffix.size()
        && value.substr(value.size() - suffix.size()) == suffix;
}

ComponentKind ParseComponentKind(std::string_view type)
{
    if (type == "MAGICA_CLOTH") {
        return ComponentKind::MagicaCloth;
    }
    if (type == "SPHERE_COLLIDER") {
        return ComponentKind::SphereCollider;
    }
    if (type == "CAPSULE_COLLIDER") {
        return ComponentKind::CapsuleCollider;
    }
    if (type == "PLANE_COLLIDER") {
        return ComponentKind::PlaneCollider;
    }
    if (type == "CACHE_OUTPUT") {
        return ComponentKind::CacheOutput;
    }
    return ComponentKind::Unknown;
}

const char* ComponentIconText(ComponentKind kind)
{
    switch (kind) {
    case ComponentKind::MagicaCloth:
        return "[Cloth]";
    case ComponentKind::SphereCollider:
        return "[Sphere]";
    case ComponentKind::CapsuleCollider:
        return "[Capsule]";
    case ComponentKind::PlaneCollider:
        return "[Plane]";
    case ComponentKind::CacheOutput:
        return "[Cache]";
    case ComponentKind::Unknown:
    default:
        return "[Comp]";
    }
}

std::string ComponentTitle(const ComponentData& component)
{
    if (!component.display_name.empty()) {
        return component.display_name;
    }
    return component.type;
}

std::string ComponentObjectLabel(const ComponentData& component)
{
    if (!component.object_name.empty()) {
        return component.object_name;
    }
    return "(unbound)";
}

std::string GetField(const ComponentData& component, const std::string& path)
{
    const auto it = component.fields.find(path);
    return it != component.fields.end() ? it->second : std::string();
}

float GetFieldFloat(const ComponentData& component, const std::string& path, float fallback = 0.0f)
{
    return ParseFloat(GetField(component, path), fallback);
}

bool GetFieldBool(const ComponentData& component, const std::string& path, bool fallback = false)
{
    const auto it = component.fields.find(path);
    return it != component.fields.end() ? ParseBool(it->second) : fallback;
}

CurveField GetCurveField(const ComponentData& component, const std::string& path)
{
    const auto it = component.curves.find(path);
    return it != component.curves.end() ? it->second : CurveField{};
}

std::vector<std::string> GetListField(const ComponentData& component, const std::string& path)
{
    const auto it = component.lists.find(path);
    return it != component.lists.end() ? it->second : std::vector<std::string>{};
}

std::string FormatBoolText(bool value)
{
    return value ? "1" : "0";
}

std::string FormatFloatText(float value)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.6f", value);
    return buffer;
}

void ApplyOptimisticComponentBool(ComponentData& component, std::string_view path, bool value)
{
    if (path == "enabled") {
        component.enabled = value;
        return;
    }
    if (EndsWith(path, ".use_curve")) {
        const std::string curve_path(path.substr(0, path.size() - std::string_view(".use_curve").size()));
        component.curves[curve_path].use_curve = value;
        return;
    }
    component.fields[std::string(path)] = FormatBoolText(value);
}

void ApplyOptimisticComponentFloat(ComponentData& component, std::string_view path, float value)
{
    if (EndsWith(path, ".value")) {
        const std::string curve_path(path.substr(0, path.size() - std::string_view(".value").size()));
        const auto found = component.curves.find(curve_path);
        if (found != component.curves.end()) {
            found->second.value = value;
            return;
        }
    }
    component.fields[std::string(path)] = FormatFloatText(value);
}

void ApplyOptimisticComponentEnum(ComponentData& component, std::string_view path, const std::string& value)
{
    component.fields[std::string(path)] = value;
}

void ApplyOptimisticSceneBool(RuntimeData& runtime, std::string_view path, bool value)
{
    if (path == "apply_pose_on_step") {
        runtime.apply_pose_on_step = value;
    }
}

void ApplyOptimisticSceneFloat(RuntimeData& runtime, std::string_view path, float value)
{
    if (path == "dt") {
        runtime.dt = value;
    }
}

void ApplyOptimisticSceneInt(RuntimeData& runtime, std::string_view path, int value)
{
    if (path == "simulation_frequency") {
        runtime.simulation_frequency = value;
    }
}

void EnsureCommandDirectory()
{
    std::error_code error;
    fs::create_directories(fs::path(kCommandsDir), error);
}

bool WriteCommandFile(const std::vector<std::string>& lines)
{
    EnsureCommandDirectory();
    const auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
    const std::string filename = "cmd_" + std::to_string(timestamp.count()) + ".txt";
    const fs::path command_path = fs::path(kCommandsDir) / filename;

    std::ofstream output(command_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    for (const std::string& line : lines) {
        output << line << '\n';
    }
    return true;
}

bool QueueSetCommand(std::string_view component_id, std::string_view path, std::string_view value)
{
    return WriteCommandFile({
        "set\t" + std::string(component_id) + "\t" + std::string(path) + "\t" + std::string(value),
    });
}

bool QueueSceneCommand(std::string_view path, std::string_view value)
{
    return WriteCommandFile({
        "scene\t" + std::string(path) + "\t" + std::string(value),
    });
}

bool QueueActionCommand(std::string_view action)
{
    return WriteCommandFile({
        "action\t" + std::string(action),
    });
}

bool QueueDeleteComponent(std::string_view component_id)
{
    return WriteCommandFile({
        "delete_component\t" + std::string(component_id),
    });
}

bool QueueCurvePointSet(
    std::string_view component_id,
    std::string_view path,
    int index,
    float time_value,
    float curve_value)
{
    char time_buffer[64];
    char value_buffer[64];
    std::snprintf(time_buffer, sizeof(time_buffer), "%.6f", time_value);
    std::snprintf(value_buffer, sizeof(value_buffer), "%.6f", curve_value);
    return WriteCommandFile({
        "curve_point_set\t" + std::string(component_id) + "\t" + std::string(path) + "\t"
            + std::to_string(index) + "\t" + time_buffer + "\t" + value_buffer,
    });
}

bool QueueCurvePointAdd(
    std::string_view component_id,
    std::string_view path,
    float time_value,
    float curve_value)
{
    char time_buffer[64];
    char value_buffer[64];
    std::snprintf(time_buffer, sizeof(time_buffer), "%.6f", time_value);
    std::snprintf(value_buffer, sizeof(value_buffer), "%.6f", curve_value);
    return WriteCommandFile({
        "curve_point_add\t" + std::string(component_id) + "\t" + std::string(path) + "\t"
            + time_buffer + "\t" + value_buffer,
    });
}

bool QueueCurvePointRemove(std::string_view component_id, std::string_view path, int index)
{
    return WriteCommandFile({
        "curve_point_remove\t" + std::string(component_id) + "\t" + std::string(path) + "\t" + std::to_string(index),
    });
}

std::optional<BridgeState> LoadBridgeStateFromDisk()
{
    std::ifstream input(fs::path(kStateFile), std::ios::binary);
    if (!input.is_open()) {
        return std::nullopt;
    }

    BridgeState state;
    ComponentData* current_component = nullptr;

    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> tokens = Split(line, '\t');
        if (tokens.empty()) {
            continue;
        }

        const std::string& record_type = tokens[0];
        if (record_type == "scene" && tokens.size() >= 3) {
            if (tokens[1] == "name") {
                state.scene_name = tokens[2];
            }
            continue;
        }

        if (record_type == "runtime" && tokens.size() >= 3) {
            const std::string& path = tokens[1];
            const std::string& value = tokens[2];
            if (path == "status") {
                state.runtime.status = value;
            } else if (path == "backend") {
                state.runtime.backend = value;
            } else if (path == "step_count") {
                state.runtime.step_count = ParseInt(value);
            } else if (path == "transform_count") {
                state.runtime.transform_count = ParseInt(value);
            } else if (path == "apply_pose_on_step") {
                state.runtime.apply_pose_on_step = ParseBool(value);
            } else if (path == "dt") {
                state.runtime.dt = ParseFloat(value, state.runtime.dt);
            } else if (path == "simulation_frequency") {
                state.runtime.simulation_frequency = ParseInt(value, state.runtime.simulation_frequency);
            } else if (path == "live_running") {
                state.runtime.live_running = ParseBool(value);
            }
            continue;
        }

        if (record_type == "component" && tokens.size() >= 6) {
            ComponentData component;
            component.type = tokens[1];
            component.kind = ParseComponentKind(component.type);
            component.id = tokens[2];
            component.display_name = tokens[3];
            component.object_name = tokens[4];
            component.enabled = ParseBool(tokens[5]);

            state.component_index_by_id[component.id] = static_cast<int>(state.components.size());
            state.components.push_back(std::move(component));
            current_component = &state.components.back();
            continue;
        }

        if (current_component == nullptr || tokens.size() < 4) {
            continue;
        }

        const std::string& component_id = tokens[1];
        if (component_id != current_component->id) {
            const auto found = state.component_index_by_id.find(component_id);
            if (found == state.component_index_by_id.end()) {
                current_component = nullptr;
                continue;
            }
            current_component = &state.components[found->second];
        }

        if (record_type == "curve" && tokens.size() >= 5) {
            current_component->curves[tokens[2]] = CurveField{
                ParseFloat(tokens[3]),
                ParseBool(tokens[4]),
            };
            continue;
        }

        if (record_type == "curve_point" && tokens.size() >= 6) {
            CurveField& curve = current_component->curves[tokens[2]];
            const int index = ParseInt(tokens[3], -1);
            if (index < 0) {
                continue;
            }
            if (static_cast<int>(curve.points.size()) <= index) {
                curve.points.resize(static_cast<std::size_t>(index) + 1);
            }
            curve.points[static_cast<std::size_t>(index)] = {
                ParseFloat(tokens[4]),
                ParseFloat(tokens[5]),
            };
            continue;
        }

        if (record_type == "list") {
            current_component->lists[tokens[2]] = Split(tokens[3], ';');
            continue;
        }

        current_component->fields[tokens[2]] = tokens[3];
    }

    state.loaded = true;
    return state;
}

void PollBridgeState(InspectorUiState& ui_state)
{
    const float now = static_cast<float>(ImGui::GetTime());
    if ((now - ui_state.last_state_poll_time) < kStatePollIntervalSeconds) {
        return;
    }

    ui_state.last_state_poll_time = now;
    if (const auto loaded = LoadBridgeStateFromDisk()) {
        ui_state.bridge = *loaded;
        if (ui_state.selected_component_index < 0 || ui_state.selected_component_index >= static_cast<int>(ui_state.bridge.components.size())) {
            ui_state.selected_component_index = ui_state.bridge.components.empty() ? -1 : 0;
        }
    }
}

void DockBuilderResetLayout()
{
    static bool initialized = false;
    if (initialized) {
        return;
    }

    initialized = true;
    ImGuiID dockspace_id = ImGui::GetID("HoClothDockspace");
    const ImVec2 dockspace_size = ImGui::GetContentRegionAvail();

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, dockspace_size);

    ImGuiID left_id = 0;
    ImGuiID right_id = 0;
    ImGuiID right_bottom_id = 0;
    ImGuiID center_id = dockspace_id;

    ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Left, 0.24f, &left_id, &center_id);
    ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Right, 0.28f, &right_id, &center_id);
    ImGui::DockBuilderSplitNode(right_id, ImGuiDir_Down, 0.36f, &right_bottom_id, &right_id);

    ImGui::DockBuilderDockWindow("Objects", left_id);
    ImGui::DockBuilderDockWindow("Component Inspector", center_id);
    ImGui::DockBuilderDockWindow("Runtime Actions", right_id);
    ImGui::DockBuilderDockWindow("Runtime", right_bottom_id);

    if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(left_id)) {
        node->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;
    }
    if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(center_id)) {
        node->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;
    }
    if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(right_id)) {
        node->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;
    }
    if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(right_bottom_id)) {
        node->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;
    }

    ImGui::DockBuilderFinish(dockspace_id);
}

bool BeginDockspaceHost()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);

    const ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool visible = ImGui::Begin("HoCloth Inspector", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    if (visible) {
        const ImGuiID dockspace_id = ImGui::GetID("HoClothDockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        DockBuilderResetLayout();
    }
    ImGui::End();
    return visible;
}

bool DrawBooleanField(
    const char* label,
    bool current_value,
    std::string_view component_id,
    std::string_view path,
    ComponentData* optimistic_component = nullptr)
{
    bool value = current_value;
    const std::string widget_label = MakeWidgetLabel(label, std::string(component_id) + ":" + std::string(path));
    if (ImGui::Checkbox(widget_label.c_str(), &value) && value != current_value) {
        if (QueueSetCommand(component_id, path, value ? "1" : "0")) {
            if (optimistic_component != nullptr) {
                ApplyOptimisticComponentBool(*optimistic_component, path, value);
            }
            return true;
        }
    }
    return false;
}

bool DrawSceneBooleanField(const char* label, bool current_value, std::string_view path, RuntimeData* optimistic_runtime = nullptr)
{
    bool value = current_value;
    const std::string widget_label = MakeWidgetLabel(label, std::string("scene:") + std::string(path));
    if (ImGui::Checkbox(widget_label.c_str(), &value) && value != current_value) {
        if (QueueSceneCommand(path, value ? "1" : "0")) {
            if (optimistic_runtime != nullptr) {
                ApplyOptimisticSceneBool(*optimistic_runtime, path, value);
            }
            return true;
        }
    }
    return false;
}

bool DrawFloatField(
    const char* label,
    float current_value,
    float min_value,
    float max_value,
    const char* format,
    std::string_view component_id,
    std::string_view path,
    ComponentData* optimistic_component = nullptr)
{
    float value = current_value;
    const std::string widget_label = MakeWidgetLabel(label, std::string(component_id) + ":" + std::string(path));
    if (ImGui::SliderFloat(widget_label.c_str(), &value, min_value, max_value, format)) {
        const std::string serialized = FormatFloatText(value);
        if (QueueSetCommand(component_id, path, serialized)) {
            if (optimistic_component != nullptr) {
                ApplyOptimisticComponentFloat(*optimistic_component, path, value);
            }
            return true;
        }
    }
    return false;
}

bool DrawSceneFloatField(
    const char* label,
    float current_value,
    float min_value,
    float max_value,
    const char* format,
    std::string_view path,
    RuntimeData* optimistic_runtime = nullptr)
{
    float value = current_value;
    const std::string widget_label = MakeWidgetLabel(label, std::string("scene:") + std::string(path));
    if (ImGui::SliderFloat(widget_label.c_str(), &value, min_value, max_value, format)) {
        const std::string serialized = FormatFloatText(value);
        if (QueueSceneCommand(path, serialized)) {
            if (optimistic_runtime != nullptr) {
                ApplyOptimisticSceneFloat(*optimistic_runtime, path, value);
            }
            return true;
        }
    }
    return false;
}

bool DrawSceneIntField(
    const char* label,
    int current_value,
    int min_value,
    int max_value,
    std::string_view path,
    RuntimeData* optimistic_runtime = nullptr)
{
    int value = current_value;
    const std::string widget_label = MakeWidgetLabel(label, std::string("scene:") + std::string(path));
    if (ImGui::SliderInt(widget_label.c_str(), &value, min_value, max_value)) {
        if (QueueSceneCommand(path, std::to_string(value))) {
            if (optimistic_runtime != nullptr) {
                ApplyOptimisticSceneInt(*optimistic_runtime, path, value);
            }
            return true;
        }
    }
    return false;
}

bool DrawEnumCombo(
    const char* label,
    const std::vector<std::string>& values,
    const std::string& current_value,
    std::string_view component_id,
    std::string_view path,
    ComponentData* optimistic_component = nullptr)
{
    int current_index = 0;
    for (int index = 0; index < static_cast<int>(values.size()); ++index) {
        if (values[index] == current_value) {
            current_index = index;
            break;
        }
    }

    const auto getter = [](void* data, int index) -> const char* {
        const auto* items = static_cast<const std::vector<std::string>*>(data);
        if (index < 0 || index >= static_cast<int>(items->size())) {
            return "";
        }
        return (*items)[index].c_str();
    };

    int edited_index = current_index;
    const std::string widget_label = MakeWidgetLabel(label, std::string(component_id) + ":" + std::string(path));
    if (ImGui::Combo(widget_label.c_str(), &edited_index, getter, const_cast<std::vector<std::string>*>(&values), static_cast<int>(values.size()))) {
        if (edited_index >= 0 && edited_index < static_cast<int>(values.size())) {
            if (QueueSetCommand(component_id, path, values[edited_index])) {
                if (optimistic_component != nullptr) {
                    ApplyOptimisticComponentEnum(*optimistic_component, path, values[edited_index]);
                }
                return true;
            }
        }
    }
    return false;
}

void DrawCurveBlock(const char* title, ComponentData& component, std::string_view path, float min_value, float max_value)
{
    CurveField& curve = component.curves[std::string(path)];
    const std::string tree_label = MakeWidgetLabel(title, std::string(component.id) + ":" + std::string(path));
    if (ImGui::TreeNodeEx(tree_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawBooleanField("Use Curve", curve.use_curve, component.id, std::string(path) + ".use_curve", &component);
        DrawFloatField("Value", curve.value, min_value, max_value, "%.3f", component.id, std::string(path) + ".value", &component);
        ImGui::Spacing();
        ImGui::TextDisabled("控制点");
        if (curve.points.empty()) {
            ImGui::TextDisabled("(空)");
        }
        for (int index = 0; index < static_cast<int>(curve.points.size()); ++index) {
            const std::string row_id = std::string(path) + "_point_" + std::to_string(index);
            ImGui::PushID(row_id.c_str());
            float time_value = curve.points[static_cast<std::size_t>(index)].first;
            float curve_value = curve.points[static_cast<std::size_t>(index)].second;
            if (ImGui::SliderFloat("位置", &time_value, 0.0f, 1.0f, "%.3f")) {
                if (QueueCurvePointSet(component.id, path, index, time_value, curve_value)) {
                    curve.points[static_cast<std::size_t>(index)].first = time_value;
                }
            }
            if (ImGui::SliderFloat("值", &curve_value, min_value, max_value, "%.3f")) {
                if (QueueCurvePointSet(component.id, path, index, time_value, curve_value)) {
                    curve.points[static_cast<std::size_t>(index)].second = curve_value;
                }
            }
            const bool can_remove = index > 0 && index < static_cast<int>(curve.points.size()) - 1;
            if (!can_remove) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("删除")) {
                if (QueueCurvePointRemove(component.id, path, index)) {
                    curve.points.erase(curve.points.begin() + index);
                    ImGui::PopID();
                    break;
                }
            }
            if (!can_remove) {
                ImGui::EndDisabled();
            }
            ImGui::Separator();
            ImGui::PopID();
        }
        if (ImGui::Button(("添加控制点##" + component.id + ":" + std::string(path)).c_str())) {
            float insert_time = 0.5f;
            float insert_value = curve.value;
            if (!curve.points.empty()) {
                insert_time = 0.5f;
                insert_value = curve.points.back().second;
            }
            if (QueueCurvePointAdd(component.id, path, insert_time, insert_value)) {
                curve.points.push_back({insert_time, insert_value});
            }
        }
        ImGui::TreePop();
    }
}

void DrawStringList(const char* label, const std::vector<std::string>& values)
{
    ImGui::SeparatorText(label);
    if (values.empty()) {
        ImGui::TextDisabled("(none)");
        return;
    }
    for (const std::string& value : values) {
        if (!TrimCopy(value).empty()) {
            ImGui::BulletText("%s", value.c_str());
        }
    }
}

void DrawBuildWindow(InspectorUiState& ui_state)
{
    ImGui::TextUnformatted("运行操作");
    ImGui::Separator();
    ImGui::TextDisabled("场景");
    ImGui::TextWrapped("%s", ui_state.bridge.scene_name.empty() ? "(unknown)" : ui_state.bridge.scene_name.c_str());
    ImGui::Spacing();
    ImGui::TextWrapped("这里负责组件参数编辑。粒子类 authoring 仍留在 Blender，Live 运行前会先从 Blender 重建。");
    ImGui::Spacing();

    if (ImGui::Button("从 Blender 刷新", ImVec2(-1.0f, 0.0f))) {
        ui_state.last_state_poll_time = -100.0f;
        PollBridgeState(ui_state);
    }
    if (ImGui::Button("构建 Runtime", ImVec2(-1.0f, 0.0f))) {
        QueueActionCommand("rebuild");
    }
    if (ImGui::Button(ui_state.bridge.runtime.live_running ? "停止 Live" : "启动 Live", ImVec2(-1.0f, 0.0f))) {
        QueueActionCommand("toggle_live");
    }

    ImGui::Spacing();
    DrawSceneFloatField("步进 dt", ui_state.bridge.runtime.dt, 0.001f, 0.100f, "%.4f", "dt", &ui_state.bridge.runtime);
    DrawSceneIntField("模拟频率", ui_state.bridge.runtime.simulation_frequency, 1, 240, "simulation_frequency", &ui_state.bridge.runtime);
    DrawSceneBooleanField("步进时应用姿态", ui_state.bridge.runtime.apply_pose_on_step, "apply_pose_on_step", &ui_state.bridge.runtime);
    ImGui::Checkbox("显示高级参数##scene_advanced_blocks", &ui_state.show_advanced);
}

void DrawRuntimeWindow(const InspectorUiState& ui_state)
{
    ImGui::TextUnformatted("运行状态");
    ImGui::Separator();
    ImGui::BulletText("后端: %s", ui_state.bridge.runtime.backend.empty() ? "unknown" : ui_state.bridge.runtime.backend.c_str());
    ImGui::BulletText("步数: %d", ui_state.bridge.runtime.step_count);
    ImGui::BulletText("变换数: %d", ui_state.bridge.runtime.transform_count);
    ImGui::BulletText("Live: %s", ui_state.bridge.runtime.live_running ? "running" : "stopped");
    ImGui::Spacing();
    ImGui::TextWrapped("%s", ui_state.bridge.runtime.status.empty() ? "等待运行状态..." : ui_state.bridge.runtime.status.c_str());
    ImGui::Spacing();
    ImGui::TextDisabled("Live 运行前会先从 Blender authoring 数据重建。");

    ImGui::Spacing();
    if (ImGui::Button("执行一步", ImVec2(-1.0f, 0.0f))) {
        QueueActionCommand("step");
    }
}

void DrawObjectTree(InspectorUiState& ui_state)
{
    ImGui::TextUnformatted("参与模拟对象");
    ImGui::Separator();

    if (!ui_state.bridge.loaded) {
        ImGui::TextDisabled("等待 bridge 状态...");
        return;
    }

    if (ui_state.bridge.components.empty()) {
        ImGui::TextDisabled("没有导出的组件。");
        return;
    }

    std::map<std::string, std::vector<int>> grouped_indices;
    for (int index = 0; index < static_cast<int>(ui_state.bridge.components.size()); ++index) {
        const ComponentData& component = ui_state.bridge.components[index];
        grouped_indices[ComponentObjectLabel(component)].push_back(index);
    }

    for (const auto& [object_name, indices] : grouped_indices) {
        const bool has_physics = !indices.empty();
        const ImGuiTreeNodeFlags object_flags =
            ImGuiTreeNodeFlags_DefaultOpen |
            (indices.size() <= 1 ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None);

        const bool opened = ImGui::TreeNodeEx(object_name.c_str(), object_flags);
        ImGui::SameLine();
        ImGui::TextDisabled(has_physics ? "[模拟]" : "[忽略]");

        if (opened) {
            for (const int index : indices) {
                const ComponentData& component = ui_state.bridge.components[index];
                ImGui::PushID(component.id.c_str());
                const bool selected = ui_state.selected_component_index == index;
                const std::string label = std::string(ComponentIconText(component.kind)) + " " + ComponentTitle(component);
                if (ImGui::Selectable(label.c_str(), selected)) {
                    ui_state.selected_component_index = index;
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }
}

void DrawClothInspector(InspectorUiState& ui_state, ComponentData& component)
{
    ImGui::TextDisabled("%s", component.type.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(ComponentObjectLabel(component).c_str());
    ImGui::Separator();

    DrawBooleanField("Enabled", component.enabled, component.id, "enabled", &component);

    if (ImGui::CollapsingHeader("基础", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawEnumCombo("类型", {"BONE_CLOTH", "BONE_SPRING"}, GetField(component, "authoring_mode"), component.id, "authoring_mode", &component);
        DrawEnumCombo(
            "预设",
            {
                "ACCESSORY",
                "CAPE",
                "FRONT_HAIR",
                "LONG_HAIR",
                "SHORT_HAIR",
                "SKIRT",
                "SOFT_SKIRT",
                "MIDDLE_SPRING",
                "SOFT_SPRING",
                "HARD_SPRING",
                "TAIL",
            },
            GetField(component, "preset_profile"),
            component.id,
            "preset_profile",
            &component);
        DrawEnumCombo("连接模式", {"Line", "AutomaticMesh", "SequentialLoopMesh", "SequentialNonLoopMesh"}, GetField(component, "bone_connection_mode"), component.id, "bone_connection_mode", &component);
        DrawEnumCombo("中心来源", {"NONE", "OBJECT", "BONE"}, GetField(component, "center_source"), component.id, "center_source", &component);
        DrawStringList("根骨骼", GetListField(component, "root_bones"));
        ImGui::TextDisabled("根骨骼绑定与粒子 authoring 仍在 Blender 侧。");
    }

    if (ImGui::CollapsingHeader("重力与基础参数", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawFloatField("重力", GetFieldFloat(component, "gravity_strength", 0.0f), 0.0f, 20.0f, "%.3f", component.id, "gravity_strength", &component);
        DrawCurveBlock("阻尼", component, "damping_curve", 0.0f, 1.0f);
        DrawCurveBlock("半径", component, "radius_curve", 0.0f, 0.100f);
    }

    if (ImGui::CollapsingHeader("约束", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawCurveBlock("距离刚性", component, "distance_constraint.stiffness", 0.0f, 1.0f);
        DrawFloatField(
            "Tether 压缩",
            GetFieldFloat(component, "tether_constraint.distance_compression", 0.0f),
            0.0f,
            1.0f,
            "%.3f",
            component.id,
            "tether_constraint.distance_compression",
            &component);
        DrawFloatField(
            "三角弯曲",
            GetFieldFloat(component, "triangle_bending_constraint.stiffness", 0.0f),
            0.0f,
            1.0f,
            "%.3f",
            component.id,
            "triangle_bending_constraint.stiffness",
            &component);
    }

    if (ImGui::CollapsingHeader("弹簧", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawBooleanField("启用弹簧", GetFieldBool(component, "spring_constraint.use_spring", false), component.id, "spring_constraint.use_spring", &component);
        DrawFloatField(
            "弹力",
            GetFieldFloat(component, "spring_constraint.spring_power", 0.0f),
            0.0f,
            1.0f,
            "%.3f",
            component.id,
            "spring_constraint.spring_power",
            &component);
        DrawFloatField("限制距离", GetFieldFloat(component, "spring_constraint.limit_distance", 0.0f), 0.0f, 0.5f, "%.3f", component.id, "spring_constraint.limit_distance", &component);
        DrawFloatField("法线限制比", GetFieldFloat(component, "spring_constraint.normal_limit_ratio", 0.0f), 0.0f, 1.0f, "%.3f", component.id, "spring_constraint.normal_limit_ratio", &component);
        DrawFloatField("弹簧噪声", GetFieldFloat(component, "spring_constraint.spring_noise", 0.0f), 0.0f, 1.0f, "%.3f", component.id, "spring_constraint.spring_noise", &component);
    }

    if (ImGui::CollapsingHeader("角度约束", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawBooleanField("启用角度恢复", GetFieldBool(component, "angle_restoration_constraint.use_angle_restoration", false), component.id, "angle_restoration_constraint.use_angle_restoration", &component);
        DrawCurveBlock("角度恢复刚性", component, "angle_restoration_constraint.stiffness", 0.0f, 1.0f);
        DrawFloatField("速度衰减", GetFieldFloat(component, "angle_restoration_constraint.velocity_attenuation", 0.0f), 0.0f, 1.0f, "%.3f", component.id, "angle_restoration_constraint.velocity_attenuation", &component);
        DrawFloatField("重力衰减", GetFieldFloat(component, "angle_restoration_constraint.gravity_falloff", 0.0f), 0.0f, 1.0f, "%.3f", component.id, "angle_restoration_constraint.gravity_falloff", &component);
        DrawBooleanField("启用角度限制", GetFieldBool(component, "angle_limit_constraint.use_angle_limit", false), component.id, "angle_limit_constraint.use_angle_limit", &component);
        DrawCurveBlock("角度限制", component, "angle_limit_constraint.limit_angle", 0.0f, 180.0f);
        DrawFloatField("角度限制刚性", GetFieldFloat(component, "angle_limit_constraint.stiffness", 0.0f), 0.0f, 1.0f, "%.3f", component.id, "angle_limit_constraint.stiffness", &component);
    }

    if (ImGui::CollapsingHeader("碰撞", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawBooleanField(
            "启用碰撞",
            GetFieldBool(component, "collider_collision_constraint.enabled", true),
            component.id,
            "collider_collision_constraint.enabled",
            &component);
        DrawEnumCombo(
            "模式",
            {"Point", "Edge"},
            GetField(component, "collider_collision_constraint.mode"),
            component.id,
            "collider_collision_constraint.mode",
            &component);
        DrawFloatField(
            "摩擦",
            GetFieldFloat(component, "collider_collision_constraint.friction", 0.0f),
            0.0f,
            0.5f,
            "%.3f",
            component.id,
            "collider_collision_constraint.friction",
            &component);
        DrawCurveBlock("碰撞半径补偿", component, "collider_collision_constraint.limit_distance", 0.0f, 0.250f);
        DrawStringList("碰撞体引用", GetListField(component, "collider_refs"));
    }

    if (!ui_state.show_advanced) {
        return;
    }

    if (ImGui::CollapsingHeader("惯性", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawFloatField("世界惯性", GetFieldFloat(component, "inertia_constraint.world_inertia", 0.0f), 0.0f, 1.0f, "%.3f", component.id, "inertia_constraint.world_inertia", &component);
        DrawFloatField("世界惯性平滑", GetFieldFloat(component, "inertia_constraint.movement_inertia_smoothing", 0.0f), 0.0f, 1.0f, "%.3f", component.id, "inertia_constraint.movement_inertia_smoothing", &component);
        DrawBooleanField("启用世界位移限速", GetFieldBool(component, "inertia_constraint.movement_speed_limit.use", false), component.id, "inertia_constraint.movement_speed_limit.use", &component);
        DrawFloatField("世界位移限速", GetFieldFloat(component, "inertia_constraint.movement_speed_limit.value", 0.0f), 0.0f, 20.0f, "%.3f", component.id, "inertia_constraint.movement_speed_limit.value", &component);
        DrawBooleanField("启用世界旋转限速", GetFieldBool(component, "inertia_constraint.rotation_speed_limit.use", false), component.id, "inertia_constraint.rotation_speed_limit.use", &component);
        DrawFloatField("世界旋转限速", GetFieldFloat(component, "inertia_constraint.rotation_speed_limit.value", 0.0f), 0.0f, 1440.0f, "%.3f", component.id, "inertia_constraint.rotation_speed_limit.value", &component);
        DrawFloatField("局部惯性", GetFieldFloat(component, "inertia_constraint.local_inertia", 0.0f), 0.0f, 1.0f, "%.3f", component.id, "inertia_constraint.local_inertia", &component);
        DrawBooleanField("启用局部位移限速", GetFieldBool(component, "inertia_constraint.local_movement_speed_limit.use", false), component.id, "inertia_constraint.local_movement_speed_limit.use", &component);
        DrawFloatField("局部位移限速", GetFieldFloat(component, "inertia_constraint.local_movement_speed_limit.value", 0.0f), 0.0f, 20.0f, "%.3f", component.id, "inertia_constraint.local_movement_speed_limit.value", &component);
        DrawBooleanField("启用局部旋转限速", GetFieldBool(component, "inertia_constraint.local_rotation_speed_limit.use", false), component.id, "inertia_constraint.local_rotation_speed_limit.use", &component);
        DrawFloatField("局部旋转限速", GetFieldFloat(component, "inertia_constraint.local_rotation_speed_limit.value", 0.0f), 0.0f, 1440.0f, "%.3f", component.id, "inertia_constraint.local_rotation_speed_limit.value", &component);
        DrawFloatField("深度惯性", GetFieldFloat(component, "inertia_constraint.depth_inertia", 0.0f), 0.0f, 1.0f, "%.3f", component.id, "inertia_constraint.depth_inertia", &component);
        DrawFloatField("离心加速度", GetFieldFloat(component, "inertia_constraint.centrifugal_acceleration", 0.0f), 0.0f, 1.0f, "%.3f", component.id, "inertia_constraint.centrifugal_acceleration", &component);
        DrawBooleanField("启用粒子限速", GetFieldBool(component, "inertia_constraint.particle_speed_limit.use", false), component.id, "inertia_constraint.particle_speed_limit.use", &component);
        DrawFloatField("粒子限速", GetFieldFloat(component, "inertia_constraint.particle_speed_limit.value", 0.0f), 0.0f, 20.0f, "%.3f", component.id, "inertia_constraint.particle_speed_limit.value", &component);
    }
}

void DrawColliderInspector(ComponentData& component)
{
    ImGui::TextDisabled("%s", component.type.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(ComponentObjectLabel(component).c_str());
    ImGui::Separator();

    DrawBooleanField("启用", component.enabled, component.id, "enabled", &component);
    DrawEnumCombo("类型", {"SPHERE", "CAPSULE", "PLANE"}, GetField(component, "collider_type"), component.id, "collider_type", &component);
    DrawFloatField("半径", GetFieldFloat(component, "radius", 0.05f), 0.001f, 0.500f, "%.3f", component.id, "radius", &component);

    if (component.kind == ComponentKind::CapsuleCollider) {
        DrawBooleanField("半径分离", GetFieldBool(component, "radius_separation", false), component.id, "radius_separation", &component);
        DrawFloatField("末端半径", GetFieldFloat(component, "end_radius", 0.05f), 0.001f, 0.500f, "%.3f", component.id, "end_radius", &component);
        DrawFloatField("长度", GetFieldFloat(component, "length", 0.1f), 0.001f, 5.000f, "%.3f", component.id, "length", &component);
        DrawEnumCombo("方向", {"X", "Y", "Z"}, GetField(component, "direction"), component.id, "direction", &component);
    }
}

void DrawCacheInspector(ComponentData& component)
{
    ImGui::TextDisabled("%s", component.type.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(ComponentObjectLabel(component).c_str());
    ImGui::Separator();
    DrawBooleanField("启用", component.enabled, component.id, "enabled", &component);
    ImGui::BulletText("格式: %s", GetField(component, "cache_format").c_str());
    ImGui::TextWrapped("路径: %s", GetField(component, "cache_path").c_str());
}

void DrawComponentInspector(InspectorUiState& ui_state)
{
    if (!ui_state.bridge.loaded) {
        ImGui::TextDisabled("等待 bridge 状态...");
        return;
    }

    if (ui_state.selected_component_index < 0 || ui_state.selected_component_index >= static_cast<int>(ui_state.bridge.components.size())) {
        ImGui::TextDisabled("请先在左侧选择一个组件。");
        return;
    }

    ComponentData& component = ui_state.bridge.components[ui_state.selected_component_index];
    ImGui::Text("%s", ComponentTitle(component).c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("/ %s", component.id.c_str());
    ImGui::Separator();
    ImGui::TextWrapped("这里负责组件参数。绑定、顶点组和 bone 自定义属性 authoring 仍留在 Blender。");
    ImGui::Spacing();
    if (ImGui::Button(("删除组件##" + component.id).c_str(), ImVec2(-1.0f, 0.0f))) {
        if (QueueDeleteComponent(component.id)) {
            ui_state.selected_component_index = -1;
            ui_state.last_state_poll_time = -100.0f;
        }
    }
    ImGui::Spacing();

    switch (component.kind) {
    case ComponentKind::MagicaCloth:
        DrawClothInspector(ui_state, component);
        break;
    case ComponentKind::SphereCollider:
    case ComponentKind::CapsuleCollider:
    case ComponentKind::PlaneCollider:
        DrawColliderInspector(component);
        break;
    case ComponentKind::CacheOutput:
        DrawCacheInspector(component);
        break;
    case ComponentKind::Unknown:
    default:
        ImGui::TextWrapped("Unsupported component type: %s", component.type.c_str());
        break;
    }
}

}  // namespace

InspectorStatus GetInspectorStatus()
{
    return InspectorStatus{
        true,
        "imgui-docking-win32-dx11",
        "Bridge-backed HoCloth inspector is available. Blender remains the authoring source; inspector edits component parameters and runtime actions.",
    };
}

bool DrawInspectorUi()
{
    InspectorUiState& ui_state = GetUiState();
    PollBridgeState(ui_state);

    const bool host_visible = BeginDockspaceHost();
    if (!host_visible) {
        return false;
    }

    ImGui::SetNextWindowDockID(ImGui::GetID("HoClothDockspace"), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("对象")) {
        DrawObjectTree(ui_state);
    }
    ImGui::End();

    ImGui::SetNextWindowDockID(ImGui::GetID("HoClothDockspace"), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("组件检查器")) {
        DrawComponentInspector(ui_state);
    }
    ImGui::End();

    ImGui::SetNextWindowDockID(ImGui::GetID("HoClothDockspace"), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("运行操作")) {
        DrawBuildWindow(ui_state);
    }
    ImGui::End();

    ImGui::SetNextWindowDockID(ImGui::GetID("HoClothDockspace"), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("运行状态")) {
        DrawRuntimeWindow(ui_state);
    }
    ImGui::End();

    return true;
}

}  // namespace hocloth::inspector
