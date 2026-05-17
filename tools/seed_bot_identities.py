"""
seed_bot_identities.py
----------------------
Generate a first-wave population of persistent world-bot identities for
`living_world_bot_identity`.

Default behavior:
- 1,000 total bots
- 500 Alliance / 500 Horde
- normalized 1-80 level distribution
- no Death Knight below level 58
- guaranteed unique generated names within the wave

Usage:
    python tools/seed_bot_identities.py --dry-run
    python tools/seed_bot_identities.py --dry-run --print-sql
    python tools/seed_bot_identities.py --wipe

The script reads the authoritative DB connection from `tools/lw-editor/config.ini`.
"""

from __future__ import annotations

import argparse
import configparser
import math
import os
import random
from collections import Counter
from pathlib import Path


CFG_PATH = Path(__file__).resolve().parent / "lw-editor" / "config.ini"
SCHEMA_PATH = (
    Path(__file__).resolve().parent.parent
    / "modules/mod-living-world/data/sql/characters/living_world_bot_identity.sql"
)

DEFAULT_TOTAL_COUNT = 1000
DEFAULT_RANDOM_SEED = 1337

LEVEL_MIN = 1
LEVEL_MAX = 80
DEATH_KNIGHT_CLASS_ID = 6
DEATH_KNIGHT_MIN_LEVEL = 58


def resolve_home_base(faction: int, level: int) -> tuple[int, str, str]:
    if level >= 68:
        return 4395, "dalaran_inn", "dalaran_inn"
    if level >= 58:
        return 3703, "shattrath_inn", "shattrath_inn"
    if faction == 1:
        return 1519, "stormwind_inn", "stormwind_inn"
    return 1637, "orgrimmar_inn", "orgrimmar_inn"


RACE_DATA = {
    1:  (1, [49, 50, 51, 52],         [53, 54, 55, 56]),
    3:  (1, [131, 132, 133],           [134, 135]),
    4:  (1, [55, 56, 57, 58],          [59, 60, 61]),
    7:  (1, [111, 112, 113],           [114, 115]),
    11: (1, [16125, 16126],            [16127, 16128]),
    2:  (2, [27, 28, 29, 30],          [31, 32, 33]),
    5:  (2, [57, 58, 59, 60],          [61, 62, 63]),
    6:  (2, [59, 60, 61, 62],          [63, 64, 65]),
    8:  (2, [70, 71, 72, 73],          [74, 75]),
    10: (2, [15476, 15477, 15478],     [15475, 15479]),
}


CLASS_DATA = {
    1:  ("Warrior",     [1, 2, 3, 4, 5, 6, 7, 8, 10, 11], ["warrior_arms", "warrior_fury", "warrior_prot"]),
    2:  ("Paladin",     [1, 3, 10, 11],                   ["paladin_holy", "paladin_prot", "paladin_ret"]),
    3:  ("Hunter",      [1, 2, 3, 4, 5, 6, 7, 8, 10, 11], ["hunter_bm", "hunter_mm", "hunter_sv"]),
    4:  ("Rogue",       [1, 2, 3, 4, 5, 7, 8, 10],        ["rogue_assn", "rogue_combat", "rogue_sub"]),
    5:  ("Priest",      [1, 3, 4, 5, 7, 8, 10, 11],       ["priest_disc", "priest_holy", "priest_shadow"]),
    6:  ("DeathKnight", [1, 2, 3, 4, 5, 6, 7, 8, 10, 11], ["dk_blood", "dk_frost", "dk_unholy"]),
    7:  ("Shaman",      [2, 5, 6, 8, 11],                 ["shaman_ele", "shaman_enh", "shaman_resto"]),
    8:  ("Mage",        [1, 3, 4, 5, 7, 8, 10, 11],       ["mage_arcane", "mage_fire", "mage_frost"]),
    9:  ("Warlock",     [1, 2, 5, 7, 8, 10],              ["warlock_afflic", "warlock_demo", "warlock_destro"]),
    11: ("Druid",       [4, 6],                           ["druid_balance", "druid_feral", "druid_resto"]),
}


PROFESSION_COMBOS = [
    (0, 0, 0), (0, 0, 0), (0, 0, 0),
    (1, 0, 0), (0, 1, 0), (0, 0, 1),
    (1, 0, 1), (0, 1, 1),
]


RACE_FANTASY_NAMES = {
    1: ["Marcus", "Elena", "Thomas", "Claire", "Roland", "Sera", "Aldric", "Mira", "Gareth", "Lena", "Oswin", "Tara", "Bram", "Nessa", "Hugo", "Alys", "Corwin", "Delia", "Emric", "Fiona", "Hadwin", "Isolde", "Joric", "Kira", "Lewin"],
    2: ["Grak", "Thruk", "Morg", "Draka", "Vorn", "Kurg", "Raka", "Thok", "Brolgur", "Darkjaw", "Gorefist", "Ironscar", "Krom", "Lukar", "Malgok", "Narak"],
    3: ["Bronk", "Thora", "Gimble", "Dugal", "Bera", "Thordin", "Kelga", "Rimdar", "Agna", "Borik", "Gunda", "Ulfar", "Snorra", "Dvallin", "Frika", "Hegir"],
    4: ["Malfas", "Tyrenna", "Shal", "Elandir", "Dusk", "Moonfang", "Ashwhisper", "Celaen", "Leafsong", "Silvara", "Stormclaw", "Vanya", "Wynnara", "Zephyr"],
    5: ["Mors", "Vellus", "Shade", "Grimwald", "Cryptar", "Gashmore", "Pallor", "Rotwick", "Sallow", "Tenebre", "Wormtongue", "Bleakhaven", "Deathmere"],
    6: ["Hamuul", "Mornehoof", "Tarnis", "Bainestone", "Greathorn", "Earthshaker", "Stonehoof", "Swiftwind", "Thunderhoof", "Skydancer", "Ironhorn", "Duskmane"],
    7: ["Fizz", "Cogsworth", "Tinkle", "Zapwick", "Nimbolt", "Sprocket", "Gizmo", "Whirly", "Clanksworth", "Doodad", "Fizzpop", "Glimmer", "Hacksaw", "Inkwhistle"],
    8: ["Zul", "Vol", "Jinrak", "Raxsha", "Kazzan", "Shadtusk", "Ziplax", "Bogtusk", "Darkfang", "Hexveil", "Mudcloth", "Razorbeak", "Skullsplitter", "Trollheim"],
    10: ["Arano", "Sylviel", "Kaelion", "Dawnblade", "Sunwhisper", "Aelindra", "Brightmantle", "Crimsonthorn", "Duskshroud", "Evelaith", "Goldmane", "Hawkspire", "Illyria", "Jadewing", "Keldorei", "Lunarglow"],
    11: ["Akama", "Veleth", "Kirana", "Azuremist", "Sorel", "Caiel", "Drakoris", "Elodra", "Faeron", "Galadar", "Holytear", "Imari", "Jaina", "Khanaros"],
}


CASUAL_GIVEN_NAMES = [
    "Alex", "Andy", "Ash", "Ben", "Bobby", "Brad", "Bree", "Casey", "Chris", "Cody",
    "Dan", "Danny", "Dave", "Derek", "Eli", "Evan", "Finn", "Frank", "Greg", "Jake",
    "Jamie", "Jess", "Jimmy", "Joe", "John", "Josh", "Katie", "Kelly", "Kevin", "Kyle",
    "Liam", "Lucy", "Maddie", "Mason", "Matt", "Max", "Mia", "Mike", "Milo", "Nick",
    "Nina", "Noah", "Owen", "Ryan", "Sam", "Scott", "Sean", "Steve", "Tara", "Tony",
    "Tyler", "Vince", "Zack",
]


CASUAL_PREFIXES = [
    "Big", "Chill", "Cold", "Dark", "Fast", "Lucky", "Mad", "Mellow", "Old", "Quick",
    "Rusty", "Shadow", "Sleepy", "Slow", "Sneaky", "Storm", "Tiny", "Wild",
]


CASUAL_SUFFIXES = [
    "blade", "brew", "bro", "buddy", "burn", "caller", "claw", "craft", "dude", "guy",
    "hammer", "hunter", "jack", "lad", "lord", "mane", "runner", "shot", "spark",
    "ster", "stone", "walker", "ward", "weaver", "wolf",
]


MEME_PREFIXES = [
    "Bad", "Bonk", "Bonked", "Crusty", "Dirty", "Dumb", "Grumpy", "Lil", "Moist",
    "Moldy", "Nasty", "Salty", "Shifty", "Silly", "Smelly", "Sneaky", "Spicy",
    "Stinky", "Thicc", "Trashy", "Weird",
]


MEME_CORES = [
    "alfred", "bacon", "beans", "bert", "blob", "bob", "bonk", "boots", "burt", "cheese",
    "chunk", "dave", "dong", "fred", "gary", "george", "goober", "gravy", "greg", "jim",
    "joe", "larry", "lump", "mike", "mop", "nugget", "pickle", "randy", "ron", "scrub",
    "socks", "spud", "steve", "terry", "tim", "toes", "walter",
]


MEME_SUFFIXES = [
    "banger", "beard", "belly", "boi", "brain", "bucket", "cakes", "cheeks", "chonk",
    "crank", "fang", "feet", "fist", "goblin", "juice", "lad", "lord", "mage", "man",
    "master", "munch", "pants", "picker", "runner", "snack", "sneak", "spank", "tank",
    "totem", "wagon", "wizard",
]


NAME_SUFFIXES = [
    "ash", "bane", "beam", "blade", "bloom", "brook", "crest", "dawn",
    "fall", "flame", "forge", "gaze", "glow", "guard", "heart", "mane",
    "root", "scar", "shade", "song", "spark", "spire", "stone", "strike",
    "thorn", "vale", "ward", "weave", "whisper", "wind", "wing", "wrath",
]


NAME_STYLE_WEIGHTS = (
    ("fantasy", 0.55),
    ("casual", 0.25),
    ("meme", 0.20),
)


def gear_tier_for_level(level: int) -> int:
    if level < 40:
        return 1
    if level < 70:
        return 2
    return 3


def load_config() -> configparser.ConfigParser:
    cfg = configparser.ConfigParser()
    if not cfg.read(CFG_PATH):
        raise FileNotFoundError(f"Could not read config file: {CFG_PATH}")
    return cfg


def connect_characters_db(cfg: configparser.ConfigParser):
    import mysql.connector

    ssh_enabled = cfg.getboolean("ssh", "enabled", fallback=False)
    tunnel = None

    host = cfg["database"].get("host", "127.0.0.1")
    port = cfg["database"].getint("port", 3306)

    if ssh_enabled:
        try:
            from sshtunnel import SSHTunnelForwarder
        except ImportError as exc:
            raise ImportError("SSH is enabled in config.ini but sshtunnel is not installed") from exc

        ssh_host = cfg["ssh"].get("host", "")
        ssh_port = cfg["ssh"].getint("port", 22)
        ssh_user = cfg["ssh"].get("user", "")
        ssh_password = cfg["ssh"].get("password", "")
        ssh_key_file = cfg["ssh"].get("key_file", "")
        db_host = cfg["ssh"].get("db_host", "127.0.0.1")
        db_port = cfg["ssh"].getint("db_port", 3306)

        ssh_auth = {}
        if ssh_key_file and os.path.exists(ssh_key_file):
            ssh_auth["ssh_private_key"] = ssh_key_file
            if ssh_password:
                ssh_auth["ssh_private_key_password"] = ssh_password
        elif ssh_password:
            ssh_auth["ssh_password"] = ssh_password
        else:
            raise ValueError("SSH enabled but no ssh password or key_file configured")

        tunnel = SSHTunnelForwarder(
            (ssh_host, ssh_port),
            ssh_username=ssh_user,
            remote_bind_address=(db_host, db_port),
            **ssh_auth,
        )
        tunnel.start()
        host = "127.0.0.1"
        port = tunnel.local_bind_port

    conn = mysql.connector.connect(
        host=host,
        port=port,
        user=cfg["database"].get("user", "acore"),
        password=cfg["database"].get("password", "acore"),
        database="acore_characters",
        autocommit=True,
        charset="utf8mb4",
    )
    return tunnel, conn


def apply_schema(conn, wipe: bool) -> None:
    schema_sql = SCHEMA_PATH.read_text(encoding="utf-8")
    statements: list[str] = []
    buffer: list[str] = []
    for line in schema_sql.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("--"):
            continue
        buffer.append(line)
        if stripped.endswith(";"):
            statements.append("\n".join(buffer))
            buffer = []
    if buffer:
        statements.append("\n".join(buffer))

    with conn.cursor(buffered=True) as cur:
        for statement in statements:
            cur.execute(statement)
        cur.execute("SHOW COLUMNS FROM living_world_bot_identity LIKE 'population_role'")
        if cur.fetchone() is None:
            cur.execute(
                "ALTER TABLE living_world_bot_identity "
                "ADD COLUMN population_role VARCHAR(32) NOT NULL DEFAULT 'world' AFTER has_fishing"
            )
        cur.execute("SHOW COLUMNS FROM living_world_bot_identity LIKE 'reserve_city_zone_id'")
        if cur.fetchone() is None:
            cur.execute(
                "ALTER TABLE living_world_bot_identity "
                "ADD COLUMN reserve_city_zone_id INT UNSIGNED NULL AFTER population_role"
            )
        cur.execute("SHOW INDEX FROM living_world_bot_identity WHERE Key_name = 'idx_population_role'")
        if cur.fetchone() is None:
            cur.execute(
                "ALTER TABLE living_world_bot_identity "
                "ADD INDEX idx_population_role (population_role, reserve_city_zone_id, faction, is_available, is_retired)"
            )
        if wipe:
            cur.execute("DELETE FROM living_world_bot_identity")


def build_level_weights(center: float, sigma: float, floor: float) -> dict[int, float]:
    weights: dict[int, float] = {}
    for level in range(LEVEL_MIN, LEVEL_MAX + 1):
        gaussian = math.exp(-0.5 * (((level - center) / sigma) ** 2))
        weights[level] = floor + gaussian
    total = sum(weights.values())
    return {level: value / total for level, value in weights.items()}


def allocate_levels(total_count: int, weights: dict[int, float]) -> dict[int, int]:
    expected = {level: weights[level] * total_count for level in range(LEVEL_MIN, LEVEL_MAX + 1)}
    counts = {level: int(math.floor(value)) for level, value in expected.items()}
    remaining = total_count - sum(counts.values())

    remainders = sorted(
        ((expected[level] - counts[level], level) for level in range(LEVEL_MIN, LEVEL_MAX + 1)),
        key=lambda item: (-item[0], item[1]),
    )
    for _, level in remainders[:remaining]:
        counts[level] += 1
    return counts


def valid_classes_for_race_and_level(race_id: int, level: int) -> list[int]:
    valid = []
    for class_id, (_, races, _) in CLASS_DATA.items():
        if race_id not in races:
            continue
        if class_id == DEATH_KNIGHT_CLASS_ID and level < DEATH_KNIGHT_MIN_LEVEL:
            continue
        valid.append(class_id)
    return valid


class NameGenerator:
    def __init__(self):
        self.used: set[str] = set()
        self.base_usage: Counter[str] = Counter()

    def pick(self, race_id: int, rng: random.Random) -> str:
        style = self._choose_style(rng)
        if style == "fantasy":
            base = self._build_fantasy_name(race_id, rng)
        elif style == "casual":
            base = self._build_casual_name(rng)
        else:
            base = self._build_meme_name(rng)

        if base not in self.used:
            self.used.add(base)
            self.base_usage[base] += 1
            return base

        usage = self.base_usage[base]
        while True:
            suffix = NAME_SUFFIXES[usage % len(NAME_SUFFIXES)].capitalize()
            serial = (usage // len(NAME_SUFFIXES)) + 1
            candidate = f"{base}{suffix}{serial if serial > 1 else ''}"
            candidate = candidate[:32]
            usage += 1
            if candidate not in self.used:
                self.used.add(candidate)
                self.base_usage[base] = usage
                return candidate

    def _choose_style(self, rng: random.Random) -> str:
        roll = rng.random()
        threshold = 0.0
        for style, weight in NAME_STYLE_WEIGHTS:
            threshold += weight
            if roll <= threshold:
                return style
        return NAME_STYLE_WEIGHTS[-1][0]

    def _build_fantasy_name(self, race_id: int, rng: random.Random) -> str:
        pool = RACE_FANTASY_NAMES.get(race_id, RACE_FANTASY_NAMES[1])
        if rng.random() < 0.75:
            return rng.choice(pool)

        left = rng.choice(pool)
        right = rng.choice(NAME_SUFFIXES).capitalize()
        return f"{left}{right}"[:32]

    def _build_casual_name(self, rng: random.Random) -> str:
        given = rng.choice(CASUAL_GIVEN_NAMES)
        roll = rng.random()
        if roll < 0.40:
            return given
        if roll < 0.75:
            prefix = rng.choice(CASUAL_PREFIXES)
            return f"{prefix}{given}"[:32]

        suffix = rng.choice(CASUAL_SUFFIXES).capitalize()
        return f"{given}{suffix}"[:32]

    def _build_meme_name(self, rng: random.Random) -> str:
        roll = rng.random()
        if roll < 0.45:
            prefix = rng.choice(MEME_PREFIXES)
            core = rng.choice(MEME_CORES).capitalize()
            return f"{prefix}{core}"[:32]
        if roll < 0.80:
            core = rng.choice(MEME_CORES).capitalize()
            suffix = rng.choice(MEME_SUFFIXES).capitalize()
            return f"{core}{suffix}"[:32]

        prefix = rng.choice(MEME_PREFIXES)
        core = rng.choice(MEME_CORES).capitalize()
        suffix = rng.choice(MEME_SUFFIXES).capitalize()
        return f"{prefix}{core}{suffix}"[:32]


def generate_identity(
    faction: int,
    level: int,
    rng: random.Random,
    name_gen: NameGenerator,
    overrides: dict | None = None,
) -> dict:
    valid_races = [race_id for race_id, (race_faction, *_rest) in RACE_DATA.items() if race_faction == faction]
    race_id = rng.choice(valid_races)

    valid_classes = valid_classes_for_race_and_level(race_id, level)
    class_id = rng.choice(valid_classes)
    spec_key = rng.choice(CLASS_DATA[class_id][2])

    gender = rng.choice([0, 0, 0, 1])
    _, males, females = RACE_DATA[race_id]
    display_id = rng.choice(males if gender == 0 else females)

    herb, mine, fish = rng.choice(PROFESSION_COMBOS)
    name = name_gen.pick(race_id, rng)
    home_zone_id, home_anchor_point_key, home_bind_point_key = resolve_home_base(faction, level)

    identity = {
        "name": name,
        "race_id": race_id,
        "class_id": class_id,
        "spec_key": spec_key,
        "faction": faction,
        "display_id": display_id,
        "gender": gender,
        "level": level,
        "gear_tier": gear_tier_for_level(level),
        "has_herbalism": herb,
        "has_mining": mine,
        "has_fishing": fish,
        "home_zone_id": home_zone_id,
        "home_anchor_point_key": home_anchor_point_key,
        "home_bind_point_key": home_bind_point_key,
    }
    if overrides:
        identity.update({key: value for key, value in overrides.items() if value is not None})
    return identity


def row_to_tuple(identity: dict) -> tuple:
    return (
        identity["name"], identity["race_id"], identity["class_id"], identity["spec_key"],
        identity["faction"], identity["display_id"], identity["gender"], identity["level"],
        identity["gear_tier"], identity["has_herbalism"], identity["has_mining"], identity["has_fishing"],
        identity["population_role"], identity["reserve_city_zone_id"],
        identity["home_zone_id"], identity["home_anchor_point_key"], identity["home_bind_point_key"],
    )


def row_to_sql(identity: dict) -> str:
    return (
        f"('{identity['name']}', {identity['race_id']}, {identity['class_id']}, '{identity['spec_key']}', "
        f"{identity['faction']}, {identity['display_id']}, {identity['gender']}, {identity['level']}, "
        f"{identity['gear_tier']}, {identity['has_herbalism']}, {identity['has_mining']}, {identity['has_fishing']}, "
        f"'{identity['population_role']}', "
        f"{'NULL' if identity['reserve_city_zone_id'] is None else identity['reserve_city_zone_id']}, "
        f"{identity['home_zone_id']}, '{identity['home_anchor_point_key']}', '{identity['home_bind_point_key']}')"
    )


def generate_population(
    alliance_count: int,
    horde_count: int,
    rng: random.Random,
    center: float,
    sigma: float,
    floor: float,
    identity_overrides: dict | None = None,
) -> list[dict]:
    weights = build_level_weights(center=center, sigma=sigma, floor=floor)
    name_gen = NameGenerator()
    identities: list[dict] = []

    for faction, count in ((1, alliance_count), (2, horde_count)):
        per_level = allocate_levels(count, weights)
        for level in range(LEVEL_MIN, LEVEL_MAX + 1):
            for _ in range(per_level[level]):
                identities.append(generate_identity(faction, level, rng, name_gen, identity_overrides))

    return identities


def summarize_identities(identities: list[dict]) -> str:
    faction_counts = Counter(identity["faction"] for identity in identities)
    level_counts = Counter(identity["level"] for identity in identities)
    class_counts = Counter(identity["class_id"] for identity in identities)
    dk_under_58 = sum(1 for i in identities if i["class_id"] == DEATH_KNIGHT_CLASS_ID and i["level"] < DEATH_KNIGHT_MIN_LEVEL)
    unique_names = len({i['name'] for i in identities})

    lines = [
        f"Total generated: {len(identities)}",
        f"Unique names:   {unique_names}",
        f"Alliance:       {faction_counts.get(1, 0)}",
        f"Horde:          {faction_counts.get(2, 0)}",
        f"DKs below 58:   {dk_under_58}",
        "",
        "Level bands:",
    ]

    bands = [(1,10), (11,20), (21,30), (31,40), (41,50), (51,57), (58,69), (70,79), (80,80)]
    for low, high in bands:
        band_total = sum(level_counts[level] for level in range(low, high + 1))
        lines.append(f"  {low:02d}-{high:02d}: {band_total}")

    lines.append("")
    lines.append("Top classes:")
    for class_id, count in class_counts.most_common():
        lines.append(f"  {CLASS_DATA[class_id][0]:<12} {count}")

    return "\n".join(lines)


def build_insert_sql(identities: list[dict]) -> str:
    columns = (
        "name, race_id, class_id, spec_key, faction, display_id, gender, "
        "level, gear_tier, has_herbalism, has_mining, has_fishing, "
        "population_role, reserve_city_zone_id, "
        "home_zone_id, home_anchor_point_key, home_bind_point_key"
    )
    values_sql = ",\n    ".join(row_to_sql(identity) for identity in identities)
    return (
        "INSERT IGNORE INTO living_world_bot_identity\n"
        f"    ({columns})\n"
        f"VALUES\n    {values_sql};"
    )


def insert_identities(conn, identities: list[dict], batch_size: int) -> None:
    sql = (
        "INSERT IGNORE INTO living_world_bot_identity "
        "(name, race_id, class_id, spec_key, faction, display_id, gender, level, gear_tier, has_herbalism, has_mining, has_fishing, population_role, reserve_city_zone_id, home_zone_id, home_anchor_point_key, home_bind_point_key) "
        "VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)"
    )
    rows = [row_to_tuple(identity) for identity in identities]
    with conn.cursor() as cur:
        for start in range(0, len(rows), batch_size):
            cur.executemany(sql, rows[start:start + batch_size])


def resolve_counts(args: argparse.Namespace) -> tuple[int, int, int]:
    if args.alliance_count is not None or args.horde_count is not None:
        if args.alliance_count is None or args.horde_count is None:
            raise ValueError("If either --alliance-count or --horde-count is supplied, both are required")
        total = args.alliance_count + args.horde_count
        if args.count not in (None, total):
            raise ValueError("--count must equal alliance+horde when per-faction counts are provided")
        return total, args.alliance_count, args.horde_count

    total = args.count if args.count is not None else DEFAULT_TOTAL_COUNT
    if total % 2 != 0:
        raise ValueError("--count must be even when automatically split between factions")
    return total, total // 2, total // 2


def main() -> int:
    parser = argparse.ArgumentParser(description="Seed world bot identities")
    parser.add_argument("--count", type=int, default=None,
                        help=f"Total identities to generate (default: {DEFAULT_TOTAL_COUNT})")
    parser.add_argument("--alliance-count", type=int, default=None,
                        help="Explicit Alliance count (requires --horde-count)")
    parser.add_argument("--horde-count", type=int, default=None,
                        help="Explicit Horde count (requires --alliance-count)")
    parser.add_argument("--seed", type=int, default=DEFAULT_RANDOM_SEED,
                        help=f"Deterministic RNG seed (default: {DEFAULT_RANDOM_SEED})")
    parser.add_argument("--curve-center", type=float, default=42.0,
                        help="Center of normalized level curve (default: 42.0)")
    parser.add_argument("--curve-sigma", type=float, default=18.0,
                        help="Spread of normalized level curve (default: 18.0)")
    parser.add_argument("--curve-floor", type=float, default=0.25,
                        help="Flat tail added before normalization (default: 0.25)")
    parser.add_argument("--batch-size", type=int, default=200,
                        help="DB insert batch size (default: 200)")
    parser.add_argument("--dry-run", action="store_true",
                        help="Generate identities and print summary without touching DB")
    parser.add_argument("--print-sql", action="store_true",
                        help="When used with --dry-run, also print full SQL")
    parser.add_argument("--wipe", action="store_true",
                        help="Delete existing ledger rows before inserting")
    parser.add_argument("--population-role", default="world",
                        help="Value for living_world_bot_identity.population_role (default: world)")
    parser.add_argument("--reserve-city-zone-id", type=int, default=None,
                        help="Optional reserve city zone id for city_reserve pools")
    parser.add_argument("--home-zone-id", type=int, default=None,
                        help="Override generated home_zone_id")
    parser.add_argument("--home-anchor-point-key", default=None,
                        help="Override generated home_anchor_point_key")
    parser.add_argument("--home-bind-point-key", default=None,
                        help="Override generated home_bind_point_key")
    args = parser.parse_args()

    try:
        total_count, alliance_count, horde_count = resolve_counts(args)
    except ValueError as exc:
        print(f"ERROR: {exc}")
        return 2

    rng = random.Random(args.seed)
    identity_overrides = {
        "population_role": args.population_role,
        "reserve_city_zone_id": args.reserve_city_zone_id,
        "home_zone_id": args.home_zone_id,
        "home_anchor_point_key": args.home_anchor_point_key,
        "home_bind_point_key": args.home_bind_point_key,
    }
    identities = generate_population(
        alliance_count=alliance_count,
        horde_count=horde_count,
        rng=rng,
        center=args.curve_center,
        sigma=args.curve_sigma,
        floor=args.curve_floor,
        identity_overrides=identity_overrides,
    )

    summary = summarize_identities(identities)

    if args.dry_run:
        print(summary)
        if args.print_sql:
            if args.wipe:
                print("\nDELETE FROM living_world_bot_identity;")
            print("\n" + build_insert_sql(identities))
        return 0

    cfg = load_config()
    tunnel = None
    conn = None
    try:
        tunnel, conn = connect_characters_db(cfg)
        apply_schema(conn, wipe=args.wipe)
        insert_identities(conn, identities, batch_size=args.batch_size)

        print(summary)
        with conn.cursor() as cur:
            cur.execute("SELECT faction, COUNT(*) FROM living_world_bot_identity GROUP BY faction ORDER BY faction")
            print("\nLedger faction counts:")
            for faction, count in cur.fetchall():
                label = "Alliance" if faction == 1 else "Horde" if faction == 2 else str(faction)
                print(f"  {label}: {count}")

            cur.execute("SELECT COUNT(*) FROM living_world_bot_identity WHERE class_id = 6 AND level < 58")
            dk_under_58 = cur.fetchone()[0]
            print(f"\nLedger Death Knights below 58: {dk_under_58}")

        return 0
    except Exception as exc:
        print(f"ERROR: {exc}")
        return 1
    finally:
        if conn is not None:
            conn.close()
        if tunnel is not None:
            tunnel.stop()


if __name__ == "__main__":
    raise SystemExit(main())
