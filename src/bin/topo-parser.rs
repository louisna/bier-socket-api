use std::collections::BinaryHeap;
use std::collections::HashMap;
use std::env;
use std::fmt::Write as fmtWrite;
use std::fs;
use std::fs::File;
use std::io::Write;
use std::io::{BufRead, BufReader, Lines};

struct Node {
    name: String,
    neighbours: Vec<(usize, i32)>, // (id, metric)
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 3 {
        println!("Usage: {} <topology_path> <output_directory_path>", args[0]);
        return;
    }

    match fs::create_dir_all(&args[2]) {
        Ok(_) => (),
        Err(e) => {
            println!("Could not create the output directory {}: {}", args[2], e);
        }
    }

    let file = File::open(&args[1]).expect("Impossible to open the file");
    let reader = BufReader::new(file);
    let graph = parse_file(reader.lines());
    bier_config_build(&graph, &args[2]).unwrap();
}

fn bier_config_build(graph: &[Node], output_dir: &str) -> std::io::Result<()> {
    let nb_nodes = (*graph).len(); // The * just to test
    for node in 0..nb_nodes {
        let next_hop = dijkstra(graph, node);
        let mut s = String::new();
        for bfr_id in 0..nb_nodes {
            let the_next_hop = next_hop[bfr_id];
            let next_hop_str = &graph[the_next_hop].name;
            let bfm = next_hop.iter().rev().fold(String::new(), |mut fbm, nh| {
                if *nh == the_next_hop {
                    fbm.push('1');
                    fbm
                } else {
                    if !fbm.is_empty() {
                        fbm.push('0');
                    }
                    fbm
                }
            });
            writeln!(s, "{} {} {}", bfr_id + 1, bfm, next_hop_str).unwrap();
        }
        println!("Pour node {}:\n{}", graph[node].name, s);
        // Write the configuration file in the output directory
        let path = std::path::Path::new(output_dir)
            .join(std::path::Path::new(&format!("{}.txt", &graph[node].name)));
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

fn dijkstra(graph: &[Node], start: usize) -> Vec<usize> {
    let mut heap: BinaryHeap<(i32, (usize, usize))> = BinaryHeap::new();
    let nb_nodes = graph.len();
    let mut visited = vec![false; nb_nodes];
    // Must store for each destination where it comes from
    let mut predecessor = vec![nb_nodes + 1; nb_nodes];

    heap.push((0, (start, start)));
    while !heap.is_empty() {
        let (val, (node, from)) = heap.pop().unwrap();
        if visited[node] {
            continue;
        }
        visited[node] = true;
        predecessor[node] = from;

        // Add all neighbours
        for (neigh, metric) in graph[node]
            .neighbours
            .iter()
            .filter(|(neigh, _)| !visited[*neigh])
        {
            heap.push((val - metric, (*neigh, node)));
        }
    }

    // Now we have to build the nexthop map for each possible destination T_T
    let mut next_hop = vec![start; nb_nodes];
    for (node, node_next_hop) in next_hop.iter_mut().enumerate().take(nb_nodes) {
        // For each node, go in reverse order until we find the nexthop
        let mut runner = node;
        while predecessor[runner] != start {
            runner = predecessor[runner];
        }
        *node_next_hop = runner;
    }

    next_hop
}

fn parse_file(lines: Lines<BufReader<File>>) -> Vec<Node> {
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
            let node = Node {
                name: split[0].to_string(),
                neighbours: Vec::new(),
            };
            graph.push(node);
        }

        let b_id: usize = *name_to_id.entry(split[1].to_string()).or_insert(current_id);
        if b_id == current_id {
            current_id += 1;
            let node = Node {
                name: split[1].to_string(),
                neighbours: Vec::new(),
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
