FROM ubuntu:20.04
MAINTAINER PeerPlays Blockchain Standards Association

#===============================================================================
# Ubuntu setup
#===============================================================================

RUN \
    apt-get update -y && \
      DEBIAN_FRONTEND=noninteractive apt-get install -y \
      apt-utils \
      autoconf \
      bash \
      build-essential \
      ca-certificates \
      cmake \
      dnsutils \
      doxygen \
      expect \
      git \
      graphviz \
      libboost1.67-all-dev \
      libbz2-dev \
      libcurl4-openssl-dev \
      libncurses-dev \
      libreadline-dev \
      libsnappy-dev \
      libssl-dev \
      libtool \
      libzip-dev \
      libzmq3-dev \
      locales \
      mc \
      nano \
      net-tools \
      ntp \
      openssh-server \
      pkg-config \
      perl \
      python3 \
      python3-jinja2 \
      sudo \
      wget

ENV HOME /home/peerplays
RUN useradd -rm -d /home/peerplays -s /bin/bash -g root -G sudo -u 1000 peerplays
RUN echo "peerplays  ALL=(ALL) NOPASSWD:ALL" | sudo tee /etc/sudoers.d/peerplays
RUN chmod 440 /etc/sudoers.d/peerplays

RUN service ssh start
RUN echo 'peerplays:peerplays' | chpasswd

# SSH
EXPOSE 22

#===============================================================================
# Peerplays setup
#===============================================================================

WORKDIR /home/peerplays/

## Clone Peerplays
#RUN \
#    git clone https://gitlab.com/PBSA/peerplays.git && \
#    cd peerplays && \
#    git checkout develop && \
#    git submodule update --init --recursive && \
#    git branch --show-current && \
#    git log --oneline -n 5

# Add local source
ADD . peerplays

# Configure Peerplays
RUN \
    cd peerplays && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release ..

# Build Peerplays
RUN \
    cd peerplays/build && \
    make -j$(nproc) cli_wallet witness_node

WORKDIR /home/peerplays/peerplays-network

# Setup Peerplays runimage
RUN \
    ln -s /home/peerplays/peerplays/build/programs/cli_wallet/cli_wallet ./ && \
    ln -s /home/peerplays/peerplays/build/programs/witness_node/witness_node ./

RUN ./witness_node --create-genesis-json genesis.json && \
    rm genesis.json

RUN chown peerplays:root -R /home/peerplays/peerplays-network

# Peerplays RPC
EXPOSE 8090
# Peerplays P2P:
EXPOSE 9777

# Peerplays
CMD ["./witness_node", "-d", "./witness_node_data_dir"]
