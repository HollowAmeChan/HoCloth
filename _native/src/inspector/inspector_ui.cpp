#include "hocloth/inspector/inspector_ui.hpp"

#include <array>

#include "imgui.h"
#include "imgui_internal.h"

namespace hocloth::inspector {
namespace {

enum class DemoComponentKind {
    MagicaCloth,
    SphereCollider,
    CapsuleCollider,
    CacheOutput,
};

struct DemoComponentEntry {
    const char* label;
    DemoComponentKind kind;
};

struct DemoObjectEntry {
    const char* object_name;
    bool participates_in_physics;
    std::array<int, 3> component_indices;
    int component_count;
};

struct DemoInspectorState {
    int selected_object = 0;
    int selected_component = 0;
    bool auto_rebuild = true;
    bool live_sync = false;
    bool show_advanced = false;

    int build_target = 0;
    int simulation_mode = 1;
    int preset_profile = 7;
    int collider_mode = 0;
    int mc2_attribute = 0;
    int selected_joint_override = 0;

    bool use_radius_curve = true;
    bool use_damping_curve = true;
    bool collider_enabled = true;
    bool angle_restoration = true;
    bool angle_limit_enabled = true;
    bool prebuild_enabled = false;
    bool gizmo_show_particles = true;
    bool gizmo_show_radius = true;
    bool gizmo_show_colliders = true;

    float radius_value = 0.032f;
    float damping_value = 0.180f;
    float gravity_strength = 0.900f;
    float distance_stiffness = 0.700f;
    float compression_limit = 0.350f;
    float spring_power = 0.100f;
    float collider_friction = 0.050f;
    float collider_limit_distance = 0.020f;
    float inertia_world = 0.900f;
    float inertia_smoothing = 0.150f;
    float angle_limit = 45.0f;
    float joint_override_radius = 0.020f;
    float joint_override_stiffness = 0.242f;
    float joint_override_damping = 0.300f;
    float joint_override_drag = 0.600f;
    float joint_override_gravity_scale = 1.000f;
};

constexpr std::array<DemoComponentEntry, 5> kComponents = {{
    {"MagicaCloth", DemoComponentKind::MagicaCloth},
    {"MagicaSphereCollider", DemoComponentKind::SphereCollider},
    {"MagicaCloth", DemoComponentKind::MagicaCloth},
    {"MagicaCapsuleCollider", DemoComponentKind::CapsuleCollider},
    {"Cache Output", DemoComponentKind::CacheOutput},
}};

constexpr std::array<DemoObjectEntry, 4> kObjects = {{
    {"HairFrontRig", true, {0, 1, -1}, 2},
    {"CapeRig", true, {2, 3, 4}, 3},
    {"Body", false, {-1, -1, -1}, 0},
    {"HelperLocator", false, {-1, -1, -1}, 0},
}};

DemoInspectorState& GetDemoState()
{
    static DemoInspectorState state;
    return state;
}

const DemoComponentEntry& SelectedComponent(const DemoInspectorState& state)
{
    return kComponents[state.selected_component];
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
    ImGui::DockBuilderSplitNode(right_id, ImGuiDir_Down, 0.34f, &right_bottom_id, &right_id);

    ImGui::DockBuilderDockWindow("Objects", left_id);
    ImGui::DockBuilderDockWindow("Component Inspector", center_id);
    ImGui::DockBuilderDockWindow("Build", right_id);
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

void DrawToolbar(DemoInspectorState& state)
{
    static constexpr const char* build_targets[] = {"Selected Objects", "Active Armature", "Current Collection"};

    ImGui::PushItemWidth(-1.0f);
    ImGui::Combo("Build Target", &state.build_target, build_targets, IM_ARRAYSIZE(build_targets));
    ImGui::PopItemWidth();

    ImGui::Checkbox("Auto rebuild on apply", &state.auto_rebuild);
    ImGui::Checkbox("Live sync bridge", &state.live_sync);
    ImGui::Checkbox("Advanced blocks", &state.show_advanced);

    ImGui::Spacing();
    if (ImGui::Button("Pull From Blender", ImVec2(-1.0f, 0.0f))) {
    }
    if (ImGui::Button("Build Native Runtime", ImVec2(-1.0f, 0.0f))) {
    }
    if (ImGui::Button("Write Shared JSON", ImVec2(-1.0f, 0.0f))) {
    }
}

void DrawObjectTree(DemoInspectorState& state)
{
    ImGui::TextUnformatted("Physics Objects");
    ImGui::Separator();

    for (int object_index = 0; object_index < static_cast<int>(kObjects.size()); ++object_index) {
        const DemoObjectEntry& object_entry = kObjects[object_index];
        ImGuiTreeNodeFlags object_flags = ImGuiTreeNodeFlags_DefaultOpen;
        if (object_entry.component_count == 0) {
            object_flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (state.selected_object == object_index) {
            object_flags |= ImGuiTreeNodeFlags_Selected;
        }

        const bool opened = ImGui::TreeNodeEx(object_entry.object_name, object_flags);
        if (ImGui::IsItemClicked()) {
            state.selected_object = object_index;
            if (object_entry.component_count > 0) {
                state.selected_component = object_entry.component_indices[0];
            }
        }

        ImGui::SameLine();
        ImGui::TextDisabled(object_entry.participates_in_physics ? "[Sim]" : "[Ignored]");

        if (opened) {
            for (int component_slot = 0; component_slot < object_entry.component_count; ++component_slot) {
                const int component_index = object_entry.component_indices[component_slot];
                if (component_index < 0) {
                    continue;
                }

                const DemoComponentEntry& component = kComponents[component_index];
                ImGui::PushID(component_index);
                const bool selected = (state.selected_component == component_index);
                if (ImGui::Selectable(component.label, selected)) {
                    state.selected_object = object_index;
                    state.selected_component = component_index;
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }
}

void DrawStatusBlock()
{
    ImGui::TextUnformatted("Runtime Construction");
    ImGui::Separator();
    ImGui::BulletText("Edit Mesh: vertex=148 edge=147 triangle=0");
    ImGui::BulletText("Skin Bone Count: 12");
    ImGui::BulletText("Transform Count: 12");
}

void DrawMainSection(DemoInspectorState& state)
{
    static constexpr const char* simulation_modes[] = {"BoneSpring", "BoneCloth", "MeshCloth"};
    static constexpr const char* presets[] = {
        "Accessory",
        "Cape",
        "FrontHair",
        "LongHair",
        "ShortHair",
        "Skirt",
        "SoftSkirt",
        "MiddleSpring",
        "SoftSpring",
        "HardSpring",
        "Tail",
    };
    static constexpr const char* root_bones[] = {"HairRoot_A", "HairRoot_B", "HairRoot_C"};

    if (ImGui::CollapsingHeader("Main", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(-1.0f);
        ImGui::Combo("Cloth Type", &state.simulation_mode, simulation_modes, IM_ARRAYSIZE(simulation_modes));
        ImGui::Combo("Preset", &state.preset_profile, presets, IM_ARRAYSIZE(presets));
        ImGui::PopItemWidth();

        ImGui::SeparatorText("Root Bones");
        for (const char* root_bone : root_bones) {
            ImGui::BulletText("%s", root_bone);
        }

        ImGui::SeparatorText("Center");
        ImGui::BulletText("Center Source: None");
    }
}

void DrawParametersSection(DemoInspectorState& state)
{
    static constexpr const char* collider_modes[] = {"Point", "Edge"};
    static constexpr const char* mc2_attributes[] = {"Default", "Move", "Fixed", "Disable Collision", "Invalid"};
    static constexpr const char* collider_refs[] = {"Head", "Shoulder_L", "Shoulder_R"};
    static constexpr const char* joint_names[] = {"Hair_01", "Hair_02", "Hair_03", "Hair_04", "Hair_05"};

    if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Use radius curve", &state.use_radius_curve);
        ImGui::SliderFloat("Radius", &state.radius_value, 0.001f, 0.100f, "%.3f");
        ImGui::Checkbox("Use damping curve", &state.use_damping_curve);
        ImGui::SliderFloat("Damping", &state.damping_value, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Gravity", &state.gravity_strength, 0.0f, 2.0f, "%.3f");
        ImGui::SliderFloat("Distance Stiffness", &state.distance_stiffness, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Tether Compression", &state.compression_limit, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Spring Power", &state.spring_power, 0.0f, 1.0f, "%.3f");

        ImGui::SeparatorText("Collider Collision");
        ImGui::Checkbox("Enabled", &state.collider_enabled);
        ImGui::Combo("Mode", &state.collider_mode, collider_modes, IM_ARRAYSIZE(collider_modes));
        ImGui::SliderFloat("Friction", &state.collider_friction, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Limit Distance", &state.collider_limit_distance, 0.0f, 0.25f, "%.3f");
        for (const char* collider_ref : collider_refs) {
            ImGui::BulletText("%s", collider_ref);
        }

        ImGui::SeparatorText("Inertia");
        ImGui::SliderFloat("World Inertia", &state.inertia_world, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Inertia Smoothing", &state.inertia_smoothing, 0.0f, 1.0f, "%.3f");

        ImGui::SeparatorText("Angle");
        ImGui::Checkbox("Angle Restoration", &state.angle_restoration);
        ImGui::Checkbox("Angle Limit", &state.angle_limit_enabled);
        ImGui::SliderFloat("Limit Angle", &state.angle_limit, 0.0f, 180.0f, "%.1f deg");

        ImGui::SeparatorText("Joint Overrides");
        if (ImGui::BeginListBox("Bones", ImVec2(-1.0f, 96.0f))) {
            for (int index = 0; index < IM_ARRAYSIZE(joint_names); ++index) {
                const bool selected = (state.selected_joint_override == index);
                if (ImGui::Selectable(joint_names[index], selected)) {
                    state.selected_joint_override = index;
                }
            }
            ImGui::EndListBox();
        }
        ImGui::Combo("MC2 Attribute", &state.mc2_attribute, mc2_attributes, IM_ARRAYSIZE(mc2_attributes));
        ImGui::SliderFloat("Joint Radius", &state.joint_override_radius, 0.001f, 0.100f, "%.3f");
        ImGui::SliderFloat("Joint Stiffness", &state.joint_override_stiffness, 0.0f, 2.0f, "%.3f");
        ImGui::SliderFloat("Joint Damping", &state.joint_override_damping, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Joint Drag", &state.joint_override_drag, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Gravity Scale", &state.joint_override_gravity_scale, 0.0f, 2.0f, "%.3f");
    }
}

void DrawGizmoSection(DemoInspectorState& state)
{
    if (ImGui::CollapsingHeader("Gizmos", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show Particles", &state.gizmo_show_particles);
        ImGui::Checkbox("Show Radius", &state.gizmo_show_radius);
        ImGui::Checkbox("Show Colliders", &state.gizmo_show_colliders);
    }
}

void DrawPreBuildSection(DemoInspectorState& state)
{
    if (ImGui::CollapsingHeader("Pre-Build", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Use Pre-Build Data", &state.prebuild_enabled);
        ImGui::TextWrapped("MC2 prebuild/export flow will be attached here. This area becomes the bridge between Blender-selected objects and native-side build artifacts.");
        ImGui::BeginDisabled();
        ImGui::Button("Create Pre-Build Data", ImVec2(-1.0f, 0.0f));
        ImGui::Button("Open Build Asset", ImVec2(-1.0f, 0.0f));
        ImGui::EndDisabled();
    }
}

void DrawMagicaClothInspector(DemoInspectorState& state)
{
    DrawStatusBlock();
    ImGui::Spacing();
    DrawMainSection(state);
    DrawParametersSection(state);
    DrawGizmoSection(state);
    DrawPreBuildSection(state);
}

void DrawSphereColliderInspector()
{
    static float radius = 0.120f;

    DrawStatusBlock();
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Main", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextUnformatted("Object");
        ImGui::BulletText("HeadCollider");
        ImGui::SliderFloat("Radius", &radius, 0.001f, 0.5f, "%.3f");
    }
}

void DrawCapsuleColliderInspector()
{
    static bool radius_separation = false;
    static float start_radius = 0.120f;
    static float end_radius = 0.090f;
    static float length = 0.220f;
    static int direction = 1;
    static constexpr const char* directions[] = {"X", "Y", "Z"};

    DrawStatusBlock();
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Main", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextUnformatted("Object");
        ImGui::BulletText("CapeCollider");
        ImGui::Combo("Direction", &direction, directions, IM_ARRAYSIZE(directions));
        ImGui::Checkbox("Radius Separation", &radius_separation);
        ImGui::SliderFloat(radius_separation ? "Start Radius" : "Radius", &start_radius, 0.001f, 0.5f, "%.3f");
        if (radius_separation) {
            ImGui::SliderFloat("End Radius", &end_radius, 0.001f, 0.5f, "%.3f");
        }
        ImGui::SliderFloat("Length", &length, 0.0f, 2.0f, "%.3f");
    }
}

void DrawCacheOutputInspector()
{
    static int cache_format = 0;
    static constexpr const char* cache_formats[] = {"pc2", "mdd", "mc2_external"};

    if (ImGui::CollapsingHeader("Main", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Source Object: CapeMesh");
        ImGui::Combo("Cache Format", &cache_format, cache_formats, IM_ARRAYSIZE(cache_formats));
        ImGui::TextWrapped("Path: build/cache/cape.mc2cache");
    }
}

void DrawComponentInspector(DemoInspectorState& state)
{
    const DemoObjectEntry& object_entry = kObjects[state.selected_object];
    const DemoComponentEntry& component_entry = SelectedComponent(state);

    ImGui::Text("%s", object_entry.object_name);
    ImGui::SameLine();
    ImGui::TextDisabled(object_entry.participates_in_physics ? "[Participates]" : "[Ignored]");
    ImGui::SameLine();
    ImGui::TextDisabled("/");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", component_entry.label);
    ImGui::Separator();

    switch (component_entry.kind) {
    case DemoComponentKind::MagicaCloth:
        DrawMagicaClothInspector(state);
        break;
    case DemoComponentKind::SphereCollider:
        DrawSphereColliderInspector();
        break;
    case DemoComponentKind::CapsuleCollider:
        DrawCapsuleColliderInspector();
        break;
    case DemoComponentKind::CacheOutput:
        DrawCacheOutputInspector();
        break;
    }
}

void DrawBuildWindow(DemoInspectorState& state)
{
    ImGui::TextUnformatted("Build Actions");
    ImGui::Separator();
    DrawToolbar(state);
}

void DrawRuntimeWindow()
{
    ImGui::TextUnformatted("Runtime");
    ImGui::Separator();
    ImGui::TextWrapped("This panel will host runtime build status, native bridge status, and shared JSON synchronization feedback.");
    ImGui::Spacing();
    ImGui::BulletText("Inspector backend: Dear ImGui docking host");
    ImGui::BulletText("Bridge mode: Blender datablock + external shared data");
    ImGui::BulletText("Viewport gizmos: build-result only");
}

bool BeginDockspaceHost()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);

    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool visible = ImGui::Begin("HoCloth Inspector", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    if (visible) {
        ImGuiID dockspace_id = ImGui::GetID("HoClothDockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        DockBuilderResetLayout();
    }
    ImGui::End();
    return visible;
}

}  // namespace

InspectorStatus GetInspectorStatus()
{
    return InspectorStatus{
        true,
        "imgui-docking-win32-dx11",
        "Docking HoCloth inspector shell is available; physics objects and component inspector panels are dockable.",
    };
}

bool DrawInspectorUi()
{
    DemoInspectorState& state = GetDemoState();
    const bool host_visible = BeginDockspaceHost();
    if (!host_visible) {
        return false;
    }

    ImGui::SetNextWindowDockID(ImGui::GetID("HoClothDockspace"), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Objects")) {
        DrawObjectTree(state);
    }
    ImGui::End();

    ImGui::SetNextWindowDockID(ImGui::GetID("HoClothDockspace"), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Component Inspector")) {
        DrawComponentInspector(state);
    }
    ImGui::End();

    ImGui::SetNextWindowDockID(ImGui::GetID("HoClothDockspace"), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Build")) {
        DrawBuildWindow(state);
    }
    ImGui::End();

    ImGui::SetNextWindowDockID(ImGui::GetID("HoClothDockspace"), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Runtime")) {
        DrawRuntimeWindow();
    }
    ImGui::End();
    return true;
}

}  // namespace hocloth::inspector
