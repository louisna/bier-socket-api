use std::collections::BinaryHeap;
use std::collections::HashMap;
use std::collections::VecDeque;
use std::env;
use std::fmt::Write as fmtWrite;
use std::fs::File;
use std::io::Write;
use std::io::{BufRead, BufReader, Lines};
use bier_rust::dijkstra::{Graph, dijkstra};


#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
struct Node {
    _id: u32,
    name: String,
    ipv6_addr_str: String,
    neighbours: Vec<(usize, i32)>, // (id, metric)
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 5 {
        println!(
            "Usage: {} <topology_path> <node_to_id_path> <id_to_ipv6_path> <output_directory_path>",
            args[0]
        );
        return;
    }

    let node_to_id_file = File::open(&args[2]).expect("Impossible to open the node id mapping");
    let node_to_id = parse_node_to_id(node_to_id_file);

    let id_to_address_file = File::open(&args[3]).expect("Impossible to open the id ipv6 mapping");
    let id_to_address = parse_id_to_ipv6(id_to_address_file);
    println!("JE NE COMPRENDS PAS {:?}", id_to_address);

    let file = File::open(&args[1]).expect("Impossible to open the file");
    let reader = BufReader::new(file);
    let graph = parse_file(reader.lines(), node_to_id, id_to_address);
    bier_config_build(&graph, &args[4]).unwrap();
}

fn parse_node_to_id(node_to_id_file: File) -> HashMap<String, u32> {
    let mut map: HashMap<String, u32> = HashMap::new();
    let reader = BufReader::new(node_to_id_file);

    for line_unw in reader.lines() {
        let line = line_unw.unwrap();
        let split: Vec<&str> = line.split(' ').collect();
        let name = split[1];
        let id: u32 = split[0].parse::<u32>().unwrap();
        map.insert(name.to_string(), id);
    }

    map
}

fn parse_id_to_ipv6(id_to_ipv6_file: File) -> HashMap<u32, String> {
    let mut map: HashMap<u32, String> = HashMap::new();
    let reader = BufReader::new(id_to_ipv6_file);

    for line_unw in reader.lines() {
        let line = line_unw.unwrap();
        let split: Vec<&str> = line.split(' ').collect();
        let id: u32 = split[0].parse::<u32>().unwrap();
        let address = split[1].split('/').collect::<Vec<&str>>()[0];
        map.insert(id, address.to_string());
    }

    map
}

fn graph_node_to_usize(graph: &[Node]) -> Vec<Vec<(usize, i32)>> {
    graph.iter().map(|node| node.neighbours.to_owned()).collect()
}

fn get_all_out_interfaces_to_destination(predecessors: &HashMap<&usize, Vec<&usize>>, source: usize, destination: usize) -> Vec<usize> {
    if source == destination {
        return vec![source];
    }
    
    let mut out: Vec<usize> = Vec::new();
    let mut visited = vec![false; predecessors.len()];
    let mut stack = VecDeque::new();
    stack.push_back(destination);
    println!("From {} to {}", source, destination);
    while !stack.is_empty() {
        let elem = stack.pop_back().unwrap();
        println!("Visit {}", elem);
        if visited[elem] {
            continue;
        }
        visited[elem] = true;
        for &&pred in predecessors.get(&elem).unwrap() {
            println!("    pred is {}", pred);
            if pred == source {
                out.push(elem);
                continue;
            }
            if visited[pred] {
                continue;
            }
            stack.push_back(pred);
        }
    }
    out
}

fn bier_config_build(graph: &[Node], output_dir: &str) -> std::io::Result<()> {
    let nb_nodes = (*graph).len(); // The * just to test
    let graph_id = graph_node_to_usize(graph);
    for node in 0..nb_nodes {
        // Predecessor(s) for each node, alongside the shortest path(s) from `node`
        let predecessors = dijkstra(&graph_id, &node).unwrap();

        // Construct the next hop mapping, possibly there are multiple paths so multiple output interfaces
        let next_hop: Vec<Vec<usize>> = (0..nb_nodes).map(|i| get_all_out_interfaces_to_destination(&predecessors, node, i)).collect();

        let mut s = String::new();

        // Write name of the node and total number of nodes
        writeln!(
            s,
            "{}\n{}\n{}",
            &graph[node].ipv6_addr_str,
            nb_nodes,
            &graph[node]._id + 1
        )
        .unwrap();
        for bfr_id in 0..nb_nodes {
            let mut hops_vec = Vec::new();
            for &the_next_hop in &next_hop[bfr_id] {
                let next_hop_str = &graph[the_next_hop].ipv6_addr_str;
                let bfm = next_hop.iter().rev().fold(String::new(), |mut fbm, nh| {
                    if nh.contains(&the_next_hop) {
                        fbm.push('1');
                        fbm
                    } else {
                        if !fbm.is_empty() {
                            fbm.push('0');
                        }
                        fbm
                    }
                });
                hops_vec.push((bfm, next_hop_str));
            }
            let st = hops_vec.iter().fold(String::new(), |s, (bfm, nxthop)| s + " " + &format!("{} {}", bfm, nxthop));
            writeln!(s, "{} {}", bfr_id + 1, st).unwrap();
        }
        println!("Pour node {}:\n{}", graph[node].name, s);
        println!("L'id du node {}", graph[node]._id);
        //let path = std::path::Path::new(&format!("files-{}", graph[node]._id))
        //    .join(std::path::Path::new(&format!("bier-config-{}.txt", graph[node]._id)));
        let pathname = format!("bier-config-{}.txt", graph[node]._id);
        let path = std::path::Path::new(&pathname);
        println!("Le path est {:?}", path);
        let mut file = match File::create(&path) {
            Ok(f) => f,
            Err(e) => {
                println!("Impossible to create the file {:?}: {}", path.to_str(), e);
                panic!();
            }
        };
        file.write_all(s.as_bytes())?;
    }
    Ok(())
}

fn parse_file(
    lines: Lines<BufReader<File>>,
    node_to_id: HashMap<String, u32>,
    id_to_address: HashMap<u32, String>,
) -> Vec<Node> {
    let mut graph = Vec::new();
    let mut name_to_id: HashMap<String, usize> = HashMap::new();
    let mut current_id: usize = 0;

    // TODO
    for line_unw in lines {
        let line = line_unw.unwrap();
        let split: Vec<&str> = line.split(' ').collect();
        let a_id: usize = *name_to_id.entry(split[0].to_string()).or_insert(current_id);
        if a_id == current_id {
            current_id += 1;
            let id = node_to_id[&split[0].to_string()];
            let node = Node {
                name: split[0].to_string(),
                neighbours: Vec::new(),
                _id: id,
                ipv6_addr_str: id_to_address[&id].to_string(),
            };
            graph.push(node);
        }

        let b_id: usize = *name_to_id.entry(split[1].to_string()).or_insert(current_id);
        if b_id == current_id {
            current_id += 1;
            let id = node_to_id[&split[1].to_string()];
            let node = Node {
                name: split[1].to_string(),
                neighbours: Vec::new(),
                _id: id,
                ipv6_addr_str: id_to_address[&id].to_string(),
            };
            graph.push(node);
        }

        // Get the metric from the line
        let metric: i32 = split[2].parse::<i32>().unwrap();

        // Add in neighbours adjacency list
        graph[a_id].neighbours.push((b_id, metric));
        graph[b_id].neighbours.push((a_id, metric));
    }

    graph
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_get_all_out_interfaces_to_destination_fixed() {
        // Create a dummy graph and provide the predecessors list
        // It tests ECMP
        //    0
        //   / \
        //  1   2
        //   \ /
        //    3
        //    |
        //    4
        // Expect to get two output interfaces from 0 to 3
        let mut predecessors: HashMap<&usize, Vec<&usize>> = HashMap::new();
        predecessors.insert(&0, vec![&0]);
        predecessors.insert(&1, vec![&0]);
        predecessors.insert(&2, vec![&0]);
        predecessors.insert(&3, vec![&1, &2]);
        predecessors.insert(&4, vec![&3]);
        
        // From 0 to 1
        let mut output_interfaces = get_all_out_interfaces_to_destination(&predecessors, 0, 1);
        assert_eq!(output_interfaces.len(), 1);
        assert!(output_interfaces.contains(&1));
        
        // From 0 to 2
        output_interfaces = get_all_out_interfaces_to_destination(&predecessors, 0, 2);
        assert_eq!(output_interfaces.len(), 1);
        assert!(output_interfaces.contains(&2));
        
        // From 0 to 3
        output_interfaces = get_all_out_interfaces_to_destination(&predecessors, 0, 3);
        assert_eq!(output_interfaces.len(), 2);
        assert!(output_interfaces.contains(&1));
        assert!(output_interfaces.contains(&2));
        
        // From 0 to 4
        output_interfaces = get_all_out_interfaces_to_destination(&predecessors, 0, 4);
        assert_eq!(output_interfaces.len(), 2);
        assert!(output_interfaces.contains(&1));
        assert!(output_interfaces.contains(&2));
    }
}