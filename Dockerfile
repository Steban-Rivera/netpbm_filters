FROM ubuntu:24.04

RUN apt-get update && \
    apt-get install -y \
        openmpi-bin \
        libopenmpi-dev \
        g++ \
        openssh-server \
        openssh-client && \
    rm -rf /var/lib/apt/lists/*

RUN mkdir -p /run/sshd /root/.ssh

RUN ssh-keygen -t ed25519 -f /root/.ssh/id_ed25519 -N "" && \
    cp /root/.ssh/id_ed25519.pub /root/.ssh/authorized_keys && \
    chmod 600 /root/.ssh/authorized_keys

RUN printf '%s\n' \
'Host *' \
'    StrictHostKeyChecking no' \
'    UserKnownHostsFile /dev/null' \
> /root/.ssh/config && \
chmod 600 /root/.ssh/config

RUN printf '%s\n' \
    'PermitRootLogin yes' \
    'PubkeyAuthentication yes' \
    'PasswordAuthentication no' \
    'StrictModes no' \
    > /etc/ssh/sshd_config.d/mpi.conf

WORKDIR /app

COPY src/mpi_processor.cpp /app/mpi_processor.cpp

RUN mpic++ -std=c++17 -O2 mpi_processor.cpp -o mpi_processor

COPY images /app/images

RUN printf '#!/bin/bash\n\
mkdir -p /run/sshd\n\
/usr/sbin/sshd\n\
exec "$@"\n' > /entrypoint.sh && \
    chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]