
#include <iostream>
#include <string>
#include <algorithm>
#include <random>
#include <iterator>
#define GAMESTART 3

#include "sc2api/sc2_api.h"

using namespace sc2;

namespace sc2 {
struct MicroInfo {
    const Unit* myUnit;
    const Unit* target;
    bool move_back_;
    bool search;
    uint32_t game_loop_;
    Point2D backup_target_;
    Point2D original_point_;
};


class Scenario3 : public Agent {
public:
    virtual void OnGameStart() final;
    virtual void OnStep() final;
    virtual void OnUnitDestroyed(const Unit* unit) override;
private:
    void Normalize(Point2D& a);
    bool GetMaxRangeEnemy(MicroInfo *unit);
    bool GetRangeEnemy(MicroInfo *unit);
    void Fire();

    
    //micro for single unit
    std::vector<MicroInfo> micro_info_;
    //
    bool check_fire_ = false;
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
    sc2::Scenario3 bot;
    coordinator.SetParticipants({
        CreateParticipant(sc2::Race::Zerg, &bot),
        CreateComputer(sc2::Race::Terran),
    });

    // Start the game.
    coordinator.LaunchStarcraft();
    coordinator.StartGame(sc2::TestMap);

    while (coordinator.Update()) {
    }


    
    return 0;
}






namespace sc2 {


void Scenario3::OnGameStart() {
    check_fire_ = false;
}

void Scenario3::OnStep() {
    const ObservationInterface* observation = Observation();
    uint32_t game_loop_ = observation->GetGameLoop();
    
    
    if (game_loop_ < GAMESTART) return;
    // initialize the micro_info_ vector
    else if (game_loop_ == GAMESTART) {
        Units units = observation->GetUnits(Unit::Alliance::Self);
        
        // add all my units to micro_info_ vector
        for (const auto& u: units) {
            sc2::MicroInfo add;
            add.myUnit = u;
            add.move_back_ = false;
            add.search = false;
            add.target = NULL;
            add.original_point_ = u->pos;
            add.game_loop_ = GAMESTART;
            micro_info_.push_back(add);
        }
    }
    // game starts
    else {
        ActionInterface* action = Actions();

        Units my_units_ = observation->GetUnits(Unit::Alliance::Self);

        Units enemy_ = observation->GetUnits(Unit::Alliance::Enemy);

        // if no more my units, then return
        if (my_units_.empty()) return;
        if (enemy_.empty()) return;
        if (!check_fire_) {
            Fire();
            return;
        }

       
        
        // std::cout << "game loop = " << game_loop_ << std::endl;
        for (const auto& unit : my_units_) {

            // find the corresponding unit in micro_info_ vector
            std::vector<MicroInfo>::iterator which_unit_;
            for (which_unit_ = micro_info_.begin(); which_unit_ != micro_info_.end(); which_unit_++) {
                if (which_unit_->myUnit == unit)
                    break;
            }
            // if not found, add the unit to micro_info_ vector
            if (which_unit_ == micro_info_.end()) {
                sc2::MicroInfo add;
                add.myUnit = unit;
                add.move_back_ = false;
                add.search = false;
                add.target = NULL;
                add.original_point_ = unit->pos;
                add.game_loop_ = observation->GetGameLoop();
                micro_info_.push_back(add);
                which_unit_ = --(micro_info_.end());
            }


            // deal with each units
            switch (static_cast<UNIT_TYPEID>(unit->unit_type)) {
                case UNIT_TYPEID::TERRAN_SIEGETANKSIEGED: {
                    if (unit->weapon_cooldown) {
                        action->UnitCommand(unit, ABILITY_ID::BEHAVIOR_HOLDFIREON);
                    }
                    else if (!which_unit_->target) {
                        // get max range enemy first
                        if (!GetMaxRangeEnemy(which_unit_.base()))
                            // then get nearest if max range not found
                            GetRangeEnemy(which_unit_.base());
                    }
                    else
                        action->UnitCommand(unit, ABILITY_ID::ATTACK, which_unit_->target);
                    break;
                }
                default:
                    break;
            }
        }
    }
}
    

void Scenario3::OnUnitDestroyed(const Unit* unit) {
    std::vector<MicroInfo>::iterator which_unit_;
    
    
    // no action if my unit destroyed
    if (unit->alliance == Unit::Alliance::Self) return;
    
    // search which unit targetted the destroyed one
    for (which_unit_ = micro_info_.begin(); which_unit_ != micro_info_.end(); which_unit_++) {
        if (which_unit_->target == unit)
            GetMaxRangeEnemy(which_unit_.base());
    }
}

// focus on the x
void Scenario3::Normalize(Point2D& a) {
    a /= std::sqrt(a.x*a.x+a.y*a.y);
    a.y /= 5.0;
}









// get the enemy at the maximum range
bool Scenario3::GetMaxRangeEnemy(MicroInfo *unit) {
    const ObservationInterface* observation = Observation();
    Units units = observation->GetUnits(Unit::Alliance::Enemy);
    bool found = false;
    
    // return if there are no more enemy
    if (units.empty()) {
        return false;
    }
    

    Point2D mp = unit->myUnit->pos;
    for (const auto& enemy: units) {
        if (Distance2D(mp, enemy->pos) <= 13.0f && Distance2D(mp, enemy->pos) >= 11.5f) {
            unit->target = enemy;
            found = true;
            break;
        }
    }
    
    if (!found)
        unit->target = NULL;
    
    return found;
}


// get any enmey in range
bool sc2::Scenario3::GetRangeEnemy(MicroInfo *unit) {
    const ObservationInterface* observation = Observation();
    Units units = observation->GetUnits(Unit::Alliance::Enemy);

    // return if there are no more enemy
    if (units.empty()) {
        return false;
    }
    
    float distance = std::numeric_limits<float>::max();
    Point2D mp = unit->myUnit->pos;
    Point2D zp;
    bool found = false;
    
    for (const auto& enemy: units) {
        // the minimum range for siege tank is 2.0f
        if (Distance2D(mp, enemy->pos) <= 13.0f && Distance2D(mp, enemy->pos) >= 2.0f) {
            zp = enemy->pos;
            unit->target = enemy;
            distance = Distance2D(mp, zp);
            found = true;
            break;
        }
    }
    
    if (!found)
        unit->target = NULL;
    else {
        // record the backup point
        Vector2D diff = mp - zp;
        // variation of Normalize2D in sc2_commmon.cc
        Normalize(diff);
        unit->backup_target_ = mp + diff * 9.0f;
    }
    
    return found;
}

void sc2::Scenario3::Fire() {
    const ObservationInterface* observation = Observation();
    Units units = observation->GetUnits(Unit::Alliance::Enemy);
    
    for (const auto& enemy : units)
        if (enemy->pos.x <= 126.0f) {
            check_fire_ = true;
            return;
        }
    
}


};
