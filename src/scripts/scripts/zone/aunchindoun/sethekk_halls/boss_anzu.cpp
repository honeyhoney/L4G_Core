#include "precompiled.h"
#include "def_sethekk_halls.h"

#define EVENT_ANZU 1

enum Spells
{
    SPELL_OTHERWORLDLY_PORTAL      = 39952,
    SPELL_BLUE_SUMMON_BEAMS        = 39978,
    SPELL_RED_LIGHTNING_BOLT       = 39990,
    SPELL_PURPLE_ORB               = 44451,
    SPELL_SHADOWFORM               = 39579,

    SPELL_SPELL_BOMB               = 40303,
    SPELL_CYCLONE_OF_FEATHERS      = 40321,
    SPELL_PARALYZING_SCREECH       = 40184,
    SPELL_BANISH                   = 42354,   // Probably the completely wrong spell

    SPELL_PROTECTION_OF_THE_HAWK   = 40237,
    SPELL_SPITE_OF_THE_EAGLE       = 40240,
    SPELL_SPEED_OF_THE_FALCON      = 40241,

    SPELL_CAMERA_SHAKE             = 39983
};

enum Creatures
{
    NPC_ANZU                       = 23035,

    NPC_RAVEN_GOD_TARGET           = 23057,
    NPC_RAVEN_GOD_CASTER           = 23058,

    NPC_HAWK_SPIRIT                = 23134,
    NPC_FALCON_SPIRIT              = 23135,
    NPC_EAGLE_SPIRIT               = 23136,

    NPC_BROOD_OF_ANZU              = 23132
};

enum GameObjects
{
    GO_RAVENS_CLAW                 = 185554,
    GO_MOONSTONE                   = 185590,
    GO_SUMMONING_RIFT              = 185595
};

enum Quotes
{
    SAY_ANZU_INTRO_01              = -2000000,
    SAY_ANZU_INTRO_02              = -2000001
};

enum Events
{
    EVENT_SUMMONING_RITUAL_START   = 1,
    EVENT_SUMMONING_RITUAL_BOLT    = 2,
    EVENT_SUMMONING_RITUAL_END     = 3,
    EVENT_SUMMONING_INTRO_START    = 4,
    EVENT_SUMMONING_INTRO_END      = 5,
    EVENT_PARALYZING_SCREECH       = 6, 
    EVENT_SPELL_BOMB               = 7,
    EVENT_CYCLONE_OF_FEATHERS      = 8,
    EVENT_BANISH  = 49,
};

enum Phases
{
    PHASE_INTRO = 0,
    PHASE_COMBAT = 1
};

uint32 AnzuSpirits[] = {NPC_HAWK_SPIRIT, NPC_EAGLE_SPIRIT, NPC_FALCON_SPIRIT};

float AnzuSpiritLoc[][3] = {
    { -113, 293, 27 },
    { -77, 315, 27 },
    { -62, 288, 27 }
};

float BeamCasterLoc[][2] =
{
    {-64, 286},
    {-77, 279},
    {-87, 270},
    {-101, 278},
    {-110, 288},
    {-96, 299},
    {-89, 304},
    {-75, 297}
};

struct boss_anzuAI : public BossAI
{
    boss_anzuAI(Creature* c) : BossAI(c, EVENT_ANZU), summons(c)
    {
        pInstance = c->GetInstanceData();
    }

    ScriptedInstance* pInstance;
    SummonList summons;

    uint8 BroodCount;
    uint64 BeamTargetGuid;

    void Reset()
    {
        ClearCastQueue();
        events.Reset();
        summons.DespawnAll();

        events.ScheduleEvent(EVENT_SUMMONING_RITUAL_START, 0);

        BeamTargetGuid = 0;

        pInstance->SetData(DATA_ANZUEVENT, NOT_STARTED);
    }

    void IsSummonedBy(Unit *summoner) 
    {
        me->SetVisibility(VISIBILITY_OFF);
    }

    void JustSummoned(Creature *summon)
    {
        if(summon->GetEntry() == NPC_BROOD_OF_ANZU)
        {
            summon->AI()->AttackStart(me->getVictim());
            BroodCount++;
        }
        if(summon->GetEntry() == NPC_RAVEN_GOD_CASTER)
        {
            sLog.outString("Spawned a caster");
            summon->SetLevitate(true);
            if (pInstance->GetCreature(BeamTargetGuid)) {
                sLog.outString("got it! %s", pInstance->GetCreature(BeamTargetGuid)->GetGuidStr());
            }
            summon->CastSpell(pInstance->GetCreature(BeamTargetGuid), SPELL_BLUE_SUMMON_BEAMS, true);
        }
        if(summon->GetEntry() == NPC_RAVEN_GOD_TARGET)
        {
            sLog.outString("Spawned a target");
            summon->CastSpell(summon, SPELL_PURPLE_ORB, false);
            summon->SetLevitate(true);
            summon->SetSpeed(MOVE_FLIGHT, 0.1f);
            summon->GetMotionMaster()->MovePoint(0, summon->GetPositionX(), summon->GetPositionY(), summon->GetPositionZ()-12.6);
            sLog.outString("Done!");
        }
        summons.Summon(summon);
    }

    void SummonedCreatureDespawn(Creature *summon)
    {
        if(summon->GetEntry() == NPC_BROOD_OF_ANZU)
            BroodCount--;
        summons.Despawn(summon);
    }

    void SummonPurpleOrb()
    {
        BeamTargetGuid = me->SummonCreature(NPC_RAVEN_GOD_TARGET, -87.5742, 287.856, 48.5, 0,  TEMPSUMMON_CORPSE_DESPAWN, 0)->GetGUID();
    }

    void SummonBeamCasters()
    {
        for(uint8 i = 0; i < 8; i++)
        {
            me->SummonCreature(NPC_RAVEN_GOD_CASTER, BeamCasterLoc[i][0], BeamCasterLoc[i][1], urand(38, 45), 0, TEMPSUMMON_CORPSE_DESPAWN, 0);
        }
    }

    void SummonSpirits()
    {
        for(uint8 i = 0; i < 3; i++)
        {
            me->SummonCreature(AnzuSpirits[i], AnzuSpiritLoc[i][0], AnzuSpiritLoc[i][1], AnzuSpiritLoc[i][2], 0, TEMPSUMMON_MANUAL_DESPAWN, 0);
        }
    }

    void SummonBrood()
    {
        for(uint8 i = 0; i < 5; i++)
        {
            DoSummon(NPC_BROOD_OF_ANZU, me, 5, 0, TEMPSUMMON_CORPSE_DESPAWN);
        }
    }

    void EnterCombat(Unit *who)
    {
        if (pInstance)
        {
            pInstance->SetData(DATA_ANZUEVENT, IN_PROGRESS);
        }
    }

    void JustDied(Unit* Killer)
    {
        if (pInstance)
        {
            pInstance->SetData(DATA_ANZUEVENT, DONE);
        }
        summons.DespawnAll();
    }

    void UpdateAI(const uint32 diff)
    {
        events.Update(diff);

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {    
            case EVENT_SUMMONING_RITUAL_START:
                {
                    me->RemoveAurasDueToSpell(SPELL_BANISH);
                    SummonPurpleOrb();
                    SummonBeamCasters();

                    events.ScheduleEvent(EVENT_SUMMONING_RITUAL_BOLT, 20000, 0, 0);
                    break;
                }
            case EVENT_SUMMONING_RITUAL_BOLT:
                {
                    DoCast(me, SPELL_CAMERA_SHAKE);
                    pInstance->GetCreature(BeamTargetGuid)->CastSpell(me, SPELL_RED_LIGHTNING_BOLT, false); // cast it on 'me', it will target self (BeamTarget NPC) anyway

                    events.ScheduleEvent(EVENT_SUMMONING_RITUAL_END, 2000, 0, 0);
                    break;
                }
            case EVENT_SUMMONING_RITUAL_END:
                {
                    FindGameObject(GO_RAVENS_CLAW, 20, me)->Delete();
                    FindGameObject(GO_MOONSTONE, 20, me)->Delete();
                    FindGameObject(GO_SUMMONING_RIFT, 20, me)->Delete();
                    pInstance->GetCreature(BeamTargetGuid)->DisappearAndDie();
                    DoCast(me, SPELL_SHADOWFORM);
                    me->SetVisibility(VISIBILITY_ON);
                    DoScriptText(SAY_ANZU_INTRO_01, me);

                    events.ScheduleEvent(EVENT_SUMMONING_INTRO_START, 5000, 0, 0);
                    break;
                }
            case EVENT_SUMMONING_INTRO_START:
                {
                    DoScriptText(SAY_ANZU_INTRO_02, me);

                    events.ScheduleEvent(EVENT_SUMMONING_INTRO_END, 10000, 0, 0);
                    break;
                }
            case EVENT_SUMMONING_INTRO_END:
                {
                    me->RemoveAurasDueToSpell(SPELL_SHADOWFORM);

                    events.SetPhase(PHASE_COMBAT);
                    events.ScheduleEvent(EVENT_PARALYZING_SCREECH, 1000, 0, 1);
                    events.ScheduleEvent(EVENT_SPELL_BOMB, 1000, 0, 1);
                    events.ScheduleEvent(EVENT_CYCLONE_OF_FEATHERS, 1000, 0, 1);
                    break;
                }
            case EVENT_PARALYZING_SCREECH:
                {
                    AddSpellToCast(me, SPELL_PARALYZING_SCREECH);

                    events.RescheduleEvent(EVENT_PARALYZING_SCREECH, 26000, 0, 1);
                    break;
                }
            case EVENT_SPELL_BOMB:
                {
                    if (Unit *target = SelectUnit(SELECT_TARGET_RANDOM, 0))
                    {
                        AddSpellToCast(target, SPELL_SPELL_BOMB);
                    }

                    events.RescheduleEvent(EVENT_SPELL_BOMB, urand(24000, 40000), 0, 1);
                    break;
                }
            case EVENT_CYCLONE_OF_FEATHERS:
                {
                    if (Unit *target = SelectUnit(SELECT_TARGET_RANDOM, 1, 45.0f, true))
                    {
                        AddSpellToCast(target, SPELL_CYCLONE_OF_FEATHERS);
                    }

                    events.RescheduleEvent(SPELL_CYCLONE_OF_FEATHERS, 21000, 0, 1);
                    break;
                }
            }
        }

        if (UpdateVictim() & (events.GetPhase() == 1));
        {
            CastNextSpellIfAnyAndReady();
            DoMeleeAttackIfReady();
        }

        //CastNextSpellIfAnyAndReady();
        //DoMeleeAttackIfReady();
        /**
        if (!UpdateVictim())
            return;

        if(Banished)
        {
            if(BroodCount == 0 || Banish_Timer < diff)
            {
                Banished = false;
                me->RemoveAurasDueToSpell(SPELL_BANISH);
            } else 
                Banish_Timer -= diff;
        } else {

            if(ParalyzingScreech_Timer < diff)
            {
                AddSpellToCast(me, SPELL_PARALYZING_SCREECH);
                ParalyzingScreech_Timer = 26000;
            } else 
                ParalyzingScreech_Timer -= diff;

            if(SpellBomb_Timer < diff)
            {
                if(Unit *target = SelectUnit(SELECT_TARGET_RANDOM, 0))
                    AddSpellToCast(target, SPELL_SPELL_BOMB);
                SpellBomb_Timer = urand(24000, 40000);
            } else
                SpellBomb_Timer -= diff;

            if(CycloneOfFeathers_Timer < diff)
            {
                if(Unit *target = SelectUnit(SELECT_TARGET_RANDOM, 1, 45.0f, true))
                    AddSpellToCast(target, SPELL_CYCLONE_OF_FEATHERS);
                CycloneOfFeathers_Timer = 21000;
            } else
                CycloneOfFeathers_Timer -= diff;

            if(HealthBelowPct(33*BanishedTimes))
            {
                BanishedTimes--;
                Banished = true;
                Banish_Timer = 45000;
                ForceSpellCast(me, SPELL_BANISH, INTERRUPT_AND_CAST_INSTANTLY, true);
                SummonBrood();
            }          
        }
        
        CastNextSpellIfAnyAndReady();
        if(!Banished)
            DoMeleeAttackIfReady();
        */
    }
};

CreatureAI* GetAI_boss_anzu(Creature *_Creature)
{
    return new boss_anzuAI(_Creature);
}

struct npc_anzu_spiritAI : public Scripted_NoMovementAI
{
    npc_anzu_spiritAI(Creature* c, uint32 spell) : Scripted_NoMovementAI(c)
    {
        Spell = spell;
    }

    uint32 Spell;
    uint32 Timer;

    void Reset()
    {
        Timer = 5000;
        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_PL_SPELL_TARGET);
    }

    bool isDruidHotSpell(const SpellEntry *spellProto)
    {
        return spellProto->SpellFamilyName == SPELLFAMILY_DRUID && (spellProto->SpellFamilyFlags & 0x1000000050LL);
    }

    void OnAuraApply(Aura *aur, Unit *caster, bool stackApply)
    {
        if(isDruidHotSpell(aur->GetSpellProto()))
        {
            DoCast(me, Spell);
            Timer = 5000;
        }
    }

    void UpdateAI(const uint32 diff)
    {
        if(Timer < diff)
        {
            const Unit::AuraList& auras = me->GetAurasByType(SPELL_AURA_PERIODIC_HEAL);
            for(Unit::AuraList::const_iterator i = auras.begin(); i != auras.end(); ++i)
            {
                if(isDruidHotSpell((*i)->GetSpellProto()))
                {
                    DoCast(me, Spell);
                    break;
                }
            }
            Timer = 5000;
        } else
            Timer -= diff;
    }
};

CreatureAI* GetAI_npc_eagle_spirit(Creature *_Creature)
{
    return new npc_anzu_spiritAI(_Creature, SPELL_SPITE_OF_THE_EAGLE);
}

CreatureAI* GetAI_npc_hawk_spirit(Creature *_Creature)
{
    return new npc_anzu_spiritAI(_Creature, SPELL_PROTECTION_OF_THE_HAWK);
}

CreatureAI* GetAI_npc_falcon_spirit(Creature *_Creature)
{
    return new npc_anzu_spiritAI(_Creature, SPELL_SPEED_OF_THE_FALCON);
}

bool go_raven_claw(Player *player, GameObject* go)
{
    ScriptedInstance* pInstance = (ScriptedInstance*) go->GetInstanceData();

    if (!pInstance)
        return false;

    go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NOTSELECTABLE);
    go->SummonGameObject(GO_MOONSTONE, go->GetPositionX(), go->GetPositionY()+0.2, go->GetPositionZ()+4, go->GetOrientation(), 0, 0, 0, 0, 0);
    go->SummonGameObject(GO_SUMMONING_RIFT, go->GetPositionX(), go->GetPositionY(), go->GetPositionZ()+5, go->GetOrientation(), 0, 0, 0, 0, 0);
    go->CastSpell(go, SPELL_OTHERWORLDLY_PORTAL);

    return true;
}

void AddSC_boss_anzu()
{
    Script *newscript;

    newscript = new Script;
    newscript->Name="boss_anzu";
    newscript->GetAI = &GetAI_boss_anzu;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="npc_eagle_spirit";
    newscript->GetAI = &GetAI_npc_eagle_spirit;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="npc_falcon_spirit";
    newscript->GetAI = &GetAI_npc_falcon_spirit;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="npc_hawk_spirit";
    newscript->GetAI = &GetAI_npc_hawk_spirit;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="go_raven_claw";
    newscript->pGOUse = &go_raven_claw;
    newscript->RegisterSelf();
}