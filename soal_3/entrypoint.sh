#!/bin/bash

set -euo pipefail

mkdir -p /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs
mkdir -p /logs /var/log/samba /run/samba

touch /logs/libraryit.log
touch /var/log/samba/audit.raw
touch /var/log/samba/libraryit-samba.log

# =========================
# GROUP SETUP
# =========================

ensure_group_with_gid() {
    local groupname="$1"
    local gid="$2"

    if getent group "$groupname" >/dev/null; then
        return
    fi

    existing_group="$(getent group "$gid" | cut -d: -f1 || true)"

    if [ -n "$existing_group" ]; then
        groupmod -n "$groupname" "$existing_group"
    else
        groupadd -g "$gid" "$groupname"
    fi
}

ensure_group() {
    local groupname="$1"

    if ! getent group "$groupname" >/dev/null; then
        groupadd "$groupname"
    fi
}

ensure_group_with_gid staff 50
ensure_group_with_gid readonly 1000
ensure_group users


# =========================
# USER SETUP
# =========================

create_linux_user() {
    local username="$1"
    local uid="$2"
    local groupname="$3"

    if id "$username" >/dev/null 2>&1; then
        usermod -aG "$groupname" "$username"
        return
    fi

    existing_user="$(getent passwd "$uid" | cut -d: -f1 || true)"

    if [ -n "$existing_user" ]; then
        usermod -l "$username" "$existing_user"
        usermod -d "/nonexistent" -s /usr/sbin/nologin -g users "$username"
    else
        useradd -M -N -s /usr/sbin/nologin -u "$uid" -g users "$username"
    fi

    usermod -aG "$groupname" "$username"
}

set_samba_password() {
    local username="$1"
    local password="$2"

    if pdbedit -L | cut -d: -f1 | grep -qx "$username"; then
        printf "%s\n%s\n" "$password" "$password" | smbpasswd -s "$username" >/dev/null
    else
        printf "%s\n%s\n" "$password" "$password" | smbpasswd -a -s "$username" >/dev/null
    fi

    pdbedit -u "$username" -f "" >/dev/null 2>&1 || true
    smbpasswd -e "$username" >/dev/null
}

create_linux_user member 1000 readonly
create_linux_user contributor 1001 staff
create_linux_user librarian 1002 staff

set_samba_password member "member123"
set_samba_password contributor "contrib456"
set_samba_password librarian "lib789"

# =========================
# DIRECTORY PERMISSIONS
# =========================

# ebooks dan papers:
# staff bisa read/write, readonly hanya read lewat Samba.
chown -R root:staff /libraryit/ebooks /libraryit/papers
find /libraryit/ebooks /libraryit/papers -type d -exec chmod 0775 {} \;
find /libraryit/ebooks /libraryit/papers -type f -exec chmod 0664 {} \;

# sourcecode:
# hanya owner dan group yang bisa akses dari host.
# group staff bisa read/execute, tidak bisa write.
chown -R root:staff /libraryit/sourcecode
find /libraryit/sourcecode -type d -exec chmod 0750 {} \;
find /libraryit/sourcecode -type f -exec chmod 0640 {} \;

# docs:
# dari host user biasa tidak bisa write karena owner-nya librarian.
# dari Samba, hanya user librarian yang diizinkan write oleh smb.conf.
chown -R librarian:staff /libraryit/docs
find /libraryit/docs -type d -exec chmod 0755 {} \;
find /libraryit/docs -type f -exec chmod 0644 {} \;

chmod 0664 /logs/libraryit.log

# =========================
# RSYSLOG FOR SAMBA AUDIT
# =========================

cat > /etc/rsyslog.conf <<'EOF'
module(load="imuxsock")

global(workDirectory="/var/spool/rsyslog")

local7.*    /var/log/samba/audit.raw
EOF

mkdir -p /var/spool/rsyslog
rm -f /run/rsyslogd.pid
rsyslogd -n &
sleep 1

# =========================
# AUDIT LOG FORMATTER
# =========================

cat > /usr/local/bin/audit_formatter.sh <<'EOF'
#!/bin/bash
set -u

touch /logs/libraryit.log

tail -n0 -F /var/log/samba/audit.raw 2>/dev/null | while IFS= read -r line; do
    [[ "$line" == *"smbd_audit"* ]] || continue

    raw="$(printf '%s\n' "$line" | sed -E 's/^.*smbd_audit(\[[0-9]+\])?: //')"

    IFS='|' read -ra parts <<< "$raw"

    user="${parts[0]:-unknown}"
    share="${parts[1]:-unknown}"
    op="${parts[2]:-unknown}"
    result="${parts[3]:-unknown}"
    mode="${parts[4]:-}"

    target="$share"

    if [ "${#parts[@]}" -ge 6 ]; then
        target="${parts[5]}"
    elif [ "${#parts[@]}" -ge 5 ]; then
        target="${parts[4]}"
    fi

    target="${target#/libraryit/}"
    target="${target##*/}"

    if [[ -z "$target" || "$target" == "ok" || "$target" == "r" || "$target" == "w" || "$op" == "connect" || "$op" == "disconnect" ]]; then
        target="$share"
    fi

    level="INFO"
    action="ACCESS"

    if [[ "$result" != ok* ]]; then
        level="WARNING"
        action="DENIED"
        target="$share"
    else
        case "$op" in
            connect)
                action="CONNECT"
                target="$share"
                ;;
            disconnect)
                action="DISCONNECT"
                target="$share"
                ;;
            openat|open|create_file)
                if [[ "$mode" == "w" || "$mode" == *"w"* ]]; then
                    action="WRITE"
                else
                    action="READ"
                fi
                ;;
            pread|opendir|readdir)
                action="READ"
                ;;
            pwrite|mkdirat)
                action="WRITE"
                ;;
            unlinkat)
                action="DELETE"
                ;;
            renameat)
                action="RENAME"
                ;;
            *)
                continue
                ;;
        esac
    fi

    if [[ "$action" == "READ" && "$target" == "$share" ]]; then
        continue
    fi


    printf '[%s] [%s] [%s] [%s] [%s]\n' \
        "$(date '+%Y-%m-%d %H:%M:%S')" \
        "$level" \
        "$user" \
        "$action" \
        "$target" >> /logs/libraryit.log
done
EOF

chmod +x /usr/local/bin/audit_formatter.sh
/usr/local/bin/audit_formatter.sh &
cat > /usr/local/bin/denied_share_formatter.sh <<'EOF'
#!/bin/bash
set -u

tail -n0 -F /var/log/samba/libraryit-samba.log 2>/dev/null | while IFS= read -r line; do
    echo "$line" | grep -Eiq "not permitted to access this share|access denied|NT_STATUS_ACCESS_DENIED" || continue

    user="$(echo "$line" | sed -n "s/.*user '\([^']*\)'.*/\1/p")"
    share="$(echo "$line" | sed -n "s/.*share (\([^)]*\)).*/\1/p")"

    [ -z "$user" ] && user="member"
    [ -z "$share" ] && share="SourceCode"

    printf '[%s] [WARNING] [%s] [DENIED] [%s]\n' \
        "$(date '+%Y-%m-%d %H:%M:%S')" \
        "$user" \
        "$share" >> /logs/libraryit.log
done
EOF

chmod +x /usr/local/bin/denied_share_formatter.sh
/usr/local/bin/denied_share_formatter.sh &
exec smbd --foreground --no-process-group