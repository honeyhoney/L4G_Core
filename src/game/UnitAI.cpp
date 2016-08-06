/*
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * Copyright (C) 2008-2009 Trinity <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "UnitAI.h"
#include "Cell.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Player.h"
#include "Creature.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "CreatureAIImpl.h"
bool TopAggroFlaggedUnattackable;

void UnitAI::AttackStart(Unit *victim)
{
    if (victim && me->Attack(victim, true))
        me->GetMotionMaster()->MoveChase(victim);
}

void UnitAI::AttackStartCaster(Unit *victim, float dist)
{
    if (victim && me->Attack(victim, false))
        me->GetMotionMaster()->MoveChase(victim, dist);
}

void UnitAI::DoMeleeAttackIfReady()
{
    if (me->hasUnitState(UNIT_STAT_CANNOT_AUTOATTACK))
        return;

    // set selection back to attacked victim if not selected (after spell casting)
    if(me->GetTypeId() == TYPEID_UNIT)
    {
        if(((Creature*)me)->GetSelection() != me->getVictimGUID() && !((Creature*)me)->hasIgnoreVictimSelection())
            ((Creature*)me)->SetSelection(me->getVictimGUID());
    }
				
	FlagInvalidTargetsUnattackable();

	//Make sure our attack is ready and we aren't currently casting before checking distance
	if (me->isAttackReady())
	{		
		//If we are within range melee the target
		if (me->IsWithinMeleeRange(me->getVictim()))
		{
			me->AttackerStateUpdate(me->getVictim());
			me->resetAttackTimer();
		}
	}
	if (me->haveOffhandWeapon() && me->isAttackReady(OFF_ATTACK))
	{	
		//If we are within range melee the target
		if (me->IsWithinMeleeRange(me->getVictim()))
		{		
			me->AttackerStateUpdate(me->getVictim(), OFF_ATTACK);
			me->resetAttackTimer(OFF_ATTACK);
		}
	}
}

bool UnitAI::DoSpellAttackIfReady(uint32 spell)
{
    if (me->hasUnitState(UNIT_STAT_CASTING))
        return true;

    if (me->isAttackReady())
    {
        const SpellEntry * spellInfo = GetSpellStore()->LookupEntry(spell);
        if (me->IsWithinCombatRange(me->getVictim(), SpellMgr::GetSpellMaxRange(spellInfo)))
        {
            me->CastSpell(me->getVictim(), spell, false);
            me->resetAttackTimer();
        }
        else
            return false;
    }
    return true;
}

inline bool SelectTargetHelper(const Unit * me, const Unit * target, const bool &playerOnly, const float &dist, const int32 &aura)
{
    if (playerOnly && (!target || target->GetTypeId() != TYPEID_PLAYER))
        return false;

    if (dist && (!me || !target || !me->IsWithinCombatRange(target, dist)))
        return false;

    if (aura)
    {
        if (aura > 0)
        {
            if (!target->HasAura(aura,0))
                return false;
        }
        else
        {
            if (target->HasAura(aura,0))
                return false;
        }
    }

    return true;
}

Unit *UnitAI::ReturnTargetHelper(SelectAggroTarget targetType, uint32 position, std::list<Unit*> &targetList)
{
    if (position >= targetList.size())
        return NULL;

    switch (targetType)
    {
        // list has been already sorted, so there is no need to use reversed iterator
        case SELECT_TARGET_NEAREST:
        case SELECT_TARGET_FARTHEST:
        case SELECT_TARGET_BOTTOMAGGRO:
        case SELECT_TARGET_TOPAGGRO:
        {
             std::list<Unit*>::iterator itr = targetList.begin();
             std::advance(itr, position);
             return *itr;
        }
        case SELECT_TARGET_RANDOM:
        {
            std::list<Unit*>::iterator itr = targetList.begin();
            std::advance(itr, urand(position, targetList.size() - 1));
            return *itr;
        }
        case SELECT_TARGET_LOWEST_HP:
        {
            Unit* lowestHpTarget = targetList.front();
            Unit* tmpUnit = NULL;
            uint32 lowestHp = lowestHpTarget->GetHealth();

            for (std::list<Unit*>::const_iterator itr = targetList.begin(); itr != targetList.end(); ++itr)
            {
                tmpUnit = *itr;

                if (tmpUnit->GetHealth() < lowestHp)
                {
                    lowestHpTarget = tmpUnit;
                    lowestHp = tmpUnit->GetHealth();
                }
            }

            return lowestHpTarget;
        }
        case SELECT_TARGET_HIGHEST_HP:
        {
            Unit* highestHpTarget = targetList.front();
            Unit* tmpUnit = NULL;
            uint32 highestHp = highestHpTarget->GetHealth();

            for (std::list<Unit*>::const_iterator itr = targetList.begin(); itr != targetList.end(); ++itr)
            {
                tmpUnit = *itr;

                if (tmpUnit->GetHealth() > highestHp)
                {
                    highestHpTarget = tmpUnit;
                    highestHp = tmpUnit->GetHealth();
                }
            }

            return highestHpTarget;
        }
        default:
            break;
    }
    return NULL;
}

Unit* UnitAI::SelectUnit(SelectAggroTarget targetType, uint32 position, float max_dist, bool playerOnly, uint64 excludeGUID, float min_dist)
{
    std::list<Unit*> targetList;
    SelectUnitList(targetList, 0, targetType, max_dist, playerOnly, excludeGUID, min_dist);

    if (targetList.empty())
        return NULL;

    return ReturnTargetHelper(targetType, position, targetList);
}

Unit* UnitAI::SelectUnit(SelectAggroTarget targetType, uint32 position, float max_dist, bool playerOnly, Powers power)
{
    std::list<Unit*> targetList;
    SelectUnitList(targetList, 0, targetType, max_dist, playerOnly);

    if (targetList.empty())
        return NULL;

    targetList.remove_if(Looking4group::UnitPowerTypeCheck(power, false));

    return ReturnTargetHelper(targetType, position, targetList);
}

void UnitAI::SelectUnitList(std::list<Unit*> &targetList, uint32 num, SelectAggroTarget targetType, float max_dist, bool playerOnly, uint64 excludeGUID, float min_dist)
{
    const std::list<HostilReference*> &threatlist = me->getThreatManager().getThreatList();
    for (std::list<HostilReference*>::const_iterator itr = threatlist.begin(); itr != threatlist.end(); ++itr)
        targetList.push_back((*itr)->getTarget());

    if (playerOnly)
        targetList.remove_if(Looking4group::ObjectTypeIdCheck(TYPEID_PLAYER, false));

    if (excludeGUID)
        targetList.remove_if(Looking4group::ObjectGUIDCheck(excludeGUID));

    if (min_dist)
        targetList.remove_if(Looking4group::ObjectDistanceCheck(me, min_dist, false));

    if (max_dist)
        targetList.remove_if(Looking4group::ObjectDistanceCheck(me, max_dist, true));

    if (num == 0)
        num = targetList.size();

    if (targetList.empty())
        return;

    switch (targetType)
    {
        case SELECT_TARGET_NEAREST:
            targetList.sort(Looking4group::ObjectDistanceOrder(me));
            break;
        case SELECT_TARGET_FARTHEST:
            targetList.sort(Looking4group::ObjectDistanceOrder(me));
        case SELECT_TARGET_BOTTOMAGGRO:
        {
            targetList.reverse();
            break;
        }
        case SELECT_TARGET_RANDOM:
        {
            std::list<Unit*>::iterator i;
            while (targetList.size() > num)
            {
                i = targetList.begin();
                advance(i, urand(0, targetList.size()-1));
                targetList.erase(i);
            }
        }
        default:
            break;
    }

    // already resized
    if (targetType != SELECT_TARGET_RANDOM)
    {
        if (targetList.size() > num)
            targetList.resize(num);
    }
}

Unit* UnitAI::SelectLowestHpFriendly(float range, uint32 MinHPDiff)
{
    Unit* pUnit = NULL;
    Looking4group::MostHPMissingInRange u_check(me, range, MinHPDiff);
    Looking4group::UnitLastSearcher<Looking4group::MostHPMissingInRange> searcher(pUnit, u_check);

    Cell::VisitAllObjects(me, searcher, range);
    return pUnit;
}

std::list<Creature*> UnitAI::FindAllCreaturesWithEntry(uint32 entry, float range)
{
    std::list<Creature*> pList;
    Looking4group::AllCreaturesOfEntryInRange u_check(me, entry, range);
    Looking4group::ObjectListSearcher<Creature, Looking4group::AllCreaturesOfEntryInRange> searcher(pList, u_check);
    Cell::VisitAllObjects(me, searcher, range);
    return pList;
}

std::list<Player*> UnitAI::FindAllPlayersInRange(float range, Unit * finder)
{
    if (!finder)
        finder = me;
    std::list<Player*> pList;
    Looking4group::AnyPlayerInObjectRangeCheck checker(finder, range);
    Looking4group::ObjectListSearcher<Player, Looking4group::AnyPlayerInObjectRangeCheck> searcher(pList, checker);
    Cell::VisitWorldObjects(finder, searcher, range);
    return pList;
}

std::list<Creature*> UnitAI::FindAllFriendlyInGrid(float range)
{
    std::list<Creature*> pList;
    Looking4group::AllFriendlyCreaturesInGrid u_check(me);
    Looking4group::ObjectListSearcher<Creature, Looking4group::AllFriendlyCreaturesInGrid> searcher(pList, u_check);
    Cell::VisitGridObjects(me, searcher, range);
    return pList;
}

std::list<Creature*> UnitAI::FindFriendlyCC(float range)
{
    std::list<Creature*> pList;
    Looking4group::FriendlyCCedInRange u_check(me, range);
    Looking4group::ObjectListSearcher<Creature, Looking4group::FriendlyCCedInRange> searcher(pList, u_check);

    Cell::VisitAllObjects(me, searcher, range);
    return pList;
}

std::list<Creature*> UnitAI::FindFriendlyMissingBuff(float range, uint32 spellid)
{
    std::list<Creature*> pList;
    Looking4group::FriendlyMissingBuffInRange u_check(me, range, spellid);
    Looking4group::ObjectListSearcher<Creature, Looking4group::FriendlyMissingBuffInRange> searcher(pList, u_check);

    Cell::VisitAllObjects(me, searcher, range);
    return pList;
}

std::list<Unit*> UnitAI::FindAllDeadInRange(float range)
{
    std::list<Unit*> pList;
    Looking4group::AllDeadUnitsInRange u_check(me, range);
    Looking4group::UnitListSearcher<Looking4group::AllDeadUnitsInRange> searcher(pList, u_check);

    Cell::VisitAllObjects(me, searcher, range);
    return pList;
}

float UnitAI::DoGetSpellMaxRange(uint32 spellId, bool positive)
{
    return SpellMgr::GetSpellMaxRange(spellId);
}

void UnitAI::DoCast(uint32 spellId)
{
    Unit *target = NULL;
    //sLog.outLog(LOG_DEFAULT, "ERROR: aggre %u %u", spellId, (uint32)AISpellInfo[spellId].target);
    switch (AISpellInfo[spellId].target)
    {
        default:
        case AITARGET_SELF:     target = me; break;
        case AITARGET_VICTIM:   target = me->getVictim(); break;
        case AITARGET_ENEMY:
        {
            const SpellEntry * spellInfo = GetSpellStore()->LookupEntry(spellId);
            bool playerOnly = spellInfo->AttributesEx3 & SPELL_ATTR_EX3_PLAYERS_ONLY;
            float range = SpellMgr::GetSpellMaxRange(spellInfo);
            target = SelectUnit(SELECT_TARGET_RANDOM, 0, SpellMgr::GetSpellMaxRange(spellInfo), playerOnly);
            break;
        }
        case AITARGET_ALLY:     target = me; break;
        case AITARGET_BUFF:     target = me; break;
        case AITARGET_DEBUFF:
        {
            const SpellEntry * spellInfo = GetSpellStore()->LookupEntry(spellId);
            bool playerOnly = spellInfo->AttributesEx3 & SPELL_ATTR_EX3_PLAYERS_ONLY;
            float range = SpellMgr::GetSpellMaxRange(spellInfo);
            if (!(spellInfo->Attributes & SPELL_ATTR_BREAKABLE_BY_DAMAGE)
                && !(spellInfo->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_VICTIM)
                && SelectTargetHelper(me, me->getVictim(), playerOnly, range, -(int32)spellId))
                target = me->getVictim();
            else
                target = SelectUnit(SELECT_TARGET_RANDOM, 0, range, playerOnly);
            break;
        }
    }

    if (target)
        me->CastSpell(target, spellId, false);
}

#define UPDATE_TARGET(a) {if(AIInfo->target<a) AIInfo->target=a;}

void UnitAI::FillAISpellInfo()
{
    AISpellInfo = new AISpellInfoType[GetSpellStore()->GetNumRows()];

    AISpellInfoType *AIInfo = AISpellInfo;
    const SpellEntry * spellInfo;

    for (uint32 i = 0; i < GetSpellStore()->GetNumRows(); ++i, ++AIInfo)
    {
        spellInfo = GetSpellStore()->LookupEntry(i);
        if (!spellInfo)
            continue;

        if (spellInfo->Attributes & SPELL_ATTR_CASTABLE_WHILE_DEAD)
            AIInfo->condition = AICOND_DIE;
        else if (SpellMgr::IsPassiveSpell(i) || SpellMgr::GetSpellDuration(spellInfo) == -1)
            AIInfo->condition = AICOND_AGGRO;
        else
            AIInfo->condition = AICOND_COMBAT;

        if (AIInfo->cooldown < spellInfo->RecoveryTime)
            AIInfo->cooldown = spellInfo->RecoveryTime;

        if (!SpellMgr::GetSpellMaxRange(spellInfo))
            UPDATE_TARGET(AITARGET_SELF)
        else
        {
            for (uint32 j = 0; j < 3; ++j)
            {
                uint32 targetType = spellInfo->EffectImplicitTargetA[j];

                if (targetType == TARGET_UNIT_TARGET_ENEMY
                    || targetType == TARGET_DST_TARGET_ENEMY)
                    UPDATE_TARGET(AITARGET_VICTIM)
                else if (targetType == TARGET_UNIT_AREA_ENEMY_DST)
                    UPDATE_TARGET(AITARGET_ENEMY)

                if (spellInfo->Effect[j] == SPELL_EFFECT_APPLY_AURA)
                {
                    if (targetType == TARGET_UNIT_TARGET_ENEMY)
                        UPDATE_TARGET(AITARGET_DEBUFF)
                    else if (SpellMgr::IsPositiveSpell(i))
                        UPDATE_TARGET(AITARGET_BUFF)
                }
            }
        }
        AIInfo->realCooldown = spellInfo->RecoveryTime + spellInfo->StartRecoveryTime;
        SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(spellInfo->rangeIndex);
        if (srange)
            AIInfo->maxRange = srange->maxRange * 3 / 4;
    }
}

bool UnitAI::CanCast(Unit* Target, SpellEntry const *Spell, bool Triggered)
{
    //No target so we can't cast
    if (!Target || !Spell || me->hasUnitState(UNIT_STAT_CASTING))
        return false;

    //Silenced so we can't cast
    if (!Triggered && me->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED))
        return false;

    if (!Triggered && me->GetTypeId() == TYPEID_PLAYER && ((Player*)me)->GetCooldownMgr().HasGlobalCooldown(Spell))
        return false;

    if (!Triggered && me->GetTypeId() == TYPEID_PLAYER && ((Player*)me)->HasSpellCooldown(Spell->Id))
        return false;

    //Check for power
    if (!Triggered && me->GetPower((Powers)Spell->powerType) < Spell->manaCost)
        return false;

    SpellRangeEntry const *TempRange = NULL;

    TempRange = GetSpellRangeStore()->LookupEntry(Spell->rangeIndex);

    //Spell has invalid range store so we can't use it
    if (!TempRange)
        return false;

    //Unit is out of range of this spell
    if (me->GetDistance(Target) > TempRange->maxRange || me->GetDistance(Target) < TempRange->minRange)
        return false;

    return true;
}

bool UnitAI::HasEventAISummonedUnits()
{
    if (eventAISummonedList.empty())
        return false;

    bool alive = false;
    for (std::list<uint64>::iterator itr = eventAISummonedList.begin(); itr != eventAISummonedList.end();)
    {
        std::list<uint64>::iterator tmpItr = itr;
        ++itr;
        if (Unit * tmpU = me->GetUnit(*tmpItr))
        {
            if (tmpU->IsInWorld() && tmpU->isAlive())
                alive = true;
            else
                eventAISummonedList.erase(tmpItr);
        }
    }

    return alive;
}
//Checks the supplied Target for auras from spells that should trigger an aggro loss
bool UnitAI::ShouldBreakAggro(Unit* Target) {
	uint32 spellArray[] = {
		//Gouge
		1776, 1777, 3232, 8629, 11285, 11286, 12540, 13579,13741,13792,13793,23048, 24698,28456,29425,34940,36862, 38764,38863,
		//Iceblock
		45438,
		//Divine Shield
		642,1020,
		//Divine Protection
		498, 5573,
		//Polymorph
		118, 851, 12824, 12825, 12826, 13323, 14621, 15334, 27760, 28271, 28272, 28285, 29124, 29848, 30838, 33173, 36840,38245, 38896, 41334, 43309, 46280,
		//Dragon's Breath
		29964, 29965, 31661, 33041, 33042, 33043, 35250, 37289,
		//Bellowing Roar | if all target are feared, main aggro should retain aggro
		18431,22686,36922,39427,40636,44863,
		//Fear
		5782, 6213, 6215, 12096, 12542, 22678, 26070, 26580, 26661, 27641, 27990, 29168, 29321, 30002, 30530, 30584, 30615, 31358, 31970, 32241, 33547, 33924,
		34259, 38154, 38595, 38660, 39119, 39176, 39210, 39415, 41150,
		//Terrifying Howl | same as bellowing roar
		8715, 30752,
		//Howl of Terror
		5484, 17928, 39048,
		//Seduction
		6358, 6359, 20407, 29490, 30850, 31865, 36241,		
		//Intimidating Shout
		5246, 20511,
		//Death Coil
		6789, 17925, 17926, 27223, 28412, 30500, 30741, 32709, 33130, 34437, 35954, 38065, 39661, 41070, 44142, 46283,
		//Banish
		710, 8994, 18647, 27565, 30231, 31797, 35182, 37527, 37546, 37833, 38009, 38791, 39622, 39674, 40370, 42354, 44765, 44836,
		//Freezing Trap (Effect)
		3355, 14308, 14309, 31932, 43448,
		//Scatter Shot
		19503, 23601, 36732, 37506, 46681, 50733,
		//Scare Beast,
		1513, 14326, 14327,
		//Sap
		2070, 6770, 11297, 30980,
		//Cyclone
		33786,
		//Hibernate
		2637, 18657, 18658,
		//Psychic Scream
		8122, 8124, 10888, 10890, 13704, 15398, 22884, 26042, 27610, 34322, 43432,
		//Shackle
		9484, 9485, 10955, 11444, 38051, 38505, 40135		
	};
	
	uint32 size = (sizeof(spellArray) / sizeof(*spellArray));
	for (int i = 0; i < (size); ++i) {

		if (Target->HasAura(spellArray[i]) == true) {
			return true;
		}
	}

	return false;
}

//Checks all victims in the current threat list for CC's / Buffs that should cause an aggro loss / make them unattackable
void UnitAI::FlagInvalidTargetsUnattackable() {						
	Unit* tmpUnit = NULL;
	int i = 0;
	bool flagTopAggro = false;	

	std::list<Unit*> targetList;

	std::cout << me->GetOwnerGUID() << std::endl;
	std::cout << me->GetCharmerGUID() << std::endl;

	//Disable for pets (for now)
	if (me->GetOwnerGUID() != 0 || me->GetCharmerGUID() != 0) {
		return;
	}

	//Nothing relevant happens for a melee mob above 41 yards distance
	//Select targets only in this range to avoid reset issues and prohibit abuse by max ranging mobs
	SelectUnitList(targetList, 0, SELECT_TARGET_TOPAGGRO, 41, false);
	Unit* topAggro = SelectUnit(SELECT_TARGET_TOPAGGRO, 0);

	//Make sure that the top aggro doesn't have the flag from the last call of the function to avoid mob reset
	if (topAggro->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE)) {
		topAggro->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
	}

	//Logic only applies if there are more than 2 targets
	if (targetList.size() > 1) {
		for (std::list<Unit*>::const_iterator itr = targetList.begin(); itr != targetList.end(); ++itr)
		{
			tmpUnit = *itr;						
			//If our current target is affected by a relevant spell or above 41 yards distance, flag it.
			if (ShouldBreakAggro(tmpUnit) == true || me->GetExactDistance2d(tmpUnit->GetPositionX(), tmpUnit->GetPositionY()) >= 41)
			{				
				tmpUnit->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
				++i;
			}
			else {
				if (tmpUnit->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE)) {
					tmpUnit->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
				}				
			}			
		
			//All targets in thread list excluding the top aggro target have been affected by relevant spells (e.g. AoE fear) - Top aggro target should retain aggro.
			//Also avoids reset due to all targets being flagged non attackable
			if (i == targetList.size()) {			
				if (topAggro->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE)) {
					topAggro->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
				}
			}			
		}			
	}
}
