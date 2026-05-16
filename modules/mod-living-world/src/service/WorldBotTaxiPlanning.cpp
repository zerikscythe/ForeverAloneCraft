#include "service/WorldBotTaxiPlanning.h"

#include "service/WorldBotRoutePlanning.h"

#include "DBCStores.h"
#include "DBCStructure.h"
#include "MapMgr.h"
#include "SharedDefines.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <unordered_map>
#include <utility>

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

float ComputeDistanceYards(
    float ax,
    float ay,
    float az,
    float bx,
    float by,
    float bz)
{
    float const dx = bx - ax;
    float const dy = by - ay;
    float const dz = bz - az;
    return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
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

float ComputeTaxiPathDistanceYards(std::uint32_t pathId)
{
    if (pathId >= sTaxiPathNodesByPath.size())
        return 0.0f;

    TaxiPathNodeList const& pathNodes = sTaxiPathNodesByPath[pathId];
    if (pathNodes.size() < 2)
        return 0.0f;

    float totalDistance = 0.0f;
    for (std::size_t index = 1; index < pathNodes.size(); ++index)
    {
        TaxiPathNodeEntry const* previous = pathNodes[index - 1];
        TaxiPathNodeEntry const* current = pathNodes[index];
        if (!previous || !current)
            continue;

        float const dx = current->x - previous->x;
        float const dy = current->y - previous->y;
        float const dz = current->z - previous->z;
        totalDistance += std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    }

    return totalDistance;
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

WorldBotTaxiNetwork::WorldBotTaxiNetwork(
    std::vector<WorldBotTaxiNode> nodes,
    std::vector<WorldBotTaxiPathLink> links)
    : _nodes(std::move(nodes))
    , _links(std::move(links))
{}

std::vector<WorldBotTaxiNode> const& WorldBotTaxiNetwork::GetNodes() const
{
    return _nodes;
}

std::vector<WorldBotTaxiPathLink> const& WorldBotTaxiNetwork::GetLinks() const
{
    return _links;
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

std::optional<WorldBotTaxiNode> WorldBotTaxiNetwork::FindNearestKnownNode(
    std::uint16_t mapId,
    float x,
    float y,
    float z,
    std::unordered_set<std::uint32_t> const& exploredZoneIds,
    std::uint8_t faction,
    float maxDistanceYards) const
{
    std::optional<WorldBotTaxiNode> bestNode;
    float bestDistance = maxDistanceYards;

    for (WorldBotTaxiNode const& node : _nodes)
    {
        if (node.mapId != mapId || node.zoneId == 0)
            continue;

        if (exploredZoneIds.find(node.zoneId) == exploredZoneIds.end())
            continue;

        if (!IsWorldBotTaxiNodeUsableForFaction(node, faction))
            continue;

        float const distance = ComputeDistanceYards(x, y, z, node.x, node.y, node.z);
        if (!bestNode.has_value() || distance < bestDistance)
        {
            bestNode = node;
            bestDistance = distance;
        }
    }

    return bestNode;
}

std::optional<WorldBotResolvedTaxiRoute> WorldBotTaxiNetwork::ResolveKnownRoute(
    std::uint32_t sourceNodeId,
    std::uint32_t destinationNodeId,
    std::unordered_set<std::uint32_t> const& exploredZoneIds,
    std::uint8_t faction) const
{
    if (sourceNodeId == 0 || destinationNodeId == 0)
        return std::nullopt;

    std::unordered_map<std::uint32_t, WorldBotTaxiNode const*> knownNodesById;
    knownNodesById.reserve(_nodes.size());

    for (WorldBotTaxiNode const& node : _nodes)
    {
        if (node.zoneId == 0)
            continue;

        if (exploredZoneIds.find(node.zoneId) == exploredZoneIds.end())
            continue;

        if (!IsWorldBotTaxiNodeUsableForFaction(node, faction))
            continue;

        knownNodesById.emplace(node.nodeId, &node);
    }

    if (knownNodesById.find(sourceNodeId) == knownNodesById.end()
        || knownNodesById.find(destinationNodeId) == knownNodesById.end())
    {
        return std::nullopt;
    }

    struct QueueEntry
    {
        std::uint32_t nodeId = 0;
        std::uint32_t totalEtaMs = 0;

        bool operator>(QueueEntry const& other) const
        {
            return totalEtaMs > other.totalEtaMs;
        }
    };

    std::unordered_map<std::uint32_t, std::uint32_t> bestEtaByNode;
    std::unordered_map<std::uint32_t, std::uint32_t> previousNodeByNode;
    std::unordered_map<std::uint32_t, WorldBotTaxiPathLink const*> previousLinkByNode;
    bestEtaByNode.reserve(knownNodesById.size());
    previousNodeByNode.reserve(knownNodesById.size());
    previousLinkByNode.reserve(knownNodesById.size());

    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> frontier;
    bestEtaByNode[sourceNodeId] = 0;
    frontier.push({ sourceNodeId, 0u });

    while (!frontier.empty())
    {
        QueueEntry const current = frontier.top();
        frontier.pop();

        auto const bestItr = bestEtaByNode.find(current.nodeId);
        if (bestItr == bestEtaByNode.end() || current.totalEtaMs != bestItr->second)
            continue;

        if (current.nodeId == destinationNodeId)
            break;

        for (WorldBotTaxiPathLink const& link : _links)
        {
            if (link.fromNodeId != current.nodeId)
                continue;

            if (knownNodesById.find(link.toNodeId) == knownNodesById.end())
                continue;

            std::uint32_t const nextEta = current.totalEtaMs + link.rideEtaMs;
            auto const nextBestItr = bestEtaByNode.find(link.toNodeId);
            if (nextBestItr != bestEtaByNode.end() && nextEta >= nextBestItr->second)
                continue;

            bestEtaByNode[link.toNodeId] = nextEta;
            previousNodeByNode[link.toNodeId] = current.nodeId;
            previousLinkByNode[link.toNodeId] = &link;
            frontier.push({ link.toNodeId, nextEta });
        }
    }

    auto const destinationEtaItr = bestEtaByNode.find(destinationNodeId);
    if (destinationEtaItr == bestEtaByNode.end())
        return std::nullopt;

    WorldBotResolvedTaxiRoute route;
    route.totalEtaMs = destinationEtaItr->second;

    std::vector<WorldBotTaxiPathLink> reversedLinks;
    std::vector<std::uint32_t> reversedNodeIds;
    reversedNodeIds.push_back(destinationNodeId);

    std::uint32_t cursor = destinationNodeId;
    while (cursor != sourceNodeId)
    {
        auto const previousNodeItr = previousNodeByNode.find(cursor);
        auto const previousLinkItr = previousLinkByNode.find(cursor);
        if (previousNodeItr == previousNodeByNode.end() || previousLinkItr == previousLinkByNode.end())
            return std::nullopt;

        reversedLinks.push_back(*previousLinkItr->second);
        cursor = previousNodeItr->second;
        reversedNodeIds.push_back(cursor);
    }

    route.links.assign(reversedLinks.rbegin(), reversedLinks.rend());
    route.nodeIds.assign(reversedNodeIds.rbegin(), reversedNodeIds.rend());

    for (WorldBotTaxiPathLink const& link : route.links)
        route.totalDistanceYards += link.rideDistanceYards;

    return route;
}

std::optional<WorldBotTaxiTravelCandidate> WorldBotTaxiNetwork::ResolveTravelCandidate(
    std::uint16_t startMapId,
    float startX,
    float startY,
    float startZ,
    std::uint16_t destinationMapId,
    float destinationX,
    float destinationY,
    float destinationZ,
    std::unordered_set<std::uint32_t> const& exploredZoneIds,
    std::uint8_t faction,
    float maxSourceAttachDistanceYards,
    float maxDestinationDetachDistanceYards) const
{
    auto const sourceNode = FindNearestKnownNode(
        startMapId,
        startX,
        startY,
        startZ,
        exploredZoneIds,
        faction,
        maxSourceAttachDistanceYards);
    if (!sourceNode.has_value())
        return std::nullopt;

    auto const destinationNode = FindNearestKnownNode(
        destinationMapId,
        destinationX,
        destinationY,
        destinationZ,
        exploredZoneIds,
        faction,
        maxDestinationDetachDistanceYards);
    if (!destinationNode.has_value())
        return std::nullopt;

    auto const route = ResolveKnownRoute(
        sourceNode->nodeId,
        destinationNode->nodeId,
        exploredZoneIds,
        faction);
    if (!route.has_value())
        return std::nullopt;

    WorldBotTaxiTravelCandidate candidate;
    candidate.sourceNode = *sourceNode;
    candidate.destinationNode = *destinationNode;
    candidate.route = *route;
    candidate.sourceAttachDistanceYards = ComputeDistanceYards(
        startX,
        startY,
        startZ,
        sourceNode->x,
        sourceNode->y,
        sourceNode->z);
    candidate.destinationDetachDistanceYards = ComputeDistanceYards(
        destinationNode->x,
        destinationNode->y,
        destinationNode->z,
        destinationX,
        destinationY,
        destinationZ);
    return candidate;
}

std::optional<WorldBotTaxiNode> WorldBotTaxiNetwork::FindNode(std::uint32_t nodeId) const
{
    for (WorldBotTaxiNode const& node : _nodes)
    {
        if (node.nodeId == nodeId)
            return node;
    }

    return std::nullopt;
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

    std::vector<WorldBotTaxiPathLink> links;
    for (auto const& [fromNodeId, destinations] : sTaxiPathSetBySource)
    {
        for (auto const& [toNodeId, pathEntry] : destinations)
        {
            if (!pathEntry || pathEntry->ID == 0)
                continue;

            WorldBotTaxiPathLink link;
            link.pathId = pathEntry->ID;
            link.fromNodeId = fromNodeId;
            link.toNodeId = toNodeId;
            link.price = pathEntry->price;
            link.rideDistanceYards = ComputeTaxiPathDistanceYards(pathEntry->ID);
            float const taxiSpeed = ResolveWorldBotTravelSpeedYardsPerSecond(WorldBotTravelCapabilityTier::Taxi);
            link.rideEtaMs = taxiSpeed > 0.0f
                ? static_cast<std::uint32_t>(std::lround((link.rideDistanceYards / taxiSpeed) * 1000.0f))
                : 0u;
            links.push_back(std::move(link));
        }
    }

    return WorldBotTaxiNetwork(std::move(nodes), std::move(links));
}

std::optional<WorldBotResolvedTaxiJourney> ResolveBestTaxiJourney(
    WorldBotTaxiNetwork const& network,
    WorldBotGroundRouteResolver const& groundRouteResolver,
    std::uint16_t mapId,
    std::uint32_t startZoneIdHint,
    std::uint32_t destZoneId,
    float startX,
    float startY,
    float startZ,
    float destX,
    float destY,
    float destZ,
    std::unordered_set<std::uint32_t> const& exploredZoneIds,
    std::uint8_t faction,
    WorldBotTravelCapabilityTier groundTier,
    WorldBotTravelCapabilityConfig const& capabilityConfig)
{
    if (!groundRouteResolver || destZoneId == 0)
        return std::nullopt;

    std::vector<WorldBotTaxiNode> const knownNodes = network.GetKnownNodes(exploredZoneIds, faction);
    if (knownNodes.empty())
        return std::nullopt;

    std::optional<WorldBotResolvedTaxiJourney> bestJourney;

    for (WorldBotTaxiNode const& sourceNode : knownNodes)
    {
        if (sourceNode.mapId != mapId)
            continue;

        auto const sourceGroundPlan = groundRouteResolver(
            mapId,
            startZoneIdHint,
            sourceNode.zoneId,
            startX,
            startY,
            startZ,
            sourceNode.x,
            sourceNode.y,
            sourceNode.z,
            groundTier,
            capabilityConfig);
        if (!sourceGroundPlan || sourceGroundPlan->empty())
            continue;

        for (WorldBotTaxiNode const& destinationNode : knownNodes)
        {
            if (destinationNode.mapId != mapId)
                continue;

            auto const taxiRoute = network.ResolveKnownRoute(
                sourceNode.nodeId,
                destinationNode.nodeId,
                exploredZoneIds,
                faction);
            if (!taxiRoute || taxiRoute->empty())
                continue;

            auto const destinationGroundPlan = groundRouteResolver(
                mapId,
                destinationNode.zoneId,
                destZoneId,
                destinationNode.x,
                destinationNode.y,
                destinationNode.z,
                destX,
                destY,
                destZ,
                groundTier,
                capabilityConfig);
            if (!destinationGroundPlan || destinationGroundPlan->empty())
                continue;

            WorldBotResolvedTaxiJourney journey;
            journey.sourceGroundPlan = *sourceGroundPlan;
            journey.taxiCandidate.sourceNode = sourceNode;
            journey.taxiCandidate.destinationNode = destinationNode;
            journey.taxiCandidate.route = *taxiRoute;
            journey.taxiCandidate.sourceAttachDistanceYards = sourceGroundPlan->totalDistanceYards;
            journey.taxiCandidate.destinationDetachDistanceYards = destinationGroundPlan->totalDistanceYards;
            journey.destinationGroundPlan = *destinationGroundPlan;
            journey.totalDistanceYards =
                sourceGroundPlan->totalDistanceYards
                + taxiRoute->totalDistanceYards
                + destinationGroundPlan->totalDistanceYards;
            journey.totalEtaMs =
                sourceGroundPlan->etaMs
                + taxiRoute->totalEtaMs
                + destinationGroundPlan->etaMs;

            if (!bestJourney.has_value() || journey.totalEtaMs < bestJourney->totalEtaMs)
                bestJourney = std::move(journey);
        }
    }

    return bestJourney;
}

std::optional<WorldBotResolvedTravelOption> ResolveBestTravelOption(
    WorldBotTaxiNetwork const& network,
    WorldBotGroundRouteResolver const& groundRouteResolver,
    std::uint16_t mapId,
    std::uint32_t startZoneIdHint,
    std::uint32_t destZoneId,
    float startX,
    float startY,
    float startZ,
    float destX,
    float destY,
    float destZ,
    std::unordered_set<std::uint32_t> const& exploredZoneIds,
    std::uint8_t faction,
    WorldBotTravelCapabilityTier groundTier,
    WorldBotTravelCapabilityConfig const& capabilityConfig)
{
    if (!groundRouteResolver || destZoneId == 0)
        return std::nullopt;

    auto const groundPlan = groundRouteResolver(
        mapId,
        startZoneIdHint,
        destZoneId,
        startX,
        startY,
        startZ,
        destX,
        destY,
        destZ,
        groundTier,
        capabilityConfig);

    auto const taxiJourney = ResolveBestTaxiJourney(
        network,
        groundRouteResolver,
        mapId,
        startZoneIdHint,
        destZoneId,
        startX,
        startY,
        startZ,
        destX,
        destY,
        destZ,
        exploredZoneIds,
        faction,
        groundTier,
        capabilityConfig);

    if (taxiJourney.has_value()
        && (!groundPlan.has_value() || taxiJourney->totalEtaMs < groundPlan->etaMs))
    {
        WorldBotResolvedTravelOption option;
        option.mode = taxiJourney->taxiCandidate.destinationNode.zoneId == destZoneId
            ? WorldBotTravelOptionMode::TaxiFull
            : WorldBotTravelOptionMode::TaxiPartial;
        option.groundPlan = groundPlan;
        option.taxiJourney = taxiJourney;
        option.totalDistanceYards = taxiJourney->totalDistanceYards;
        option.totalEtaMs = taxiJourney->totalEtaMs;
        return option;
    }

    if (groundPlan.has_value())
    {
        WorldBotResolvedTravelOption option;
        option.mode = WorldBotTravelOptionMode::Ground;
        option.groundPlan = groundPlan;
        option.taxiJourney = taxiJourney;
        option.totalDistanceYards = groundPlan->totalDistanceYards;
        option.totalEtaMs = groundPlan->etaMs;
        return option;
    }

    if (taxiJourney.has_value())
    {
        WorldBotResolvedTravelOption option;
        option.mode = taxiJourney->taxiCandidate.destinationNode.zoneId == destZoneId
            ? WorldBotTravelOptionMode::TaxiFull
            : WorldBotTravelOptionMode::TaxiPartial;
        option.taxiJourney = taxiJourney;
        option.totalDistanceYards = taxiJourney->totalDistanceYards;
        option.totalEtaMs = taxiJourney->totalEtaMs;
        return option;
    }

    return std::nullopt;
}

} // namespace service
} // namespace living_world
