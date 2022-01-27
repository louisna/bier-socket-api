extern crate pnet;

use pnet_macros::packet;
use pnet_macros_support::types::*;

#[packet]
pub struct BIER {
    pub bift_id: u20be, // Really big endian?
    pub tc: u3,
    pub s: u1,
    pub ttl: u8,
    pub nibble: u4,
    pub version: u4,
    pub bsl: u4,
    pub entropy: u20be,
    pub oam: u2,
    pub rsv: u2,
    pub dscp: u8, // Should be u6...
    pub proto: u4, // Should be u6...
    pub bfir_id: u16be,
    //pub dscp: u4,
    //pub proto: u4,
    //pub bfir_id: u24be,
    #[length_fn="bitstring_length"]
    pub bitstring: Vec<u32be>,
    #[payload]
    pub payload: Vec<u8>,
}

#[inline]
fn bitstring_length(bier: &BIERPacket) -> usize {
    println!("{}", 1 << (bier.get_bsl() + 5));
    2 << (bier.get_bsl() + 5 - 3 - 1)
}