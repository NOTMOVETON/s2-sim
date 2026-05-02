#pragma once

#include <s2/types.hpp>
#include <string>
#include <variant>
#include <vector>

namespace s2 {
namespace cmd {

struct SpawnEntity   { EntityId id; EntityType type; std::string name; Pose3D pose; };
struct DespawnEntity { EntityId id; };
struct SetPose       { EntityId id; Pose3D pose; };
struct SetEnabled    { EntityId id; bool enabled; };
struct AddPlugin     { EntityId id; std::string plugin_type; std::string config_json; };
struct RemovePlugin  { EntityId id; std::string plugin_type; };
struct ConfigPlugin  { EntityId id; std::string plugin_type; std::string config_json; };
struct SpawnZone     { ZoneId id; ZoneShape shape; bool enabled{true}; };
struct DespawnZone   { ZoneId id; };
struct ToggleZone    { ZoneId id; bool enabled; };
struct SetZoneShape  { ZoneId id; ZoneShape shape; };
struct SetZoneStrength { ZoneId id; double strength; };
struct Interact      { EntityId initiator; EntityId target; std::string action; std::string params_json; };
struct AttachObject  { ObjectId object; EntityId agent; std::string link; };
struct DetachObject  { ObjectId object; EntityId agent; };
struct PauseSim      {};
struct ResumeSim     {};
struct ResetSim      {};
struct StepSim       { int n{1}; };
struct SetSpeed      { double factor; };
struct LoadScene     { std::string path; };
struct SaveScene     { std::string path; };
struct NewScene      {};
struct RemoveOwnEffect { EntityId id; std::vector<std::string> effect_tags; };

} // namespace cmd

using KernelCommand = std::variant<
    cmd::SpawnEntity,
    cmd::DespawnEntity,
    cmd::SetPose,
    cmd::SetEnabled,
    cmd::AddPlugin,
    cmd::RemovePlugin,
    cmd::ConfigPlugin,
    cmd::SpawnZone,
    cmd::DespawnZone,
    cmd::ToggleZone,
    cmd::SetZoneShape,
    cmd::SetZoneStrength,
    cmd::Interact,
    cmd::AttachObject,
    cmd::DetachObject,
    cmd::PauseSim,
    cmd::ResumeSim,
    cmd::ResetSim,
    cmd::StepSim,
    cmd::SetSpeed,
    cmd::LoadScene,
    cmd::SaveScene,
    cmd::NewScene,
    cmd::RemoveOwnEffect
>;

} // namespace s2
