#!/bin/bash

find ./libraries/app -regex ".*[c|h]pp" | xargs clang-format -i
find ./libraries/chain/hardfork.d -regex ".*hf" | xargs clang-format -i
find ./libraries/plugins/peerplays_sidechain -regex ".*[c|h]pp" | xargs clang-format -i
