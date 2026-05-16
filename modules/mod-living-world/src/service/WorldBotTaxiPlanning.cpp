#include "service/WorldBotTaxiPlanning.h"

#include "DBCStores.h"
#include "DBCStructure.h"
#include "MapMgr.h"
#include "SharedDefines.h"

namespace living_world
{
namespace service
{
namespace
{

bool HasTaxiMaskBit(TaxiMask const& mask, std::uint32_t nodeId)
{
    if (nodeId == 0)
        return false;

    std::uint8_t const field = static_cast<std::uint8_t>((nodeId - 1) / 32);
    std::uint32_t const submask = 1u << ((nodeId - 1) % 32);

    return field < TaxiMaskSize && (mask[field] & submask) != 0;
}

std::uint32_t ResolveTaxiNodeZoneId(
    WorldBotTaxiZoneResolver const& resolver,
    TaxiNodesEntry const& entry)
{
    if (resolver)
        return resolver(
            static_cast<std::uint16_t>(entry.map_id),
            entry.x,
            entry.y,
            entry.z);

    if (entry.map_id == 0)
        return 0;

    return sMapMgr->GetZoneId(
        PHASEMASK_NORMAL,
        entry.map_id,
        entry.x,
        entry.y,
        entry.z);
}

std::string ResolveTaxiNodeName(TaxiNodesEntry const& entry)
{
    if (entry.name[0] && *entry.name[0])
        return entry.name[0];

    for (char const* localizedName : entry.name)
    {
        if (localizedName && *localizedName)
            return localizedName;
    }

    return {};
}

} // namespace

bool IsWorldBotTaxiNodeUsableForFaction(
    WorldBotTaxiNode const& node,
    std::uint8_t faction)
{
    switch (faction)
    {
        case 1:
            return node.usableByAlliance;
        case 2:
            return node.usableByHorde;
        default:
            return node.usableByAlliance || node.usableByHorde;
    }
}

WorldBotTaxiNetwork::WorldBotTaxiNetwork(std::vector<WorldBotTaxiNode> nodes)
    : _nodes(std::move(nodes))
{}

std::vector<WorldBotTaxiNode> const& WorldBotTaxiNetwork::GetNodes() const
{
    return _nodes;
}

std::vector<WorldBotTaxiNode> WorldBotTaxiNetwork::GetKnownNodes(
    std::unordered_set<std::uint32_t> const& exploredZoneIds,
    std::uint8_t faction) const
{
    std::vector<WorldBotTaxiNode> knownNodes;
    knownNodes.reserve(_nodes.size());

    for (WorldBotTaxiNode const& node : _nodes)
    {
        if (node.zoneId == 0)
            continue;

        if (exploredZoneIds.find(node.zoneId) == exploredZoneIds.end())
            continue;

        if (!IsWorldBotTaxiNodeUsableForFaction(node, faction))
            continue;

        knownNodes.push_back(node);
    }

    return knownNodes;
}

WorldBotTaxiNetwork LoadWorldBotTaxiNetwork(WorldBotTaxiZoneResolver zoneResolver)
{
    std::vector<WorldBotTaxiNode> nodes;
    nodes.reserve(sTaxiNodesStore.GetNumRows());

    for (std::uint32_t nodeId = 1; nodeId < sTaxiNodesStore.GetNumRows(); ++nodeId)
    {
        TaxiNodesEntry const* entry = sTaxiNodesStore.LookupEntry(nodeId);
        if (!entry)
            continue;

        if (!HasTaxiMaskBit(sTaxiNodesMask, nodeId))
            continue;

        WorldBotTaxiNode node;
        node.nodeId = nodeId;
        node.mapId = static_cast<std::uint16_t>(entry->map_id);
        node.zoneId = ResolveTaxiNodeZoneId(zoneResolver, *entry);
        node.x = entry->x;
        node.y = entry->y;
        node.z = entry->z;
        node.usableByAlliance =
            HasTaxiMaskBit(sAllianceTaxiNodesMask, nodeId)
            || HasTaxiMaskBit(sDeathKnightTaxiNodesMask, nodeId);
        node.usableByHorde =
            HasTaxiMaskBit(sHordeTaxiNodesMask, nodeId)
            || HasTaxiMaskBit(sDeathKnightTaxiNodesMask, nodeId);
        node.name = ResolveTaxiNodeName(*entry);
        nodes.push_back(std::move(node));
    }

    return WorldBotTaxiNetwork(std::move(nodes));
}

} // namespace service
} // namespace living_world
