#pragma once

namespace hocloth::mc2::define::system {

// Ported from Magica Cloth 2: Scripts/Core/Define/SystemDefine.cs
inline constexpr const char* DefineSymbol = "MAGICACLOTH2";
inline constexpr int LatestPreBuildVersion = 2;
inline constexpr float Epsilon = 1.0e-8f;
inline constexpr int MaxRendererCount = 31;
inline constexpr float MinimumGridSize = 0.00001f;
inline constexpr int MaximumTeamCount = 4096;

inline constexpr int DefaultSimulationFrequency = 90;
inline constexpr int SimulationFrequencyLow = 30;
inline constexpr int SimulationFrequencyHigh = 150;
inline constexpr int DefaultMaxSimulationCountPerFrame = 3;
inline constexpr int MaxSimulationCountPerFrameLow = 1;
inline constexpr int MaxSimulationCountPerFrameHigh = 5;

inline constexpr float SameSurfaceAngle = 80.0f;

inline constexpr bool ReductionEnable = true;
inline constexpr float ReductionSameDistance = 0.001f;
inline constexpr bool ReductionDontMakeLine = true;
inline constexpr float ReductionJoinPositionAdjustment = 1.0f;
inline constexpr int ReductionMaxStep = 100;

inline constexpr int MaxProxyMeshVertexCount = 32767;
inline constexpr int MaxProxyMeshEdgeCount = 32767;
inline constexpr int MaxProxyMeshTriangleCount = 32767;
inline constexpr float ProxyMeshTrianglePairAngle = 20.0f;
inline constexpr float ProxyMeshBoneClothTriangleAngle = 120.0f;

inline constexpr float FrictionMass = 3.0f;
inline constexpr float DepthMass = 5.0f;
inline constexpr float FrictionDampingRate = 0.6f;
inline constexpr float PositionAverageExponent = 0.5f;
inline constexpr float MaxRealVelocity = 0.5f;

inline constexpr float TetherCompressionStiffness = 1.0f;
inline constexpr float TetherStretchStiffness = 1.0f;
inline constexpr float TetherStretchLimit = 0.03f;
inline constexpr float TetherStiffnessWidth = 0.3f;
inline constexpr float TetherCompressionVelocityAttenuation = 0.7f;
inline constexpr float TetherStretchVelocityAttenuation = 0.7f;

inline constexpr float DistanceVelocityAttenuation = 0.3f;
inline constexpr float DistanceVerticalStiffness = 1.0f;
inline constexpr float DistanceHorizontalStiffness = 0.5f;

inline constexpr float TriangleBendingMaxAngle = 120.0f;
inline constexpr float VolumeMinAngle = 90.0f;
inline constexpr float MaxAngleLimit = 179.0f;
inline constexpr int AngleLimitIteration = 3;
inline constexpr float AngleLimitAttenuation = 0.9f;

inline constexpr float MaxMovementSpeedLimit = 10.0f;
inline constexpr float MaxRotationSpeedLimit = 1440.0f;
inline constexpr float MaxParticleSpeedLimit = 10.0f;

inline constexpr int ExpandedColliderCount = 8;
inline constexpr float ColliderCollisionDynamicFrictionRatio = 1.0f;
inline constexpr float ColliderCollisionStaticFrictionRatio = 1.0f;

inline constexpr float CustomSkinningAngularAttenuation = 1.0f;
inline constexpr float CustomSkinningDistanceReduction = 0.6f;
inline constexpr float CustomSkinningDistancePow = 2.0f;

inline constexpr int SelfCollisionSolverIteration = 4;
inline constexpr float SelfCollisionFixedMass = 100.0f;
inline constexpr float SelfCollisionFrictionMass = 10.0f;
inline constexpr float SelfCollisionClothMass = 50.0f;
inline constexpr float SelfCollisionSCR = 2.0f;
inline constexpr float SelfCollisionPointTriangleAngleCos = 0.5f;
inline constexpr int SelfCollisionIntersectDiv = 8;
inline constexpr float SelfCollisionThicknessMin = 0.001f;
inline constexpr float SelfCollisionThicknessMax = 0.05f;

inline constexpr float WindMaxTime = 10000.0f;
inline constexpr float WindBaseSpeed = 7.5f;

inline constexpr float BoneSpringDistanceStiffness = 0.5f;
inline constexpr float BoneSpringTetherCompressionLimit = 0.8f;
inline constexpr float BoneSpringCollisionFriction = 0.5f;

inline constexpr float DistanceCullingMaxLength = 100.0f;

}  // namespace hocloth::mc2::define::system
