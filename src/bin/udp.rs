use std::{io, mem, net::UdpSocket};

fn main() {
    let udp_socket = UdpSocket::bind("[::1]:8080").unwrap();
    let buff = vec![1; 100];
    udp_socket.send_to(&buff, "[::1]:8888").unwrap();
}