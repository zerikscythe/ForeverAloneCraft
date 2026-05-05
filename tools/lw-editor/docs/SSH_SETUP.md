# SSH Tunnel Setup Guide for LivingWorld Bot Editor

This guide explains how to connect to your database server through an SSH "jump host" when direct network access is not available.

## Architecture

```
[Your PC] --SSH--> [Jump Host] --MySQL--> [Database Server]
```

**Scenario**: You want to connect to a MySQL database server, but:
- The database server is on a private network
- You can SSH into a "middle" machine that has access to the database
- The database server is not directly accessible from your PC

## Quick Setup

### 1. Install Required Package

```bash
pip install sshtunnel
```

This package is optional - if not installed, the editor will still work for direct connections.

### 2. Configure SSH Tunnel in the Editor

When you open the bot editor, you'll see two connection rows:

**Direct Connection Row:**
- Host: `127.0.0.1` (localhost, since we'll tunnel)
- Port: `3306` (or whatever your MySQL uses)
- User: Your MySQL username (e.g., `acore`)
- Pass: Your MySQL password

**SSH Tunnel Row:**
- ☑ **SSH Tunnel** (check this box)
- SSH Host: The IP/hostname of your jump host (e.g., `192.168.0.50` or `jump.example.com`)
- Port: SSH port (usually `22`)
- User: Your SSH username on the jump host
- Pass: Your SSH password OR leave blank if using key
- Key: Path to your SSH private key file (e.g., `C:\Users\You\.ssh\id_rsa`)
- DB Host: The database server address *as seen from the jump host* (e.g., `192.168.0.93` or `127.0.0.1` if DB is on jump host)
- DB Port: MySQL port on the database server (usually `3306`)

### 3. Example Configurations

#### Example 1: Database on a Different Server than SSH Host
```
Direct Connection:
  Host: 127.0.0.1
  Port: 3306
  User: acore
  Pass: acore

SSH Tunnel:
  ☑ SSH Tunnel
  SSH Host: 192.168.0.50  (your jump/middle PC)
  Port: 22
  User: john
  Pass: (your SSH password)
  Key: (or browse to your key)
  DB Host: 192.168.0.93  (database server IP from jump host's perspective)
  DB Port: 3306
```

#### Example 2: Database and SSH on Same Server
```
Direct Connection:
  Host: 127.0.0.1
  Port: 3306
  User: acore
  Pass: acore

SSH Tunnel:
  ☑ SSH Tunnel
  SSH Host: myserver.com
  Port: 22
  User: admin
  Pass: (your SSH password)
  Key: (or browse to your key)
  DB Host: 127.0.0.1  (database is on the same server as SSH)
  DB Port: 3306
```

#### Example 3: Using SSH Key Authentication (Recommended)
```
Direct Connection:
  Host: 127.0.0.1
  Port: 3306
  User: acore
  Pass: acore

SSH Tunnel:
  ☑ SSH Tunnel
  SSH Host: 10.0.0.100
  Port: 22
  User: dbadmin
  Pass: (leave blank if key has no passphrase, or enter passphrase)
  Key: C:\Users\YourName\.ssh\id_rsa
  DB Host: 192.168.1.10
  DB Port: 3306
```

## SSH Key Setup (Recommended for Security)

### Windows

1. **Generate SSH Key** (if you don't have one):
   ```powershell
   ssh-keygen -t rsa -b 4096 -C "your_email@example.com"
   ```
   - Save to default location: `C:\Users\YourName\.ssh\id_rsa`
   - Optionally set a passphrase

2. **Copy Public Key to Jump Host**:
   ```powershell
   type C:\Users\YourName\.ssh\id_rsa.pub | ssh user@jumphost "cat >> ~/.ssh/authorized_keys"
   ```
   Or manually copy the contents of `id_rsa.pub` to `~/.ssh/authorized_keys` on the jump host

3. **In the Editor**:
   - Click "Browse Key"
   - Select `C:\Users\YourName\.ssh\id_rsa`
   - Leave "Pass" blank (unless your key has a passphrase)

### Linux/Mac

1. **Generate SSH Key** (if you don't have one):
   ```bash
   ssh-keygen -t rsa -b 4096 -C "your_email@example.com"
   ```

2. **Copy Public Key to Jump Host**:
   ```bash
   ssh-copy-id user@jumphost
   ```

3. **In the Editor**:
   - Click "Browse Key"
   - Select `~/.ssh/id_rsa` (usually `/home/username/.ssh/id_rsa`)
   - Leave "Pass" blank (unless your key has a passphrase)

## Troubleshooting

### "SSH tunnel requested but sshtunnel package not installed"
**Solution**: Install the package:
```bash
pip install sshtunnel
```

### "Connection refused" or "Unable to connect"
**Possible causes**:
1. **Wrong SSH credentials**: Verify your SSH username/password or key
2. **SSH service not running**: Make sure SSH server is running on jump host
3. **Firewall blocking SSH**: Check firewall on jump host (port 22)
4. **Wrong DB Host/Port**: Verify the database server address from jump host's perspective

**Test SSH separately**:
```bash
ssh user@jumphost
# If this works, your SSH credentials are correct
```

### "Access denied for user"
**Possible causes**:
1. **Wrong MySQL credentials**: This is a MySQL authentication error, not SSH
2. **Database user restricted by host**: MySQL user might only allow connections from specific hosts

**Test from jump host**:
```bash
# SSH into jump host first
ssh user@jumphost

# Then test MySQL connection from there
mysql -h 192.168.0.93 -u acore -p
```

### SSH Key Permission Errors
**Windows**: No special permissions needed usually
**Linux/Mac**: Key file must have restricted permissions:
```bash
chmod 600 ~/.ssh/id_rsa
```

### "DB Host" Confusion
**Important**: The "DB Host" field is the database server address *as seen from the jump host*, not from your PC.

If you SSH into the jump host and can connect to the database with:
```bash
mysql -h 192.168.0.93 -u acore -p
```
Then your "DB Host" should be `192.168.0.93`.

If the database is on the same machine as SSH:
```bash
mysql -h localhost -u acore -p
```
Then your "DB Host" should be `127.0.0.1` or `localhost`.

## Config File Format

Settings are saved to `config.ini`:

```ini
[database]
host = 127.0.0.1
port = 3306
user = acore
password = acore

[ssh]
enabled = 1
host = 192.168.0.50
port = 22
user = john
password = 
key_file = C:\Users\John\.ssh\id_rsa
db_host = 192.168.0.93
db_port = 3306
```

**Security Note**: The config file stores passwords in plain text. Use SSH keys when possible, and protect the `config.ini` file (don't commit to git).

## Testing Your Connection

### Step-by-Step Validation

1. **Test SSH Connection**:
   ```bash
   ssh -p 22 user@jumphost
   ```
   If this fails, fix SSH credentials first.

2. **Test MySQL from Jump Host**:
   ```bash
   # While SSH'd into jump host:
   mysql -h 192.168.0.93 -u acore -p
   ```
   If this fails, check MySQL credentials or DB server accessibility.

3. **Test in Editor**:
   - Configure all fields
   - Click "Connect"
   - Status should show "● Connected (via SSH tunnel)" in green

### Verify It's Working

When connected successfully, the status will show:
```
● Connected (via SSH tunnel)
```

If it shows just "● Connected" without "(via SSH tunnel)", the tunnel is not being used (SSH checkbox might be unchecked).

## Advanced: Multiple Jump Hosts (Chain)

If you need to hop through multiple servers:

```
[Your PC] --SSH--> [Jump1] --SSH--> [Jump2] --MySQL--> [DB]
```

The `sshtunnel` package doesn't directly support multiple hops, but you can:

**Option 1**: Set up SSH config on your PC with ProxyJump:
```
# In ~/.ssh/config (Linux/Mac) or C:\Users\You\.ssh\config (Windows)
Host final
    HostName jump2
    User youruser
    ProxyJump jump1
```

Then use "final" as your SSH Host in the editor.

**Option 2**: Create a tunnel on Jump1 that forwards to Jump2, then use Jump1 as your SSH host in the editor.

## Security Best Practices

1. ✅ **Use SSH keys instead of passwords**
2. ✅ **Add passphrase to your SSH key**
3. ✅ **Restrict `config.ini` file permissions**
4. ✅ **Don't commit `config.ini` to version control**
5. ✅ **Use different MySQL passwords for production vs development**
6. ✅ **Restrict MySQL users by host (e.g., only allow from jump host)**

## Need Help?

If you're still having issues:

1. Check the editor status message - it usually indicates what went wrong
2. Test each layer separately (SSH, then MySQL from jump host)
3. Verify firewall rules on all machines
4. Check MySQL user host restrictions in `mysql.user` table
5. Try with SSH password first, then switch to keys once it works
