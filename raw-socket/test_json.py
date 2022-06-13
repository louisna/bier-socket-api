import json
import unittest
import os


def get_nb_packets(filepath):
    with open(filepath) as fd:
        data = json.load(fd)
    return len(data)


def cmp_received_packets(filepath, theoric):
    with open(filepath) as fd:
        data = json.load(fd)
    return len(data) == theoric


def parse_mapping_file(filepath):
    with open(filepath) as fd:
        data = fd.readlines()
    nb_nodes, nb_links = data[0].split()
    nb_nodes, nb_links = int(nb_nodes), int(nb_links)
    node2id = dict()
    link2id = dict()
    id2item = [None] * (nb_nodes + nb_links)
    node2intf = dict()
    node_current = [list() for _ in range(nb_nodes)]
    for i in range(nb_nodes):
        tab = data[1 + i].split()
        _id, name = int(tab[0]), tab[1]
        node2id[name] = _id
        id2item[_id] = name
    for i in range(nb_links):
        tab = data[1 + nb_nodes + i].split()
        id1, id2, _id = int(tab[0]), int(tab[1]), int(tab[2])
        link2id[(id1, id2)] = _id
        link2id[(id2, id1)] = _id
        id2item[_id] = (id1, id2)
        node_current[id1].append((_id, id2))
        node_current[id2].append((_id, id1))
    
    # Now sort the links by node to get the interface
    node_current = [sorted(i) for i in node_current]
    for id_node, node_links in enumerate(node_current):
        for intf, (id, id_peer) in enumerate(node_links):
            node2intf[(id_node, id_peer)] = intf

    return node2id, link2id, id2item, node2intf


def link_to_pcap_json(id2item, node2intf, bit_position):
    # The goal is to find the pcap file corresponding to the bit position of the bitstring
    # We do not look for the nodes directly, but instead if the routers correctly forward
    # the packets to the correct next-hop routers
    link = id2item[bit_position]
    assert type(link) == tuple
    a, b = link

    name_a = id2item[a]
    name_b = id2item[b]

    intf_a = node2intf[(a, b)]
    intf_b = node2intf[(b, a)]

    json_a = f"{name_a}-{intf_a}.json"
    json_b = f"{name_b}-{intf_b}.json"

    return json_a, json_b


class TestPcapLength(unittest.TestCase):
    def test(self):
        directory = "../../pcaps/json"
        mapping = {"a": 2, "b": 2, "c": 2, "d": 3, "e": 1}
        theoric_nb = get_nb_packets(os.path.join(directory, "a-0.json"))
        for node in mapping:
            for intf in range(mapping[node]):
                filepath = os.path.join(directory, f"{node}-{intf}.json")
                if node == "d" and intf == 1 or node == "c" and intf == 1:
                    self.assertTrue(cmp_received_packets(filepath, 0))
                else:
                    self.assertTrue(cmp_received_packets(filepath, theoric_nb))
    
    def test_bitstring(self):
        directory = "../../pcaps/json"
        node2id, link2id, id2item, node2intf = parse_mapping_file("../mapping-link-to-bp.txt")

        bitstring = 0x2ff
        idx = 0
        nb_theoric = 0
        while bitstring >> idx > 0:
            # Node bp
            if idx < len(node2id): 
                idx += 1
                continue
            json_a, json_b = link_to_pcap_json(id2item, node2intf, idx)
            filepath_a = os.path.join(directory, json_a)
            filepath_b = os.path.join(directory, json_b)
            if nb_theoric == 0:
                nb_theoric = get_nb_packets(filepath_a)
            
            print(filepath_a, filepath_b)
            # Bit not set => should not receive traffic
            if bitstring & (1 << idx) == 0:
                self.assertEqual(get_nb_packets(filepath_a), 0)
                self.assertEqual(get_nb_packets(filepath_b), 0)
            else:
                self.assertEqual(get_nb_packets(filepath_a), nb_theoric)
                self.assertEqual(get_nb_packets(filepath_b), nb_theoric)
            idx += 1


if __name__ == "__main__":
    unittest.main()