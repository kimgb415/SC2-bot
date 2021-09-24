
#include <iostream>
#include <string>
#include <algorithm>
#include <random>
#include <iterator>
#define GAMESTART 140

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


class Scenario3 : public Agent {
public:
    virtual void OnGameStart() final;
    virtual void OnStep() final;
    virtual void OnUnitDestroyed(const Unit* unit) override;
private:
    void Normalize(Point2D& a);
    void GetNearestEnemy(MicroInfo *unit);


    
    //micro for single unit
    std::vector<MicroInfo> micro_info_;
    Point2D back = Point2D(2.33, 64.13);
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
    coordinator.StartGame(sc2::Project3Marine);

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
        if (enemy_.empty() && game_loop_ < 250) return;

       
        std::cout << game_loop_ << std::endl;

        for (const auto& unit : my_units_) {
            if (enemy_.empty()) {
                action->UnitCommand(unit, ABILITY_ID::STOP);
                continue;
            }
            if (unit->buffs.empty())
                action->UnitCommand(unit, ABILITY_ID::EFFECT_STIM);
            

            // find the corresponding unit in micro_info_ vector
            std::vector<MicroInfo>::iterator which_unit_;
            for (which_unit_ = micro_info_.begin(); which_unit_ != micro_info_.end(); which_unit_++) {
                if (which_unit_->myUnit == unit)
                    break;
            }


            // deal with each units
            switch (static_cast<UNIT_TYPEID>(unit->unit_type)) {
                case UNIT_TYPEID::TERRAN_MARINE:
                    
                    GetNearestEnemy(which_unit_.base());
                    if (unit->pos.y < 53.0f || unit->pos.y > 75.0f)
                        action->UnitCommand(unit, ABILITY_ID::SMART, Point2D(unit->pos.x-20.0f, 64.0f));
                    else if (unit->weapon_cooldown && Distance2D(unit->pos, which_unit_->target->pos) < 10.0f)
                        action->UnitCommand(unit, ABILITY_ID::SMART, which_unit_->backup_target_);
                    else {
                        action->UnitCommand(unit, ABILITY_ID::ATTACK, which_unit_->target);
                    }
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
            GetNearestEnemy(which_unit_.base());
    }
}

// focus on the x
void Scenario3::Normalize(Point2D& a) {
    a /= std::sqrt(a.x*a.x+a.y*a.y);
    a.y /= 10;
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
        if (distance > Distance2D(mp, enemy->pos) && enemy->pos.x > mp.x) {
            zp = enemy->pos;
            unit->target = enemy;
            distance = Distance2D(mp, zp);
        }
    }
    
    
    // record the backup point
    Vector2D diff = mp - zp;
    // variation of Normalize2D in sc2_commmon.cc
    Normalize(diff);
    unit->backup_target_ = mp + diff * 9.0f;
    
    
    return;
}




};
