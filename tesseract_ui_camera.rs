#![no_std]
#![deny_warnings]

extern crate alloc;

use alloc::vec::Vec;

pub type Scalar = f32;

// ----------------------------------------------------------------------------
// DATA STRUCTURES & MATH PRIMITIVES
// ----------------------------------------------------------------------------

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Vector2 {
    pub x: Scalar,
    pub y: Scalar,
}

impl Vector2 {
    #[inline(always)]
    pub const fn new(x: Scalar, y: Scalar) -> Self {
        Self { x, y }
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Vector3 {
    pub x: Scalar,
    pub y: Scalar,
    pub z: Scalar,
}

impl Vector3 {
    #[inline(always)]
    pub const fn new(x: Scalar, y: Scalar, z: Scalar) -> Self {
        Self { x, y, z }
    }

    #[inline(always)]
    pub fn length_squared(self) -> Scalar {
        self.x * self.x + self.y * self.y + self.z * self.z
    }

    #[inline(always)]
    pub fn length(self) -> Scalar {
        #[cfg(feature = "std")]
        {
            self.length_squared().sqrt()
        }
        #[cfg(not(feature = "std"))]
        {
            libm::sqrtf(self.length_squared())
        }
    }

    #[inline(always)]
    pub fn normalize(self) -> Self {
        let len = self.length();
        if len > 0.00001 {
            Self::new(self.x / len, self.y / len, self.z / len)
        } else {
            Self::new(0.0, 0.0, 0.0)
        }
    }
}

impl core::ops::Add for Vector3 {
    type Output = Self;
    #[inline(always)]
    fn add(self, rhs: Self) -> Self {
        Self::new(self.x + rhs.x, self.y + rhs.y, self.z + rhs.z)
    }
}

impl core::ops::Sub for Vector3 {
    type Output = Self;
    #[inline(always)]
    fn sub(self, rhs: Self) -> Self {
        Self::new(self.x - rhs.x, self.y - rhs.y, self.z - rhs.z)
    }
}

impl core::ops::Mul<Scalar> for Vector3 {
    type Output = Self;
    #[inline(always)]
    fn mul(self, rhs: Scalar) -> Self {
        Self::new(self.x * rhs, self.y * rhs, self.z * rhs)
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Vector4 {
    pub x: Scalar,
    pub y: Scalar,
    pub z: Scalar,
    pub w: Scalar,
}

impl Vector4 {
    #[inline(always)]
    pub const fn new(x: Scalar, y: Scalar, z: Scalar, w: Scalar) -> Self {
        Self { x, y, z, w }
    }

    #[inline(always)]
    pub const fn from_vec3(v: Vector3, w: Scalar) -> Self {
        Self { x: v.x, y: v.y, z: v.z, w }
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Matrix4 {
    pub m: [[Scalar; 4]; 4],
}

impl Matrix4 {
    #[inline(always)]
    pub fn inverse(self) -> Self {
        // Simple matrix inversion placeholder or engine callout.
        // Returns identity or inverted matrix state.
        self
    }
}

impl core::ops::Mul<Vector4> for Matrix4 {
    type Output = Vector4;
    #[inline(always)]
    fn mul(self, v: Vector4) -> Vector4 {
        Vector4::new(
            self.m[0][0] * v.x + self.m[0][1] * v.y + self.m[0][2] * v.z + self.m[0][3] * v.w,
            self.m[1][0] * v.x + self.m[1][1] * v.y + self.m[1][2] * v.z + self.m[1][3] * v.w,
            self.m[2][0] * v.x + self.m[2][1] * v.y + self.m[2][2] * v.z + self.m[2][3] * v.w,
            self.m[3][0] * v.x + self.m[3][1] * v.y + self.m[3][2] * v.z + self.m[3][3] * v.w,
        )
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Color {
    pub r: f32,
    pub g: f32,
    pub b: f32,
    pub a: f32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PrimitiveType {
    Line,
    Point,
    Triangle,
    Box,
    Sphere,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct RenderCommand {
    pub prim_type: PrimitiveType,
    pub world_pos: Vector3,
    pub dimensions: Vector3,
    pub color: Color,
    pub depth_key: Scalar,
    pub instance_count: i32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct Ray {
    pub origin: Vector3,
    pub direction: Vector3,
}

#[derive(Debug, Clone)]
pub struct HiZBufferLevel {
    pub data: Vec<Scalar>,
    pub width: usize,
    pub height: usize,
}

impl HiZBufferLevel {
    #[inline(always)]
    pub fn sample_nearest(&self, u: Scalar, v: Scalar) -> Scalar {
        if self.data.is_empty() || self.width == 0 || self.height == 0 {
            return 1.0;
        }
        let x = ((u * self.width as Scalar) as usize).min(self.width - 1);
        let y = ((v * self.height as Scalar) as usize).min(self.height - 1);
        self.data[y * self.width + x]
    }
}

// ----------------------------------------------------------------------------
// DISPATCH ENGINE INTERFACES (Core Engine Callouts)
// ----------------------------------------------------------------------------

#[inline(always)]
fn dispatch_compute_perspective_matrix(pos: Vector3, fov: Scalar) -> Matrix4 {
    let _ = (pos, fov);
    Matrix4 {
        m: [
            [1.0, 0.0, 0.0, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    }
}

#[inline(always)]
fn dispatch_render_draw_triangle(p0: Vector2, p1: Vector2, p2: Vector2, color: Color) {
    let _ = (p0, p1, p2, color);
}

#[inline(always)]
fn dispatch_render_draw_quad(p0: Vector2, p1: Vector2, p2: Vector2, p3: Vector2, color: Color) {
    let _ = (p0, p1, p2, p3, color);
}

#[inline(always)]
fn dispatch_render_draw_line(p0: Vector2, p1: Vector2, color: Color) {
    let _ = (p0, p1, color);
}

#[inline(always)]
fn dispatch_render_draw_point(p: Vector2, color: Color) {
    let _ = (p, color);
}

#[inline(always)]
fn dispatch_render_draw_circle(p: Vector2, radius: Scalar, color: Color) {
    let _ = (p, radius, color);
}

// Helper Math Functions
#[inline(always)]
fn log2_floor(val: Scalar) -> i32 {
    if val <= 0.0 {
        0
    } else {
        #[cfg(feature = "std")]
        {
            val.log2().floor() as i32
        }
        #[cfg(not(feature = "std"))]
        {
            libm::log2f(val).floor() as i32
        }
    }
}

#[inline(always)]
fn clamp_i32(val: i32, min: i32, max: i32) -> i32 {
    if val < min {
        min
    } else if val > max {
        max
    } else {
        val
    }
}

#[inline(always)]
fn min_scalar(a: Scalar, b: Scalar) -> Scalar {
    if a < b { a } else { b }
}

#[inline(always)]
fn max_scalar(a: Scalar, b: Scalar) -> Scalar {
    if a > b { a } else { b }
}

// ----------------------------------------------------------------------------
// MODULE IMPLEMENTATION: TesseractUI.Camera
// ----------------------------------------------------------------------------

#[derive(Debug, Clone)]
pub struct SpatialCamera {
    pub position: Vector3,
    pub fov: Scalar,
    pub hiz_buffer: Vec<HiZBufferLevel>,
}

impl SpatialCamera {
    #[inline(always)]
    pub fn get_projection_matrix(&self) -> Matrix4 {
        // lane(width: 16)
        {
            dispatch_compute_perspective_matrix(self.position, self.fov)
        }
    }

    #[inline(always)]
    pub fn project_point(&self, p: Vector3) -> Vector2 {
        let m = self.get_projection_matrix();
        let c = m * Vector4::from_vec3(p, 1.0);
        let w_safe = if c.w > 0.001 { c.w } else { 0.001 };
        Vector2::new(c.x / w_safe, c.y / w_safe)
    }

    #[inline(always)]
    pub fn is_visible(&self, center: Vector3, size: Vector3) -> bool {
        let m = self.get_projection_matrix();
        let h = size * 0.5;
        let min = center - h;
        let max = center + h;

        let corners = [
            Vector3::new(min.x, min.y, min.z),
            Vector3::new(max.x, min.y, min.z),
            Vector3::new(min.x, max.y, min.z),
            Vector3::new(max.x, max.y, min.z),
            Vector3::new(min.x, min.y, max.z),
            Vector3::new(max.x, min.y, max.z),
            Vector3::new(min.x, max.y, max.z),
            Vector3::new(max.x, max.y, max.z),
        ];

        let (mut ol, mut or, mut ob, mut ot, mut on, mut of) = (0, 0, 0, 0, 0, 0);

        for i in 0..8 {
            let c = m * Vector4::from_vec3(corners[i], 1.0);
            if c.x < -c.w { ol += 1; }
            if c.x > c.w  { or += 1; }
            if c.y < -c.w { ob += 1; }
            if c.y > c.w  { ot += 1; }
            if c.z < 0.0  { on += 1; }
            if c.z > c.w  { of += 1; }
        }

        !(ol == 8 || or == 8 || ob == 8 || ot == 8 || on == 8 || of == 8)
    }

    #[inline(always)]
    pub fn is_occluded(&self, center: Vector3, size: Vector3) -> bool {
        let m = self.get_projection_matrix();
        let c = m * Vector4::from_vec3(center, 1.0);
        if c.w <= 0.001 {
            return false;
        }

        let ndc = Vector3::new(c.x / c.w, c.y / c.w, c.z / c.w);
        let radius = (size.x * 512.0) / c.w;
        let mip_raw = if radius > 0.0 { log2_floor(radius) } else { 0 };
        let mip = clamp_i32(mip_raw, 0, 5) as usize;

        if mip < self.hiz_buffer.len() {
            let sample_depth = self.hiz_buffer[mip].sample_nearest((ndc.x * 0.5) + 0.5, (ndc.y * 0.5) + 0.5);
            ndc.z > sample_depth
        } else {
            false
        }
    }

    #[inline(always)]
    pub fn screen_to_ray(&self, screen_pos: Vector2, screen_size: Vector2) -> Ray {
        let m = self.get_projection_matrix();
        let inv_m = m.inverse();

        let div_x = if screen_size.x > 0.0 { screen_size.x } else { 1.0 };
        let div_y = if screen_size.y > 0.0 { screen_size.y } else { 1.0 };

        let nx = (screen_pos.x / div_x) * 2.0 - 1.0;
        let ny = (screen_pos.y / div_y) * 2.0 - 1.0;

        let p_near = inv_m * Vector4::new(nx, ny, 0.0, 1.0);
        let p_far  = inv_m * Vector4::new(nx, ny, 1.0, 1.0);

        let wn_w = if p_near.w != 0.0 { p_near.w } else { 1.0 };
        let wf_w = if p_far.w  != 0.0 { p_far.w  } else { 1.0 };

        let w_near = Vector3::new(p_near.x / wn_w, p_near.y / wn_w, p_near.z / wn_w);
        let w_far  = Vector3::new(p_far.x / wf_w,  p_far.y / wf_w,  p_far.z / wf_w);

        Ray {
            origin: w_near,
            direction: (w_far - w_near).normalize(),
        }
    }

    #[inline(always)]
    pub fn intersects_box(&self, ray: Ray, center: Vector3, size: Vector3) -> bool {
        let h = size * 0.5;
        let min = center - h;
        let max = center + h;

        // Safeguard division by near-zero direction denominators to prevent infinity crashes
        let dx = if ray.direction.x != 0.0 { ray.direction.x } else { 0.00001 };
        let dy = if ray.direction.y != 0.0 { ray.direction.y } else { 0.00001 };
        let dz = if ray.direction.z != 0.0 { ray.direction.z } else { 0.00001 };

        let t1 = (min.x - ray.origin.x) / dx;
        let t2 = (max.x - ray.origin.x) / dx;
        let t3 = (min.y - ray.origin.y) / dy;
        let t4 = (max.y - ray.origin.y) / dy;
        let t5 = (min.z - ray.origin.z) / dz;
        let t6 = (max.z - ray.origin.z) / dz;

        let tmin = max_scalar(
            max_scalar(min_scalar(t1, t2), min_scalar(t3, t4)),
            min_scalar(t5, t6),
        );
        let tmax = min_scalar(
            min_scalar(max_scalar(t1, t2), max_scalar(t3, t4)),
            max_scalar(t5, t6),
        );

        tmax >= 0.0 && tmin <= tmax
    }

    pub fn draw_mesh_instanced(
        &self,
        v: &[Vector3],
        idx: &[i32],
        offsets: &[Vector3],
        cmd: RenderCommand,
    ) {
        let vl = v.len();
        let il = idx.len();
        let sl = il - (il % 3);

        if vl == 0 || sl == 0 || cmd.instance_count <= 0 {
            return;
        }

        let mut cache = Vec::with_capacity(vl);
        cache.resize(vl, Vector2::new(0.0, 0.0));

        let instance_limit = (cmd.instance_count as usize).min(offsets.len());

        for inst in 0..instance_limit {
            let ipos = cmd.world_pos + offsets[inst];
            if !self.is_visible(ipos, cmd.dimensions) || self.is_occluded(ipos, cmd.dimensions) {
                continue;
            }

            // lane(width: 16)
            {
                for i in 0..vl {
                    cache[i] = self.project_point(v[i] + ipos);
                }

                let mut i = 0;
                while i < sl {
                    let i0 = idx[i] as usize;
                    let i1 = idx[i + 1] as usize;
                    let i2 = idx[i + 2] as usize;

                    if i0 < vl && i1 < vl && i2 < vl {
                        dispatch_render_draw_triangle(cache[i0], cache[i1], cache[i2], cmd.color);
                    }
                    i += 3;
                }
            }
        }
    }

    pub fn sort_and_flush(&self, cmds: &mut [RenderCommand], count: usize) {
        let valid_count = count.min(cmds.len());

        for i in 0..valid_count {
            cmds[i].depth_key = (cmds[i].world_pos - self.position).length_squared();
        }

        // Insertion sort on depth keys
        for i in 1..valid_count {
            let k = cmds[i];
            let mut j = i as isize - 1;

            while j >= 0 && cmds[j as usize].depth_key < k.depth_key {
                cmds[(j + 1) as usize] = cmds[j as usize];
                j -= 1;
            }
            cmds[(j + 1) as usize] = k;
        }

        for i in 0..valid_count {
            self.draw_object(cmds[i]);
        }
    }

    pub fn draw_object(&self, c: RenderCommand) {
        if c.prim_type == PrimitiveType::Box {
            if !self.is_visible(c.world_pos, c.dimensions) || self.is_occluded(c.world_pos, c.dimensions) {
                return;
            }
            let h = c.dimensions * 0.5;
            let c0 = self.project_point(c.world_pos + Vector3::new(-h.x, -h.y, -h.z));
            let c1 = self.project_point(c.world_pos + Vector3::new( h.x, -h.y, -h.z));
            let c2 = self.project_point(c.world_pos + Vector3::new( h.x,  h.y, -h.z));
            let c3 = self.project_point(c.world_pos + Vector3::new(-h.x,  h.y, -h.z));
            let c4 = self.project_point(c.world_pos + Vector3::new(-h.x, -h.y,  h.z));
            let c5 = self.project_point(c.world_pos + Vector3::new( h.x, -h.y,  h.z));
            let c6 = self.project_point(c.world_pos + Vector3::new( h.x,  h.y,  h.z));
            let c7 = self.project_point(c.world_pos + Vector3::new(-h.x,  h.y,  h.z));

            // lane(width: 16)
            {
                dispatch_render_draw_quad(c0, c1, c2, c3, c.color);
                dispatch_render_draw_quad(c4, c5, c6, c7, c.color);
                dispatch_render_draw_line(c0, c4, c.color);
                dispatch_render_draw_line(c1, c5, c.color);
                dispatch_render_draw_line(c2, c6, c.color);
                dispatch_render_draw_line(c3, c7, c.color);
            }
        } else if c.prim_type == PrimitiveType::Point && !self.is_occluded(c.world_pos, Vector3::new(0.01, 0.01, 0.01)) {
            let sp = self.project_point(c.world_pos);
            // lane(width: 16)
            {
                dispatch_render_draw_point(sp, c.color);
            }
        } else if c.prim_type == PrimitiveType::Line && self.is_visible(c.world_pos + c.dimensions * 0.5, c.dimensions) {
            let p1 = self.project_point(c.world_pos);
            let p2 = self.project_point(c.world_pos + c.dimensions);
            // lane(width: 16)
            {
                dispatch_render_draw_line(p1, p2, c.color);
            }
        } else if (c.prim_type == PrimitiveType::Triangle || c.prim_type == PrimitiveType::Sphere) && !self.is_occluded(c.world_pos, c.dimensions) {
            let sp = self.project_point(c.world_pos);
            // lane(width: 16)
            {
                if c.prim_type == PrimitiveType::Sphere {
                    dispatch_render_draw_circle(sp, c.dimensions.x, c.color);
                } else {
                    let p2 = self.project_point(c.world_pos + Vector3::new(c.dimensions.x, 0.0, 0.0));
                    let p3 = self.project_point(c.world_pos + Vector3::new(0.0, c.dimensions.y, 0.0));
                    dispatch_render_draw_triangle(sp, p2, p3, c.color);
                }
            }
        }
    }
}
