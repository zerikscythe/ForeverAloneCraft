# SSH Tunnel Quick Reference

## Connection Flow

```
Without SSH Tunnel:
┌─────────┐                           ┌──────────────┐
│ Your PC │───── Direct MySQL ───────→│ MySQL Server │
│ Editor  │      Port 3306            │   Database   │
└─────────┘                           └──────────────┘


With SSH Tunnel:
┌─────────┐         SSH          ┌───────────┐       MySQL       ┌──────────────┐
│ Your PC │─────   Port 22   ────→│ Jump Host │───── Port 3306 ──→│ MySQL Server │
│ Editor  │   (Encrypted Tunnel)  │  Middle   │   (Private Net)   │   Database   │
└─────────┘                       └───────────┘                   └──────────────┘
```

## Quick Config

### Scenario 1: DB on same server as SSH
```ini
[database]
host = 127.0.0.1
port = 3306
user = acore
password = acore

[ssh]
enabled = 1
host = myserver.com
port = 22
user = admin
password = 
key_file = C:\Users\You\.ssh\id_rsa
db_host = 127.0.0.1    ← Database is localhost from jump host
db_port = 3306
```

### Scenario 2: DB on different server
```ini
[database]
host = 127.0.0.1
port = 3306
user = acore
password = acore

[ssh]
enabled = 1
host = 192.168.0.50    ← Jump host IP
port = 22
user = youruser
password = 
key_file = C:\Users\You\.ssh\id_rsa
db_host = 192.168.0.93  ← Database server IP (from jump host)
db_port = 3306
```

### Scenario 3: No SSH (Direct)
```ini
[database]
host = 192.168.0.93    ← Direct to database
port = 3306
user = acore
password = acore

[ssh]
enabled = 0
```

## Field Meanings

| Field | Meaning | Example |
|-------|---------|---------|
| `[database] host` | Where MySQL connects | `127.0.0.1` (with SSH) or `192.168.0.93` (direct) |
| `[database] port` | MySQL port | `3306` |
| `[ssh] host` | Jump host address | `192.168.0.50` or `jump.example.com` |
| `[ssh] port` | SSH port | `22` |
| `[ssh] db_host` | DB address *from jump host* | `192.168.0.93` or `127.0.0.1` |
| `[ssh] db_port` | DB port on remote | `3306` |

## Common Mistakes

❌ **Wrong**: `db_host` is the address from YOUR PC
✅ **Right**: `db_host` is the address from the JUMP HOST

❌ **Wrong**: Using direct DB IP in `[database] host` when SSH enabled
✅ **Right**: Use `127.0.0.1` in `[database] host` when SSH enabled

❌ **Wrong**: SSH enabled but `sshtunnel` not installed
✅ **Right**: Run `pip install sshtunnel` first

## Testing Steps

1. **Test SSH Access**:
   ```bash
   ssh user@jumphost
   ```
   Should work without errors.

2. **Test DB from Jump Host**:
   ```bash
   ssh user@jumphost
   mysql -h 192.168.0.93 -u acore -p
   ```
   Should connect to database.

3. **Test in Editor**:
   - Configure all fields
   - Click "Connect"
   - Should show: `● Connected (via SSH tunnel)`

## Troubleshooting

| Error | Solution |
|-------|----------|
| "SSH tunnel not available" | `pip install sshtunnel` |
| "Connection refused" | Check SSH credentials, test SSH manually |
| "Access denied for user" | MySQL credentials wrong, test from jump host |
| "Can't connect to MySQL" | Check `db_host` (must be from jump host view) |
| Checkbox grayed out | Install `sshtunnel` package |

## Security Tips

🔒 **Best Practices**:
1. Use SSH keys instead of passwords
2. Protect SSH key with passphrase
3. Don't commit `config.ini` to git
4. Restrict config file permissions (Linux/Mac: `chmod 600 config.ini`)
5. Use different MySQL passwords for dev/prod

## Files

| File | Purpose |
|------|---------|
| `config.ini` | Your actual config (don't commit!) |
| `config.ini.example` | Example config template |
| `SSH_SETUP.md` | Detailed setup guide |
| `CHANGELOG_SSH.md` | Technical changelog |

## Getting Help

1. Check `SSH_SETUP.md` for detailed instructions
2. Test each layer separately (SSH, then MySQL)
3. Verify `db_host` is correct from jump host's perspective
4. Check firewall rules on all machines
5. Try SSH password first, then switch to keys
