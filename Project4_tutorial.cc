
#include <iostream>
#include <string>
#include <algorithm>
#include <random>
#include <iterator>
#define GAMESTART 160
#define GROUP 3
#define PI 3.1415926535

#include "sc2api/sc2_api.h"
#include "sc2utils/sc2_manage_process.h"

using namespace sc2;

namespace sc2 {
struct MicroInfo {
    const Unit* myUnit;
    const Unit* target;
    Point2D backup_target_;
};

enum GameState {
    Phase0,
    Phase1,
    Phase2,
    Phase2Attack,
    Phase2Start,
    Phase3,
    Finish,
};


class Scenario3 : public Agent {
public:
    virtual void OnGameStart() final;
    virtual void OnStep() final;
    virtual void OnUnitDestroyed(const Unit* unit) override;
private:
    void Normalize(Point2D& a);
    float DistToLine(Point2D start, Point2D end, Point2D unit);
    bool GetNearestEnemy(MicroInfo* unit);
    void GetAvgPosition();
    bool GatherUp(Units units);

    
    //micro for single unit
    std::vector<MicroInfo> micro_info_;
    Point2D avg_pos_;
    Point2D avg_ene_pos_;
    int cyclone_num_;
    int hellion_num_;
    uint32_t last_loop_ = 0;
    GameState state;
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
    avg_pos_ = Point2D(0.0f, 0.0f);
    state = Phase0;
}

void Scenario3::OnStep() {
    const ObservationInterface* observation = Observation();
    QueryInterface* query = Query();
    DebugInterface* debug = Debug();
    ActionInterface* action = Actions();
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
            add.target = nullptr;
            micro_info_.push_back(add);
        }
        state = Phase3;
        
    }
    // game starts
    else if (state == Phase1) {

        
        Units my_units_ = observation->GetUnits(Unit::Alliance::Self);
        Units enemy_ = observation->GetUnits(Unit::Alliance::Enemy);
        
        if (enemy_.empty()) {
            state = Phase2;
            action->UnitCommand(my_units_, ABILITY_ID::STOP);
            last_loop_ = game_loop_;
            return;
        }


//        for (const auto& unit : my_units_) {
//
//            // find the corresponding unit in micro_info_ vector
//            std::vector<MicroInfo>::iterator which_unit_;
//            for (which_unit_ = micro_info_.begin(); which_unit_ != micro_info_.end(); which_unit_++) {
//                if (which_unit_->myUnit == unit)
//                    break;
//            }
//
//            // deal with each units
//            switch (static_cast<UNIT_TYPEID>(unit->unit_type)) {
//                case UNIT_TYPEID::TERRAN_CYCLONE:
//
//                    break;
//                default:
//                    std::cout << unit->unit_type << std::endl;
//                    break;
//            }
//        }
        
        std::vector<MicroInfo>::iterator which_unit_;
        std::cout << game_loop_ << std::endl;
        for (which_unit_ = micro_info_.begin(); which_unit_ != micro_info_.end(); which_unit_++) {
            if (which_unit_->myUnit->is_alive) {
                // in case of cyclone
                if (which_unit_->myUnit->unit_type == UNIT_TYPEID::TERRAN_CYCLONE) {
                    
                    AvailableAbilities abils = query->GetAbilitiesForUnit(which_unit_->myUnit);
                    bool found = false;
                    // figure out if the effect_lockon is available
                    for (const auto& ability : abils.abilities) {
                        if (ability.ability_id == ABILITY_ID::EFFECT_LOCKON) {
                            found = true;
                            break;
                        }
                    }
                    // effect lock on the nearest target enemy
                    if (found) {
                        if (GetNearestEnemy(which_unit_.base()));
                            action->UnitCommand(which_unit_->myUnit, ABILITY_ID::EFFECT_LOCKON, which_unit_->target);
                    }
                    else {
                        // keep safe distance with the unit
                        if (which_unit_->target && which_unit_->target->is_alive) {
                            if (Distance2D(which_unit_->myUnit->pos, which_unit_->target->pos) < 12.0f)
                                action->UnitCommand(which_unit_->myUnit, ABILITY_ID::SMART, which_unit_->backup_target_);
                            else if (Distance2D(which_unit_->myUnit->pos, which_unit_->target->pos) > 15.0f)
                                action->UnitCommand(which_unit_->myUnit, ABILITY_ID::SMART, which_unit_->target->pos);
                        }
                        // when target unit is dead
                        else {
                            GetNearestEnemy(which_unit_.base());
                            if (which_unit_->target->unit_type == UNIT_TYPEID::ZERG_ROACH || which_unit_->target->unit_type == UNIT_TYPEID::ZERG_RAVAGER) {
                                if (Distance2D(which_unit_->myUnit->pos, which_unit_->target->pos) < 10.0f)
                                    action->UnitCommand(which_unit_->myUnit, ABILITY_ID::SMART, which_unit_->backup_target_);
                                else if (Distance2D(which_unit_->myUnit->pos, which_unit_->target->pos) > 12.0f)
                                    action->UnitCommand(which_unit_->myUnit, ABILITY_ID::STOP);
                            }
                            else {
                                if (which_unit_->myUnit->weapon_cooldown)
                                    action->UnitCommand(which_unit_->myUnit, ABILITY_ID::SMART, which_unit_->backup_target_);
                                else
                                    action->UnitCommand(which_unit_->myUnit, ABILITY_ID::ATTACK, which_unit_->target);
                            }
                        }
                        
                    }
                }
                // in case of hellion
                else {
                    GetNearestEnemy(which_unit_.base());
                    if (which_unit_->myUnit->weapon_cooldown)
                        action->UnitCommand(which_unit_->myUnit, ABILITY_ID::SMART, which_unit_->backup_target_);
                    else
                        action->UnitCommand(which_unit_->myUnit, ABILITY_ID::ATTACK, which_unit_->target);
                }
            }
        }
    }
    // phase 2 start
    else if (state == Phase2) {
        Units enemy = observation->GetUnits(Unit::Alliance::Enemy);
        if (!enemy.empty()) {
            state = Phase2Attack;
            last_loop_ = game_loop_;
        }
    }
    
    // phase 2: zergling attack from 360
    else if (state == Phase2Attack) {
        Units enemy_ = observation->GetUnits(Unit::Alliance::Enemy);
        Units cyclones = observation->GetUnits(Unit::Alliance::Self, [] (const Unit& unit) {return unit.unit_type == UNIT_TYPEID::TERRAN_CYCLONE ? 1 : 0;});
        Units hellions_ = observation->GetUnits(Unit::Alliance::Self, [] (const Unit& unit) {return unit.unit_type == UNIT_TYPEID::TERRAN_HELLION ? 1 : 0;});
        cyclone_num_ = cyclones.size();
        hellion_num_ = hellions_.size();
        

        
        // form up the troops
        if (avg_pos_ == Point2D(0.0f, 0.0f))
            GetAvgPosition();
        int i = 0, j = 0;
        std::vector<MicroInfo>::iterator which_unit_;
        for (which_unit_ = micro_info_.begin(); which_unit_ != micro_info_.end(); which_unit_++) {
            Point2D point;
            switch (static_cast<UNIT_TYPEID>(which_unit_->myUnit->unit_type)) {
                case UNIT_TYPEID::TERRAN_CYCLONE:
                    point = 2.0f * Point2D(cos(2*PI*i/cyclone_num_), sin(2*PI*i/cyclone_num_)) + avg_pos_;
                    i += 1;
                    break;
                case UNIT_TYPEID::TERRAN_HELLION:
                    point = 3.3f * Point2D(cos(2*PI*j/hellion_num_), sin(2*PI*j/hellion_num_)) + avg_pos_;
                    j += 1;
                    break;
                default:
                    break;
            }
            if (Distance2D(which_unit_->myUnit->pos, point) > 0.3f)
                action->UnitCommand(which_unit_->myUnit, ABILITY_ID::SMART, point);
        }
        
        if (game_loop_ - last_loop_ > 100)
            state = Phase2Start;
    }
    else if (state == Phase2Start) {
        Units enemy = observation->GetUnits(Unit::Alliance::Enemy);
        if (enemy.empty()) {
            state = Phase3;
            last_loop_ = game_loop_;
        }
    }
    
    else if (state == Phase3) {
        std::vector<Effect> effs = observation->GetEffects();
        std::vector<std::vector<Point2D>> effs_lines;
        std::cout << game_loop_ << std::endl;
        // add all lurker spine lines to effs_lines vector
        // each lurker spine has length of 8.0f, consisting of 9 points in total with 1.0f separation
        // each luerker spine lasts for 15 game loops
        for (const auto& eff: effs) {
            if (eff.effect_id == 12)
                effs_lines.push_back(eff.positions);
        }
//        Units my_units_ = observation->GetUnits(Unit::Alliance::Self, [] (const Unit &unit) {return (unit.unit_type == UNIT_TYPEID::TERRAN_CYCLONE) ? 1 : 0;});
        Units my_units_ = observation->GetUnits(Unit::Alliance::Self);
        Units enemy_ = observation->GetUnits(Unit::Alliance::Enemy);
        
        if (enemy_.empty()) {
            action->UnitCommand(my_units_, ABILITY_ID::SMART, Point2D(221.0f, 48.0f));
            state = Finish;
            return;
        }
        
        avg_ene_pos_ = Point2D(0.0f, 0.0f);
        int enemy_num_ = 0;
        for (const auto& enemy: enemy_) {
            avg_ene_pos_ += enemy->pos;
            enemy_num_++;
        }
        avg_ene_pos_ /= enemy_num_;
        
        std::vector<MicroInfo>::iterator which_unit;
        for (which_unit = micro_info_.begin(); which_unit != micro_info_.end(); which_unit++) {
            GetNearestEnemy(which_unit.base());
            const Unit* unit = which_unit->myUnit;
            bool attack = true;
            switch (static_cast<UNIT_TYPEID>(unit->unit_type)) {
                case UNIT_TYPEID::PROTOSS_STALKER: {
                    for (const auto& line: effs_lines) {
                        Point2D start = line.front();
                        Point2D end = line.back();
                        float dist = DistToLine(start, end, unit->pos);
                        float slope = (end.y - start.y) / (end.x - start.x);
                        // if the unit is close enough to the lurker spine
                        if (abs(dist) <= 2.5f) {
                            // since lurker spine is a line segment
                            if ((unit->pos.x <= start.x && unit->pos.x >= end.x-0.5f) || (unit->pos.x >= start.x && unit->pos.x <= end.x+0.5f)) {
                                // move away from the lurker spine
                                Vector2D away;
                                if (abs(start.x - end.x) < 0.05f)
                                    away = Point2D(-1.0f, 0.0f);
                                else if ((dist >= 0 && slope > 0) || (dist < 0 && slope < 0))
                                    away = Point2D(-1.0f, 1/slope);
                                else
                                    away = Point2D(1.0f, -1/slope);
                                Normalize2D(away);
                                attack = false;
                                AvailableAbilities abils = query->GetAbilitiesForUnit(unit);
                                bool found = false;
                                // figure out if the blink is available
                                for (const auto& ability : abils.abilities) {
                                    if (ability.ability_id == ABILITY_ID::EFFECT_BLINK) {
                                        found = true;
                                        break;
                                    }
                                }
                                action->UnitCommand(unit, found ? ABILITY_ID::EFFECT_BLINK : ABILITY_ID::SMART, unit->pos + away * 2.5f);
                                break;
                            }
                        }
                    }
                    if (attack && which_unit->target)
                        action->UnitCommand(unit, ABILITY_ID::ATTACK, which_unit->target);
                }
                    break;
                case UNIT_TYPEID::TERRAN_REAPER: {
                    for (const auto& line: effs_lines) {
                        Point2D start = line.front();
                        Point2D end = line.back();
                        float dist = DistToLine(start, end, unit->pos);
                        float slope = (end.y - start.y) / (end.x - start.x);
                        // if the unit is close enough to the lurker spine
                        if (abs(dist) <= 2.0f) {
                            // since lurker spine is a line segment
                            if ((unit->pos.x <= start.x && unit->pos.x >= end.x-0.5f) || (unit->pos.x >= start.x && unit->pos.x <= end.x+0.5f)) {
                                // move away from the lurker spine
                                Vector2D away;
                                if (abs(start.x - end.x) < 0.05f)
                                    away = Point2D(-1.0f, 0.0f);
                                else if ((dist >= 0 && slope > 0) || (dist < 0 && slope < 0))
                                    away = Point2D(-1.0f, 1/slope);
                                else
                                    away = Point2D(1.0f, -1/slope);
                                Normalize2D(away);
                                attack = false;
                                action->UnitCommand(unit, ABILITY_ID::SMART, unit->pos + away * 2.0f);
                                break;
                            }
                        }
                    }
                    if (attack && which_unit->target) {
                        AvailableAbilities abils = query->GetAbilitiesForUnit(unit);
                        bool found = false;
                        // figure out if the blink is available
                        for (const auto& ability : abils.abilities) {
                            if (ability.ability_id == ABILITY_ID::EFFECT_KD8CHARGE) {
                                found = true;
                                break;
                            }
                        }
                        action->UnitCommand(unit, found ? ABILITY_ID::EFFECT_KD8CHARGE : ABILITY_ID::ATTACK, which_unit->target);
                    }
                }
                    break;
                default: {
                    if (unit->buffs.empty())
                        action->UnitCommand(unit, ABILITY_ID::EFFECT_STIM);
                    for (const auto& line: effs_lines) {
                        Point2D start = line.front();
                        Point2D end = line.back();
                        float dist = DistToLine(start, end, unit->pos);
                        float slope = (end.y - start.y) / (end.x - start.x);
                        // if the unit is close enough to the lurker spine
                        if (abs(dist) <= 2.5f) {
                            // since lurker spine is a line segment
                            if ((unit->pos.x <= start.x && unit->pos.x >= end.x-0.5f) || (unit->pos.x >= start.x && unit->pos.x <= end.x+0.5f)) {
                                // move away from the lurker spine
                                Vector2D away;
                                if (abs(start.x - end.x) < 0.05f)
                                    away = Point2D(-1.0f, 0.0f);
                                else if ((dist >= 0 && slope > 0) || (dist < 0 && slope < 0))
                                    away = Point2D(-1.0f, 1/slope);
                                else
                                    away = Point2D(1.0f, -1/slope);
                                Normalize2D(away);
                                attack = false;
                                action->UnitCommand(unit, ABILITY_ID::SMART, unit->pos + away * 5.0f);
                                break;
                            }
                        }
                    }
                    if (attack && which_unit->target)
                        action->UnitCommand(unit, ABILITY_ID::ATTACK, which_unit->target);
                }
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
    a.y /= 10;
}

float Scenario3::DistToLine(Point2D start, Point2D end, Point2D unit) {
    // line of form ax + by + c = 0
    // if start point is right
    float a = - (start.y - end.y);
    float b = start.x - end.x;
    float c = end.x * start.y - start.x * end.y;
    // if start point is left
    if (start.x < end.x) {
        a *= -1;
        b *= -1;
        c *= -1;
    }
    
    return (a * unit.x + b * unit.y + c) / sqrt(a*a + b*b);
}

// get the enmey farthest from the center of the enemy
bool sc2::Scenario3::GetNearestEnemy(MicroInfo* unit) {
    const ObservationInterface* observation = Observation();

    Units units = observation->GetUnits(Unit::Alliance::Enemy);

    // return if there are no more enemy
    if (units.empty()) {
        return false;
    }
    
    float distance = std::numeric_limits<float>::max();
    
    for (const auto& enemy: units) {
        float d = Distance2D(unit->myUnit->pos, enemy->pos);
        if (distance > d) {
            distance = d;
            unit->target = enemy;
        }
    }
    
    Vector2D diff = unit->myUnit->pos - unit->target->pos;
    Normalize(diff);
    unit->backup_target_ = unit->myUnit->pos + diff * 20.0f;
    
    

    return true;
}

void Scenario3::GetAvgPosition() {
    const ObservationInterface* observation = Observation();

    Units cyclones = observation->GetUnits(Unit::Alliance::Self, [] (const Unit& unit) {return unit.unit_type == UNIT_TYPEID::TERRAN_CYCLONE ? 1 : 0;});
    
    avg_pos_ = Point2D(0.0f, 0.0f);
    int num = 0;
    for (const auto& unit: cyclones) {
        avg_pos_ += unit->pos;
        num++;
    }
    avg_pos_ /= num;
    
}


bool Scenario3::GatherUp(Units units) {
    for (const auto & unit: units) {
        if (Distance2D(unit->pos, avg_pos_) > 3.0f)
            return true;
    }
    return false;
}

};
