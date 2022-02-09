extern crate pnet;

pub mod bier {

use pnet_macros::packet;
use pnet_macros_support::types::*;
#[packet]
pub struct Bier {
    pub bift_id: u20be,
    pub tc: u3,
    pub s: u1,
    pub ttl: u8,
    pub nibble: u4,
    pub version: u4,
    pub bsl: u4,
    pub entropy: u20be,
    pub oam: u2,
    pub rsv: u2,
    pub dscp4: u4, // Somehow does not want it to be a u6
    pub dscp2: u2,
    pub proto: u6,
    pub bfir_id: u16be,
    #[length_fn = "bitstring_length"]
    pub bitstring: Vec<u32be>,
    #[payload]
    pub payload: Vec<u8>,
}

impl Bier {
    pub fn get_dscp(&self) -> u8 {
        (self.dscp4 << 2) + self.dscp2
    }
}

impl MutableBierPacket<'_> {
    /// Override the automatically built "set_dscp4" and "set_dscp2" functions
    /// and provides a wrapper to use the field on a whole.
    /// PLEASE use this function instead of "set_dscp4" and "seet_dscp2"
    pub fn set_dscp(&mut self, dscp: u8) {
        assert!(dscp < 64); // Ensure that the passed value is a u6
        self.set_dscp4(dscp & 0x3c);
        self.set_dscp2(dscp & 3);
    }

    #[allow(dead_code)]
    pub fn get_dscp(&mut self) -> u8 {
        (self.get_dscp4() << 2) + self.get_dscp2()
    }
}

// Do not know if useful to repeat it
impl BierPacket<'_> {
    #[allow(dead_code)]
    pub fn get_dscp(&self) -> u8 {
        (self.get_dscp4() << 2) + self.get_dscp2()
    }
}

#[inline]
fn bitstring_length(bier: &BierPacket) -> usize {
    println!("{}", 1 << (bier.get_bsl() + 5));
    2 << (bier.get_bsl() + 5 - 3 - 1)
}
}