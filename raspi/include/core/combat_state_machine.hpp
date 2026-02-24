#pragma once
#include "types.hpp"
#include <vector>
#include <map>
#include <optional>

namespace sancak::core {

struct TrackedTarget {
    int id = -1;
    TargetClass target_class = TargetClass::Unknown;
    Affiliation affiliation = Affiliation::Unknown;
    float distance_m = 0.0f;
    float pan_deg = 0.0f;
    float tilt_deg = 0.0f;
    float confidence = 0.0f;
};

struct CombatDecision {
    CombatState state = CombatState::Idle;
    int locked_target_id = -1;
};

struct Geofence {
    float pan_min = -90.0f;
    float pan_max = 90.0f;
    float tilt_min = -10.0f;
    float tilt_max = 45.0f;
    bool isInside(float pan, float tilt) const {
        return pan >= pan_min && pan <= pan_max && tilt >= tilt_min && tilt <= tilt_max;
    }
};

class CombatStateMachine {
public:
    CombatStateMachine(const std::map<TargetClass, TargetRule>& rules,
                      const Geofence& geofence)
        : target_rules_(rules), geofence_(geofence) {}

    CombatDecision update(const std::vector<TrackedTarget>& targets,
                         float current_pan, float current_tilt) {
        // Geofence kontrolü
        if (!geofence_.isInside(current_pan, current_tilt)) {
            return {CombatState::SafeLock, -1};
        }

        // Hedef seçimi: priority ve mesafe
        const TrackedTarget* best = nullptr;
        int best_priority = INT32_MAX;
        float best_distance = 1e9;
        for (const auto& t : targets) {
            auto rule_it = target_rules_.find(t.target_class);
            if (rule_it == target_rules_.end()) continue;
            const auto& rule = rule_it->second;
            // Mesafe ve öncelik kontrolü
            if (t.distance_m < rule.min_range_m || t.distance_m > rule.max_range_m)
                continue;
            if (t.affiliation == Affiliation::Friend) continue;
            // En yüksek öncelikli ve en yakın hedef
            if (rule.priority < best_priority || (rule.priority == best_priority && t.distance_m < best_distance)) {
                best = &t;
                best_priority = rule.priority;
                best_distance = t.distance_m;
            }
        }
        if (!best) {
            return {CombatState::Searching, -1};
        }
        // FOE ise ve menzildeyse ENGAGING
        if (best->affiliation == Affiliation::Foe) {
            return {CombatState::Engaging, best->id};
        }
        // Diğer durumlar
        return {CombatState::Searching, -1};
    }

private:
    std::map<TargetClass, TargetRule> target_rules_;
    Geofence geofence_;
};

} // namespace sancak::core
