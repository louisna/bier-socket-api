from mininet.topo import Topo
from mininet.net import Mininet
from mininet.util import dumpNodeConnections
from mininet.cli import CLI
from mininet.node import OVSController

import sys
import time
import argparse
import os
import shutil


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
    logs_dir = args.logs
    if os.path.exists(logs_dir) and os.path.isdir(logs_dir):
        shutil.rmtree(logs_dir)
    os.makedirs(logs_dir, exist_ok=False)
    os.makedirs(os.path.join(logs_dir, "pcaps"), exist_ok=True)
    os.makedirs(os.path.join(logs_dir, "logs"), exist_ok=True)
    topo = MyTopo(**args_dict)
    net = Mininet(topo = topo, controller = OVSController)
    net.start()
    
    mc_groups = parse_mc_groups(f"{args.config}/mc_group_mapping.txt")

    dumpNodeConnections(net.hosts)

    nb_nodes = 0
    loopbacks = dict()

    with open(args_dict["loopbacks"]) as fd:
        txt = fd.read().split("\n")
        for loopback_info in txt:
            if len(loopback_info) == 0: continue
            id1, loopback = loopback_info.split(" ")
            cmd = f"ip -6 addr add {loopback} dev lo"
            loopbacks[id1] = loopback[:-3]
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
            id1, id2, itf, link, loopback = link_info.split(" ")
            cmd = f"sysctl net.ipv6.conf.{id1}-eth{itf}.forwarding=1"
            net[id1].cmd(cmd)
            cmd = f"sysctl net.ipv6.conf.{id1}-eth{itf}.mc_forwarding=1"
            net[id1].cmd(cmd)

            cmd = f"sysctl net.ipv4.{id1}-eth{itf}.ip_forward=1"
            net[id1].cmd(cmd)
            
            # Attempt to get the interface number
            intfs = net[id1].connectionsTo(net[id2])
            intf_name = str(intfs[0][0])
            cmd = f"ip -6 addr add {link} dev {intf_name}"
            print(id1, cmd)
            net[id1].cmd(cmd)

            # Start a tcpdump capture for each interface
            net[id1].cmd(f"tcpdump -i {intf_name} -w {logs_dir}/pcaps/{id1}-{itf}.pcap &")

    with open(args_dict["paths"]) as fd:
        txt = fd.read().split("\n")
        for path_info in txt:
            if len(path_info) == 0: continue
            id1, itf, link, loopback = path_info.split(" ")
            cmd = f"ip -6 route add {loopback} via {link}"
            print(id1, cmd)
            net[id1].cmd(cmd)
    
    print("Setup links")
    time.sleep(5)
    
    # net["0"].cmd("unicast/sender babe:cafe:0::1 babe:cafe:3::1 1000 &")

    if nb_nodes - 1 < args.nb_receivers:
        print(f"NB NODES: {nb_nodes}, but NB RECEIVERS: {args.nb_receivers}")
        return

    if not args.cli:
        if args.multicast:  # Multicast forwarding
            do_multicast(net, args, loopbacks, mc_groups, nb_nodes)
        else:  # Unicast forwarding
            id_recv = -1
            while id_recv < args.nb_receivers:
                id_recv += 1
                if id_recv == args.sender: continue  # Sender is not a receiver
                cmd = f"perf stat -o {logs_dir}/logs/perf-receiver-{str(id_recv)}.txt unicast/receiver {loopbacks[f'{id_recv}']} 100 2> {logs_dir}/logs/receiver-{str(id_recv)}.txt &"
                print(cmd)
                net[f"{id_recv}"].cmd(cmd)
            time.sleep(3)
            id_recv = -1
            while id_recv < args.nb_receivers:
                id_recv += 1
                if id_recv == args.sender: continue
                cmd = f"perf stat -o {logs_dir}/logs/perf-sender-{args.sender}-{str(id_recv)}.txt unicast/sender {loopbacks[f'{args.sender}']} {loopbacks[f'{id_recv}']} 100 2> {logs_dir}/logs/sender-{str(args.sender)}-{str(id_recv)}.txt"
                print(cmd)
                net[f"{args.sender}"].cmd(cmd)
            print("Exiting...")
            time.sleep(2)
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


def do_multicast(net, args, loopbacks, mc_groups, nb_nodes):
    tmp_bfr = lambda idx: f"raw-socket/bier-bfr -c {args.config}/bier-config-{idx}.txt -b /tmp/socket-bfr-{idx} -a /tmp/socket-app-{idx} -m {args.loopbacks} -g {args.config}/mc_group_mapping.txt > {args.logs}/logs/bier-{idx}.txt 2>&1 &"
    tmp_app = lambda idx, mc_i: f"raw-socket/receiver -l /tmp/socket-app-{idx}-{mc_i} -g {mc_groups[mc_i]} -b /tmp/socket-bfr-{idx} -n 100 > {args.logs}/logs/app-{idx}-{mc_i}.txt 2>&1 &"
    tmp_sdr = lambda mc_i: f"raw-socket/sender-mc -d {mc_groups[mc_i]} -l {loopbacks[str(mc_i)]} -b /tmp/socket-bfr-{mc_i} -s /tmp/sender-sock-{mc_i} -n 100 -i 1 -r {args.nb_receivers} > {args.logs}/logs/sender-{mc_i}-{mc_i}.txt 2>&1 &"
    
    # BIER
    for i in range(nb_nodes):
        cmd = f"{tmp_bfr(i)}"
        print(cmd)
        net[str(i)].cmd(cmd)
    
    print("Setup BIER...")
    time.sleep(5)
    
    # Sender
    cmd = f"perf stat -o {args.logs}/logs/perf-sender-{args.sender}.txt {tmp_sdr(args.sender)}"
    print(cmd)
    net[str(args.sender)].cmd(cmd)
    
    time.sleep(5)
    print("Setup sender...")
    
    # Receivers
    nb = -1
    while nb < args.nb_receivers - 1:
        nb += 1
        if nb == args.sender: continue
        cmd = f"{tmp_app(nb, 0)}"
        print(cmd)
        net[str(i)].cmd(cmd)
    nb += 1
    while nb == args.sender:
        nb += 1
        continue
    cmd = f"perf stat -o {args.logs}/logs/perf-mc-receiver-{str(nb)}.txt {tmp_app(nb, 0)[:-1]}"
    print(cmd)
    net[str(i)].cmd(cmd)
    
    print("Exiting...")
    time.sleep(2)
    

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--loopbacks", type=str, default="configs/topo-loopbacks.txt")
    parser.add_argument("--links", type=str, default="configs/topo-links.txt")
    parser.add_argument("--paths", type=str, default="configs/topo-paths.txt")
    parser.add_argument("--config", type=str, help="Path to the config files", default="configs")
    parser.add_argument("--cli", action="store_true")
    parser.add_argument("--sender", type=int, default="0", help="Select the node sending the trafic")
    parser.add_argument("--nb-receivers", type=int, default=1, help="Select the number of receivers")
    parser.add_argument("--multicast", action="store_true", help="Use multicast forwarding instead of unicast")
    parser.add_argument("--logs", type=str, default="logs-1-unicast", help="Repository containing the logs")
    args = parser.parse_args()
    simpleRun(args)

