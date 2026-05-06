#include "hocloth/virtual_mesh/virtual_mesh_bone_weight.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>

namespace hocloth::mc2 {

VirtualMeshBoneWeight::VirtualMeshBoneWeight(const int4& indices, const float4& weight_values)
{
    bone_indices = {indices.x, indices.y, indices.z, indices.w};
    weights = {weight_values.x, weight_values.y, weight_values.z, weight_values.w};
}

bool VirtualMeshBoneWeight::IsValid() const
{
    return weights[0] >= 1.0e-6f;
}

int VirtualMeshBoneWeight::Count() const
{
    if (weights[3] > 0.0f) {
        return 4;
    }
    if (weights[2] > 0.0f) {
        return 3;
    }
    if (weights[1] > 0.0f) {
        return 2;
    }
    if (weights[0] > 0.0f) {
        return 1;
    }
    return 0;
}

void VirtualMeshBoneWeight::AddWeight(int bone_index, float weight)
{
    if (weight < 1.0e-6f) {
        return;
    }

    for (int i = 0; i < 4; ++i) {
        float current_weight = weights[static_cast<std::size_t>(i)];
        if (current_weight == 0.0f) {
            break;
        }
        if (bone_indices[static_cast<std::size_t>(i)] != bone_index) {
            continue;
        }

        current_weight += weight;
        weights[static_cast<std::size_t>(i)] = current_weight;

        for (int j = i; j >= 1; --j) {
            if (weights[static_cast<std::size_t>(j)] <= weights[static_cast<std::size_t>(j - 1)]) {
                break;
            }
            std::swap(weights[static_cast<std::size_t>(j - 1)], weights[static_cast<std::size_t>(j)]);
            std::swap(
                bone_indices[static_cast<std::size_t>(j - 1)],
                bone_indices[static_cast<std::size_t>(j)]
            );
        }
        return;
    }

    for (int i = 0; i < 4; ++i) {
        const float current_weight = weights[static_cast<std::size_t>(i)];
        if (current_weight == 0.0f) {
            weights[static_cast<std::size_t>(i)] = weight;
            bone_indices[static_cast<std::size_t>(i)] = bone_index;
            return;
        }
        if (weight <= current_weight) {
            continue;
        }

        for (int j = 2; j >= i; --j) {
            weights[static_cast<std::size_t>(j + 1)] = weights[static_cast<std::size_t>(j)];
            bone_indices[static_cast<std::size_t>(j + 1)] =
                bone_indices[static_cast<std::size_t>(j)];
        }
        weights[static_cast<std::size_t>(i)] = weight;
        bone_indices[static_cast<std::size_t>(i)] = bone_index;
        return;
    }
}

void VirtualMeshBoneWeight::AddWeight(const VirtualMeshBoneWeight& bone_weight)
{
    if (!bone_weight.IsValid()) {
        return;
    }
    for (int i = 0; i < 4; ++i) {
        AddWeight(
            bone_weight.bone_indices[static_cast<std::size_t>(i)],
            bone_weight.weights[static_cast<std::size_t>(i)]
        );
    }
}

void VirtualMeshBoneWeight::AdjustWeight()
{
    if (!IsValid()) {
        return;
    }

    const float total = weights[0] + weights[1] + weights[2] + weights[3];
    assert(total >= 1.0e-6f);
    const float scale = 1.0f / total;
    for (float& weight : weights) {
        weight *= scale;
    }
}

std::string VirtualMeshBoneWeight::ToString() const
{
    std::ostringstream stream;
    stream << "[" << bone_indices[0] << "," << bone_indices[1] << "," << bone_indices[2]
           << "," << bone_indices[3] << "] w(" << weights[0] << "," << weights[1] << ","
           << weights[2] << "," << weights[3] << ")";
    return stream.str();
}

}  // namespace hocloth::mc2
