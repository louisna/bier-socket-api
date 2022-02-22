use std::collections::{BinaryHeap, HashSet, HashMap};
use std::cmp::Ord;
use core::hash::Hash;

pub trait Graph<T: Ord + Hash> {
    fn get_successors(&self, from: &T) -> Vec<(&T, i32)>;
}

pub fn dijkstra<'a, T: Ord + Hash>(graph: &'a dyn Graph<T>, start: &'a T) -> Option<HashMap<&'a T, Vec<&'a T>>> {
    let mut heap: BinaryHeap<(i32, (&T, &T))> = BinaryHeap::new();
    let mut visited: HashSet<&T> = HashSet::new();
    let mut cost_to_reach: HashMap<&T, i32> = HashMap::new();
    let mut predecessors: HashMap<&T, Vec<&T>> = HashMap::new();

    heap.push((0, (&start, &start)));
    while !heap.is_empty() {
        let (cost, (current, from)) = match heap.pop() {
            Some(infos) => infos,
            None => return None,
        };

        if visited.contains(current) {
            // Maybe ECMP?
            match cost_to_reach.get(current) {
                None => continue,
                Some(optimal_cost) => {
                    if *optimal_cost == cost {
                        // This is ECMP!
                        predecessors.entry(current).or_insert(Vec::new()).push(from);
                    }
                }
            }
            // Do not need to expand the node, we already did it
            continue;
        }

        visited.insert(&current);
        predecessors.entry(current).or_insert(Vec::new()).push(from);
        cost_to_reach.insert(current, cost);

        // Add all neighbours
        for (neigh, local_cost) in graph.get_successors(current).iter().filter(|(neigh, _)| !visited.contains(neigh)) {
            heap.push((cost - local_cost, (neigh, current)));
        }
    }
    Some(predecessors)
}

impl Graph<usize> for Vec<Vec<(usize, i32)>> {
    fn get_successors(&self, from: &usize) -> Vec<(&usize, i32)> {
        self.get(*from).unwrap().iter().map(|(node, cost)| (node, *cost as i32)).collect()
    }

}

fn main() {
    let mut v: Vec<Vec<(usize, i32)>> = Vec::new();
    v.push(vec![(1, 1)]);
    v.push(vec![(0, 1)]);
    let start: usize = 0;
    let next_hop = dijkstra(&v, &start).unwrap();
    println!("{:?}", next_hop);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_dijkstra_dummy() {
        let mut v: Vec<Vec<(usize, i32)>> = Vec::new();
        v.push(vec![(1, 1)]);
        v.push(vec![(0, 1)]);
        let start: usize = 0;
        let next_hop = dijkstra(&v, &start);
        assert!(next_hop.is_some());
        let nh_unw = next_hop.unwrap();
        assert!(nh_unw.contains_key(&0));
        assert!(nh_unw.contains_key(&1));
        assert!(nh_unw.get(&0).is_some());
        assert_eq!(nh_unw.get(&0).unwrap().len(), 1);
        assert!(nh_unw.get(&1).is_some());
        assert_eq!(nh_unw.get(&1).unwrap().len(), 1);

        assert_eq!(*nh_unw.get(&0).unwrap()[0], 0);
        assert_eq!(*nh_unw.get(&1).unwrap()[0], 0);
    }

    #[test]
    fn test_dijkstra_medium_topo() {
        let mut v: Vec<Vec<(usize, i32)>> = Vec::with_capacity(5);
        v.push(vec![(1, 1), (2, 1)]);
        v.push(vec![(0, 1), (3, 1)]);
        v.push(vec![(0, 1), (3, 2)]);
        v.push(vec![(1, 1), (2, 2), (4, 1)]);
        v.push(vec![(3, 1)]);

        let start: usize = 1;
        let next_hop = dijkstra(&v, &start);
        assert!(next_hop.is_some());
        let nh_unw = next_hop.unwrap();

        let len_paths: Vec<usize> = vec![1; 5];
        let true_next_hops: Vec<usize> = vec![1, 1, 0, 1, 3];

        for i in 0..5 {
            assert!(nh_unw.contains_key(&i));
            assert!(nh_unw.get(&i).is_some());
            assert_eq!(nh_unw.get(&i).unwrap().len(), len_paths[i]);
            assert_eq!(*nh_unw.get(&i).unwrap()[0], true_next_hops[i]);
        }
    }

    #[test]
    fn test_dijkstra_medium_topo_ecmp() {
        let mut v: Vec<Vec<(usize, i32)>> = Vec::with_capacity(5);
        v.push(vec![(1, 1), (2, 1)]);
        v.push(vec![(0, 1), (3, 1)]);
        v.push(vec![(0, 1), (3, 1)]);
        v.push(vec![(1, 1), (2, 1), (4, 1)]);
        v.push(vec![(3, 1)]);

        let start: usize = 0;
        let next_hop = dijkstra(&v, &start);
        assert!(next_hop.is_some());
        let nh_unw = next_hop.unwrap();

        let len_paths: Vec<usize> = vec![1; 5];
        let true_next_hops: Vec<usize> = vec![0, 0, 0, 0, 3];

        for i in 0..5 {
            assert!(nh_unw.contains_key(&i));
            assert!(nh_unw.get(&i).is_some());
            if i == 3 {
                continue; // We will test 3 later
            }
            assert_eq!(nh_unw.get(&i).unwrap().len(), len_paths[i]);
            assert_eq!(*nh_unw.get(&i).unwrap()[0], true_next_hops[i]);
        }

        // Test 3, we should have ECMP there
        assert_eq!(nh_unw.get(&3).unwrap().len(), 2);
        assert!(nh_unw.get(&3).unwrap().contains(&&1));
        assert!(nh_unw.get(&3).unwrap().contains(&&2));
    }
}