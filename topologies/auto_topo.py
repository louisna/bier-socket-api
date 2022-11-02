from mininet.topo import Topo
from mininet.net import Mininet
from mininet.util import dumpNodeConnections
from mininet.cli import CLI
from mininet.node import OVSController

import sys
import time
import argparse


class MyTopo(Topo):
    def build(self):
        self.a = self.addHost("a", ip="fc00:a::1/64", mac='00:00:00:00:00:01')
        self.b = self.addHost("b", ip="fc00:b::1/64", mac='00:00:00:00:00:02')
        self.c = self.addHost("c", ip="fc00:c::1/64", mac='00:00:00:00:00:03')
        self.d = self.addHost("d", ip="fc00:d::1/64", mac='00:00:00:00:00:04')
        self.e = self.addHost("e", ip="fc00:e::1/64", mac='00:00:00:00:00:05')
        self.addLink(self.a, self.b)
        self.addLink(self.a, self.c)
        self.addLink(self.b, self.d)
        self.addLink(self.c, self.d)
        self.addLink(self.d, self.e)


def parse_mc_groups(filename):
    with open(filename) as fd:
        data = fd.read().strip().split("\n")
        return [i.split(" ")[0] for i in data]


def simpleRun(cli=False, bitstring="1f", bift_id=1, use_valgrind=False, nb_groups=1, nb_packets=10, mc_groups=["ff00:babe:cafe::1"]):
    topo = MyTopo()
    net = Mininet(topo = topo, controller = OVSController)
    net.start()

    dumpNodeConnections(net.hosts)

    # Add default routes to see the packets
    net["a"].cmd("ip -6 addr add fc00:ab::2/64 dev a-eth0")
    net["b"].cmd("ip -6 addr add fc00:ab::3/64 dev b-eth0")

    net["a"].cmd("ip -6 addr add fc00:ac::2/64 dev a-eth1")
    net["c"].cmd("ip -6 addr add fc00:ac::3/64 dev c-eth0")

    net["b"].cmd("ip -6 addr add fc00:bd::3/64 dev b-eth1")
    net["d"].cmd("ip -6 addr add fc00:bd::4/64 dev d-eth0")

    net["c"].cmd("ip -6 addr add fc00:cd::3/64 dev c-eth1")
    net["d"].cmd("ip -6 addr add fc00:cd::4/64 dev d-eth1")

    net["d"].cmd("ip -6 addr add fc00:de::3/64 dev d-eth2")
    net["e"].cmd("ip -6 addr add fc00:de::2/64 dev e-eth0")

    net["b"].cmd("ip -6 route add fc00:a::/64 via fc00:ab::2 dev b-eth0")
    net["c"].cmd("ip -6 route add fc00:a::/64 via fc00:ac::2 dev c-eth0")

    net["a"].cmd("ip -6 route add fc00:b::/64 via fc00:ab::3 dev a-eth0")
    net["a"].cmd("ip -6 route add fc00:c::/64 via fc00:ac::3 dev a-eth1")

    net["b"].cmd("ip -6 route add fc00:d::/64 via fc00:bd::4 dev b-eth1")

    net["c"].cmd("ip -6 route add fc00:d::/64 via fc00:cd::4 dev c-eth1")

    net["d"].cmd("ip -6 route add fc00:e::/64 via fc00:de::2 dev d-eth2")
    net["d"].cmd("ip -6 route add fc00:b::/64 via fc00:bd::3 dev d-eth0")
    net["d"].cmd("ip -6 route add fc00:c::/64 via fc00:cd::3 dev d-eth1")

    net["e"].cmd("ip -6 route add fc00:d::/64 via fc00:de::3 dev e-eth0")

    net["a"].cmd("ip -6 addr add fc00:a::1 dev lo")
    net["b"].cmd("ip -6 addr add fc00:b::1 dev lo")
    net["c"].cmd("ip -6 addr add fc00:c::1 dev lo")
    net["d"].cmd("ip -6 addr add fc00:d::1 dev lo")
    net["e"].cmd("ip -6 addr add fc00:e::1 dev lo")

    net["a"].cmd("ip -6 route add fc00:d::1/64 via fc00:ab::3 dev a-eth0")
    net["a"].cmd("ip -6 route add fc00:e::1/64 via fc00:ab::3 dev a-eth1")

    net["b"].cmd("ip -6 route add fc00:e::1/64 via fc00:bd::4 dev b-eth1")
    net["c"].cmd("ip -6 route add fc00:e::1/64 via fc00:cd::4 dev c-eth1")

    net["b"].cmd("ip -6 route add fc00:c::1/64 via fc00:ab::3 dev b-eth0")
    net["c"].cmd("ip -6 route add fc00:b::1/64 via fc00:ac::3 dev c-eth0")

    net["d"].cmd("ip -6 route add fc00:a::1/64 via fc00:bd::3 dev d-eth0")

    net["e"].cmd("ip -6 route add fc00:a::1/64 via fc00:de::3 dev e-eth1")
    net["e"].cmd("ip -6 route add fc00:b::1/64 via fc00:de::3 dev e-eth1")
    net["e"].cmd("ip -6 route add fc00:c::1/64 via fc00:de::3 dev e-eth1")

    node2id = dict()
    for i, val in enumerate(["a", "b", "c", "d", "e"]):
        node2id[val] = i

    valgrind_str = "valgrind --track-origins=yes --leak-check=full" if use_valgrind else ""

    template_cmd = lambda idx: f"{valgrind_str} ~/bier-socket-api/raw-socket/bier-bfr -c ~/bier-socket-api/ipv6-configs/bier-config-{idx}.txt -b /tmp/socket-bfr-{idx} -a /tmp/socket-app-{idx} -m ~/bier-socket-api/topologies/ecmp/id2ipv6.ntf -g ~/bier-socket-api/topologies/ecmp/mc_groups.ntf > ~/bier-socket-api/logs/{idx}.txt 2>&1 &"
    template_app_cmd = lambda idx, mc_i: f"{valgrind_str} ~/bier-socket-api/raw-socket/receiver -l /tmp/socket-app-{idx}-{mc_i} -g {mc_groups[mc_i]} -b /tmp/socket-bfr-{idx} -n 4 > ~/bier-socket-api/logs/app-{idx}-{mc_i}.txt 2>&1 &"
    
    if not cli:
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
    parser.add_argument()
    args = parser.parse_args()
    simpleRun(args)

