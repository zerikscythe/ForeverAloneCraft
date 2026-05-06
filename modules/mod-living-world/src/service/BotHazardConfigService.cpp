#include "service/BotHazardConfigService.h"

namespace living_world
{
namespace service
{

BotHazardConfigService::BotHazardConfigService(
    integration::BotHazardConfigRepository const& repo)
    : _repo(repo)
    , _expiresAt(std::chrono::steady_clock::time_point{}) // expired immediately
{
}

void BotHazardConfigService::RefreshIfStale() const
{
    auto const now = std::chrono::steady_clock::now();
    if (now < _expiresAt)
        return;

    std::vector<model::HazardAuraEntry> auras  = _repo.LoadHazardAuras();
    std::vector<model::HazardRoleRule>  rules  = _repo.LoadRoleRules();
    model::HazardTuning                 tuning = _repo.LoadTuning();

    std::unordered_set<uint32_t> auraIds;
    auraIds.reserve(auras.size());
    for (auto const& e : auras)
        auraIds.insert(e.spellId);

    std::unordered_map<std::string, model::HazardRoleRule> roleRules;
    for (auto& r : rules)
        roleRules[r.roleKey] = std::move(r);

    _auras     = std::move(auras);
    _auraIds   = std::move(auraIds);
    _roleRules = std::move(roleRules);
    _tuning    = tuning;
    _expiresAt = now + CacheTtl;
}

std::unordered_set<uint32_t> BotHazardConfigService::GetHazardAuraIds() const
{
    std::lock_guard<std::mutex> lk(_mutex);
    RefreshIfStale();
    return _auraIds;
}

model::HazardRoleRule const* BotHazardConfigService::GetRoleRule(std::string const& roleKey) const
{
    std::lock_guard<std::mutex> lk(_mutex);
    RefreshIfStale();
    auto const it = _roleRules.find(roleKey);
    if (it == _roleRules.end())
        return nullptr;
    return &it->second;
}

model::HazardTuning BotHazardConfigService::GetTuning() const
{
    std::lock_guard<std::mutex> lk(_mutex);
    RefreshIfStale();
    return _tuning;
}

void BotHazardConfigService::Invalidate()
{
    std::lock_guard<std::mutex> lk(_mutex);
    _expiresAt = std::chrono::steady_clock::time_point{};
}

} // namespace service
} // namespace living_world
