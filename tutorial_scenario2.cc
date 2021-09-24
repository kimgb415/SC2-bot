
#include <iostream>
#include <string>
#include <algorithm>
#include <random>
#include <iterator>
#define GAMESTART 222

#include "sc2api/sc2_api.h"

using namespace sc2;

namespace sc2 {
struct MicroInfo {
    const Unit* myUnit;
    const Unit* target;
    bool move_back_;
    uint32_t game_loop_;
    Point2D backup_target_;
    Point2D original_point_;
};


class Scenario1 : public Agent {
public:
    virtual void OnGameStart() final;
    virtual void OnStep() final;
    virtual void OnUnitDestroyed(const Unit* unit) override;
private:
    void Normalize(Point2D& a);
    const Unit* GetFrontestStalker();
    const Unit* GetFrontest();
    bool GetNearestEnemy(MicroInfo *unit);
    
    //micro for single unit
    std::vector<MicroInfo> micro_info_;
    const Unit* lastly_loaded_ = NULL;
};



};

int main(int argc, char* argv[]) {
    sc2::Coordinator coordinator;
    if (!coordinator.LoadSettings(argc, argv)) {
        return 1;
    }

    coordinator.SetRealtime(true);
    coordinator.SetWindowSize(5760, 3240);

    // Add the custom bot, it will control the player.
    sc2::Scenario1 bot;
    coordinator.SetParticipants({
        CreateParticipant(sc2::Race::Protoss, &bot),
        CreateComputer(sc2::Race::Zerg),
    });

    // Start the game.
    coordinator.LaunchStarcraft();
    coordinator.StartGame(sc2::Project2);

    while (coordinator.Update()) {
    }


    
    return 0;
}






namespace sc2 {


void Scenario1::OnGameStart() {

}

void Scenario1::OnStep() {
    const ObservationInterface* observation = Observation();
    uint32_t game_loop_ = observation->GetGameLoop();
    
    std::cout << game_loop_ << std::endl;
    
    
    if (game_loop_ < GAMESTART) return;
    // initialize the micro_info_ vector
    else if (game_loop_ == GAMESTART) {
        Units units = observation->GetUnits(Unit::Alliance::Self);
        
        // add all my units to micro_info_ vector
        for (const auto& u: units) {
            sc2::MicroInfo add;
            add.myUnit = u;
            add.move_back_ = false;
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

        // record the frontest attackable unit for the gameloop
        const Unit* frontest_unit_ = GetFrontest();
        const Unit* frontest_stalker_ = GetFrontestStalker();


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
                add.target = NULL;
                add.original_point_ = unit->pos;
                add.game_loop_ = observation->GetGameLoop();
                micro_info_.push_back(add);
                which_unit_ = --(micro_info_.end());
            }


            // deal with each units
            switch (static_cast<UNIT_TYPEID>(unit->unit_type)) {
                case UNIT_TYPEID::PROTOSS_STALKERPURIFIER:
                case UNIT_TYPEID::PROTOSS_STALKER:
                    GetNearestEnemy(which_unit_.base());
                    if (unit->weapon_cooldown < 3.0f) {
                        action->UnitCommand(unit, ABILITY_ID::ATTACK, which_unit_->target);

                    }
                    else if (unit->weapon_cooldown >= 18.0f) {
                        action->UnitCommand(unit, unit->unit_type == UNIT_TYPEID::PROTOSS_STALKER ? ABILITY_ID::EFFECT_BLINK : ABILITY_ID::EFFECT_BLINK_MULTIPLE, (which_unit_->backup_target_+unit->pos*2)/3);
                        which_unit_->game_loop_ = game_loop_;
                    }
                    else
                        action->UnitCommand(unit, ABILITY_ID::SMART, which_unit_->backup_target_);
                    break;

                case UNIT_TYPEID::PROTOSS_COLOSSUSPURIFIER:
                    GetNearestEnemy(which_unit_.base());
                    // go back and attack if weapon is almost cooled down
                    if (unit->weapon_cooldown < 2.0f)
                        action->UnitCommand(unit, ABILITY_ID::ATTACK, which_unit_->target);
                    else action->UnitCommand(unit, ABILITY_ID::SMART, which_unit_->backup_target_);

                    break;


                default:
                    break;
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
        if (which_unit_->myUnit->unit_type != UNIT_TYPEID::PROTOSS_STALKER && which_unit_->myUnit->unit_type != UNIT_TYPEID::PROTOSS_STALKERPURIFIER)
            continue;
        if (which_unit_->target == unit) {
            which_unit_->move_back_ = false;
            // re-get the target
            GetNearestEnemy(which_unit_.base());
        }
    }
}

// focus on the x
void Scenario1::Normalize(Point2D& a) {
    a /= std::sqrt(a.x*a.x+a.y*a.y);
    a.y /= 5.0;
}


// return the frontest stalker
const Unit* Scenario1::GetFrontestStalker() {
    const ObservationInterface* observation = Observation();
    std::vector<MicroInfo>::iterator which_unit_;
    MicroInfo *frontest_stalker_;
    float distance = std::numeric_limits<float>::min();
    for (which_unit_ = micro_info_.begin(); which_unit_ != micro_info_.end(); which_unit_++) {
        if (which_unit_->myUnit->unit_type == UNIT_TYPEID::PROTOSS_STALKER || which_unit_->myUnit->unit_type == UNIT_TYPEID::PROTOSS_STALKERPURIFIER) {
            // the frontest one should be alive
            if (which_unit_->myUnit->pos.x > distance && which_unit_->myUnit->is_alive) {
                distance = which_unit_->myUnit->pos.x;
                frontest_stalker_ = which_unit_.base();
            }
        }
    }
    return frontest_stalker_->myUnit;
}


// get frontest attackable unit
const Unit* Scenario1::GetFrontest() {
    const ObservationInterface* observation = Observation();
    std::vector<MicroInfo>::iterator which_unit_;
    MicroInfo *frontest_;
    float distance = std::numeric_limits<float>::min();
    bool found = false;
    for (which_unit_ = micro_info_.begin(); which_unit_ != micro_info_.end(); which_unit_++) {
        if (which_unit_->myUnit->unit_type == UNIT_TYPEID::PROTOSS_COLOSSUSPURIFIER || which_unit_->myUnit->unit_type == UNIT_TYPEID::PROTOSS_STALKER || which_unit_->myUnit->unit_type == UNIT_TYPEID::PROTOSS_STALKERPURIFIER) {
            found = true;
            // the frontest one should be alive
            if (which_unit_->myUnit->pos.x > distance && which_unit_->myUnit->is_alive) {
                distance = which_unit_->myUnit->pos.x;
                frontest_ = which_unit_.base();
            }
        }
    }
    
    if (!found) return NULL;
    else return frontest_->myUnit;
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
        if (d < distance && unit->myUnit->pos.x < enemy->pos.x) {
            distance = d;
            unit->target = enemy;
            zp = enemy->pos;
        }
    }
    
    // record the backup point
    Vector2D diff = mp - zp;
    // variation of Normalize2D in sc2_commmon.cc
    Normalize(diff);
    unit->backup_target_ = mp + diff * 9.0f;
    
    return true;
}


};
