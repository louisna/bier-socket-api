import os
import argparse


NB_REP = 3


def test_unicast(nb_max: int = 10):
    for i in range(nb_max):
        for j in range(NB_REP):
            print(f"REP {i} {j}")
            directory = f"unicast-{i + 1}-{j + 1}"
            os.system(f"python3 topologies/auto_topo.py --logs {directory} --nb-receivers {i + 1}")


def test_multicast(nb_max: int = 10):
    for i in range(nb_max):
        for j in range(NB_REP):
            print(f"REP {i} {j}")
            directory = f"multicast-{i + 1}-{j + 1}"
            os.system(f"python3 topologies/auto_topo.py --logs {directory} --nb-receivers {i + 1} --multicast")


if __name__ == "__main__":
    # test_unicast(15)
    test_multicast(15)
