#include "hocloth/cloth/constraints/angle_constraint.hpp"

#include "hocloth/core/define/system_define.hpp"
#include "hocloth/manager/simulation/simulation_manager.hpp"
#include "hocloth/manager/team/team_manager.hpp"
#include "hocloth/manager/virtual_mesh/virtual_mesh_manager.hpp"
#include "hocloth/utility/data/data_utility.hpp"
#include "hocloth/utility/math/math_extensions.hpp"
#include "hocloth/utility/math/math_utility.hpp"

#include <cmath>
#include <cstddef>

namespace hocloth::mc2 {

namespace {

float Radians(float degrees)
{
    constexpr float pi = 3.14159265358979323846f;
    return degrees * pi / 180.0f;
}

}  // namespace

void AngleConstraint::Dispose()
{
    length_buffer_.Dispose();
    local_pos_buffer_.Dispose();
    local_rot_buffer_.Dispose();
    rotation_buffer_.Dispose();
    restoration_vector_buffer_.Dispose();
}

void AngleConstraint::WorkBufferUpdate(int particle_count)
{
    length_buffer_.Dispose();
    local_pos_buffer_.Dispose();
    local_rot_buffer_.Dispose();
    rotation_buffer_.Dispose();
    restoration_vector_buffer_.Dispose();
    length_buffer_ = ExNativeArray<float>(particle_count);
    local_pos_buffer_ = ExNativeArray<float3>(particle_count);
    local_rot_buffer_ = ExNativeArray<quaternion>(particle_count);
    rotation_buffer_ = ExNativeArray<quaternion>(particle_count);
    restoration_vector_buffer_ = ExNativeArray<float3>(particle_count);
    length_buffer_.AddRange(particle_count, 0.0f);
    local_pos_buffer_.AddRange(particle_count, float3{});
    local_rot_buffer_.AddRange(particle_count, quaternion{});
    rotation_buffer_.AddRange(particle_count, quaternion{});
    restoration_vector_buffer_.AddRange(particle_count, float3{});
}

void AngleConstraint::Solve(
    const float4& simulation_power,
    const TeamManager& team_manager,
    const VirtualMeshManager& virtual_mesh_manager,
    SimulationManager& simulation_manager
)
{
    // Ported from Magica Cloth 2: Scripts/Core/Cloth/Constraints/AngleConstraint.cs
    const auto& baseline_steps = simulation_manager.ProcessingStepBaseLines();
    if (baseline_steps.Count() <= 0) {
        return;
    }
    if (length_buffer_.Length() < simulation_manager.ParticleCount()) {
        WorkBufferUpdate(simulation_manager.ParticleCount());
    }

    const auto& baseline_buffer = baseline_steps.Buffer();
    const auto& attributes = virtual_mesh_manager.Attributes();
    const auto& vertex_depths = virtual_mesh_manager.VertexDepths();
    const auto& vertex_parent_indices = virtual_mesh_manager.VertexParentIndices();
    const auto& baseline_start_indices = virtual_mesh_manager.BaseLineStartDataIndices();
    const auto& baseline_data_counts = virtual_mesh_manager.BaseLineDataCounts();
    const auto& baseline_data = virtual_mesh_manager.BaseLineData();
    auto& next_positions = simulation_manager.NextPositions();
    auto& velocity_positions = simulation_manager.VelocityPositions();
    const auto& frictions = simulation_manager.Frictions();
    const auto& step_basic_positions = simulation_manager.StepBasicPositions();
    const auto& step_basic_rotations = simulation_manager.StepBasicRotations();

    for (int step_index = 0; step_index < baseline_steps.Count(); ++step_index) {
        const std::uint32_t pack = static_cast<std::uint32_t>(
            baseline_buffer[static_cast<std::size_t>(step_index)]
        );
        const int team_id = data::Unpack32Hi(pack);
        const int baseline_index = data::Unpack32Low(pack);
        if (!team_manager.IsValidTeam(team_id)) {
            continue;
        }

        const TeamManager::TeamData& team_data = team_manager.GetTeamData(team_id);
        const ClothParameters& cloth_parameters = team_manager.GetParameters(team_id);
        const AngleConstraintParams& angle_parameters = cloth_parameters.angle_constraint;
        if (!angle_parameters.use_angle_limit && !angle_parameters.use_angle_restoration) {
            continue;
        }

        if (baseline_index < 0
            || baseline_index >= baseline_start_indices.Length()
            || baseline_index >= baseline_data_counts.Length()) {
            continue;
        }

        const int data_start = team_data.baseline_data_chunk.start_index;
        const int particle_start = team_data.particle_chunk.start_index;
        const int vertex_start = team_data.proxy_common_chunk.start_index;
        const int start = baseline_start_indices[baseline_index];
        const int data_count = baseline_data_counts[baseline_index];
        if (data_count <= 0) {
            continue;
        }

        const bool use_angle_limit = angle_parameters.use_angle_limit != 0;
        const bool use_angle_restoration = angle_parameters.use_angle_restoration != 0;
        const float limit_stiffness = angle_parameters.limit_stiffness;
        const float restoration_attenuation =
            angle_parameters.restoration_velocity_attenuation;
        const float gravity_falloff = (1.0f - angle_parameters.restoration_gravity_falloff)
            + (1.0f - (1.0f - angle_parameters.restoration_gravity_falloff))
                * team_data.gravity_dot;

        int data_index = start + data_start;
        for (int index = 0; index < data_count; ++index, ++data_index) {
            if (data_index < 0 || data_index >= baseline_data.Length()) {
                continue;
            }
            const int local_index = baseline_data[data_index];
            const int particle_index = particle_start + local_index;
            const int vertex_index = vertex_start + local_index;
            if (particle_index < 0
                || particle_index >= next_positions.Length()
                || particle_index >= step_basic_positions.Length()
                || particle_index >= step_basic_rotations.Length()
                || vertex_index < 0
                || vertex_index >= vertex_parent_indices.Length()) {
                continue;
            }

            const float3 basic_position = step_basic_positions[particle_index];
            const quaternion basic_rotation = step_basic_rotations[particle_index];
            rotation_buffer_[particle_index] = basic_rotation;

            if (index <= 0) {
                continue;
            }

            const int parent_local_index = vertex_parent_indices[vertex_index];
            const int parent_particle_index = parent_local_index + particle_start;
            if (parent_particle_index < 0
                || parent_particle_index >= step_basic_positions.Length()
                || parent_particle_index >= step_basic_rotations.Length()) {
                continue;
            }

            const float3 parent_basic_position = step_basic_positions[parent_particle_index];
            const quaternion parent_basic_rotation = step_basic_rotations[parent_particle_index];
            if (use_angle_limit) {
                const float current_length =
                    Distance(next_positions[particle_index], next_positions[parent_particle_index]);
                const float3 basic_vector = Subtract(basic_position, parent_basic_position);
                const float3 normalized_basic_vector = Normalize(basic_vector);
                const quaternion inverse_parent_rotation = Inverse(parent_basic_rotation);
                length_buffer_[particle_index] = current_length;
                local_pos_buffer_[particle_index] =
                    Rotate(inverse_parent_rotation, normalized_basic_vector);
                local_rot_buffer_[particle_index] =
                    Multiply(inverse_parent_rotation, basic_rotation);
            }

            if (use_angle_restoration) {
                restoration_vector_buffer_[particle_index] =
                    Subtract(basic_position, parent_basic_position);
            }
        }

        for (int iteration = 0; iteration < define::system::AngleLimitIteration; ++iteration) {
            const float iteration_ratio =
                static_cast<float>(iteration)
                / static_cast<float>(define::system::AngleLimitIteration - 1);
            const float limit_rotation_ratio = 0.4f;
            const float restoration_rotation_ratio =
                0.1f + (0.5f - 0.1f) * iteration_ratio;

            data_index = start + data_start;
            for (int index = 0; index < data_count; ++index, ++data_index) {
                if (data_index < 0 || data_index >= baseline_data.Length()) {
                    continue;
                }
                const int local_index = baseline_data[data_index];
                const int particle_index = particle_start + local_index;
                const int vertex_index = vertex_start + local_index;
                if (particle_index < 0
                    || particle_index >= next_positions.Length()
                    || particle_index >= velocity_positions.Length()
                    || particle_index >= frictions.Length()
                    || vertex_index < 0
                    || vertex_index >= attributes.Length()
                    || vertex_index >= vertex_depths.Length()
                    || vertex_index >= vertex_parent_indices.Length()) {
                    continue;
                }

                const VertexAttribute child_attr = attributes[vertex_index];
                if (!child_attr.IsMove()) {
                    continue;
                }

                const int parent_local_index = vertex_parent_indices[vertex_index];
                const int parent_particle_index = parent_local_index + particle_start;
                const int parent_vertex_index = parent_local_index + vertex_start;
                if (parent_particle_index < 0
                    || parent_particle_index >= next_positions.Length()
                    || parent_particle_index >= velocity_positions.Length()
                    || parent_particle_index >= frictions.Length()
                    || parent_vertex_index < 0
                    || parent_vertex_index >= attributes.Length()) {
                    continue;
                }

                float3 child_position = next_positions[particle_index];
                float3 parent_position = next_positions[parent_particle_index];
                const float child_depth = vertex_depths[vertex_index];
                const VertexAttribute parent_attr = attributes[parent_vertex_index];
                const float child_inv_mass = CalcInverseMass(frictions[particle_index]);
                const float parent_inv_mass = CalcInverseMass(frictions[parent_particle_index]);

                if (use_angle_limit) {
                    const quaternion parent_rotation = rotation_buffer_[parent_particle_index];
                    const float3 local_position = local_pos_buffer_[particle_index];
                    const quaternion local_rotation = local_rot_buffer_[particle_index];
                    float3 vector = Subtract(child_position, parent_position);
                    const float3 target_vector = Rotate(parent_rotation, local_position);
                    float vector_length = Length(vector);
                    const float buffered_length = length_buffer_[particle_index];
                    vector_length = vector_length + (buffered_length - vector_length) * 0.5f;
                    vector = Scale(Normalize(vector), vector_length);

                    const float max_angle_degrees =
                        MC2EvaluateCurve(angle_parameters.limit_curve_data, child_depth);
                    const float max_angle_radians = Radians(max_angle_degrees);
                    const float angle = std::acos(Clamp1(Dot(
                        Normalize(vector),
                        Normalize(target_vector)
                    )));
                    float3 restored_vector = vector;
                    if (angle > max_angle_radians) {
                        const float recovery_angle =
                            angle + (max_angle_radians - angle) * limit_stiffness;
                        ClampAngle(vector, target_vector, recovery_angle, restored_vector);
                    }

                    const float3 rotation_position =
                        Add(parent_position, Scale(vector, limit_rotation_ratio));
                    const float3 parent_final_position =
                        Subtract(rotation_position, Scale(restored_vector, limit_rotation_ratio));
                    const float3 child_final_position =
                        Add(rotation_position, Scale(restored_vector, 1.0f - limit_rotation_ratio));
                    float3 parent_add = Subtract(parent_final_position, parent_position);
                    float3 child_add = Subtract(child_final_position, child_position);
                    child_add = Scale(child_add, child_inv_mass);
                    parent_add = Scale(parent_add, parent_inv_mass);

                    constexpr float attenuation = define::system::AngleLimitAttenuation;
                    if (child_attr.IsMove()) {
                        child_position = Add(child_position, child_add);
                        next_positions[particle_index] = child_position;
                        velocity_positions[particle_index] =
                            Add(velocity_positions[particle_index], Scale(child_add, attenuation));
                    }
                    if (parent_attr.IsMove()) {
                        parent_position = Add(parent_position, parent_add);
                        next_positions[parent_particle_index] = parent_position;
                        velocity_positions[parent_particle_index] =
                            Add(velocity_positions[parent_particle_index], Scale(parent_add, attenuation));
                    }

                    vector = Subtract(child_position, parent_position);
                    quaternion next_rotation = Multiply(parent_rotation, local_rotation);
                    const quaternion delta_rotation = FromToRotation(target_vector, vector);
                    next_rotation = Multiply(delta_rotation, next_rotation);
                    rotation_buffer_[particle_index] = next_rotation;
                }

                if (use_angle_restoration) {
                    const float3 vector = Subtract(child_position, parent_position);
                    const float3 target_vector = restoration_vector_buffer_[particle_index];
                    float restoration_stiffness =
                        MC2EvaluateCurveClamp01(
                            angle_parameters.restoration_stiffness,
                            child_depth
                        );
                    restoration_stiffness = Clamp01(restoration_stiffness * simulation_power.w);
                    restoration_stiffness *= gravity_falloff;

                    const quaternion rotation =
                        FromToRotation(vector, target_vector, restoration_stiffness);
                    const float3 restored_vector = Rotate(rotation, vector);
                    const float3 rotation_position =
                        Add(parent_position, Scale(vector, restoration_rotation_ratio));
                    const float3 parent_final_position =
                        Subtract(rotation_position, Scale(restored_vector, restoration_rotation_ratio));
                    const float3 child_final_position =
                        Add(rotation_position, Scale(restored_vector, 1.0f - restoration_rotation_ratio));
                    float3 parent_add = Subtract(parent_final_position, parent_position);
                    float3 child_add = Subtract(child_final_position, child_position);
                    parent_add = Scale(parent_add, child_inv_mass);
                    child_add = Scale(child_add, parent_inv_mass);

                    if (child_attr.IsMove()) {
                        child_position = Add(child_position, child_add);
                        next_positions[particle_index] = child_position;
                        velocity_positions[particle_index] =
                            Add(velocity_positions[particle_index], Scale(child_add, restoration_attenuation));
                    }
                    if (parent_attr.IsMove()) {
                        parent_position = Add(parent_position, parent_add);
                        next_positions[parent_particle_index] = parent_position;
                        velocity_positions[parent_particle_index] =
                            Add(velocity_positions[parent_particle_index], Scale(parent_add, restoration_attenuation));
                    }
                }
            }
        }
    }
}

}  // namespace hocloth::mc2
