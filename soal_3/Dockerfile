FROM ubuntu:latest

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    samba \
    samba-common-bin \
    samba-vfs-modules \
    rsyslog \
    acl \
    procps \
    && rm -rf /var/lib/apt/lists/*

COPY smb.conf /etc/samba/smb.conf
COPY entrypoint.sh /entrypoint.sh

RUN chmod +x /entrypoint.sh \
    && mkdir -p /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs \
    && mkdir -p /logs /var/log/samba /run/samba

EXPOSE 445

ENTRYPOINT ["/entrypoint.sh"]