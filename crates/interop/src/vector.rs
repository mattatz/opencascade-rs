use serde::{Deserialize, Serialize};
use typeshare::typeshare;

#[cfg(feature = "glam")]
use glam;

/// A 3D vector with double-precision floating point components
#[allow(unused)]
#[typeshare]
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct Vector3 {
    pub x: f64,
    pub y: f64,
    pub z: f64,
}

#[cfg(feature = "glam")]
impl From<glam::DVec3> for Vector3 {
    fn from(v: glam::DVec3) -> Self {
        Self { x: v.x, y: v.y, z: v.z }
    }
}

#[cfg(feature = "glam")]
impl From<Vector3> for glam::DVec3 {
    fn from(v: Vector3) -> Self {
        glam::dvec3(v.x, v.y, v.z)
    }
}

/// A 2D vector with double-precision floating point components
#[allow(unused)]
#[typeshare]
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct Vector2 {
    pub x: f64,
    pub y: f64,
}

#[cfg(feature = "glam")]
impl From<glam::DVec2> for Vector2 {
    fn from(v: glam::DVec2) -> Self {
        Self { x: v.x, y: v.y }
    }
}

#[cfg(feature = "glam")]
impl From<Vector2> for glam::DVec2 {
    fn from(v: Vector2) -> Self {
        glam::dvec2(v.x, v.y)
    }
}
