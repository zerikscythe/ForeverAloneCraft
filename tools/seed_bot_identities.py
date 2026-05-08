"""
seed_bot_identities.py
----------------------
Generates and inserts persistent world bot identities into
living_world_bot_identity on the game server (via double-hop SSH).

Usage:
    python seed_bot_identities.py              # generates 50 bots (default)
    python seed_bot_identities.py --count 200  # generates 200 bots
    python seed_bot_identities.py --dry-run    # print SQL without inserting
    python seed_bot_identities.py --wipe       # delete existing rows first

Identities are distributed evenly across factions, spread across level bands,
and use believable race-appropriate names.  Run again at any time to top up
the pool — duplicate names are skipped via INSERT IGNORE.
"""

import argparse
import configparser
import random
import sys
from pathlib import Path

import paramiko

# ---------------------------------------------------------------------------
# SSH connection
# ---------------------------------------------------------------------------
CFG_PATH = Path(__file__).resolve().parent / "lw-editor" / "config.ini"


def ssh_connect():
    cfg = configparser.ConfigParser()
    cfg.read(CFG_PATH)
    jump = paramiko.SSHClient()
    jump.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    jump.connect(
        cfg["ssh"]["host"],
        port=int(cfg["ssh"]["port"]),
        username=cfg["ssh"]["user"],
        password=cfg["ssh"]["password"],
        timeout=15,
    )
    transport = jump.get_transport()
    channel = transport.open_channel(
        "direct-tcpip", ("server.local", 22), ("127.0.0.1", 0)
    )
    server = paramiko.SSHClient()
    server.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    server.connect("server.local", username="admin", password="vajj01",
                   sock=channel, timeout=15)
    return jump, server


def run_sql(server, sql):
    stdin, out, err = server.exec_command("mysql -u acore -pacore acore_characters")
    stdin.write(sql)
    stdin.channel.shutdown_write()
    o = out.read().decode().strip()
    e = "\n".join(
        l for l in err.read().decode().strip().splitlines()
        if "password on the command line" not in l
    )
    return o, e


# ---------------------------------------------------------------------------
# WoW data tables
# ---------------------------------------------------------------------------

# race_id -> (faction, display_ids_male, display_ids_female)
# Display IDs are common humanoid NPC models from WotLK 3.3.5 that visually
# match the race.  Adjust these to match your server's creature_display_info.
RACE_DATA = {
    # Alliance
    1:  (1, [49, 50, 51, 52],         [53, 54, 55, 56]),     # Human
    3:  (1, [131, 132, 133],           [134, 135]),            # Dwarf
    4:  (1, [55, 56, 57, 58],          [59, 60, 61]),          # Night Elf
    7:  (1, [111, 112, 113],           [114, 115]),            # Gnome
    11: (1, [16125, 16126],            [16127, 16128]),        # Draenei
    # Horde
    2:  (2, [27, 28, 29, 30],          [31, 32, 33]),          # Orc
    5:  (2, [57, 58, 59, 60],          [61, 62, 63]),          # Undead
    6:  (2, [59, 60, 61, 62],          [63, 64, 65]),          # Tauren
    8:  (2, [70, 71, 72, 73],          [74, 75]),              # Troll
    10: (2, [15476, 15477, 15478],     [15475, 15479]),        # Blood Elf
}

# class_id -> (name, valid_race_ids, specs)
CLASS_DATA = {
    1:  ("Warrior",    [1,2,3,4,5,6,7,8,10,11], ["warrior_arms", "warrior_fury", "warrior_prot"]),
    2:  ("Paladin",    [1,3,10,11],              ["paladin_holy", "paladin_prot", "paladin_ret"]),
    3:  ("Hunter",     [1,2,3,4,5,6,7,8,10,11], ["hunter_bm", "hunter_mm", "hunter_sv"]),
    4:  ("Rogue",      [1,2,3,4,5,7,8,10],       ["rogue_assn", "rogue_combat", "rogue_sub"]),
    5:  ("Priest",     [1,3,4,5,7,8,10,11],      ["priest_disc", "priest_holy", "priest_shadow"]),
    6:  ("DeathKnight",[1,2,3,4,5,6,7,8,10,11], ["dk_blood", "dk_frost", "dk_unholy"]),
    7:  ("Shaman",     [2,5,6,8,11],             ["shaman_ele", "shaman_enh", "shaman_resto"]),
    8:  ("Mage",       [1,3,4,5,7,8,10,11],      ["mage_arcane", "mage_fire", "mage_frost"]),
    9:  ("Warlock",    [1,2,5,7,8,10],           ["warlock_afflic", "warlock_demo", "warlock_destro"]),
    11: ("Druid",      [4,6],                    ["druid_balance", "druid_feral", "druid_resto"]),
}

# Profession flavour: ~30% of bots get one gathering profession
PROFESSION_COMBOS = [
    (0, 0, 0),  # none — most common
    (0, 0, 0),
    (0, 0, 0),
    (1, 0, 0),  # herbalism
    (0, 1, 0),  # mining
    (0, 0, 1),  # fishing
    (1, 0, 1),  # herb + fish
    (0, 1, 1),  # mine + fish
]

# Level bands: (min, max) — weighted toward low/mid to feel like a real server pop
LEVEL_BANDS = (
    [(1, 19)] * 15 +
    [(20, 39)] * 20 +
    [(40, 59)] * 20 +
    [(60, 69)] * 15 +
    [(70, 79)] * 15 +
    [(80, 80)] * 15
)

# Gear tier from level band
def gear_tier_for_level(level: int) -> int:
    if level < 20:  return 1
    if level < 50:  return 1
    if level < 60:  return 2
    if level < 70:  return 2
    if level < 80:  return 3
    return 3

# ---------------------------------------------------------------------------
# Name pools per race
# ---------------------------------------------------------------------------

NAMES = {
    1:  # Human
        ["Marcus","Elena","Thomas","Claire","Roland","Sera","Aldric","Mira",
         "Gareth","Lena","Oswin","Tara","Bram","Nessa","Hugo","Alys","Corwin",
         "Delia","Emric","Fiona","Hadwin","Isolde","Joric","Kira","Lewin"],
    3:  # Dwarf
        ["Bronk","Thora","Gimble","Dugal","Bera","Thordin","Kelga","Rimdar",
         "Agna","Borik","Gunda","Ulfar","Snorra","Dvallin","Frika","Hegir"],
    4:  # Night Elf
        ["Malfas","Tyrenna","Shal","Elandir","Dusk","Moonfang","Ashwhisper",
         "Celaen","Leafsong","Silvara","Stormclaw","Vanya","Wynnara","Zephyr"],
    5:  # Undead
        ["Mors","Vellus","Shade","Grimwald","Cryptar","Gashmore","Pallor",
         "Rotwick","Sallow","Tenebre","Wormtongue","Bleakhaven","Deathmere"],
    6:  # Tauren
        ["Hamuul","Mornehoof","Tarnis","Bainestone","Greathorn","Earthshaker",
         "Stonehoof","Swiftwind","Thunderhoof","Skydancer","Ironhorn","Duskmane"],
    7:  # Gnome
        ["Fizz","Cogsworth","Tinkle","Zapwick","Nimbolt","Sprocket","Gizmo",
         "Whirly","Clanksworth","Doodad","Fizzpop","Glimmer","Hacksaw","Inkwhistle"],
    8:  # Troll
        ["Zul","Vol","Jinrak","Raxsha","Kazzan","Shadtusk","Ziplax","Bogtusk",
         "Darkfang","Hexveil","Mudcloth","Razorbeak","Skullsplitter","Trollheim"],
    10: # Blood Elf
        ["Arano","Sylviel","Kaelion","Dawnblade","Sunwhisper","Aelindra",
         "Brightmantle","Crimsonthorn","Duskshroud","Evelaith","Goldmane",
         "Hawkspire","Illyria","Jadewing","Keldorei","Lunarglow"],
    11: # Draenei
        ["Akama","Veleth","Kirana","Azuremist","Sorel","Caiel","Drakoris",
         "Elodra","Faeron","Galadar","Holytear","Imari","Jaina","Khanaros"],
    2:  # Orc
        ["Grak","Thruk","Morg","Draka","Vorn","Kurg","Raka","Thok","Brolgur",
         "Darkjaw","Gorefist","Ironscar","Krom","Lukar","Malgok","Narak"],
}

USED_NAMES: set = set()


def pick_name(race_id: int) -> str:
    pool = NAMES.get(race_id, NAMES[1])
    available = [n for n in pool if n not in USED_NAMES]
    if not available:
        # Extend with suffixes when pool is exhausted
        base = random.choice(pool)
        suffix = random.choice(["shadow","iron","storm","flame","dark","swift",
                                 "stone","blood","frost","ash","moon","sky"])
        candidate = base + suffix.capitalize()
        USED_NAMES.add(candidate)
        return candidate
    chosen = random.choice(available)
    USED_NAMES.add(chosen)
    return chosen


# ---------------------------------------------------------------------------
# Identity generator
# ---------------------------------------------------------------------------

def generate_identity(faction: int) -> dict:
    """Generate one bot identity row for the given faction (1=Alliance, 2=Horde)."""
    # Pick race valid for faction
    valid_races = [rid for rid, (f, *_) in RACE_DATA.items() if f == faction]
    race_id = random.choice(valid_races)

    # Pick class valid for race
    valid_classes = [cid for cid, (_, races, _) in CLASS_DATA.items()
                     if race_id in races]
    class_id = random.choice(valid_classes)

    # Pick spec
    spec_key = random.choice(CLASS_DATA[class_id][2])

    # Gender
    gender = random.choice([0, 0, 0, 1])  # skew male for now

    # Display ID
    _, males, females = RACE_DATA[race_id]
    display_id = random.choice(males if gender == 0 else females)

    # Level
    band = random.choice(LEVEL_BANDS)
    level = random.randint(band[0], band[1])

    # Professions
    herb, mine, fish = random.choice(PROFESSION_COMBOS)

    # Name
    name = pick_name(race_id)

    return {
        "name":          name,
        "race_id":       race_id,
        "class_id":      class_id,
        "spec_key":      spec_key,
        "faction":       faction,
        "display_id":    display_id,
        "gender":        gender,
        "level":         level,
        "gear_tier":     gear_tier_for_level(level),
        "has_herbalism": herb,
        "has_mining":    mine,
        "has_fishing":   fish,
    }


def row_to_sql(r: dict) -> str:
    return (
        f"('{r['name']}', {r['race_id']}, {r['class_id']}, '{r['spec_key']}', "
        f"{r['faction']}, {r['display_id']}, {r['gender']}, {r['level']}, "
        f"{r['gear_tier']}, {r['has_herbalism']}, {r['has_mining']}, {r['has_fishing']})"
    )


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Seed world bot identities.")
    parser.add_argument("--count",   type=int, default=50,
                        help="Number of identities to generate (default: 50)")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print SQL without executing")
    parser.add_argument("--wipe",    action="store_true",
                        help="DELETE existing identities before inserting")
    args = parser.parse_args()

    # Generate identities — alternate factions for even split
    identities = []
    for i in range(args.count):
        faction = 1 if i % 2 == 0 else 2
        identities.append(generate_identity(faction))

    # Build SQL
    columns = (
        "name, race_id, class_id, spec_key, faction, display_id, gender, "
        "level, gear_tier, has_herbalism, has_mining, has_fishing"
    )
    values_sql = ",\n    ".join(row_to_sql(r) for r in identities)
    insert_sql = (
        f"INSERT IGNORE INTO living_world_bot_identity\n"
        f"    ({columns})\nVALUES\n    {values_sql};"
    )

    if args.dry_run:
        if args.wipe:
            print("DELETE FROM living_world_bot_identity;")
        print(insert_sql)
        print(f"\n-- {len(identities)} identities generated ({sum(1 for i in identities if i['faction']==1)} Alliance / {sum(1 for i in identities if i['faction']==2)} Horde)")
        return

    print(f"\nGenerating {len(identities)} bot identities...")
    print(f"  Alliance: {sum(1 for i in identities if i['faction']==1)}")
    print(f"  Horde:    {sum(1 for i in identities if i['faction']==2)}")
    print(f"\nConnecting to server...")

    jump, server = ssh_connect()

    try:
        # Apply schema
        schema_path = (
            Path(__file__).resolve().parent.parent
            / "modules/mod-living-world/data/sql/characters/living_world_bot_identity.sql"
        )
        if schema_path.exists():
            run_sql(server, schema_path.read_text())

        if args.wipe:
            print("  Wiping existing identities...")
            run_sql(server, "DELETE FROM living_world_bot_identity;")

        # Insert in batches of 25
        batch_size = 25
        inserted = 0
        for start in range(0, len(identities), batch_size):
            batch = identities[start:start + batch_size]
            batch_values = ",\n    ".join(row_to_sql(r) for r in batch)
            sql = (
                f"INSERT IGNORE INTO living_world_bot_identity "
                f"({columns}) VALUES {batch_values};"
            )
            o, e = run_sql(server, sql)
            if e:
                print(f"  WARN batch {start//batch_size + 1}: {e}")
            inserted += len(batch)
            print(f"  Inserted {inserted}/{len(identities)}...")

        # Report final count
        o, _ = run_sql(server, "SELECT COUNT(*) FROM living_world_bot_identity;")
        lines = [l for l in o.splitlines() if l.strip().isdigit()]
        total = lines[0] if lines else "?"
        print(f"\nDone. Total identities in ledger: {total}")

        # Show a sample
        o, _ = run_sql(
            server,
            "SELECT id, name, race_id, class_id, spec_key, faction, level, "
            "gear_tier, session_count FROM living_world_bot_identity "
            "ORDER BY RAND() LIMIT 10;"
        )
        print(f"\nSample (10 random):\n{o}")

    finally:
        server.close()
        jump.close()


if __name__ == "__main__":
    main()
