Intro for new developers and witnesses
------------------------

This is a quick introduction to get new developers and witnesses up to speed on Peerplays blockchain. It is intended for witnesses plannig to join a live, already deployed blockchain.


# Building and Installation Instructions

Officially supported OS are Ubuntu 20.04 and Ubuntu 18.04.

## Ubuntu 20.04

Following dependencies are needed for a clean install of Ubuntu 20.04:
```
sudo apt-get install \
    apt-utils autoconf bash build-essential ca-certificates clang-format cmake \
    dnsutils doxygen expect git graphviz libboost-all-dev libbz2-dev \
    libcurl4-openssl-dev libncurses-dev libsnappy-dev \
    libssl-dev libtool libzip-dev locales lsb-release mc nano net-tools ntp \
    openssh-server pkg-config perl python3 python3-jinja2 sudo \
    systemd-coredump wget
```

Install libzmq from source:
```
wget https://github.com/zeromq/libzmq/archive/refs/tags/v4.3.4.zip
unzip v4.3.4.zip
cd libzmq-4.3.4
mkdir build
cd build
cmake ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

Install cppzmq from source:
```
wget https://github.com/zeromq/cppzmq/archive/refs/tags/v4.8.1.zip
unzip v4.8.1.zip
cd cppzmq-4.8.1
mkdir build
cd build
cmake ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

Building Peerplays
```
git clone https://gitlab.com/PBSA/peerplays.git
cd peerplays
git submodule update --init --recursive

# If you want to build Mainnet node
cmake -DCMAKE_BUILD_TYPE=Release

# If you want to build Testnet node
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_PEERPLAYS_TESTNET=1

# Update -j flag depending on your current system specs;
# Recommended 4GB of RAM per 1 CPU core
# make -j2 for 8GB RAM
# make -j4 for 16GB RAM
# make -j8 for 32GB RAM
make -j$(nproc)

sudo make install # this can install the executable files under /usr/local
```

## Ubuntu 18.04

Following dependencies are needed for a clean install of Ubuntu 18.04:
```
sudo apt-get install \
    apt-utils autoconf bash build-essential ca-certificates clang-format \
    dnsutils doxygen expect git graphviz libbz2-dev \
    libcurl4-openssl-dev libncurses-dev libsnappy-dev \
    libssl-dev libtool libzip-dev locales lsb-release mc nano net-tools ntp \
    openssh-server pkg-config perl python3 python3-jinja2 sudo \
    systemd-coredump wget
```

Install Boost libraries from source
```
wget -c 'http://sourceforge.net/projects/boost/files/boost/1.67.0/boost_1_67_0.tar.bz2/download' -O boost_1_67_0.tar.bz2
tar xjf boost_1_67_0.tar.bz2
cd boost_1_67_0/
./bootstrap.sh
sudo ./b2 install
```

Install cmake
```
wget -c 'https://cmake.org/files/v3.23/cmake-3.23.1-linux-x86_64.sh' -O cmake-3.23.1-linux-x86_64.sh
chmod 755 ./cmake-3.23.1-linux-x86_64.sh
sudo ./cmake-3.23.1-linux-x86_64.sh --prefix=/usr/ --skip-license
```

Install libzmq from source:
```
wget https://github.com/zeromq/libzmq/archive/refs/tags/v4.3.4.zip
unzip v4.3.4.zip
cd libzmq-4.3.4
mkdir build
cd build
cmake ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

Install cppzmq from source:
```
wget https://github.com/zeromq/cppzmq/archive/refs/tags/v4.8.1.zip
unzip v4.8.1.zip
cd cppzmq-4.8.1
mkdir build
cd build
cmake ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

Building Peerplays
```
git clone https://gitlab.com/PBSA/peerplays.git
cd peerplays
git submodule update --init --recursive

# If you want to build Mainnet node
cmake -DCMAKE_BUILD_TYPE=Release

# If you want to build Testnet node
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_PEERPLAYS_TESTNET=1

# Update -j flag depending on your current system specs;
# Recommended 4GB of RAM per 1 CPU core
# make -j2 for 8GB RAM
# make -j4 for 16GB RAM
# make -j8 for 32GB RAM
make -j$(nproc)

sudo make install # this can install the executable files under /usr/local
```


## Docker images

Install docker, and add current user to docker group.
```
sudo apt install docker.io
sudo usermod -a -G docker $USER

# You need to restart your shell session, to apply group membership
# Type 'groups' to verify that you are a member of a docker group
```

### Official docker image for Peerplas Mainnet

```
docker pull datasecuritynode/peerplays:latest
```

### Building docker images manually
```
# Checkout the code
git clone https://gitlab.com/PBSA/peerplays.git
cd peerplays

# Checkout the branch you want
# E.g.
# git checkout beatrice
# git checkout develop
git checkout master

git submodule update --init --recursive

# Execute from the project root, must be a docker group member

# Build docker image, using Ubuntu 20.04 base
docker build --no-cache -f Dockerfile -t peerplays .

# Build docker image, using Ubuntu 18.04 base
docker build --no-cache -f Dockerfile.18.04 -t peerplays-18-04 .
```

### Start docker image
```
# Start docker image, using Ubuntu 20.04 base
docker run peerplays:latest

# Start docker image, using Ubuntu 18.04 base
docker run peerplays-18-04:latest
```

Rest of the instructions on starting the chain remains same.

Starting A Peerplays Node
-----------------
Launching the witness creates required directories. Next, **stop the witness** and continue.

    $ vi witness_node_data_dir/config.ini
    p2p-endpoint = 0.0.0.0:9777
    rpc-endpoint = 127.0.0.1:8090
    seed-node = 213.184.225.234:59500

Start the witness back up

    ./programs/witness_node/witness_node

Upgrading A Peerplays Node
-----------------
To minimize downtime of your peerplays node when upgrading, one upgrade
idea was written in [this steemit
article](https://steemit.com/peerplays/@joseph/peerplays-update-setting-a-backup-witness-server-switching-servers).

Wallet Setup
-----------------
Then, in a separate terminal window, start the command-line wallet `cli_wallet`:

    ./programs/cli_wallet/cli_wallet

To set your initial password to 'password' use:

    >>> set_password password
    >>> unlock password

A list of CLI wallet commands is available
[here](https://github.com/PBSA/peerplays/blob/master/libraries/wallet/include/graphene/wallet/wallet.hpp).


Testnet - "Beatrice"
----------------------
- chain-id - T.B.D.

Use the `get_private_key_from_password` command
---------------------------------
You will to generate owner and active keys

```
get_private_key_from_password your_witness_username active the_key_you_received_from_the_faucet
```
This will reveal an array for your active key `["PPYxxx", "xxxx"]`

import_keys into your cli_wallet
-------------------------------
- use the second value in the array returned from the previous step for the private key
- be sure to wrap your username in quotes
- import the key with this command
```
import_key "your_witness_username" xxxx
```

Upgrade your account to lifetime membership
--------------------------------
```
upgrade_account your_witness_username true
```

Create your witness (substitute the url for your witness information)
-------------------------------
- place quotes around url
```
create_witness your_witness_username "url" true
```
**Be sure to take note of the block_signing_key**

IMPORTANT (issue below command using block_signing_key just obtained)
```
get_private_key block_signing_key
```
Compare this result to

```
dump_private_keys
```
You should see 3 pairs of keys. One of the pairs should match your block_signing_key and this is the one you will use in the next step!

Get your witness id
-----------------
```
get_witness username (note the "id" for your config)
```

Modify your witness_node config.ini to include **your** witness id and private key pair.
-------------------------
Comment out the existing private-key before adding yours
```
vim witness_node_data_dir/config.ini

witness-id = "1.6.x"
private-key = ["block_signing_key","private_key_for_your_block_signing_key"]
```

start your witness back up
------------------
```
./programs/witness_node/witness_node
```

If it fails to start, try with these flags (not for permanent use)

```
./programs/witness_node/witness_node --resync --replay
```

Vote for yourself
--------------
```
vote_for_witness your_witness_account your_witness_account true true
```

Ask to be voted in!
--------------

Join @Peerplays Telegram group to find information about the witness group.
http://t.me/@peerplayswitness

You will get logs that look like this:

```
2070264ms th_a       application.cpp:506           handle_block         ] Got block: #87913 time: 2017-05-27T16:34:30 latency: 264 ms from: bhuz-witness  irreversible: 87903 (-10)
```

Assuming you've received votes, you will start producing as a witness at the next maintenance interval (once per hour). You can check your votes with.

```
get_witness your_witness_account
```

systemd
----------------
It's important for your witness to start when your system boots up. The filepaths here assume that you installed your witness into `/home/ubuntu/peerplays`

Create a logfile to hold your stdout/err logging
```bash
sudo touch /var/log/peerplays.log
```

Save this file in your peerplays directory. `vi /home/ubuntu/peerplays/start.sh`
```bash
#!/bin/bash

cd /home/ubuntu/peerplays
./programs/witness_node/witness_node &> /var/log/peerplays.log
```
Make it executable
```bash
chmod 744 /home/ubuntu/peerplays/start.sh
```
Create this file: `sudo vi /etc/systemd/system/peerplays.service`
Note the path for start.sh. Change it to match where your start.sh file is if necessary.
```
[Unit]
Description=Peerplays Witness
After=network.target

[Service]
ExecStart=/home/ubuntu/peerplays/start.sh

[Install]
WantedBy = multi-user.target
```
Enable the service
```bash
sudo systemctl enable peerplays.service
```
Make sure you don't get any errors
```bash
sudo systemctl status peerplays.service
```
Stop your witness if it is currently running from previous steps, then start it with the service.
```bash
sudo systemctl start peerplays.service
```
Check your logfile for entries
```bash
tail -f /var/log/peerplays.log
```

Running specific tests
----------------------

- `tests/chain_tests -t block_tests/name_of_test`
