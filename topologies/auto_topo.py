from mininet.topo import Topo
from mininet.net import Mininet
from mininet.util import dumpNodeConnections
from mininet.cli import CLI
from mininet.node import OVSController

import sys
import time
import argparse


class MyTopo(Topo):
    def build(self, **kwargs):
        with open(kwargs["loopbacks"]) as fd:
            txt = fd.read().split("\n")
        all_nodes = dict()
        for node_info in txt:
            if len(node_info) == 0: continue
            node_id, loopback = node_info.split(" ")
            node = self.addHost(node_id, ip=loopback, mac=f"00:00:00:00:00:{node_id}")
            all_nodes[node_id] = node

        all_links = set()
        with open(kwargs["links"]) as fd:
            txt = fd.read().split("\n")
        for link_info in txt:
            if len(link_info) == 0: continue
            id1, id2, _, _, _ = link_info.split(" ")
            if (id1, id2) in all_links or (id2, id1) in all_links: continue
            self.addLink(all_nodes[id1], all_nodes[id2])
            all_links.add((id1, id2))


def parse_mc_groups(filename):
    with open(filename) as fd:
        data = fd.read().strip().split("\n")
        return [i.split(" ")[0] for i in data]


def simpleRun(args):
    args_dict = {
        "loopbacks": args.loopbacks,
        "links": args.links,
        "paths": args.paths
    }
    topo = MyTopo(**args_dict)
    net = Mininet(topo = topo, controller = OVSController)
    net.start()

    dumpNodeConnections(net.hosts)

    nb_nodes = 0
    loopbacks = dict()

    with open(args_dict["loopbacks"]) as fd:
        txt = fd.read().split("\n")
        for loopback_info in txt:
            if len(loopback_info) == 0: continue
            id1, loopback = loopback_info.split(" ")
            cmd = f"ip -6 addr add {loopback} dev lo"
            loopbacks[id1] = loopback
            print(cmd)
            net[id1].cmd(cmd)
            # Enable IPv6 forwarding
            cmd = "sysctl net.ipv4.ip_forward=1"
            net[id1].cmd(cmd)
            cmd = "sysctl net.ipv6.conf.all.forwarding=1"
            net[id1].cmd(cmd)
            cmd = "sysctl net.ipv6.conf.all.mc_forwarding=1"
            net[id1].cmd(cmd)
            cmd = "sysctl net.ipv6.conf.lo.forwarding=1"
            net[id1].cmd(cmd)
            cmd = "sysctl net.ipv6.conf.lo.mc_forwarding=1"
            net[id1].cmd(cmd)
            nb_nodes += 1

    with open(args_dict["links"]) as fd:
        txt = fd.read().split("\n")
        for link_info in txt:
            if len(link_info) == 0: continue
            id1, _, itf, link, loopback = link_info.split(" ")
            cmd = f"sysctl net.ipv6.conf.{id1}-eth{itf}.forwarding=1"
            net[id1].cmd(cmd)
            cmd = f"sysctl net.ipv6.conf.{id1}-eth{itf}.mc_forwarding=1"
            net[id1].cmd(cmd)

            cmd = f"sysctl net.ipv4.{id1}-eth{itf}.ip_forward=1"
            net[id1].cmd(cmd)
            cmd = f"ip -6 addr add {link} dev {id1}-eth{itf}"
            print(id1, cmd)
            net[id1].cmd(cmd)

            # Start a tcpdump capture for each interface
            # net[id1].cmd(f"tcpdump -i {id1}-eth{itf} -w pcaps/{id1}-{itf}.pcap &")

    with open(args_dict["paths"]) as fd:
        txt = fd.read().split("\n")
        for path_info in txt:
            if len(path_info) == 0: continue
            id1, itf, link, loopback = path_info.split(" ")
            cmd = f"ip -6 route add {loopback} via {link}"
            print(id1, cmd)
            net[id1].cmd(cmd)
    

    if nb_nodes - 1 < args.nb_receivers:
        print(f"NB NODES: {nb_nodes}, but NB RECEIVERS: {args.nb_receivers}")
        return

    if not args.cli:
        if args.multicast:  # Multicast forwarding
            pass
        else:  # Unicast forwarding
            id_recv = -1
            while id_recv < args.nb_receivers:
                id_recv += 1
                if id_recv == args.sender: continue  # Sender is not a receiver
                net[f"{id_recv}"].cmd(f"perf stat unicast/receiver {loopbacks[f'{id_recv}']} 100 > logs/receiver-{str(id_recv)}.txt 2>&1 &")
            time.sleep(3)
            id_recv = -1
            while id_recv < args.nb_receivers:
                id_recv += 1
                if id_recv == args.sender: continue
                net[f"{args.sender}"].cmd(f"perf stat unicast/sender {loopbacks[f'{args.sender}']} {loopbacks[f'{id_recv}']} 100 > logs/sender-{str(args.sender)}-{str(id_recv)}.txt 2>&1")
    else:
        CLI(net)
                
    
    # node2id = dict()
    # for i, val in enumerate(["a", "b", "c", "d", "e"]):
    #     node2id[val] = i

    # valgrind_str = "valgrind --track-origins=yes --leak-check=full" if use_valgrind else ""

    # template_cmd = lambda idx: f"{valgrind_str} ~/bier-socket-api/raw-socket/bier-bfr -c ~/bier-socket-api/ipv6-configs/bier-config-{idx}.txt -b /tmp/socket-bfr-{idx} -a /tmp/socket-app-{idx} -m ~/bier-socket-api/topologies/ecmp/id2ipv6.ntf -g ~/bier-socket-api/topologies/ecmp/mc_groups.ntf > ~/bier-socket-api/logs/{idx}.txt 2>&1 &"
    # template_app_cmd = lambda idx, mc_i: f"{valgrind_str} ~/bier-socket-api/raw-socket/receiver -l /tmp/socket-app-{idx}-{mc_i} -g {mc_groups[mc_i]} -b /tmp/socket-bfr-{idx} -n 4 > ~/bier-socket-api/logs/app-{idx}-{mc_i}.txt 2>&1 &"
    
    # if not args.cli:
    #     for i, val in enumerate(["a", "b", "c", "d", "e"]):
    #         print(template_cmd(i))
    #         net[val].cmd(template_cmd(i))
        
    #     time.sleep(2)

    #     template_wireshark = lambda node, idx: f"tcpdump -i {node}-eth{idx} -w ~/bier-socket-api/pcaps/{node}-{idx}.pcap &"
    #     nodes_interfaces = {"a": 2, "b": 2, "c": 2, "d": 3, "e": 2}
    #     for node in nodes_interfaces:
    #         for itf in range(nodes_interfaces[node]):
    #             net[node].cmd(template_wireshark(node, itf))

    #     time.sleep(1)

    #     nodes = ["a", "b", "c", "d", "e"]

    #     for mc_i in range(nb_groups):
    #         # net["a"].cmd(f"{valgrind_str} ~/bier-socket-api/raw-socket/sender /tmp/socket-bfr-0 {nb_packets} {bitstring} {bift_id} {mc_groups[mc_i]} fc00:a::1 /tmp/sender-sock-0 > ~/bier-socket-api/logs/sender-0-{mc_i}.txt 2>&1")
    #         cmd = f"{valgrind_str} ~/bier-socket-api/raw-socket/sender-mc -d {mc_groups[mc_i]} -l fc00:{nodes[mc_i]}::1 -b /tmp/socket-bfr-{mc_i} -s /tmp/sender-sock-{mc_i} -n 10 -i 1 -v > ~/bier-socket-api/logs/sender-{mc_i}-{mc_i}.txt 2>&1 &"
    #         print(cmd)
    #         net[nodes[mc_i]].cmd(cmd)
        
    #     time.sleep(1)
        
    #     for i, val in enumerate(nodes):
    #         if i == 0: continue
    #         for mc_i in range(nb_groups):
    #             net[val].cmd(template_app_cmd(i, mc_i))
        
    #     print("Launch BIER sending for 5 seconds...")
    #     time.sleep(20)
    #     print("Done")
    # else:
    #     CLI(net)
    net.stop()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--loopbacks", type=str, default="configs/topo-loopbacks.txt")
    parser.add_argument("--links", type=str, default="configs/topo-links.txt")
    parser.add_argument("--paths", type=str, default="configs/topo-paths.txt")
    parser.add_argument("--cli", action="store_true")
    parser.add_argument("--sender", type=int, default="0", help="Select the node sending the trafic")
    parser.add_argument("--nb-receivers", type=int, default=1, help="Select the number of receivers")
    parser.add_argument("--multicast", action="store_true", help="Use multicast forwarding instead of unicast")
    args = parser.parse_args()
    simpleRun(args)

