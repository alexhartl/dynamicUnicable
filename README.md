
# Dynamic Unicable Support for Vu+

This project aims to extend Vu+ set-top boxes by an Unicable mode that allows them to dynamically allocate user channels when and where they are needed. 

Unicable allows multiple satellite receiver boxes to be simultaneously operated on a single cable. The downside of conventional Unicable is the need for every tuner to be statically assigned a fixed separate user channel. As usually not all tuners are operated at the same time, the Unicable matrix in this case is very poorly utilized and has to yield more user channels than will ever be used simultaneously.

With this extension, Vu+ boxes dynamically allocate user channels on demand using the local network. For this purpose, one always-on box is configured to manage user channels and all other boxes request and are assigned channels when they are needed.

## Installation

As we require modifications in the C++ part of enigma, you have to compile your own image to be able to use this functionality. For this, run the following commands:
```
git clone git://code.vuplus.com/git/openvuplus_3.0.git
git clone https://github.com/alexhartl/dynamicUnicable.git
dynamicUnicable/prepare.sh openvuplus_3.0
cd openvuplus_3.0
make image MACHINE=vusolo2
```
where `vusolo2` might also be `vusolo`, `bm750`, `vuuno` or `vuultimo`, depending on your device. Note that there are some pitfalls when compiling the image like, e.g., missing hbbtv source files, so you should have some experience with this topic.

## Configuration

- Pick one always-on box and configure one tuner of this device as usual, but set "Serve Group" to "Group 1" and "First Channel" to 1. If any of your devices does not support this dynamic Unicable extension, set "First Channel" appropriately higher, so that you can use the lowest channels for static configuration.
  **Do not enable "Serve Group" on more than one device for the same group !**
- On all remaining devices set "Unicable Configuration Mode" to "Dynamic Unicable" and "Group" to "Group 1".
- done.


## The Protocol

The protocol is designed to be simple and robust. It uses UDP on port 5494 and knows 4 different message types: request, response, keepalive and release. To simplify configuration, clients can broadcast their requests and remember the address from where they received a response for subsequent messages. All values are big-endian.

### request Message

| Field      | Length  | Comment                               |
| ---------- | ------- | ------------------------------------- |
| Magic      | 2 bytes | = 0xa7d3                              |
| Type       | 1 byte  | = 1                                   |
| Group      | 1 byte  | identifies different ports of matrix  |
| Slot       | 1 byte  | identifies tuner in one device        |
| LNB Number | 1 byte  |                                       |
| Channel    | 1 byte  | = 0                                   |

### response Message

| Field              | Length  | Comment                                   |
| ------------------ | ------- | ----------------------------------------- |
| Magic              | 2 bytes | = 0xa7d3                                  |
| Type               | 1 byte  | = 2                                       |
| Group              | 1 byte  | echoed from request                       |
| Slot               | 1 byte  | echoed from request                       |
| LNB Number         | 1 byte  | echoed from request                       |
| Channel            | 1 byte  |                                           |
| Format             | 1 byte  | = 0 for EN50494, = 1 for JESS             |
| Frequency          | 2 bytes | in MHz, corresponding to assigned channel |
| LOF Low            | 2 bytes | in MHz                                    |
| LOF High           | 2 bytes | in MHz                                    |
| Threshold          | 2 bytes | in MHz                                    |
| Possible positions | 1 byte  |                                           |
| Keepalive interval | 2 bytes | in seconds                                |

### keepalive Message

| Field      | Length  | Comment  |
| ---------- | ------- | -------- |
| Magic      | 2 bytes | = 0xa7d3 |
| Type       | 1 byte  | = 3      |
| Group      | 1 byte  |          |
| Slot       | 1 byte  |          |
| LNB Number | 1 byte  |          |
| Channel    | 1 byte  |          |

### release Message

| Field      | Length  | Comment  |
| ---------- | ------- | -------- |
| Magic      | 2 bytes | = 0xa7d3 |
| Type       | 1 byte  | = 4      |
| Group      | 1 byte  |          |
| Slot       | 1 byte  |          |
| LNB Number | 1 byte  |          |
| Channel    | 1 byte  |          |

