
#include <iostream>
#include <string>
#include <algorithm>
#include <random>
#include <iterator>

#include "sc2api/sc2_api.h"

using namespace sc2;

namespace sc2 {
struct MicroInfo {
    const Unit* myUnit;
    const Unit* target;
    bool move_back_;
    Point2D backup_target_;
    Point2D original_point_;
};


class Scenario1 : public Agent {
public:
    virtual void OnGameStart() final;
    virtual void OnStep() final;
    virtual void OnUnitDestroyed(const Unit* unit) override;
private:
    bool GetPosition(UNIT_TYPEID unit_type, Unit::Alliance alliace, Point2D& position);
    bool GetNearestMarine(MicroInfo *unit);
    bool GetNearestEnemy(MicroInfo *unit);
    bool GetNearestBaneling(MicroInfo *unit);
    bool GetNearestUltralisk(MicroInfo *unit);
    bool CheckDistance(MicroInfo *unit);
    void Stimpack(const Unit *unit);
    void Boost(const Unit *unit);
    
    //micro for single unti
    std::vector<MicroInfo> micro_info_;
};



};

int main(int argc, char* argv[]) {
    sc2::Coordinator coordinator;
    if (!coordinator.LoadSettings(argc, argv)) {
        return 1;
    }

    coordinator.SetRealtime(true);
    coordinator.SetWindowSize(3840, 2160);

    // Add the custom bot, it will control the player.
    sc2::Scenario1 bot;
    coordinator.SetParticipants({
        CreateParticipant(sc2::Race::Terran, &bot),
        CreateComputer(sc2::Race::Zerg),
    });

    // Start the game.
    coordinator.LaunchStarcraft();
    coordinator.StartGame(sc2::TestMap);

    while (coordinator.Update()) {
    }


    
    return 0;
}






namespace sc2 {


void Scenario1::OnGameStart() {

}

void Scenario1::OnStep() {
    const ObservationInterface* observation = Observation();
    
    if (observation->GetGameLoop() < 500) return;
    // initialize the micro_info_ vector
    else if (observation->GetGameLoop() == 500) {
        Units units = observation->GetUnits(Unit::Alliance::Self);
        
        // add all my units to micro_info_ vector
        for (const auto& u: units) {
            sc2::MicroInfo add;
            add.myUnit = u;
            add.move_back_ = false;
            add.target = NULL;
            add.original_point_ = u->pos;
            micro_info_.push_back(add);
        }
    }
    // game starts
    else {
        ActionInterface* action = Actions();
        bool if_baneling_ = true;
        bool if_ultralisk_ = true;
        
        Units my_units_ = observation->GetUnits(Unit::Alliance::Self);

        // if no more my units, then return
        if (my_units_.empty()) return;
        if (observation->GetUnits(Unit::Alliance::Enemy).empty()) return;
        
        // check if baneling or ultralisk exisits
        if (observation->GetUnits(Unit::Alliance::Enemy, [] (const Unit& unit) {
            return unit.unit_type==UNIT_TYPEID::ZERG_BANELING? 1 : 0;
        }).empty()) if_baneling_ = false;
        if (observation->GetUnits(Unit::Alliance::Enemy, [] (const Unit& unit) {
            return unit.unit_type==UNIT_TYPEID::ZERG_ULTRALISK? 1 : 0;
        }).empty()) if_ultralisk_ = false;

        

        
        for (const auto& unit : my_units_) {
            // find the corresponding unit in micro_info_ vector
            std::vector<MicroInfo>::iterator which_unit_;
            for (which_unit_ = micro_info_.begin(); which_unit_ != micro_info_.end(); which_unit_++) {
                if (which_unit_->myUnit == unit)
                    break;
            }
            
            // deal with each units
            switch (static_cast<UNIT_TYPEID>(unit->unit_type)) {
                case UNIT_TYPEID::TERRAN_MARINE:
                    Stimpack(unit);
                    GetNearestEnemy(which_unit_.base());
                    if (which_unit_->target->unit_type == UNIT_TYPEID::ZERG_ULTRALISK && Distance2D(unit->pos, which_unit_->target->pos) < 5.0f) {
                        action->UnitCommand(unit, ABILITY_ID::SMART, which_unit_->backup_target_);
                        continue;
                    }
                    break;
                // unsiege the tanks if no more baneling
                case UNIT_TYPEID::TERRAN_SIEGETANKSIEGED:
                    if (if_baneling_)
                        GetNearestBaneling(which_unit_.base());
                    else
                        GetNearestEnemy(which_unit_.base());
                    action->UnitCommand(unit, ABILITY_ID::ATTACK, which_unit_->target);
                    continue;
                // snipe the ultralisk
                case UNIT_TYPEID::TERRAN_GHOST:
                    if (if_ultralisk_) {
                        GetNearestUltralisk(which_unit_.base());
                        if (Distance2D(unit->pos, which_unit_->target->pos) < 9.0f) {
                            // cloak & snipe if close enough
                            if (unit->NotCloaked) {
                                action->UnitCommand(unit, ABILITY_ID::BEHAVIOR_CLOAKON);
                            }
                            action->UnitCommand(unit, ABILITY_ID::EFFECT_GHOSTSNIPE, which_unit_->target);
                            continue;
                        }
                    }
                    break;
                case UNIT_TYPEID::TERRAN_MEDIVAC:
                    GetNearestMarine(which_unit_.base());
                    Boost(unit);
                    action->UnitCommand(unit, ABILITY_ID::EFFECT_HEAL, which_unit_->target);
                    continue;
                default:
                    break;
            }
            // move and attack for all units
            GetNearestEnemy(which_unit_.base());
            // attack if not moving back and weapon already cooled down
            if (!which_unit_->move_back_ && !unit->weapon_cooldown) {
                action->UnitCommand(unit, ABILITY_ID::ATTACK, which_unit_->target);
            }
            else {
                // move back if too close && enmey facing toward the unit (bigger than 150 degrees)
                // keep longer distance for baneling and ultralisk
                if (CheckDistance(which_unit_.base()) && (abs(which_unit_->target->facing - unit->facing) > (which_unit_->target->unit_type == UNIT_TYPEID::ZERG_ULTRALISK ? 1.5708 : 2.6179)))
                    which_unit_->move_back_ = true;
                else which_unit_->move_back_ = false;
                
                if (which_unit_->move_back_)
                    action->UnitCommand(unit, ABILITY_ID::SMART, which_unit_->backup_target_);
            }
            
        }
    }
}

void Scenario1::OnUnitDestroyed(const Unit* unit) {
    std::vector<MicroInfo>::iterator which_unit_;
    
    
    // no action if my unit destroyed
    if (unit->alliance == Unit::Alliance::Self) return;
    
    // search which unit targetted the destroyed one
    for (which_unit_ = micro_info_.begin(); which_unit_ != micro_info_.end(); which_unit_++) {
        if (which_unit_->target->tag == unit->tag) {
            which_unit_->move_back_ = false;
            // re-get the target
            GetNearestEnemy(which_unit_.base());
        }
    }
}

bool Scenario1::GetPosition(UNIT_TYPEID unit_type, Unit::Alliance alliance, Point2D& position) {
    
}

// for medivac to cure the lowest health marine
bool Scenario1::GetNearestMarine(MicroInfo *unit) {
    const ObservationInterface* observation = Observation();
    Units units = observation->GetUnits(Unit::Alliance::Self, [] (const Unit& unit) {
        return unit.unit_type==UNIT_TYPEID::TERRAN_MARINE? 1 : 0;
    });
    
    if (units.empty()) return false;
    
    // check the first loop
    bool flag = false;
    float health = std::numeric_limits<float>::max();
    // choose the one with lowest health and close enough
    for (const auto& marine : units) {
        if (marine->health < health && Distance2D(unit->myUnit->pos, marine->pos) < 3.0f) {
            unit->target = marine;
            health = marine->health;
            flag = true;
        }
    }
    
    // serach again
    if (!flag)
        for (const auto& marine : units) {
            if (marine->health < health) {
                unit->target = marine;
                health = marine->health;
            }
        }
    
    return true;
}

bool Scenario1::GetNearestEnemy(MicroInfo *unit) {
    const ObservationInterface* observation = Observation();
    Units units = observation->GetUnits(Unit::Alliance::Enemy);
    
    // return if there are no more enemy
    if (units.empty()) {
        return false;
    }
    
    

    float distance = std::numeric_limits<float>::max();
    Point2D mp = unit->myUnit->pos;
    Point2D zp;
    for (const auto& enemy: units) {
        float d = Distance2D(mp, enemy->pos);
        // record the nearest enemy
        if (d < distance) {
            distance = d;
            unit->target = enemy;
            zp = enemy->pos;
        }
    }
    
    // record the backup point
    Vector2D diff = mp - zp;
    Normalize2D(diff);
    unit->backup_target_ = mp + diff * 2.0f;
    
    return true;
}

bool Scenario1::GetNearestBaneling(MicroInfo *unit) {
    const ObservationInterface* observation = Observation();
    Units units = observation->GetUnits(Unit::Alliance::Enemy, [] (const Unit& unit) {
        return unit.unit_type==UNIT_TYPEID::ZERG_BANELING? 1 : 0;
    });
    
    // return if there are no more banelings
    if (units.empty()) {
        return false;
    }
    
    
 
    float distance = std::numeric_limits<float>::min();
    for (const auto& baneling: units) {
        float d = Distance2D(unit->myUnit->pos, baneling->pos);
        // record the farthest baneling, rather than the closest
        if (d > distance) {
            distance = d;
            unit->target = baneling;
        }
        
    }
    
    return true;
}

bool Scenario1::GetNearestUltralisk(MicroInfo *unit) {
    const ObservationInterface* observation = Observation();
    Units units = observation->GetUnits(Unit::Alliance::Enemy, [] (const Unit& unit) {
        return unit.unit_type==UNIT_TYPEID::ZERG_ULTRALISK? 1 : 0;
    });
    
    // return if there are no more Ultralisks
    if (units.empty()) {
        return false;
    }
    
    

    float distance = std::numeric_limits<float>::max();
    for (const auto& ultralisk: units) {
        float d = Distance2D(unit->myUnit->pos, ultralisk->pos);
        // record the nearest Ultralisk
        if ( d < distance) {
            distance = d;
            unit->target = ultralisk;
        }

    }
    
    return true;
}

bool sc2::Scenario1::CheckDistance(MicroInfo *unit) {
    float distance = 7.5f;
    switch (static_cast<UNIT_TYPEID>(unit->target->unit_type)) {
        case UNIT_TYPEID::ZERG_BANELING:
            distance = 15.0f;
            break;
        case UNIT_TYPEID::ZERG_ULTRALISK:
            distance = 20.0f;
            break;
        default:
            break;
    }
    
    return Distance2D(unit->myUnit->pos, unit->target->pos) < distance;
}

void sc2::Scenario1::Stimpack(const Unit *unit) {
    ActionInterface* action = Actions();
    int flag = 0;
    // search if already stimpacked
    for (const auto& buf: unit->buffs) {
        if (buf == BUFF_ID::STIMPACK) {
            flag = 1;
            break;
        }
    }
    
    // stimpack if not
    if (!flag)
        action->UnitCommand(unit, ABILITY_ID::EFFECT_STIM);
}

void sc2::Scenario1::Boost(const Unit *unit) {
    ActionInterface* action = Actions();
    int flag = 0;
    // search if already boosted
    for (const auto& buf: unit->buffs) {
        if (buf == BUFF_ID::MEDIVACSPEEDBOOST) {
            flag = 1;
            break;
        }
    }
    
    // Boost if not
    if (!flag)
        action->UnitCommand(unit, ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS);
}




};
