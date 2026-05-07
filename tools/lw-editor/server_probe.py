"""
SSH into the server and gather environment info needed for the build:
- OS / CPU / RAM
- Existing folder layout near the server executables
- Whether git, cmake, and a compiler are installed
- Whether the repo already exists on the machine
"""
import configparser, os, warnings
import paramiko

warnings.filterwarnings("ignore")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
cfg = configparser.ConfigParser()
cfg.read(os.path.join(SCRIPT_DIR, "config.ini"))

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect(
    hostname=cfg["ssh"]["host"],
    port=int(cfg["ssh"]["port"]),
    username=cfg["ssh"]["user"],
    password=cfg["ssh"]["password"],
)

def run(cmd):
    _, stdout, stderr = ssh.exec_command(cmd)
    out = stdout.read().decode().strip()
    err = stderr.read().decode().strip()
    return out, err

checks = [
    ("OS",              "uname -a"),
    ("CPU/RAM",         "echo \"CPUs: $(nproc)  RAM: $(free -h | awk '/Mem:/{print $2}')\""),
    ("Home dir",        "echo $HOME"),
    ("Disk free",       "df -h $HOME | tail -1"),
    ("git version",     "git --version 2>&1"),
    ("cmake version",   "cmake --version 2>&1 | head -1"),
    ("g++ version",     "g++ --version 2>&1 | head -1"),
    ("clang version",   "clang++ --version 2>&1 | head -1"),
    ("Folder listing",  "ls -la $HOME/"),
    ("Existing repos",  "find $HOME -maxdepth 3 -name 'CMakeLists.txt' 2>/dev/null | head -20"),
    ("Worldserver exe", "find $HOME -maxdepth 5 -name 'worldserver' -type f 2>/dev/null"),
    ("Conf files",      "find $HOME -maxdepth 5 -name '*.conf' 2>/dev/null | head -20"),
]

for label, cmd in checks:
    out, err = run(cmd)
    result = out if out else (err if err else "(no output)")
    print(f"\n=== {label} ===")
    print(result)

ssh.close()
print("\n[done]")
