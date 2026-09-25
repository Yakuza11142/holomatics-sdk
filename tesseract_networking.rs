pub mod tesseract_networking {

    // ----------------------------------------------------------------------------
    // 0. SPATIAL PRIMITIVES
    // ----------------------------------------------------------------------------
    #[derive(Debug, Clone, Copy, PartialEq, Default)]
    pub struct Vector3 {
        pub x: f32,
        pub y: f32,
        pub z: f32,
    }

    impl Vector3 {
        #[inline]
        pub const fn new(x: f32, y: f32, z: f32) -> Self {
            Self { x, y, z }
        }
    }

    #[derive(Debug, Clone, Copy, PartialEq, Default)]
    pub struct Quaternion {
        pub x: f32,
        pub y: f32,
        pub z: f32,
        pub w: f32,
    }

    impl Quaternion {
        #[inline]
        pub const fn new(x: f32, y: f32, z: f32, w: f32) -> Self {
            Self { x, y, z, w }
        }
    }

    #[derive(Debug, Clone, PartialEq)]
    pub struct SpatialAnchorAnchorPacket {
        pub device_id: u32,
        pub position: Vector3,
        pub rotation: Quaternion,
    }

    pub const PACKET_MAGIC_HEADER: u32 = 0x5445_5353; // 'TESS'
    pub const PACKET_TOTAL_SIZE: usize = 40;

    // ----------------------------------------------------------------------------
    // 1. BUILT-IN CRC32 CHECKSUM ENGINE (IEEE 802.3 Standard)
    // ----------------------------------------------------------------------------
    #[inline]
    pub fn compute_crc32(data: &[u8]) -> u32 {
        let mut crc: u32 = 0xFFFFFFFF;
        for &byte in data {
            crc ^= byte as u32;
            for _ in 0..8 {
                let mask = (crc & 1).wrapping_neg();
                crc = (crc >> 1) ^ (0xED88_8320 & mask);
            }
        }
        !crc
    }

    // ----------------------------------------------------------------------------
    // 2. SECURE SERIALIZATION PACKET LAYER
    // ----------------------------------------------------------------------------
    /// Serializes tracking parameters into a 40-byte big-endian network packet buffer.
    pub fn tess_serialize_anchor_packet(
        dev_id: u32,
        pos: Vector3,
        rot: Quaternion,
    ) -> Vec<u8> {
        let mut packet = vec![0u8; PACKET_TOTAL_SIZE];

        // Header and Device Identification (Big-Endian)
        packet[0..4].copy_from_slice(&PACKET_MAGIC_HEADER.to_be_bytes());
        packet[4..8].copy_from_slice(&dev_id.to_be_bytes());

        // Spatial Vector Translation Payload
        packet[8..12].copy_from_slice(&pos.x.to_be_bytes());
        packet[12..16].copy_from_slice(&pos.y.to_be_bytes());
        packet[16..20].copy_from_slice(&pos.z.to_be_bytes());

        // Spatial Quaternion Rotation Payload
        packet[20..24].copy_from_slice(&rot.x.to_be_bytes());
        packet[24..28].copy_from_slice(&rot.y.to_be_bytes());
        packet[28..32].copy_from_slice(&rot.z.to_be_bytes());
        packet[32..36].copy_from_slice(&rot.w.to_be_bytes());

        // Compute CRC32 checksum across payload slice [0..36]
        let payload_checksum = compute_crc32(&packet[0..36]);

        // Append trailing Checksum Signature [36..40]
        packet[36..40].copy_from_slice(&payload_checksum.to_be_bytes());

        packet
    }

    // ----------------------------------------------------------------------------
    // 3. DESERIALIZATION AND VERIFICATION FILTER ROUTINES
    // ----------------------------------------------------------------------------
    /// Verifies packet size, magic signature, and CRC32 payload integrity.
    #[inline]
    pub fn tess_deserialize_and_verify_packet(pkt_buf: &[u8]) -> bool {
        // Hard boundary allocation check
        if pkt_buf.len() < PACKET_TOTAL_SIZE {
            return false;
        }

        // Validate 'TESS' Magic Header Signature
        let magic_bytes: [u8; 4] = match pkt_buf[0..4].try_into() {
            Ok(bytes) => bytes,
            Err(_) => return false,
        };
        if u32::from_be_bytes(magic_bytes) != PACKET_MAGIC_HEADER {
            return false;
        }

        // Validate Payload CRC32 Checksum
        let checksum_bytes: [u8; 4] = match pkt_buf[36..40].try_into() {
            Ok(bytes) => bytes,
            Err(_) => return false,
        };
        let received_checksum = u32::from_be_bytes(checksum_bytes);
        let expected_checksum = compute_crc32(&pkt_buf[0..36]);

        received_checksum == expected_checksum
    }

    /// Deserializes a verified buffer directly into a strongly-typed Rust struct.
    pub fn tess_unpack_anchor_packet(pkt_buf: &[u8]) -> Option<SpatialAnchorAnchorPacket> {
        if !tess_deserialize_and_verify_packet(pkt_buf) {
            return None;
        }

        let dev_id = u32::from_be_bytes(pkt_buf[4..8].try_into().ok()?);

        let px = f32::from_be_bytes(pkt_buf[8..12].try_into().ok()?);
        let py = f32::from_be_bytes(pkt_buf[12..16].try_into().ok()?);
        let pz = f32::from_be_bytes(pkt_buf[16..20].try_into().ok()?);

        let rx = f32::from_be_bytes(pkt_buf[20..24].try_into().ok()?);
        let ry = f32::from_be_bytes(pkt_buf[24..28].try_into().ok()?);
        let rz = f32::from_be_bytes(pkt_buf[28..32].try_into().ok()?);
        let rw = f32::from_be_bytes(pkt_buf[32..36].try_into().ok()?);

        Some(SpatialAnchorAnchorPacket {
            device_id: dev_id,
            position: Vector3::new(px, py, pz),
            rotation: Quaternion::new(rx, ry, rz, rw),
        })
    }
}
