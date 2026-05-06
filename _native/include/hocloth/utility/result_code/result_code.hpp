#pragma once

#include <string>

namespace hocloth::mc2 {

// Port target for Magica Cloth 2: Scripts/Core/Utility/ResultCode/ResultCode.cs
enum class ResultCode {
    None = 0,
    Empty = 1,
    Success = 2,
    Cancel = 3,
    Process = 4,

    Warning = 10000,
    RenderMesh_UnknownWarning = 10100,
    RenderMesh_VertexWeightIs5BonesOrMore,
    Init_NonUniformScale = 10200,

    Error = 20000,
    SerializeData_InvalidData = 20050,
    SerializeData_Over31Renderers,
    Init_InvalidData = 20100,
    Init_InvalidPaintMap,
    Init_PaintMapNotReadable,
    Init_ScaleIsZero,
    Init_NegativeScale,
    RenderSetup_Exception = 20200,
    RenderSetup_UnknownError,
    RenderSetup_InvalidSource,
    RenderSetup_NoMeshOnRenderer,
    RenderSetup_InvalidType,
    RenderSetup_Unreadable,
    RenderSetup_Over65535vertices,
    VirtualMesh_UnknownError = 20300,
    VirtualMesh_InvalidSetup,
    VirtualMesh_InvalidRenderData,
    VirtualMesh_ImportError,
    VirtualMesh_SelectionException,
    VirtualMesh_SelectionUnknownError,
    VirtualMesh_InvalidSelection,
    CreateCloth_Exception = 20400,
    CreateCloth_UnknownError,
    CreateCloth_InvalidCloth,
    CreateCloth_InvalidSerializeData,
    CreateCloth_InvalidSetupList,
    CreateCloth_NoRenderer,
    CreateCloth_InvalidPaintMap,
    CreateCloth_PaintMapNotReadable,
    CreateCloth_PaintMapCountMismatch,
    CreateCloth_CanNotStart,
    CreateCloth_VertexAttributeListCountMismatch,
    CreateCloth_VertexAttributeListIsNull,
    CreateCloth_VertexAttributeListDataMismatch,
    CreateCloth_InvalidVertexAttributeData,
    Reduction_Exception = 20500,
    Reduction_UnknownError,
    Reduction_InitError,
    Reduction_SameDistanceException,
    Reduction_SimpleDistanceException,
    Reduction_ShapeDistanceException,
    Reduction_MaxSideLengthZero,
    Reduction_OrganizationError,
    Reduction_StoreVirtualMeshError,
    Reduction_CalcAverageException,
    Optimize_Exception = 20600,
    ProxyMesh_Exception = 20700,
    ProxyMesh_UnknownError,
    ProxyMesh_ApplySelectionError,
    ProxyMesh_ConvertError,
    ProxyMesh_Over32767Vertices,
    ProxyMesh_Over32767Edges,
    ProxyMesh_Over32767Triangles,
    MappingMesh_Exception = 20800,
    MappingMesh_UnknownError,
    MappingMesh_ProxyError,
    ClothInit_Exception = 22000,
    ClothInit_FailedAddRenderer,
    ClothProcess_Exception = 22100,
    ClothProcess_UnknownError,
    ClothProcess_Invalid,
    ClothProcess_InvalidRenderHandleList,
    ClothProcess_GenerateSelectionError,
    ClothProcess_OverflowTeamCount4096,
    Constraint_Exception = 22200,
    Constraint_UnknownError,
    Constraint_CreateDistanceException,
    Constraint_CreateTriangleBendingException,
    Constraint_CreateInertiaException,
    Constraint_CreateSelfCollisionException,
    MagicaMesh_UnknownError = 22500,
    MagicaMesh_Invalid,
    MagicaMesh_InvalidRenderer,
    MagicaMesh_InvalidMeshFilter,
    PreBuildData_UnknownError = 22600,
    PreBuildData_MagicaClothException,
    PreBuildData_VirtualMeshDeserializationException,
    PreBuildData_VerificationResult,
    PreBuildData_VersionMismatch,
    PreBuildData_InvalidClothData,
    PreBuildData_Empty,
    PreBuildData_InvalidScale,
    PreBuild_UnknownError = 22700,
    PreBuild_Exception,
    PreBuild_InvalidPreBuildData,
    PreBuild_InvalidRenderSetupData,
    PreBuild_SetupDeserializationError,
    Deserialization_UnknownError = 22800,
    Deserialization_Exception,

    InvalidArgument = Error,
    NotImplemented = Error,
};

struct Result {
    ResultCode code = ResultCode::Success;
    std::string message;

    [[nodiscard]] bool Succeeded() const;
    [[nodiscard]] bool Failed() const;

    static Result Ok();
    static Result Error(ResultCode code, std::string message);
};

class ResultStatus {
public:
    ResultStatus() = default;
    explicit ResultStatus(ResultCode initial_result);

    [[nodiscard]] ResultCode Code() const;
    [[nodiscard]] ResultCode WarningCode() const;

    void Clear();
    void SetResult(ResultCode code);
    void SetSuccess();
    void SetCancel();
    void SetError(ResultCode code = ResultCode::Error);
    void SetWarning(ResultCode code = ResultCode::Warning);
    void Merge(const ResultStatus& source);
    void SetProcess();

    [[nodiscard]] bool IsResult(ResultCode code) const;
    [[nodiscard]] bool IsNone() const;
    [[nodiscard]] bool IsSuccess() const;
    [[nodiscard]] bool IsFailed() const;
    [[nodiscard]] bool IsCancel() const;
    [[nodiscard]] bool IsNormal() const;
    [[nodiscard]] bool IsError() const;
    [[nodiscard]] bool IsProcess() const;
    [[nodiscard]] bool IsWarning() const;
    [[nodiscard]] std::string GetResultString() const;
    [[nodiscard]] std::string GetWarningString() const;
    [[nodiscard]] const char* GetResultInformation() const;
    [[nodiscard]] const char* GetWarningInformation() const;

    static ResultStatus None();
    static ResultStatus Empty();
    static ResultStatus Success();
    static ResultStatus Error();

private:
    ResultCode result_ = ResultCode::None;
    ResultCode warning_ = ResultCode::None;
};

[[nodiscard]] const char* ToString(ResultCode code);

}  // namespace hocloth::mc2
