# UI Layout Reference - Connection Configuration

## New Improved UI Layout

The connection bar is now divided into two clear sections:

```
┌─────────────────────────────────────────────────────────────────────────┐
│ ┌─ Direct Connection ─────────────────────────────────────────────────┐ │
│ │ MySQL Host: [192.168.0.93] Port: [3306] User: [acore] Pass: [****] │ │
│ │ [Connect] ● Connected                                               │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
│                                                                           │
│ ┌─ SSH Tunnel (for remote/private networks) ──────────────────────────┐ │
│ │ ☑ Enable SSH Tunnel  → When enabled, connects through a jump host   │ │
│ │ SSH:  Host: [192.168.0.50]  Port: [22]  User: [john]  Pass: [****]  │ │
│ │ SSH:  Key File: [C:\Users\...\id_rsa           ] [Browse...]         │ │
│ │       (leave blank to use password)                                  │ │
│ │ MySQL: Host: [192.168.0.93] Port: [3306] User: [acore] Pass: [****] │ │
│ │        (MySQL server as seen from SSH host)                          │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
```

## UI Behavior

### When SSH Tunnel is DISABLED (unchecked)
- **Direct Connection section**: ENABLED (you can edit all fields)
- **SSH Tunnel section**: GRAYED OUT (cannot edit)
- **Connect button**: Uses Direct Connection settings
- **What happens**: Editor connects directly to MySQL server

### When SSH Tunnel is ENABLED (checked)
- **Direct Connection section**: GRAYED OUT (auto-set to 127.0.0.1:3306)
- **SSH Tunnel section**: ENABLED (you can edit all fields)
- **Connect button**: Uses SSH Tunnel settings
- **What happens**: 
  1. Editor creates SSH tunnel to jump host
  2. Editor connects to MySQL through the tunnel

## Field Meanings

### Direct Connection Section
| Field | Description | Example |
|-------|-------------|---------|
| MySQL Host | IP/hostname of MySQL server | `192.168.0.93` |
| Port | MySQL port | `3306` |
| User | MySQL username | `acore` |
| Pass | MySQL password | `acore` |

**Use this when**: Database is directly accessible from your PC

### SSH Tunnel Section

#### SSH Credentials (Blue label)
| Field | Description | Example |
|-------|-------------|---------|
| Host | Jump host IP/hostname | `192.168.0.50` |
| Port | SSH port | `22` |
| User | SSH username | `john` |
| Pass | SSH password | `mypassword` |
| Key File | Path to SSH private key | `C:\Users\John\.ssh\id_rsa` |

**Use**: Password OR Key File (key is more secure)

#### MySQL Credentials (Green label)  
| Field | Description | Example |
|-------|-------------|---------|
| Host | DB server address *from jump host* | `192.168.0.93` or `127.0.0.1` |
| Port | MySQL port on remote | `3306` |
| User | MySQL username | `acore` |
| Pass | MySQL password | `acore` |

**Important**: "Host" here is the address the jump host uses to reach the database

## Common Configurations

### Config 1: Direct Connection (No SSH)
```
┌─ Direct Connection ────────────────────────┐
│ Host: 192.168.0.93  Port: 3306            │
│ User: acore         Pass: ****            │
└────────────────────────────────────────────┘

┌─ SSH Tunnel ───────────────────────────────┐
│ ☐ Enable SSH Tunnel (UNCHECKED)           │
│ (all fields grayed out)                    │
└────────────────────────────────────────────┘
```

### Config 2: SSH to Jump, MySQL on Different Server
```
┌─ Direct Connection ────────────────────────┐
│ (grayed out - auto-set to 127.0.0.1:3306) │
└────────────────────────────────────────────┘

┌─ SSH Tunnel ───────────────────────────────┐
│ ☑ Enable SSH Tunnel (CHECKED)             │
│ SSH:   Host: 192.168.0.50  Port: 22       │
│        User: john          Pass: ****      │
│ MySQL: Host: 192.168.0.93  Port: 3306     │
│        User: acore         Pass: ****      │
└────────────────────────────────────────────┘
```

### Config 3: SSH and MySQL on Same Server
```
┌─ Direct Connection ────────────────────────┐
│ (grayed out - auto-set to 127.0.0.1:3306) │
└────────────────────────────────────────────┘

┌─ SSH Tunnel ───────────────────────────────┐
│ ☑ Enable SSH Tunnel (CHECKED)             │
│ SSH:   Host: myserver.com  Port: 22       │
│        User: admin         Pass: ****      │
│        Key:  C:\...\id_rsa                 │
│ MySQL: Host: 127.0.0.1     Port: 3306     │
│        User: acore         Pass: ****      │
└────────────────────────────────────────────┘
```

## Color Coding

The labels use colors to help distinguish sections:
- **Blue "SSH:"** = SSH connection credentials (to jump host)
- **Green "MySQL:"** = MySQL database credentials (on remote server)
- **Gray text** = Helpful hints and explanations
- **Orange text** = Warnings (like missing package)

## Automatic Behavior

When you check "Enable SSH Tunnel":
1. Direct Connection fields become grayed out
2. SSH Tunnel fields become editable
3. Direct Connection Host auto-sets to `127.0.0.1`
4. Direct Connection Port auto-sets to `3306`
5. Direct Connection User/Pass sync from MySQL section

When you uncheck "Enable SSH Tunnel":
1. SSH Tunnel fields become grayed out
2. Direct Connection fields become editable
3. You can configure direct connection normally

## Connection Flow

### Direct Mode (SSH unchecked)
```
Your PC → MySQL Server
(Direct TCP connection on port 3306)
```

### SSH Tunnel Mode (SSH checked)
```
Your PC → SSH Tunnel → Jump Host → MySQL Server
        (encrypted)              (private network)
```

## Status Messages

| Message | Meaning |
|---------|---------|
| `Not connected` | No active connection |
| `● Connected` | Direct connection successful |
| `● Connected (via SSH tunnel)` | SSH tunnel connection successful |
| `✗ [error message]` | Connection failed (see error) |

## Tips

1. **Always fill in MySQL credentials** in the SSH section when using SSH tunnel
2. **Use SSH keys** instead of passwords for better security
3. **Test SSH first** before configuring the editor (use `ssh user@host`)
4. **Check "MySQL Host"** - it's the address from jump host's perspective, not yours
5. **Leave Key File blank** if using SSH password authentication

## Troubleshooting

| Problem | Solution |
|---------|----------|
| SSH fields are grayed out | Check the "Enable SSH Tunnel" checkbox |
| "SSH tunnel not available" | Install: `pip install sshtunnel paramiko<3.0` |
| Can't edit Direct Connection | SSH Tunnel is enabled - uncheck it |
| Connection works without SSH but not with SSH | Check MySQL Host is from jump host's view |
| "Access denied" with SSH enabled | Check MySQL User/Pass in SSH section (green labels) |
