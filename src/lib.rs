pub mod bier;
use bier::{bier::Bier, bier::MutableBierPacket};

use pnet::datalink::{Channel, MacAddr};
use pnet::packet::ethernet::MutableEthernetPacket;
use pnet::packet::ethernet::{EtherType, EtherTypes, Ethernet};
use pnet::packet::ip::IpNextHeaderProtocol;
use pnet::packet::ipv6::{MutableIpv6Packet, Ipv6};
use pnet::packet::udp::{MutableUdpPacket, Udp};
use pnet::packet::Packet;

pub fn create_bier_packet_encap_ether<'a>(
    ethernet: Ethernet,
    ipv6_opt: Option<Ipv6>,
    bier: Bier,
    ipv6: Ipv6, // BIER-Encapsulated packet
    udp: Udp,
    payload: &[u8],
) -> Vec<u8> {
    let mut udp_buffer = vec![0; payload.len() + 8];
    udp_buffer[8..].copy_from_slice(payload);

    let mut udp_packet = MutableUdpPacket::new(&mut udp_buffer).unwrap();
    udp_packet.set_length((payload.len() + 8) as u16);
    udp_packet.set_source(udp.source);
    udp_packet.set_destination(udp.destination);
    udp_packet.set_checksum(udp.checksum); // HOW?

    let mut ipv6_buffer = vec![0; payload.len() + 8 + 40];
    let mut ipv6_packet = MutableIpv6Packet::new(&mut ipv6_buffer).unwrap();

    ipv6_packet.set_version(ipv6.version);
    ipv6_packet.set_traffic_class(ipv6.traffic_class);
    ipv6_packet.set_flow_label(ipv6.flow_label);
    ipv6_packet.set_payload_length((payload.len() + 8) as u16);
    ipv6_packet.set_next_header(IpNextHeaderProtocol::new(17)); // TODO: change with Bierin6 draft
    ipv6_packet.set_hop_limit(ipv6.hop_limit);
    // ipv6_packet.set_source("babe:1::5".parse().unwrap());
    ipv6_packet.set_source(ipv6.source);
    ipv6_packet.set_destination(ipv6.destination);
    ipv6_packet.set_payload(&udp_buffer);

    let mut bier_buffer = vec![0u8; payload.len() + 8 + 40 + 20];
    let mut bier_packet = MutableBierPacket::new(&mut bier_buffer).unwrap();
    bier_packet.set_bift_id(bier.bift_id);
    bier_packet.set_tc(bier.tc);
    bier_packet.set_s(bier.s);
    bier_packet.set_ttl(bier.ttl);
    bier_packet.set_nibble(bier.nibble);
    bier_packet.set_version(bier.version);
    bier_packet.set_bsl(bier.bsl);
    bier_packet.set_entropy(bier.entropy);
    bier_packet.set_oam(bier.oam);
    bier_packet.set_rsv(bier.rsv);
    bier_packet.set_dscp(bier.get_dscp());
    bier_packet.set_proto(bier.proto);
    bier_packet.set_bfir_id(bier.bfir_id);
    bier_packet.set_bitstring(&bier.bitstring);

    bier_packet.set_payload(&ipv6_buffer);

    let mut ethernet_buffer = match ipv6_opt {
        Some(_) => vec![0u8; payload.len() + 8 + 40 + 20 + 40 + 14],
        None => vec![0u8; payload.len() + 8 + 40 + 20 + 14],
    };
    let mut ethernet_packet = match ipv6_opt {
        Some(ipv6_opt_data) => {
            let mut ipv6_buffer_encap = vec![0u8; payload.len() + 8 + 40 + 20 + 40];
            let mut ipv6_packet_encap = MutableIpv6Packet::new(&mut ipv6_buffer_encap).unwrap();

            ipv6_packet_encap.set_version(ipv6_opt_data.version);
            ipv6_packet_encap.set_traffic_class(ipv6_opt_data.traffic_class);
            ipv6_packet_encap.set_flow_label(ipv6_opt_data.flow_label);
            ipv6_packet_encap.set_payload_length((payload.len() + 8 + 40 + 20) as u16);
            ipv6_packet_encap.set_next_header(IpNextHeaderProtocol::new(253)); // TODO: change with Bierin6 draft
            ipv6_packet_encap.set_hop_limit(ipv6_opt_data.hop_limit);
            // ipv6_packet.set_source("babe:1::5".parse().unwrap());
            ipv6_packet_encap.set_source(ipv6_opt_data.source);
            ipv6_packet_encap.set_destination(ipv6_opt_data.destination);
            ipv6_packet_encap.set_payload(&bier_buffer);

            let mut tmp_packet = MutableEthernetPacket::new(&mut ethernet_buffer).unwrap();

            tmp_packet.set_ethertype(EtherTypes::Ipv6);
            tmp_packet.set_payload(&ipv6_buffer_encap);
            tmp_packet
        }
        _ => {
            let mut tmp_packet = MutableEthernetPacket::new(&mut ethernet_buffer).unwrap();

            tmp_packet.set_ethertype(EtherType::new(0xAB37));
            tmp_packet.set_payload(&bier_buffer);
            tmp_packet
        }
    };

    //ethernet_packet.set_destination(MacAddr::broadcast());
    //ethernet_packet.set_source(interface.mac.unwrap());
    ethernet_packet.set_destination(ethernet.destination);
    ethernet_packet.set_source(ethernet.source);

    ethernet_buffer
}

fn simple_ethernet_socket(buff: &[u8], encap_ipv6: Option<bool>) {
    println!("Hello, world!");

    let iface_name = "s-eth0";

    let interfaces = pnet::datalink::interfaces();
    let interface = interfaces
        .into_iter()
        .find(|iface| iface.name == iface_name)
        .unwrap();
    let _source_mac = interface.mac.unwrap();

    let _target_mac = pnet::datalink::MacAddr::broadcast();

    let (mut sender, mut _receiver) = match pnet::datalink::channel(&interface, Default::default())
    {
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
    let mut bier_packet = MutableBierPacket::new(&mut bier_buffer).unwrap();
    bier_packet.set_bift_id(1);
    bier_packet.set_tc(2);
    bier_packet.set_s(0);
    bier_packet.set_ttl(255);
    bier_packet.set_nibble(2);
    bier_packet.set_version(3);
    bier_packet.set_bsl(0);
    bier_packet.set_entropy(0xfffff);
    bier_packet.set_oam(1);
    bier_packet.set_rsv(2);
    // bier_packet.set_dscp(3 << 2);
    bier_packet.set_dscp((3 << 2) + 1);
    bier_packet.set_proto(7);
    bier_packet.set_bfir_id(0b1);
    let bitstring = vec![0b1010];
    bier_packet.set_bitstring(&bitstring);

    bier_packet.set_payload(&udp_buffer);

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
            ipv6_packet.set_next_header(IpNextHeaderProtocol::new(253)); // TODO: change with Bierin6 draft
            ipv6_packet.set_hop_limit(255);
            ipv6_packet.set_source("babe:1::5".parse().unwrap());
            ipv6_packet.set_destination("babe:2::5".parse().unwrap());
            ipv6_packet.set_payload(&bier_buffer);

            let mut tmp_packet = MutableEthernetPacket::new(&mut ethernet_buffer).unwrap();

            tmp_packet.set_ethertype(EtherTypes::Ipv6);
            tmp_packet.set_payload(&ipv6_buffer);
            tmp_packet
        }
        _ => {
            let mut tmp_packet = MutableEthernetPacket::new(&mut ethernet_buffer).unwrap();

            tmp_packet.set_ethertype(EtherType::new(0xAB37));
            tmp_packet.set_payload(&bier_buffer);
            tmp_packet
        }
    };

    ethernet_packet.set_destination(MacAddr::broadcast());
    ethernet_packet.set_source(interface.mac.unwrap());

    //let payload = vec![255u8; 5];
    //bier_packet.set_payload(&payload);
    println!("Coucou");

    for _ in 0..1 {
        sender
            .send_to(ethernet_packet.packet(), None)
            .unwrap()
            .unwrap();

        println!("Sent a packet!");
    }
}