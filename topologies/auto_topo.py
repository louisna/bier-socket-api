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
            print(node_info)
            node_id, loopback = node_info.split(" ")
            print("MA LOOPBACK", loopback)
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
            
        # self.a = self.addHost("a", ip="fc00:a::1/64", mac='00:00:00:00:00:01')
        # self.b = self.addHost("b", ip="fc00:b::1/64", mac='00:00:00:00:00:02')
        # self.c = self.addHost("c", ip="fc00:c::1/64", mac='00:00:00:00:00:03')
        # self.d = self.addHost("d", ip="fc00:d::1/64", mac='00:00:00:00:00:04')
        # self.e = self.addHost("e", ip="fc00:e::1/64", mac='00:00:00:00:00:05')
        # self.addLink(self.a, self.b)
        # self.addLink(self.a, self.c)
        # self.addLink(self.b, self.d)
        # self.addLink(self.c, self.d)
        # self.addLink(self.d, self.e)


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

    with open(args_dict["loopbacks"]) as fd:
        txt = fd.read().split("\n")
        for loopback_info in txt:
            if len(loopback_info) == 0: continue
            id1, loopback = loopback_info.split(" ")
            cmd = f"ip -6 addr add {loopback} dev lo"
            print(cmd)
            net[id1].cmd(cmd)
            # Enable IPv6 forwarding
            # cmd = "sysctl net.ipv4.ip_forward=1"
            # net[id1].cmd(cmd)
            cmd = "sysctl net.ipv6.conf.all.forwarding=1"
            net[id1].cmd(cmd)
            # cmd = "sysctl net.ipv6.conf.all.mc_forwarding=1"
            # net[id1].cmd(cmd)
            # cmd = "sysctl net.ipv6.conf.lo.forwarding=1"
            # net[id1].cmd(cmd)
            # cmd = "sysctl net.ipv6.conf.lo.mc_forwarding=1"
            net[id1].cmd(cmd)

    with open(args_dict["links"]) as fd:
        txt = fd.read().split("\n")
        for link_info in txt:
            if len(link_info) == 0: continue
            id1, _, itf, link, loopback = link_info.split(" ")
            cmd = f"sysctl net.ipv6.conf.{id1}-eth{itf}.forwarding=1"
            net[id1].cmd(cmd)
            # cmd = f"sysctl net.ipv6.conf.{id1}-eth{itf}.mc_forwarding=1"
            # net[id1].cmd(cmd)

            # cmd = f"sysctl net.ipv4.{id1}-eth{itf}.ip_forward=1"
            # net[id1].cmd(cmd)
            cmd = f"ip -6 addr add {link} dev {id1}-eth{itf}"
            print(id1, cmd)
            net[id1].cmd(cmd)

    with open(args_dict["paths"]) as fd:
        txt = fd.read().split("\n")
        for path_info in txt:
            if len(path_info) == 0: continue
            id1, itf, link, loopback = path_info.split(" ")
            cmd = f"ip -6 route add {loopback} via {link}"
            print(id1, cmd)
            net[id1].cmd(cmd)
    
    # net["1"].cmd("tcpdump -i 1-eth0 -w trace-1-1.pcap &")
    net["0"].cmd("ping babe:cafe:3::1 &")

    # # Add default routes to see the packets
    # net["a"].cmd("ip -6 addr add fc00:ab::2/64 dev a-eth0")
    # net["b"].cmd("ip -6 addr add fc00:ab::3/64 dev b-eth0")

    # net["a"].cmd("ip -6 addr add fc00:ac::2/64 dev a-eth1")
    # net["c"].cmd("ip -6 addr add fc00:ac::3/64 dev c-eth0")

    # net["b"].cmd("ip -6 addr add fc00:bd::3/64 dev b-eth1")
    # net["d"].cmd("ip -6 addr add fc00:bd::4/64 dev d-eth0")

    # net["c"].cmd("ip -6 addr add fc00:cd::3/64 dev c-eth1")
    # net["d"].cmd("ip -6 addr add fc00:cd::4/64 dev d-eth1")

    # net["d"].cmd("ip -6 addr add fc00:de::3/64 dev d-eth2")
    # net["e"].cmd("ip -6 addr add fc00:de::2/64 dev e-eth0")

    # net["b"].cmd("ip -6 route add fc00:a::/64 via fc00:ab::2 dev b-eth0")
    # net["c"].cmd("ip -6 route add fc00:a::/64 via fc00:ac::2 dev c-eth0")

    # net["a"].cmd("ip -6 route add fc00:b::/64 via fc00:ab::3 dev a-eth0")
    # net["a"].cmd("ip -6 route add fc00:c::/64 via fc00:ac::3 dev a-eth1")

    # net["b"].cmd("ip -6 route add fc00:d::/64 via fc00:bd::4 dev b-eth1")

    # net["c"].cmd("ip -6 route add fc00:d::/64 via fc00:cd::4 dev c-eth1")

    # net["d"].cmd("ip -6 route add fc00:e::/64 via fc00:de::2 dev d-eth2")
    # net["d"].cmd("ip -6 route add fc00:b::/64 via fc00:bd::3 dev d-eth0")
    # net["d"].cmd("ip -6 route add fc00:c::/64 via fc00:cd::3 dev d-eth1")

    # net["e"].cmd("ip -6 route add fc00:d::/64 via fc00:de::3 dev e-eth0")

    # net["a"].cmd("ip -6 addr add fc00:a::1 dev lo")
    # net["b"].cmd("ip -6 addr add fc00:b::1 dev lo")
    # net["c"].cmd("ip -6 addr add fc00:c::1 dev lo")
    # net["d"].cmd("ip -6 addr add fc00:d::1 dev lo")
    # net["e"].cmd("ip -6 addr add fc00:e::1 dev lo")

    # net["a"].cmd("ip -6 route add fc00:d::1/64 via fc00:ab::3 dev a-eth0")
    # net["a"].cmd("ip -6 route add fc00:e::1/64 via fc00:ab::3 dev a-eth1")

    # net["b"].cmd("ip -6 route add fc00:e::1/64 via fc00:bd::4 dev b-eth1")
    # net["c"].cmd("ip -6 route add fc00:e::1/64 via fc00:cd::4 dev c-eth1")

    # net["b"].cmd("ip -6 route add fc00:c::1/64 via fc00:ab::3 dev b-eth0")
    # net["c"].cmd("ip -6 route add fc00:b::1/64 via fc00:ac::3 dev c-eth0")

    # net["d"].cmd("ip -6 route add fc00:a::1/64 via fc00:bd::3 dev d-eth0")

    # net["e"].cmd("ip -6 route add fc00:a::1/64 via fc00:de::3 dev e-eth1")
    # net["e"].cmd("ip -6 route add fc00:b::1/64 via fc00:de::3 dev e-eth1")
    # net["e"].cmd("ip -6 route add fc00:c::1/64 via fc00:de::3 dev e-eth1")

    # node2id = dict()
    # for i, val in enumerate(["a", "b", "c", "d", "e"]):
    #     node2id[val] = i

    # valgrind_str = "valgrind --track-origins=yes --leak-check=full" if use_valgrind else ""

    # template_cmd = lambda idx: f"{valgrind_str} ~/bier-socket-api/raw-socket/bier-bfr -c ~/bier-socket-api/ipv6-configs/bier-config-{idx}.txt -b /tmp/socket-bfr-{idx} -a /tmp/socket-app-{idx} -m ~/bier-socket-api/topologies/ecmp/id2ipv6.ntf -g ~/bier-socket-api/topologies/ecmp/mc_groups.ntf > ~/bier-socket-api/logs/{idx}.txt 2>&1 &"
    # template_app_cmd = lambda idx, mc_i: f"{valgrind_str} ~/bier-socket-api/raw-socket/receiver -l /tmp/socket-app-{idx}-{mc_i} -g {mc_groups[mc_i]} -b /tmp/socket-bfr-{idx} -n 4 > ~/bier-socket-api/logs/app-{idx}-{mc_i}.txt 2>&1 &"
    
    if not args.cli:
        for i, val in enumerate(["a", "b", "c", "d", "e"]):
            print(template_cmd(i))
            net[val].cmd(template_cmd(i))
        
        time.sleep(2)

        template_wireshark = lambda node, idx: f"tcpdump -i {node}-eth{idx} -w ~/bier-socket-api/pcaps/{node}-{idx}.pcap &"
        nodes_interfaces = {"a": 2, "b": 2, "c": 2, "d": 3, "e": 2}
        for node in nodes_interfaces:
            for itf in range(nodes_interfaces[node]):
                net[node].cmd(template_wireshark(node, itf))

        time.sleep(1)

        nodes = ["a", "b", "c", "d", "e"]

        for mc_i in range(nb_groups):
            # net["a"].cmd(f"{valgrind_str} ~/bier-socket-api/raw-socket/sender /tmp/socket-bfr-0 {nb_packets} {bitstring} {bift_id} {mc_groups[mc_i]} fc00:a::1 /tmp/sender-sock-0 > ~/bier-socket-api/logs/sender-0-{mc_i}.txt 2>&1")
            cmd = f"{valgrind_str} ~/bier-socket-api/raw-socket/sender-mc -d {mc_groups[mc_i]} -l fc00:{nodes[mc_i]}::1 -b /tmp/socket-bfr-{mc_i} -s /tmp/sender-sock-{mc_i} -n 10 -i 1 -v > ~/bier-socket-api/logs/sender-{mc_i}-{mc_i}.txt 2>&1 &"
            print(cmd)
            net[nodes[mc_i]].cmd(cmd)
        
        time.sleep(1)
        
        for i, val in enumerate(nodes):
            if i == 0: continue
            for mc_i in range(nb_groups):
                net[val].cmd(template_app_cmd(i, mc_i))
        
        print("Launch BIER sending for 5 seconds...")
        time.sleep(20)
        print("Done")
    else:
        CLI(net)
    net.stop()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--loopbacks", type=str, default="configs/topo-loopbacks.txt")
    parser.add_argument("--links", type=str, default="configs/topo-links.txt")
    parser.add_argument("--paths", type=str, default="configs/topo-paths.txt")
    parser.add_argument("--cli", action="store_true")
    args = parser.parse_args()
    simpleRun(args)

