# bier-socket-api

Implementation of the Bit Index Explicit Replication (BIER) multicast forwarding mechanism.

## Use bier-rust instead

I wrote an updated version of BIER: [bier-rust](https://github.com/louisna/bier-rust). It proposes an implementation in the Rust programming language, which is both safer and cleaner (in my humble opinion), without loosing performance.

Additionally, this project exposes the BIER processing as a library, independently of the the I/O. This is similar to Cloudflare quiche. The user must handle the I/O and send the payload to the BIER processing.

Finally, this updated implementation provides tests for every part of the BIER processing, as well as for the BIER configuration binary.

In conclusion, I suggest to use [bier-rust](https://github.com/louisna/bier-rust) instead of this project.

## Bit Index Explicit Replication (BIER)

BIER is a recently standardised (by the IETF) multicast forwarding mechanism that solves some scalability issues of IP Multicast. Instead of per-group state to forward a multicast packet, a BIER header contains a *bitstring* where each bit uniquely identifies a router in the network.

The forwarding is executed using the Bit Index Forwarding Table (BIFT), which is populated with the FIB of the router. BIER hence relies on the underlying routing protocol (such as IGPs: OSPF, IS-IS) to forward the packets. This requires constant state, independent of the number of multicast groups in the domain.

## Implementation and BIER API

This work is motivated by the absence of open-source BIER implementation. This repository hence contains our implementation of the aforementionned forwarding mechanism in C, only using the standard Linux socket API.

Additionally, it exposes a socket-like API to use BIER for the application.

## Cite this paper

This work is subject to a submission for a paper at the CoNEXT Student Workshop 2022 (CoNEXT SW'22).

```
@inproceedings{navarre2022experimenting,
  title={Experimenting with bit index explicit replication},
  author={Navarre, Louis and Rybowski, Nicolas and Bonaventure, Olivier},
  booktitle={Proceedings of the 3rd International CoNEXT Student Workshop},
  pages={17--19},
  year={2022}
}
```
