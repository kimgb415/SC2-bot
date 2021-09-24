
#include <iostream>
#include <string>
#include <algorithm>
#include <random>
#include <iterator>
#define GAMESTART 128

#include "sc2api/sc2_api.h"

using namespace sc2;

namespace sc2 {
struct MicroInfo {
    const Unit* myUnit;
    const Unit* target;
    int height;
    Point2D backup_target_;
};


class Scenario3 : public Agent {
public:
    virtual void OnGameStart() final;
    virtual void OnStep() final;
    virtual void OnUnitDestroyed(const Unit* unit) override;
private:
    void Normalize(Point2D& a);
    bool GetEnemy(MicroInfo *unit);
    void GetNearestEnemy(MicroInfo *unit);
    void GetFrontestColossus(MicroInfo *unit);


    
    //micro for single unit
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
}

void Scenario3::OnStep() {
    const ObservationInterface* observation = Observation();
    uint32_t game_loop_ = observation->GetGameLoop();
    
    
    if (game_loop_ < GAMESTART) {
        // std::cout << "game loop = " << game_loop_ << std::endl;
        return;
    }
    // initialize the micro_info_ vector
    else if (game_loop_ == GAMESTART) {
        Units units = observation->GetUnits(Unit::Alliance::Self);
        
        // add all my units to micro_info_ vector
        for (const auto& u: units) {
            sc2::MicroInfo add;
            add.myUnit = u;
            add.target = NULL;
            add.backup_target_ = Point2D(0, u->pos.y);
            if (u->unit_type == UNIT_TYPEID::PROTOSS_COLOSSUS) {
                if (u->pos.y > 67.0f)
                    add.height = 6;
                else if (u->pos.y > 64.0f)
                    add.height = 5;
                else if (u->pos.y > 62.0f)
                    add.height = 3;
                else if (u->pos.y > 59.0f)
                    add.height = 2;
                else
                    add.height = 1;
            }
            else {
                if (u->pos.y > 65.0f)
                    add.height = 4;
                else if (u->pos.y > 62.0f)
                    add.height = 2;
                else
                    add.height = 0;
            }
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

       
        
        
        for (const auto& unit : my_units_) {

            // find the corresponding unit in micro_info_ vector
            std::vector<MicroInfo>::iterator which_unit_;
            for (which_unit_ = micro_info_.begin(); which_unit_ != micro_info_.end(); which_unit_++) {
                if (which_unit_->myUnit == unit)
                    break;
            }

            // deal with each units
            switch (static_cast<UNIT_TYPEID>(unit->unit_type)) {
                case UNIT_TYPEID::PROTOSS_COLOSSUS:
                    if (unit->weapon_cooldown > 17.0f)
                        continue;
                    if (unit->weapon_cooldown > 8.0f)
                        action->UnitCommand(unit, ABILITY_ID::SMART, which_unit_->backup_target_);
                    else if (!which_unit_->target) {
                        if (!GetEnemy(which_unit_.base()))
                            GetNearestEnemy(which_unit_.base());
                    }
                    else
                        action->UnitCommand(unit, ABILITY_ID::ATTACK, which_unit_->target);
                    break;
                case UNIT_TYPEID::PROTOSS_WARPPRISM:
                    GetFrontestColossus(which_unit_.base());
                    // unload if there is any
                    if (unit->cargo_space_taken) {
                        if (unit->orders.empty() || unit->orders.begin()->ability_id != ABILITY_ID::UNLOADALLAT)
                            action->UnitCommand(unit, ABILITY_ID::UNLOADALLAT, unit);
                    }
                    else if (which_unit_->target->weapon_cooldown < 15.0f && which_unit_->target->weapon_cooldown > 11.0f)
                        action->UnitCommand(unit, ABILITY_ID::LOAD, which_unit_->target);
                    else if (Distance2D(unit->pos, which_unit_->target->pos) > 5.5f && unit->pos.x < which_unit_->target->pos.x)
                        action->UnitCommand(unit, ABILITY_ID::STOP);
                    else
                        action->UnitCommand(unit, ABILITY_ID::SMART, which_unit_->backup_target_);
                    break;
                    
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
            which_unit_->target = nullptr;
    }
}

// focus on the x
void Scenario3::Normalize(Point2D& a) {
    a /= std::sqrt(a.x*a.x+a.y*a.y);
    a.y /= 4;
}

bool sc2::Scenario3::GetEnemy(MicroInfo *unit) {
    const ObservationInterface* observation = Observation();
    Units units = observation->GetUnits(Unit::Alliance::Enemy);

    // return if there are no more enemy
    if (units.empty()) {
        return false;
    }
    
    bool found = false;
    for (const auto& enemy: units) {
        if (Distance2D(unit->myUnit->pos, enemy->pos) == 7.0f) {
            unit->target = enemy;
            found = true;
            break;
        }
    }

    return found;
}



// get any enmey in range
void sc2::Scenario3::GetNearestEnemy(MicroInfo *unit) {
    const ObservationInterface* observation = Observation();
    Units units = observation->GetUnits(Unit::Alliance::Enemy);

    // return if there are no more enemy
    if (units.empty()) {
        return;
    }
    
    float distance = std::numeric_limits<float>::max();
    Point2D mp = unit->myUnit->pos;
    Point2D zp;
    
    for (const auto& enemy: units) {
        if (distance > Distance2D(mp, enemy->pos)) {
            zp = enemy->pos;
            unit->target = enemy;
            distance = Distance2D(mp, zp);
        }
    }

    return;
}

void sc2::Scenario3::GetFrontestColossus(MicroInfo *unit) {
    const ObservationInterface* observation = Observation();
    // get my colossus
    Units units = observation->GetUnits(Unit::Alliance::Self, [] (const Unit& unit) {return unit.unit_type == UNIT_TYPEID::PROTOSS_COLOSSUS ? 1 : 0;});

    // return if there are no more enemy
    if (units.empty()) {
        return;
    }
    
    float frontest = std::numeric_limits<float>::min();
    for (const auto& u: micro_info_) {
        if (u.myUnit->unit_type == UNIT_TYPEID::PROTOSS_COLOSSUS)
            if (u.myUnit->pos.x > frontest)
                if (u.height - unit->height > 0 && u.height - unit->height <= 2) {
                    frontest = u.myUnit->pos.x;
                    unit->target = u.myUnit;
                }
    }
}


};
