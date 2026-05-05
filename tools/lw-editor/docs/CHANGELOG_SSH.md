# SSH Tunnel Support - Changelog

## Version 0.2 - SSH Tunnel Support

**Date**: 2025-01-XX

### New Features

#### SSH Tunnel Connection
Added full SSH tunnel support for connecting to remote MySQL databases through jump hosts.

**Use Cases**:
- Database server on private network
- Multi-hop connection scenarios (PC → Jump Host → DB Server)
- Secure connections through bastion hosts
- Remote server access without direct port forwarding

**Configuration Options**:
- SSH Host/Port configuration
- Username/Password authentication
- SSH key file authentication (recommended)
- Key passphrase support
- Configurable database host/port from jump host perspective

### UI Changes

**Connection Bar Enhanced**:
- Added second row for SSH tunnel configuration
- SSH Tunnel checkbox to enable/disable
- Auto-disable SSH fields when unchecked
- "Browse Key" button for easy SSH key selection
- Status indicator shows "(via SSH tunnel)" when connected through SSH
- Graceful degradation if `sshtunnel` package not installed

**Config File Format Updated**:
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
user = youruser
password = 
key_file = C:\Users\You\.ssh\id_rsa
db_host = 192.168.0.93
db_port = 3306
```

### Code Changes

**DBCtx Class (`lw_bot_editor.py`)**:
- Added `_ssh_tunnel` member to track tunnel state
- Enhanced `connect()` method with SSH tunnel parameters
- Added `_close_ssh_tunnel()` method for cleanup
- Updated `disconnect()` to properly close tunnel
- Tunnel automatically starts/stops with database connection

**App Class (`lw_bot_editor.py`)**:
- Added SSH configuration UI fields
- Added `_toggle_ssh_fields()` to enable/disable SSH controls
- Added `_browse_ssh_key()` for SSH key file selection
- Updated `_connect()` to pass SSH parameters
- Updated `_save_config()` and `_load_saved_config()` for SSH settings
- Enhanced status message to show tunnel status

**Import Handling**:
- Added graceful `sshtunnel` import with availability check
- `SSH_TUNNEL_AVAILABLE` flag for runtime detection
- Clear error message if tunnel requested without package

### Dependencies

**New Required Package**:
```bash
pip install sshtunnel
```

**Updated `requirements.txt`**:
```
mysql-connector-python>=8.0
sshtunnel>=0.4.0
```

The `sshtunnel` package brings in these sub-dependencies:
- `paramiko` - SSH2 protocol implementation
- `cryptography` - Cryptographic functions
- `bcrypt` - Password hashing for SSH
- `pynacl` - Networking and cryptography
- `cffi` - Foreign function interface

### Documentation

**New Files**:
- `SSH_SETUP.md` - Comprehensive SSH tunnel setup guide
  - Architecture diagrams
  - Step-by-step configuration
  - Example configurations for common scenarios
  - SSH key setup instructions (Windows/Linux/Mac)
  - Troubleshooting section
  - Security best practices

**Updated Files**:
- `README.md` - Added SSH tunnel overview and link to setup guide
- `config.ini.example` - Updated with SSH tunnel example configuration

### Security Considerations

**What's Secure**:
- SSH tunnel encrypts all MySQL traffic
- SSH key authentication supported (recommended over passwords)
- Key passphrase protection available
- No port forwarding required on database server

**Security Warnings**:
- `config.ini` stores passwords in plain text
- Users should restrict file permissions
- SSH keys are recommended over passwords
- File should not be committed to version control

### Backward Compatibility

✅ **Fully backward compatible**:
- Existing configurations continue to work
- SSH tunnel is optional (checkbox)
- Editor works without `sshtunnel` package for direct connections
- Old `config.ini` files are still valid (SSH disabled by default)
- No database or code changes required

### Testing Checklist

- [x] Direct connection (SSH disabled) still works
- [x] SSH tunnel connection with password authentication
- [x] SSH tunnel connection with key file authentication
- [x] SSH tunnel connection with passphrase-protected key
- [x] Config save/load preserves SSH settings
- [x] UI enables/disables SSH fields based on checkbox
- [x] Status message shows tunnel status correctly
- [x] Tunnel properly closes on disconnect
- [x] Graceful error when `sshtunnel` not installed
- [x] Browse key file dialog works on Windows
- [x] Multiple database connections through same tunnel

### Known Limitations

1. **Single Tunnel Per Connection**: Only one SSH hop supported directly. Multi-hop requires SSH config ProxyJump setup.
2. **Windows SSH Keys**: Windows paths must use backslashes or double-escaped forward slashes in `config.ini`
3. **Password Storage**: Config file stores passwords in plain text (SSH keys recommended)
4. **No Tunnel Status Indicator**: No real-time tunnel health monitoring (just connected/disconnected)

### Future Enhancements

**Potential additions** (not in this release):
- [ ] Multi-hop SSH tunnel support (chained tunnels)
- [ ] Encrypted config file storage
- [ ] SSH agent support for key management
- [ ] Tunnel health monitoring/auto-reconnect
- [ ] Connection profiles (save multiple configs)
- [ ] SSH config file parsing (`~/.ssh/config`)
- [ ] Visual tunnel status indicator
- [ ] Test connection button
- [ ] SSH key generation wizard

### Migration Guide

**For Existing Users**:

1. **Update Dependencies**:
   ```bash
   pip install sshtunnel
   ```

2. **No Config Changes Needed**: Existing `config.ini` files work as-is (SSH disabled by default)

3. **To Enable SSH Tunnel**:
   - Open the editor
   - Check "SSH Tunnel" checkbox
   - Fill in SSH host, user, and authentication
   - Fill in database host/port (from jump host's perspective)
   - Click "Connect"

4. **Config File Will Auto-Update** with SSH section on next connect

### Troubleshooting New Issues

**"Module sshtunnel not found"**:
```bash
pip install sshtunnel
```

**"SSH tunnel checkbox is grayed out"**:
- Package not installed, see error message in UI
- Install `sshtunnel` package

**"Connection refused" after enabling SSH**:
- Verify SSH credentials work: `ssh user@jumphost`
- Check database host/port from jump host perspective
- Test MySQL from jump host: `mysql -h dbhost -u user -p`

**Tunnel connects but MySQL fails**:
- Check "DB Host" field (must be from jump host's viewpoint)
- Verify MySQL user allows connections from jump host
- Check MySQL is listening on correct port

## Version History

### v0.2 (Current)
- Added SSH tunnel support
- Enhanced connection configuration
- Improved security options

### v0.1
- Initial release
- Direct MySQL connection
- Basic profile editing
- Threat-aware conditions
