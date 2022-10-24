# bier-socket-api

Implementation of the Bit Index Explicit Replication (BIER) multicast forwarding mechanism.

## Bit Index Explicit Replication (BIER)

BIER is a recently standardised (by the IETF) multicast forwarding mechanism that solves some scalability issues of IP Multicast. Instead of per-group state to forward a multicast packet, a BIER header contains a *bitstring* where each bit uniquely identifies a router in the network.

The forwarding is executed using the Bit Index Forwarding Table (BIFT), which is populated with the FIB of the router. BIER hence relies on the underlying routing protocol (such as IGPs: OSPF, IS-IS) to forward the packets. This requires constant state, independent of the number of multicast groups in the domain.

## Implementation and BIER API

This work is motivated by the absence of open-source BIER implementation. This repository hence contains our implementation of the aforementionned forwarding mechanism in C, only using the standard Linux socket API.

Additionally, it exposes a socket-like API to use BIER for the application.

## Cite this paper

This work is subject to a submission for a paper at the CoNEXT Student Workshop 2022 (CoNEXT SW'22). The paper has been accepted but not published yet. Publication of this paper shall give the citation instructions.
