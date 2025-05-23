#include "RaidUlduarTriggers.h"

#include "EventMap.h"
#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidUlduarBossHelper.h"
#include "RaidUlduarScripts.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <HunterBuffStrategies.h>
#include <PaladinBuffStrategies.h>

const std::vector<uint32> availableVehicles = {NPC_VEHICLE_CHOPPER, NPC_SALVAGED_DEMOLISHER,
                                               NPC_SALVAGED_DEMOLISHER_TURRET, NPC_SALVAGED_SIEGE_ENGINE,
                                               NPC_SALVAGED_SIEGE_ENGINE_TURRET};

bool FlameLeviathanOnVehicleTrigger::IsActive()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    Vehicle* vehicle = bot->GetVehicle();
    if (!vehicleBase || !vehicle)
        return false;

    uint32 entry = vehicleBase->GetEntry();
    for (uint32 comp : availableVehicles)
    {
        if (entry == comp)
            return true;
    }
    return false;
}

bool FlameLeviathanVehicleNearTrigger::IsActive()
{
    if (bot->GetVehicle())
        return false;

    Player* master = botAI->GetMaster();
    if (!master)
        return false;

    if (!master->GetVehicle())
        return false;

    return true;
}

bool RazorscaleFlyingAloneTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (!boss)
    {
        return false;
    }

    // Check if the boss is flying
    if (boss->GetPositionZ() < RazorscaleBossHelper::RAZORSCALE_FLYING_Z_THRESHOLD)
    {
        return false;
    }

    // Get the list of attackers
    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    if (attackers.empty())
    {
        return true;  // No attackers implies flying alone
    }

    std::vector<Unit*> dark_rune_adds;

    // Loop through attackers to find dark rune adds
    for (ObjectGuid const& guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;

        uint32 entry = unit->GetEntry();

        // Check for valid dark rune entries
        if (entry == RazorscaleBossHelper::UNIT_DARK_RUNE_WATCHER ||
            entry == RazorscaleBossHelper::UNIT_DARK_RUNE_GUARDIAN ||
            entry == RazorscaleBossHelper::UNIT_DARK_RUNE_SENTINEL)
        {
            dark_rune_adds.push_back(unit);
        }
    }

    // Return whether there are no dark rune adds
    return dark_rune_adds.empty();
}

bool RazorscaleDevouringFlamesTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (!boss)
        return false;

    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == RazorscaleBossHelper::UNIT_DEVOURING_FLAME)
        {
            return true;
        }
    }

    return false;
}

bool RazorscaleAvoidSentinelTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (!boss)
        return false;

    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == RazorscaleBossHelper::UNIT_DARK_RUNE_SENTINEL)
        {
            return true;
        }
    }

    return false;
}

bool RazorscaleAvoidWhirlwindTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (!boss)
        return false;

    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == RazorscaleBossHelper::UNIT_DARK_RUNE_SENTINEL &&
            (unit->HasAura(RazorscaleBossHelper::SPELL_SENTINEL_WHIRLWIND) ||
             unit->GetCurrentSpell(CURRENT_CHANNELED_SPELL)))
        {
            return true;
        }
    }

    return false;
}

bool RazorscaleGroundedTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (!boss)
    {
        return false;
    }

    // Check if the boss is flying
    if (boss->GetPositionZ() < RazorscaleBossHelper::RAZORSCALE_FLYING_Z_THRESHOLD)
    {
        return true;
    }
    return false;
}

bool RazorscaleHarpoonAvailableTrigger::IsActive()
{
    // Get harpoon data from the helper
    const std::vector<RazorscaleBossHelper::HarpoonData>& harpoonData = RazorscaleBossHelper::GetHarpoonData();

    // Get the boss entity
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Update the boss AI context in the helper
    RazorscaleBossHelper razorscaleHelper(botAI);

    if (!razorscaleHelper.UpdateBossAI())
    {
        return false;
    }

    // Check each harpoon entry
    for (const auto& harpoon : harpoonData)
    {
        // Skip harpoons whose chain spell is already active on the boss
        if (razorscaleHelper.IsHarpoonFired(harpoon.chainSpellId))
        {
            continue;
        }

        // Find the nearest harpoon GameObject within 200 yards
        if (GameObject* harpoonGO = bot->FindNearestGameObject(harpoon.gameObjectEntry, 200.0f))
        {
            if (RazorscaleBossHelper::IsHarpoonReady(harpoonGO))
            {
                return true;  // At least one harpoon is available and ready to be fired
            }
        }
    }

    // No harpoons are available or need to be fired
    return false;
}

bool RazorscaleFuseArmorTrigger::IsActive()
{
    // Get the boss entity
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Only proceed if this bot can actually tank
    if (!botAI->IsTank(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Iterate through group members to find the main tank with Fuse Armor
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || !botAI->IsMainTank(member))
            continue;

        Aura* fuseArmor = member->GetAura(RazorscaleBossHelper::SPELL_FUSEARMOR);
        if (fuseArmor && fuseArmor->GetStackAmount() >= RazorscaleBossHelper::FUSEARMOR_THRESHOLD)
            return true;
    }

    return false;
}

bool RazorscaleFireResistanceTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (!boss || !boss->IsAlive())
        return false;

    // Check if bot is paladin
    if (bot->getClass() != CLASS_PALADIN)
        return false;

    // Check if bot have fire resistance aura
    if (bot->HasAura(SPELL_FIRE_RESISTANCE_AURA))
        return false;

    // Check if bot dont have already have fire resistance strategy
    PaladinFireResistanceStrategy paladinFireResistanceStrategy(botAI);
    if (botAI->HasStrategy(paladinFireResistanceStrategy.getName(), BotState::BOT_STATE_COMBAT))
        return false;

    // Check that the bot actually knows the spell
    if (!bot->HasActiveSpell(SPELL_FIRE_RESISTANCE_AURA))
        return false;

    // Get the group and ensure it's a raid group
    Group* group = bot->GetGroup();
    if (!group || !group->isRaidGroup())
        return false;

    // Iterate through group members to find the first alive paladin
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || !member->IsAlive())
            continue;

        // Check if the member is a hunter
        if (member->getClass() == CLASS_PALADIN)
        {
            // Return true only if the current bot is the first alive paladin
            return member == bot;
        }
    }

    return false;
}

bool IgnisFireResistanceTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "ignis the furnace master");
    if (!boss || !boss->IsAlive())
        return false;

    // Check if bot is paladin
    if (bot->getClass() != CLASS_PALADIN)
        return false;

    // Check if bot have fire resistance aura
    if (bot->HasAura(SPELL_FIRE_RESISTANCE_AURA))
        return false;

    // Check if bot dont have already have fire resistance strategy
    PaladinFireResistanceStrategy paladinFireResistanceStrategy(botAI);
    if (botAI->HasStrategy(paladinFireResistanceStrategy.getName(), BotState::BOT_STATE_COMBAT))
        return false;

    // Check that the bot actually knows the spell
    if (!bot->HasActiveSpell(SPELL_FIRE_RESISTANCE_AURA))
        return false;

    // Get the group and ensure it's a raid group
    Group* group = bot->GetGroup();
    if (!group || !group->isRaidGroup())
        return false;

    // Iterate through group members to find the first alive paladin
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || !member->IsAlive())
            continue;

        // Check if the member is a hunter
        if (member->getClass() == CLASS_PALADIN)
        {
            // Return true only if the current bot is the first alive paladin
            return member == bot;
        }
    }

    return false;
}

bool IronAssemblyLightningTendrilsTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "stormcaller brundir");
    if (!boss || !boss->IsAlive())
        return false;

    // Check if bot is within 35 yards of the boss
    if (boss->GetDistance(bot) > 35.0f)
        return false;

    // Check if the boss has the Lightning Tendrils aura
    return boss->HasAura(SPELL_LIGHTNING_TENDRILS_10_MAN) || boss->HasAura(SPELL_LIGHTNING_TENDRILS_25_MAN);
}

bool IronAssemblyOverloadTrigger::IsActive()
{
    // Check if bot is tank
    if (botAI->IsTank(bot))
        return false;

    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "stormcaller brundir");
    if (!boss || !boss->IsAlive())
        return false;

    // Check if bot is within 35 yards of the boss
    if (boss->GetDistance(bot) > 35.0f)
        return false;

    // Check if the boss has the Overload aura
    return boss->HasAura(SPELL_OVERLOAD_10_MAN) || boss->HasAura(SPELL_OVERLOAD_25_MAN) ||
           boss->HasAura(SPELL_OVERLOAD_10_MAN_2) || boss->HasAura(SPELL_OVERLOAD_25_MAN_2);
}

bool KologarnMarkDpsTargetTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "kologarn");
    if (!boss || !boss->IsAlive())
        return false;

    // Only tank bot can mark target
    if (!botAI->IsTank(bot))
        return false;

    // Get current raid dps target
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    int8 skullIndex = 7;
    ObjectGuid currentSkullTarget = group->GetTargetIcon(skullIndex);
    Unit* currentSkullUnit = botAI->GetUnit(currentSkullTarget);

    // Check that rubble is marked
    if (currentSkullUnit && currentSkullUnit->IsAlive() && currentSkullUnit->GetEntry() == NPC_RUBBLE)
    {
        return false;  // Skull marker is already set on rubble
    }

    // Check that there is rubble to mark
    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    Unit* target = nullptr;
    for (auto i = targets.begin(); i != targets.end(); ++i)
    {
        target = botAI->GetUnit(*i);
        if (!target)
            continue;

        uint32 creatureId = target->GetEntry();
        if (target->GetEntry() == NPC_RUBBLE && target->IsAlive())
        {
            return true;  // Found a rubble to mark
        }
    }

    // Check that right arm is marked
    if (currentSkullUnit && currentSkullUnit->IsAlive() && currentSkullUnit->GetEntry() == NPC_RIGHT_ARM)
    {
        return false;  // Skull marker is already set on right arm
    }

    // Check that there is right arm to mark
    Unit* rightArm = AI_VALUE2(Unit*, "find target", "right arm");
    if (rightArm && rightArm->IsAlive())
    {
        return true;  // Found a right arm to mark
    }

    // Check that main body is marked
    if (currentSkullUnit && currentSkullUnit->IsAlive() && currentSkullUnit->GetEntry() == NPC_KOLOGARN)
    {
        return false;  // Skull marker is already set on main body
    }

    // Main body is not marked
    return true;
}

bool KologarnFallFromFloorTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "kologarn");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Check if bot is on the floor
    return bot->GetPositionZ() < ULDUAR_KOLOGARN_AXIS_Z_PATHING_ISSUE_DETECT;
}

bool KologarnNatureResistanceTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "kologarn");
    if (!boss || !boss->IsAlive())
        return false;

    // Check if bot is alive
    if (!bot->IsAlive())
        return false;

    // Check if bot is hunter
    if (bot->getClass() != CLASS_HUNTER)
        return false;

    // Check if bot have nature resistance aura
    if (bot->HasAura(SPELL_ASPECT_OF_THE_WILD))
        return false;

    // Check if bot dont have already setted nature resistance aura
    HunterNatureResistanceStrategy hunterNatureResistanceStrategy(botAI);
    if (botAI->HasStrategy(hunterNatureResistanceStrategy.getName(), BotState::BOT_STATE_COMBAT))
        return false;

    // Check that the bot actually knows Aspect of the Wild
    if (!bot->HasActiveSpell(SPELL_ASPECT_OF_THE_WILD))
        return false;

    // Get the group and ensure it's a raid group
    Group* group = bot->GetGroup();
    if (!group || !group->isRaidGroup())
        return false;

    // Iterate through group members to find the first alive hunter
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || !member->IsAlive())
            continue;

        // Check if the member is a hunter
        if (member->getClass() == CLASS_HUNTER)
        {
            // Return true only if the current bot is the first alive hunter
            return member == bot;
        }
    }

    return false;
}

bool KologarnRubbleSlowdownTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "kologarn");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
        return false;

    // Check if bot is hunter
    if (bot->getClass() != CLASS_HUNTER)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Check that the current skull mark is set on rubble
    int8 skullIndex = 7;
    ObjectGuid currentSkullTarget = group->GetTargetIcon(skullIndex);
    Unit* currentSkullUnit = botAI->GetUnit(currentSkullTarget);
    if (!currentSkullUnit || !currentSkullUnit->IsAlive() || currentSkullUnit->GetEntry() != NPC_RUBBLE)
        return false;

    if (bot->HasSpellCooldown(SPELL_FROST_TRAP))
        return false;

    return true;
}

bool HodirBitingColdTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "hodir");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    Player* master = botAI->GetMaster();
    if (!master || !master->IsAlive())
        return false;

    return botAI->GetAura("biting cold", bot, false, false, 2) &&
           !botAI->GetAura("biting cold", master, false, false, 2);
}

// Snowpacked Icicle Target
bool HodirNearSnowpackedIcicleTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "hodir");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Check if boss is casting Flash Freeze
    if (!boss->HasUnitState(UNIT_STATE_CASTING) || !boss->FindCurrentSpellBySpellId(SPELL_FLASH_FREEZE))
    {
        return false;
    }

    // Find the nearest Snowpacked Icicle Target
    Creature* target = bot->FindNearestCreature(NPC_SNOWPACKED_ICICLE, 100.0f);
    if (!target)
        return false;

    // Check that bot is stacked on Snowpacked Icicle
    if (bot->GetDistance2d(target->GetPositionX(), target->GetPositionY()) <= 5.0f)
    {
        return false;
    }

    return true;
}

bool HodirFrostResistanceTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "hodir");
    if (!boss || !boss->IsAlive())
        return false;

    // Check if bot is paladin
    if (bot->getClass() != CLASS_PALADIN)
        return false;

    // Check if bot have frost resistance aura
    if (bot->HasAura(SPELL_FROST_RESISTANCE_AURA))
        return false;

    // Check if bot dont have already have frost resistance strategy
    PaladinFrostResistanceStrategy paladinFrostResistanceStrategy(botAI);
    if (botAI->HasStrategy(paladinFrostResistanceStrategy.getName(), BotState::BOT_STATE_COMBAT))
        return false;

    // Check that the bot actually knows the spell
    if (!bot->HasActiveSpell(SPELL_FROST_RESISTANCE_AURA))
        return false;

    // Get the group and ensure it's a raid group
    Group* group = bot->GetGroup();
    if (!group || !group->isRaidGroup())
        return false;

    // Iterate through group members to find the first alive paladin
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || !member->IsAlive())
            continue;

        // Check if the member is a hunter
        if (member->getClass() == CLASS_PALADIN)
        {
            // Return true only if the current bot is the first alive paladin
            return member == bot;
        }
    }

    return false;
}

bool FreyaNearNatureBombTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Find the nearest Nature Bomb
    GameObject* target = bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, 12.0f);
    return target != nullptr;
}

bool FreyaTankNearEonarsGiftTrigger::IsActive()
{
    // Only tank bot can mark target
    if (!botAI->IsTank(bot))
    {
        return false;
    }

    // Check Eonar's gift and it is alive

    // Target is not findable from threat table using AI_VALUE2(),
    // therefore need to search manually for the unit id
    GuidVector targets = AI_VALUE(GuidVector, "possible targets");

    for (auto i = targets.begin(); i != targets.end(); ++i)
    {
        Unit* unit = botAI->GetUnit(*i);
        if (!unit)
        {
            continue;
        }

        uint32 creatureId = unit->GetEntry();
        if (creatureId == NPC_EONARS_GIFT && unit->IsAlive())
        {
            return true;
        }
    }

    return false;
}
