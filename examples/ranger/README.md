# Advanced Ranger for Zolertia Remote (rev-b)
This document describes the usage and configuration of the ranger example for Contiki-NG. It is designed to work with Zolertia remote-revb nodes. The purpose of the ranger application is to gather as much information about upstream sub-GHz 802.15.4 transmissions in a location within a certain area as possible in a convenient fashion. In other words, a node placed at a predetermined location broadcasts a number of messages that may or may not be received by a number of fixed anchor nodes. From the received packets, the anchor nodes can infer a number of properties about the transmission channel, such as: RSSI, LQI, packet error rate, etc. In the most automated (and frankly the most developed) configuration of this application, the broadcasting node broadcasts for a certain time interval before requesting the anchor nodes to change the Modulation and Coding Scheme (MCS) used for transmission after which the node itself changes to the same MCS and starts transmitting again. This process repeats itself until all modulation schemes have been configured and the transmission ends.

## Table of Contents
- [Advanced Ranger for Zolertia Remote (rev-b)](#advanced-ranger-for-zolertia-remote-rev-b)
  - [Table of Contents](#table-of-contents)
  - [Getting Started](#getting-started)
  - [Usage](#usage)

## Getting Started
Coming soon

## Usage
Coming soon