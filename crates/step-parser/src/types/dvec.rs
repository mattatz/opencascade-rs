/// Re-export of glam types with typeshare support for TypeScript generation
use serde::{Deserialize, Serialize};
use typeshare::typeshare;

/// A 3D vector with double-precision floating point components
#[typeshare]
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct DVec3 {
    pub x: f64,
    pub y: f64,
    pub z: f64,
}

/// A 2D vector with double-precision floating point components
#[typeshare]
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct DVec2 {
    pub x: f64,
    pub y: f64,
}
