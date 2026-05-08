# deploy-binary tool

Deploys a freshly built binary (`worldserver.exe` or any file) to the remote
server machine over SSH/SFTP using the same credentials as the lw-editor.

## Setup (one time)

1. Copy `deploy.ini.example` to `deploy.ini` in this folder:
   ```
   copy tools\deploy-binary\deploy.ini.example tools\deploy-binary\deploy.ini
   ```
2. Edit `deploy.ini` and set `remote_path` to the folder on the server that
   holds `worldserver.exe`.
3. SSH credentials are read automatically from `tools/lw-editor/config.ini` —
   no extra setup needed if the lw-editor already works.

## Usage

**Default** — deploys `out/build-vs2022/bin/Debug/worldserver.exe`:
```
python tools/deploy-binary/deploy_binary.py
```

**Specific file:**
```
python tools/deploy-binary/deploy_binary.py path\to\worldserver.exe
```

**Drag and drop:**
Drag `worldserver.exe` onto `deploy_binary.py` in Explorer and it will deploy
that file automatically.

## Requirements

`paramiko` must be installed (already a dependency of the lw-editor):
```
pip install paramiko
```
