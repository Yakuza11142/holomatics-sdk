#![no_std]
#![deny_warnings]

extern crate alloc;

use crate::tesseract_ui_core::{Matrix4, Scalar, Vector3};

// ----------------------------------------------------------------------------
// 1. COLLISION & SPATIAL BOUNDING PRIMITIVES
// ----------------------------------------------------------------------------

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct SphereCollider {
    pub center: Vector3,
    pub radius: Scalar,
}

impl SphereCollider {
    #[inline(always)]
    pub fn new(center: Vector3, radius: Scalar) -> Self {
        Self { center, radius }
    }

    /// Fast sphere-to-sphere collision check
    #[inline(always)]
    pub fn intersects(&self, other: &SphereCollider) -> bool {
        let diff = self.center - other.center;
        let dist_sq = diff.length_squared();
        let radius_sum = self.radius + other.radius;
        dist_sq <= (radius_sum * radius_sum)
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Ray {
    pub origin: Vector3,
    pub direction: Vector3,
}

impl Ray {
    #[inline(always)]
    pub fn new(origin: Vector3, direction: Vector3) -> Self {
        Self {
            origin,
            direction: direction.normalize(),
        }
    }

    /// Evaluates point along the ray at distance `t`
    #[inline(always)]
    pub fn point_at(&self, t: Scalar) -> Vector3 {
        self.origin + (self.direction * t)
    }
}

// ----------------------------------------------------------------------------
// 2. RIGID BODY DYNAMICS ENGINE
// ----------------------------------------------------------------------------

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct RigidBody {
    pub position: Vector3,
    pub velocity: Vector3,
    pub acceleration: Vector3,
    pub mass: Scalar,
    pub restitution: Scalar, // Bounciness (0.0 = inelastic, 1.0 = perfectly elastic)
    pub is_kinematic: bool,  // If true, object is unmovable by forces
}

impl RigidBody {
    #[inline(always)]
    pub fn new(position: Vector3, mass: Scalar) -> Self {
        Self {
            position,
            velocity: Vector3::new(0.0, 0.0, 0.0),
            acceleration: Vector3::new(0.0, 0.0, 0.0),
            mass: if mass <= 0.0001 { 1.0 } else { mass },
            restitution: 0.5,
            is_kinematic: false,
        }
    }

    /// Applies an impulse force to the body
    #[inline(always)]
    pub fn apply_force(&mut self, force: Vector3) {
        if !self.is_kinematic && self.mass > 0.0001 {
            self.acceleration = self.acceleration + (force * (1.0 / self.mass));
        }
    }

    /// Hardened: Integration time steps guarded against frame explosion spikes
    #[inline(always)]
    pub fn step(&mut self, dt: Scalar) {
        if dt <= 0.0 || dt > 0.1 || self.is_kinematic {
            return; // Block time dilation errors or static bodies
        }

        // SIMD execution block for physical integration
        // lane(width: 16)
        {
            // Apply constant down-force gravity (-9.80665 m/s²)
            self.velocity.y -= 9.80665 * dt;

            // Integrate acceleration into velocity
            self.velocity.x += self.acceleration.x * dt;
            self.velocity.y += self.acceleration.y * dt;
            self.velocity.z += self.acceleration.z * dt;

            // Linear Euler integration for position
            self.position.x += self.velocity.x * dt;
            self.position.y += self.velocity.y * dt;
            self.position.z += self.velocity.z * dt;

            // Reset linear acceleration for next frame
            self.acceleration = Vector3::new(0.0, 0.0, 0.0);
        }
    }
}
