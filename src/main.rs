use pnet;
use pnet::datalink::{Channel, MacAddr, NetworkInterface};

use pnet::packet::Packet;
use pnet::packet::udp::MutableUdpPacket;
use pnet::packet::ethernet::{EtherTypes, EtherType};
use pnet::packet::ethernet::MutableEthernetPacket;
mod bier;
use bier::MutableBIERPacket;

fn main() {
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

    let mut udp_buffer = [0xffu8; 20];
    let mut udp_packet = MutableUdpPacket::new(&mut udp_buffer).unwrap();
    udp_packet.set_length(20);
    udp_packet.set_source(0b11111111);
    udp_packet.set_destination(0b1111111100000000);
    udp_packet.set_checksum(0);

    let mut bier_buffer = [0u8; 12 + 20 + 8];
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
    bier_packet.set_dscp(3 << 2);
    bier_packet.set_proto(7);
    bier_packet.set_bfir_id(0b1111111111);
    let bitstring = vec![0b10101010101010101010, 0b111111110000000];
    bier_packet.set_bitstring(&bitstring);

    bier_packet.set_payload(&mut udp_buffer);

    let mut ethernet_buffer = [0u8; 12 + 20 + 8 + 14];
    let mut ethernet_packet = MutableEthernetPacket::new(&mut ethernet_buffer).unwrap();

    ethernet_packet.set_destination(MacAddr::broadcast());
    ethernet_packet.set_source(interface.mac.unwrap());
    ethernet_packet.set_ethertype(EtherType::new(0xAB37));

    //let payload = vec![255u8; 5];
    //bier_packet.set_payload(&payload);
    println!("Coucou");

    ethernet_packet.set_payload(&mut bier_buffer);

    for _ in 0..10 {
        sender.send_to(ethernet_packet.packet(), None)
            .unwrap()
            .unwrap();
    
        println!("Sent a packet!");
    }
}
