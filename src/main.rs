use pnet;
use pnet::datalink::{Channel, MacAddr, NetworkInterface};

use pnet::packet::Packet;
use pnet::packet::udp::MutableUdpPacket;
use pnet::packet::ethernet::{EtherTypes, EtherType};
use pnet::packet::ethernet::MutableEthernetPacket;
use pnet::packet::ipv6::{MutableIpv6Packet};
use pnet::packet::ip::IpNextHeaderProtocol;
mod bier;
use bier::MutableBIERPacket;

extern crate libc;
use std::{net::UdpSocket};
use std::net::Ipv6Addr;

fn main() {
    read_udp_and_magic();
}

fn read_udp_and_magic() {
    let udp_socket = UdpSocket::bind("[::1]:8888").unwrap();
    let mut buff = vec![0; 1000];
    udp_socket.recv_from(&mut buff).unwrap();
    println!("Received: {:?}", buff);
    simple_ethernet_socket(&buff[..100], Some(true));
}

fn simple_ethernet_socket(buff: &[u8], encap_ipv6: Option<bool>) {
    println!("Hello, world!");

    let iface_name = "en0";

    let interfaces = pnet::datalink::interfaces();
    let interface = interfaces
        .into_iter()
        .find(|iface| iface.name == iface_name)
        .unwrap();
    let _source_mac = interface.mac.unwrap();

    let _target_mac = pnet::datalink::MacAddr::broadcast();

    let (mut sender, mut receiver) = match pnet::datalink::channel(&interface, Default::default()) {
        Ok(Channel::Ethernet(tx, rx)) => (tx, rx),
        Ok(_) => panic!("Unknown channel type"),
        Err(e) => panic!("Error happened {}", e),
    };

    println!("Deuxieme: {:?}", buff);

    let mut udp_buffer = vec![0; buff.len() + 8];
    println!("{} {}", udp_buffer.len(), buff.len());
    udp_buffer[8..].copy_from_slice(buff);
    let mut udp_packet = MutableUdpPacket::new(&mut udp_buffer).unwrap();
    udp_packet.set_length((buff.len() + 8) as u16);
    udp_packet.set_source(0b11111111);
    udp_packet.set_destination(0b1111111100000000);
    udp_packet.set_checksum(0);

    let mut bier_buffer = vec![0u8; buff.len() + 20 + 8];
    let mut bier_packet = MutableBIERPacket::new(&mut bier_buffer).unwrap();
    bier_packet.set_bift_id(1);
    bier_packet.set_tc(2);
    bier_packet.set_s(0);
    bier_packet.set_ttl(255);
    bier_packet.set_nibble(2);
    bier_packet.set_version(3);
    bier_packet.set_bsl(1);
    bier_packet.set_entropy(0xfffff);
    bier_packet.set_oam(1);
    bier_packet.set_rsv(2);
    // bier_packet.set_dscp(3 << 2);
    bier_packet.set_dscp((3 << 2) + 1);
    bier_packet.set_proto(7);
    bier_packet.set_bfir_id(0b1111111111);
    let bitstring = vec![0b10101010101010101010, 0b111111110000000];
    bier_packet.set_bitstring(&bitstring);

    bier_packet.set_payload(&mut udp_buffer);

    let mut ethernet_buffer = match encap_ipv6 {
        Some(true) => vec![0u8; buff.len() + 20 + 8 + 40 + 14],
        _ => vec![0u8; buff.len() + 20 + 8 + 14],
    };
    let mut ethernet_packet = match encap_ipv6 {
        Some(true) => {
            let mut ipv6_buffer = vec![0u8; buff.len() + 20 + 8 + 40];
            let mut ipv6_packet = MutableIpv6Packet::new(&mut ipv6_buffer).unwrap();

            ipv6_packet.set_version(6);
            ipv6_packet.set_traffic_class(3);
            ipv6_packet.set_flow_label(0);
            ipv6_packet.set_payload_length((buff.len() + 20 + 8) as u16);
            ipv6_packet.set_next_header(IpNextHeaderProtocol::new(253)); // TODO: change with BIERin6 draft
            ipv6_packet.set_hop_limit(255);
            ipv6_packet.set_source("babe:1::5".parse().unwrap());
            ipv6_packet.set_destination("babe:2::5".parse().unwrap());
            ipv6_packet.set_payload(&mut bier_buffer);

            let mut tmp_packet = MutableEthernetPacket::new(&mut ethernet_buffer).unwrap();

            tmp_packet.set_ethertype(EtherTypes::Ipv6);
            tmp_packet.set_payload(&mut ipv6_buffer);
            tmp_packet
        },
        _ => {
            let mut tmp_packet = MutableEthernetPacket::new(&mut ethernet_buffer).unwrap();
            
            tmp_packet.set_ethertype(EtherType::new(0xAB37));
            tmp_packet.set_payload(&mut bier_buffer);
            tmp_packet
        },
    };

    ethernet_packet.set_destination(MacAddr::broadcast());
    ethernet_packet.set_source(interface.mac.unwrap());

    //let payload = vec![255u8; 5];
    //bier_packet.set_payload(&payload);
    println!("Coucou");

    for _ in 0..10 {
        sender.send_to(ethernet_packet.packet(), None)
            .unwrap()
            .unwrap();
    
        println!("Sent a packet!");
    }
}
