-- https://mika-s.github.io/wireshark/lua/dissector/2017/11/04/creating-a-wireshark-dissector-in-lua-1.html

bier_protocol = Proto("BIER", "Bit Index Explicit Replication")

bfit_id = ProtoField.uint32("bier.bfit_id", "BFIT-ID", base.HEX, nil, 0xfffff0)
tc = ProtoField.uint8("bier.tc", "TC", base.DEC, nil, 0xe)
s = ProtoField.uint8("bier.s", "S", base.DEC, nil, 0x1)
ttl = ProtoField.uint8("bier.ttl", "TTL", base.DEC, nil)
nibble = ProtoField.uint8("bier.nibble", "Nibble", base.DEC, nil, 0xf0)
version = ProtoField.uint8("bier.version", "Version", base.DEC, nil, 0x0f)
bsl = ProtoField.uint8("bier.bsl", "BSL", base.DEC, nil, 0xf0)
entropy = ProtoField.uint24("bier.entropy", "Entropy", base.DEC, nil, 0xfffff)
oam = ProtoField.uint8("bier.oam", "OAM", base.DEC, nil, 0xc0)
rsv = ProtoField.uint8("bier.rsv", "RSV", base.DEC, nil, 0x30)
dscp = ProtoField.uint16("bier.dscp", "DSCP", base.DEC, nil, 0x0fc0)
proto = ProtoField.uint8("bier.proto", "Proto", base.DEC, nil, 0x3f)
bfir_id = ProtoField.uint16("bier.bfir_id", "BFIR-ID", base.DEC, nil)
bitstring = ProtoField.bytes("bier.bitstring", "BitString")

bier_protocol.fields = { bfit_id, tc, s, ttl, nibble, version, bsl, entropy, oam, rsv, dscp, proto, bfir_id, bitstring }

ipv6 = Dissector.get("ipv6")

function bier_protocol.dissector(buffer, pinfo, tree)
    length = buffer:len()
    if length == 0 then return end

    pinfo.cols.protocol = bier_protocol.name

    bsl_packet = buffer(5,1):bitfield(0,4)
    bitstring_length = 2^(bsl_packet + 5 - 3)  -- in bytes
    
    local subtree = tree:add(bier_protocol, buffer(), "BIER protocol data")
    subtree:add(bfit_id, buffer(0,3))
    subtree:add(tc, buffer(2,1))
    subtree:add(s, buffer(2,1))
    subtree:add(ttl, buffer(3,1))
    subtree:add(nibble, buffer(4,1))
    subtree:add(version, buffer(4,1))
    subtree:add(bsl, buffer(5,1))
    subtree:add(entropy, buffer(5,3))
    subtree:add(oam, buffer(8,1))
    subtree:add(rsv, buffer(8,1))
    subtree:add(dscp, buffer(8,2))
    subtree:add(proto, buffer(9,1))
    subtree:add(bfir_id, buffer(10,2))
    subtree:add(bitstring, buffer(12,bitstring_length))
    
    pproto = buffer(9):bitfield(2,6)
    if pproto == 0x6 then
        ipv6:call(buffer(12 + bitstring_length):tvb(), pinfo, tree)
    end
end

local ethertype = DissectorTable.get("ethertype")
ethertype:add(0xab37, bier_protocol)
local ipv6_next = DissectorTable.get("ip.proto")
ipv6_next:add(0xfd, bier_protocol)

--local udp = DissectorTable:get_dissector("udp")
local bier_proto = DissectorTable.new("bier.protolist", bier_protocol.fields.proto)
--bier_proto:call()
--local bier_proto
--local proto = bier_proto.get("bier.proto")
--bier_proto:add(0x7, udp)
