#include "hocloth/utility/result_code/result_code.hpp"

#include <sstream>
#include <utility>

namespace hocloth::mc2 {

bool Result::Succeeded() const
{
    return code == ResultCode::Success;
}

bool Result::Failed() const
{
    return !Succeeded();
}

Result Result::Ok()
{
    return Result{};
}

Result Result::Error(ResultCode code, std::string message)
{
    return Result{code, std::move(message)};
}

ResultStatus::ResultStatus(ResultCode initial_result)
    : result_(initial_result)
{
}

ResultCode ResultStatus::Code() const
{
    return result_;
}

ResultCode ResultStatus::WarningCode() const
{
    return warning_;
}

void ResultStatus::Clear()
{
    result_ = ResultCode::None;
    warning_ = ResultCode::None;
}

void ResultStatus::SetResult(ResultCode code)
{
    result_ = code;
}

void ResultStatus::SetSuccess()
{
    SetResult(ResultCode::Success);
}

void ResultStatus::SetCancel()
{
    SetResult(ResultCode::Cancel);
}

void ResultStatus::SetError(ResultCode code)
{
    result_ = code;
}

void ResultStatus::SetWarning(ResultCode code)
{
    if (code != ResultCode::None) {
        warning_ = code;
    }
}

void ResultStatus::Merge(const ResultStatus& source)
{
    if (source.IsError()) {
        result_ = source.result_;
    }
    if (source.IsWarning()) {
        warning_ = source.warning_;
    }
}

void ResultStatus::SetProcess()
{
    SetResult(ResultCode::Process);
}

bool ResultStatus::IsResult(ResultCode code) const
{
    return result_ == code;
}

bool ResultStatus::IsNone() const
{
    return result_ == ResultCode::None;
}

bool ResultStatus::IsSuccess() const
{
    return result_ == ResultCode::Success;
}

bool ResultStatus::IsFailed() const
{
    return !IsSuccess();
}

bool ResultStatus::IsCancel() const
{
    return result_ == ResultCode::Cancel;
}

bool ResultStatus::IsNormal() const
{
    return static_cast<int>(result_) < static_cast<int>(ResultCode::Warning);
}

bool ResultStatus::IsError() const
{
    return static_cast<int>(result_) >= static_cast<int>(ResultCode::Error);
}

bool ResultStatus::IsProcess() const
{
    return result_ == ResultCode::Process;
}

bool ResultStatus::IsWarning() const
{
    return warning_ != ResultCode::None;
}

std::string ResultStatus::GetResultString() const
{
    if (IsNormal()) {
        return ToString(result_);
    }
    std::ostringstream stream;
    stream << "(" << static_cast<int>(result_) << ") " << ToString(result_);
    return stream.str();
}

std::string ResultStatus::GetWarningString() const
{
    std::ostringstream stream;
    stream << "(" << static_cast<int>(warning_) << ") " << ToString(warning_);
    return stream.str();
}

const char* ResultStatus::GetResultInformation() const
{
    switch (result_) {
    case ResultCode::RenderSetup_Unreadable:
        return "It is necessary to turn on [Read/Write] in the model import settings.";
    case ResultCode::RenderSetup_Over65535vertices:
        return "Original mesh must have no more than 65,535 vertices.";
    case ResultCode::SerializeData_Over31Renderers:
        return "There are too many renderers.";
    case ResultCode::Init_ScaleIsZero:
        return "Component scale values is 0.";
    case ResultCode::Init_NegativeScale:
        return "Component has negative scale.";
    default:
        return nullptr;
    }
}

const char* ResultStatus::GetWarningInformation() const
{
    switch (warning_) {
    case ResultCode::RenderMesh_VertexWeightIs5BonesOrMore:
        return "The source renderer mesh contains vertex weights that utilize more than 5 bones.";
    case ResultCode::Init_NonUniformScale:
        return "Component scale values should be uniform.";
    default:
        return nullptr;
    }
}

ResultStatus ResultStatus::None()
{
    return ResultStatus(ResultCode::None);
}

ResultStatus ResultStatus::Empty()
{
    return ResultStatus(ResultCode::Empty);
}

ResultStatus ResultStatus::Success()
{
    return ResultStatus(ResultCode::Success);
}

ResultStatus ResultStatus::Error()
{
    return ResultStatus(ResultCode::Error);
}

const char* ToString(ResultCode code)
{
    switch (code) {
    case ResultCode::None:
        return "None";
    case ResultCode::Empty:
        return "Empty";
    case ResultCode::Success:
        return "Success";
    case ResultCode::Cancel:
        return "Cancel";
    case ResultCode::Process:
        return "Process";
    case ResultCode::Warning:
        return "Warning";
    case ResultCode::Error:
        return "Error";
    case ResultCode::RenderMesh_UnknownWarning:
        return "RenderMesh_UnknownWarning";
    case ResultCode::RenderMesh_VertexWeightIs5BonesOrMore:
        return "RenderMesh_VertexWeightIs5BonesOrMore";
    case ResultCode::Init_NonUniformScale:
        return "Init_NonUniformScale";
    case ResultCode::SerializeData_InvalidData:
        return "SerializeData_InvalidData";
    case ResultCode::SerializeData_Over31Renderers:
        return "SerializeData_Over31Renderers";
    case ResultCode::Init_InvalidData:
        return "Init_InvalidData";
    case ResultCode::Init_InvalidPaintMap:
        return "Init_InvalidPaintMap";
    case ResultCode::Init_PaintMapNotReadable:
        return "Init_PaintMapNotReadable";
    case ResultCode::Init_ScaleIsZero:
        return "Init_ScaleIsZero";
    case ResultCode::Init_NegativeScale:
        return "Init_NegativeScale";
    case ResultCode::RenderSetup_Exception:
        return "RenderSetup_Exception";
    case ResultCode::RenderSetup_UnknownError:
        return "RenderSetup_UnknownError";
    case ResultCode::RenderSetup_InvalidSource:
        return "RenderSetup_InvalidSource";
    case ResultCode::RenderSetup_NoMeshOnRenderer:
        return "RenderSetup_NoMeshOnRenderer";
    case ResultCode::RenderSetup_InvalidType:
        return "RenderSetup_InvalidType";
    case ResultCode::RenderSetup_Unreadable:
        return "RenderSetup_Unreadable";
    case ResultCode::RenderSetup_Over65535vertices:
        return "RenderSetup_Over65535vertices";
    case ResultCode::VirtualMesh_UnknownError:
        return "VirtualMesh_UnknownError";
    case ResultCode::VirtualMesh_InvalidSetup:
        return "VirtualMesh_InvalidSetup";
    case ResultCode::VirtualMesh_InvalidRenderData:
        return "VirtualMesh_InvalidRenderData";
    case ResultCode::VirtualMesh_ImportError:
        return "VirtualMesh_ImportError";
    case ResultCode::VirtualMesh_SelectionException:
        return "VirtualMesh_SelectionException";
    case ResultCode::VirtualMesh_SelectionUnknownError:
        return "VirtualMesh_SelectionUnknownError";
    case ResultCode::VirtualMesh_InvalidSelection:
        return "VirtualMesh_InvalidSelection";
    case ResultCode::CreateCloth_Exception:
        return "CreateCloth_Exception";
    case ResultCode::CreateCloth_UnknownError:
        return "CreateCloth_UnknownError";
    case ResultCode::CreateCloth_InvalidCloth:
        return "CreateCloth_InvalidCloth";
    case ResultCode::CreateCloth_InvalidSerializeData:
        return "CreateCloth_InvalidSerializeData";
    case ResultCode::CreateCloth_InvalidSetupList:
        return "CreateCloth_InvalidSetupList";
    case ResultCode::CreateCloth_NoRenderer:
        return "CreateCloth_NoRenderer";
    case ResultCode::CreateCloth_InvalidPaintMap:
        return "CreateCloth_InvalidPaintMap";
    case ResultCode::CreateCloth_PaintMapNotReadable:
        return "CreateCloth_PaintMapNotReadable";
    case ResultCode::CreateCloth_PaintMapCountMismatch:
        return "CreateCloth_PaintMapCountMismatch";
    case ResultCode::CreateCloth_CanNotStart:
        return "CreateCloth_CanNotStart";
    case ResultCode::CreateCloth_VertexAttributeListCountMismatch:
        return "CreateCloth_VertexAttributeListCountMismatch";
    case ResultCode::CreateCloth_VertexAttributeListIsNull:
        return "CreateCloth_VertexAttributeListIsNull";
    case ResultCode::CreateCloth_VertexAttributeListDataMismatch:
        return "CreateCloth_VertexAttributeListDataMismatch";
    case ResultCode::CreateCloth_InvalidVertexAttributeData:
        return "CreateCloth_InvalidVertexAttributeData";
    case ResultCode::Reduction_Exception:
        return "Reduction_Exception";
    case ResultCode::Reduction_UnknownError:
        return "Reduction_UnknownError";
    case ResultCode::Reduction_InitError:
        return "Reduction_InitError";
    case ResultCode::Reduction_SameDistanceException:
        return "Reduction_SameDistanceException";
    case ResultCode::Reduction_SimpleDistanceException:
        return "Reduction_SimpleDistanceException";
    case ResultCode::Reduction_ShapeDistanceException:
        return "Reduction_ShapeDistanceException";
    case ResultCode::Reduction_MaxSideLengthZero:
        return "Reduction_MaxSideLengthZero";
    case ResultCode::Reduction_OrganizationError:
        return "Reduction_OrganizationError";
    case ResultCode::Reduction_StoreVirtualMeshError:
        return "Reduction_StoreVirtualMeshError";
    case ResultCode::Reduction_CalcAverageException:
        return "Reduction_CalcAverageException";
    case ResultCode::Optimize_Exception:
        return "Optimize_Exception";
    case ResultCode::ProxyMesh_Exception:
        return "ProxyMesh_Exception";
    case ResultCode::ProxyMesh_UnknownError:
        return "ProxyMesh_UnknownError";
    case ResultCode::ProxyMesh_ApplySelectionError:
        return "ProxyMesh_ApplySelectionError";
    case ResultCode::ProxyMesh_ConvertError:
        return "ProxyMesh_ConvertError";
    case ResultCode::ProxyMesh_Over32767Vertices:
        return "ProxyMesh_Over32767Vertices";
    case ResultCode::ProxyMesh_Over32767Edges:
        return "ProxyMesh_Over32767Edges";
    case ResultCode::ProxyMesh_Over32767Triangles:
        return "ProxyMesh_Over32767Triangles";
    case ResultCode::MappingMesh_Exception:
        return "MappingMesh_Exception";
    case ResultCode::MappingMesh_UnknownError:
        return "MappingMesh_UnknownError";
    case ResultCode::MappingMesh_ProxyError:
        return "MappingMesh_ProxyError";
    case ResultCode::ClothInit_Exception:
        return "ClothInit_Exception";
    case ResultCode::ClothInit_FailedAddRenderer:
        return "ClothInit_FailedAddRenderer";
    case ResultCode::ClothProcess_Exception:
        return "ClothProcess_Exception";
    case ResultCode::ClothProcess_UnknownError:
        return "ClothProcess_UnknownError";
    case ResultCode::ClothProcess_Invalid:
        return "ClothProcess_Invalid";
    case ResultCode::ClothProcess_InvalidRenderHandleList:
        return "ClothProcess_InvalidRenderHandleList";
    case ResultCode::ClothProcess_GenerateSelectionError:
        return "ClothProcess_GenerateSelectionError";
    case ResultCode::ClothProcess_OverflowTeamCount4096:
        return "ClothProcess_OverflowTeamCount4096";
    case ResultCode::Constraint_Exception:
        return "Constraint_Exception";
    case ResultCode::Constraint_UnknownError:
        return "Constraint_UnknownError";
    case ResultCode::Constraint_CreateDistanceException:
        return "Constraint_CreateDistanceException";
    case ResultCode::Constraint_CreateTriangleBendingException:
        return "Constraint_CreateTriangleBendingException";
    case ResultCode::Constraint_CreateInertiaException:
        return "Constraint_CreateInertiaException";
    case ResultCode::Constraint_CreateSelfCollisionException:
        return "Constraint_CreateSelfCollisionException";
    case ResultCode::MagicaMesh_UnknownError:
        return "MagicaMesh_UnknownError";
    case ResultCode::MagicaMesh_Invalid:
        return "MagicaMesh_Invalid";
    case ResultCode::MagicaMesh_InvalidRenderer:
        return "MagicaMesh_InvalidRenderer";
    case ResultCode::MagicaMesh_InvalidMeshFilter:
        return "MagicaMesh_InvalidMeshFilter";
    case ResultCode::PreBuildData_UnknownError:
        return "PreBuildData_UnknownError";
    case ResultCode::PreBuildData_MagicaClothException:
        return "PreBuildData_MagicaClothException";
    case ResultCode::PreBuildData_VirtualMeshDeserializationException:
        return "PreBuildData_VirtualMeshDeserializationException";
    case ResultCode::PreBuildData_VerificationResult:
        return "PreBuildData_VerificationResult";
    case ResultCode::PreBuildData_VersionMismatch:
        return "PreBuildData_VersionMismatch";
    case ResultCode::PreBuildData_InvalidClothData:
        return "PreBuildData_InvalidClothData";
    case ResultCode::PreBuildData_Empty:
        return "PreBuildData_Empty";
    case ResultCode::PreBuildData_InvalidScale:
        return "PreBuildData_InvalidScale";
    case ResultCode::PreBuild_UnknownError:
        return "PreBuild_UnknownError";
    case ResultCode::PreBuild_Exception:
        return "PreBuild_Exception";
    case ResultCode::PreBuild_InvalidPreBuildData:
        return "PreBuild_InvalidPreBuildData";
    case ResultCode::PreBuild_InvalidRenderSetupData:
        return "PreBuild_InvalidRenderSetupData";
    case ResultCode::PreBuild_SetupDeserializationError:
        return "PreBuild_SetupDeserializationError";
    case ResultCode::Deserialization_UnknownError:
        return "Deserialization_UnknownError";
    case ResultCode::Deserialization_Exception:
        return "Deserialization_Exception";
    }
    return "Unknown";
}

}  // namespace hocloth::mc2
