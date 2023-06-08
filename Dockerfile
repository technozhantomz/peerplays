FROM ubuntu:20.04

#===============================================================================
# Ubuntu setup
#===============================================================================

RUN \
    apt-get update -y && \
      DEBIAN_FRONTEND=noninteractive apt-get install -y \
      apt-utils \
      autoconf \
      bash \
      bison \
      build-essential \
      ca-certificates \
      dnsutils \
      expect \
      flex \
      git \
      graphviz \
      libbz2-dev \
      libcurl4-openssl-dev \
      libncurses-dev \
      libsnappy-dev \
      libssl-dev \
      libtool \
      libzip-dev \
      locales \
      lsb-release \
      mc \
      nano \
      net-tools \
      ntp \
      openssh-server \
      pkg-config \
      python3 \
      python3-jinja2 \
      sudo \
      systemd-coredump \
      wget

ENV HOME /home/peerplays
RUN useradd -rm -d /home/peerplays -s /bin/bash -g root -G sudo -u 1000 peerplays
RUN echo "peerplays  ALL=(ALL) NOPASSWD:ALL" | sudo tee /etc/sudoers.d/peerplays
RUN chmod 440 /etc/sudoers.d/peerplays

RUN service ssh start
RUN echo 'peerplays:peerplays' | chpasswd

# SSH
EXPOSE 22

WORKDIR /home/peerplays/src

#===============================================================================
# Boost setup
#===============================================================================

RUN \
    wget https://boostorg.jfrog.io/artifactory/main/release/1.72.0/source/boost_1_72_0.tar.gz && \
    tar -xzf boost_1_72_0.tar.gz && \
    cd boost_1_72_0 && \
    ./bootstrap.sh && \
    ./b2 install && \
    ldconfig && \
    rm -rf /home/peerplays/src/*

#===============================================================================
# cmake setup
#===============================================================================

RUN \
    wget https://github.com/Kitware/CMake/releases/download/v3.24.2/cmake-3.24.2-linux-x86_64.sh && \
    chmod 755 ./cmake-3.24.2-linux-x86_64.sh && \
    ./cmake-3.24.2-linux-x86_64.sh --prefix=/usr --skip-license && \
    cmake --version && \
    rm -rf /home/peerplays/src/*

#===============================================================================
# libzmq setup
#===============================================================================

RUN \
    wget https://github.com/zeromq/libzmq/archive/refs/tags/v4.3.4.tar.gz && \
    tar -xzvf v4.3.4.tar.gz && \
    cd libzmq-4.3.4 && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    rm -rf /home/peerplays/src/*

#===============================================================================
# cppzmq setup
#===============================================================================

RUN \
    wget https://github.com/zeromq/cppzmq/archive/refs/tags/v4.9.0.tar.gz && \
    tar -xzvf v4.9.0.tar.gz && \
    cd cppzmq-4.9.0 && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    rm -rf /home/peerplays/src/*

#===============================================================================
# gsl setup
#===============================================================================

RUN \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
      libpcre3-dev

RUN \
    wget https://github.com/imatix/gsl/archive/refs/tags/v4.1.4.tar.gz && \
    tar -xzvf v4.1.4.tar.gz && \
    cd gsl-4.1.4 && \
    make -j$(nproc) && \
    make install && \
    rm -rf /home/peerplays/src/*

#===============================================================================
# libbitcoin-build setup
# libbitcoin-explorer setup
#===============================================================================

RUN \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
      libsodium-dev

RUN \
    git clone https://github.com/libbitcoin/libbitcoin-build.git && \
    cd libbitcoin-build && \
    git reset --hard 92c215fc1ffa272bab4d485d369d0306db52d69d && \
    ./generate3.sh && \
    cd ../libbitcoin-explorer && \
    ./install.sh && \
    ldconfig && \
    rm -rf /home/peerplays/src/*

#===============================================================================
# Doxygen setup
#===============================================================================

RUN \
    sudo apt install -y bison flex && \
    wget https://github.com/doxygen/doxygen/archive/refs/tags/Release_1_8_17.tar.gz && \
    tar -xvf Release_1_8_17.tar.gz && \
    cd doxygen-Release_1_8_17 && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make -j$(nproc) install && \
    ldconfig

#===============================================================================
# Perl setup
#===============================================================================

RUN \
    wget https://github.com/Perl/perl5/archive/refs/tags/v5.30.0.tar.gz && \
    tar -xvf v5.30.0.tar.gz && \
    cd perl5-5.30.0 && \
    ./Configure -des && \
    make -j$(nproc) install && \
    ldconfig

#===============================================================================
# Peerplays setup
#===============================================================================

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
    git submodule update --init --recursive && \
    git log --oneline -n 5 && \
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
    ln -s /home/peerplays/src/peerplays/build/programs/cli_wallet/cli_wallet ./ && \
    ln -s /home/peerplays/src/peerplays/build/programs/witness_node/witness_node ./

RUN ./witness_node --create-genesis-json genesis.json && \
    rm genesis.json

RUN chown peerplays:root -R /home/peerplays/peerplays-network

# Peerplays RPC
EXPOSE 8090
# Peerplays P2P:
EXPOSE 9777

# Peerplays
CMD ["./witness_node", "-d", "./witness_node_data_dir"]
