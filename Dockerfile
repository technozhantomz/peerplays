FROM ubuntu:20.04
MAINTAINER Peerplays Blockchain Standards Association

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

#===============================================================================
# Boost setup
#===============================================================================

WORKDIR /home/peerplays/

RUN \
     wget https://boostorg.jfrog.io/artifactory/main/release/1.72.0/source/boost_1_72_0.tar.gz && \
    tar -xzvf boost_1_72_0.tar.gz boost_1_72_0 && \
    cd boost_1_72_0/ && \
    ./bootstrap.sh && \
    ./b2 install

#===============================================================================
# cmake setup
#===============================================================================

WORKDIR /home/peerplays/

RUN \
    wget -c 'https://cmake.org/files/v3.23/cmake-3.23.1-linux-x86_64.sh' -O cmake-3.23.1-linux-x86_64.sh && \
    chmod 755 ./cmake-3.23.1-linux-x86_64.sh && \
    ./cmake-3.23.1-linux-x86_64.sh --prefix=/usr/ --skip-license && \
    cmake --version

#===============================================================================
# libzmq setup
#===============================================================================

WORKDIR /home/peerplays/

RUN \
    wget https://github.com/zeromq/libzmq/archive/refs/tags/v4.3.4.zip && \
    unzip v4.3.4.zip && \
    cd libzmq-4.3.4 && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make -j$(nproc) install && \
    ldconfig

#===============================================================================
# cppzmq setup
#===============================================================================

WORKDIR /home/peerplays/

RUN \
    wget https://github.com/zeromq/cppzmq/archive/refs/tags/v4.8.1.zip && \
    unzip v4.8.1.zip && \
    cd cppzmq-4.8.1 && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make -j$(nproc) install && \
    ldconfig

#===============================================================================
# Doxygen setup
#===============================================================================

WORKDIR /home/peerplays/

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

WORKDIR /home/peerplays/

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
    git submodule update --init --recursive && \
    git symbolic-ref --short HEAD && \
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
