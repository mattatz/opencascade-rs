use glam::DVec3;
use serde::{Deserialize, Serialize};
use typeshare::typeshare;

use crate::{BSplineCurveProfile, BezierCurveProfile};


/// UV parameter bounds for a face
#[typeshare]
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct UVBounds {
    pub u_min: f64,
    pub u_max: f64,
    pub v_min: f64,
    pub v_max: f64,
}

impl UVBounds {
    /// Create a new UVBounds
    pub fn new(u_min: f64, u_max: f64, v_min: f64, v_max: f64) -> Self {
        Self { u_min, u_max, v_min, v_max }
    }

    /// Get the U range (u_max - u_min)
    pub fn u_range(&self) -> f64 {
        self.u_max - self.u_min
    }

    /// Get the V range (v_max - v_min)
    pub fn v_range(&self) -> f64 {
        self.v_max - self.v_min
    }

    /// Get the center point in UV space
    pub fn center(&self) -> (f64, f64) {
        ((self.u_min + self.u_max) / 2.0, (self.v_min + self.v_max) / 2.0)
    }
}

/// A planar surface
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Plane {
    pub location: DVec3,
    pub axis_location: DVec3,
    pub axis_direction: DVec3,
    pub x_direction: DVec3,
    pub y_direction: DVec3,
    /// UV parameter bounds of the face (not the infinite plane)
    pub bounds: UVBounds,
}

/// A cylindrical surface
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Cylinder {
    pub location: DVec3,
    pub axis_location: DVec3,
    pub axis_direction: DVec3,
    pub x_direction: DVec3,
    pub y_direction: DVec3,
    pub radius: f64,
}

/// A conical surface
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Cone {
    pub location: DVec3,
    pub axis_location: DVec3,
    pub axis_direction: DVec3,
    pub x_direction: DVec3,
    pub y_direction: DVec3,
    pub ref_radius: f64,
    pub semi_angle: f64,
}

/// A spherical surface
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Sphere {
    pub location: DVec3,
    pub axis_location: DVec3,
    pub axis_direction: DVec3,
    pub x_direction: DVec3,
    pub y_direction: DVec3,
    pub radius: f64,
}

/// A toroidal surface
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Torus {
    pub location: DVec3,
    pub axis_location: DVec3,
    pub axis_direction: DVec3,
    pub x_direction: DVec3,
    pub y_direction: DVec3,
    pub major_radius: f64,
    pub minor_radius: f64,
}

/// A B-Spline surface
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BSplineSurface {
    pub u_profile: BSplineCurveProfile,
    pub v_profile: BSplineCurveProfile,
    pub poles: Vec<Vec<DVec3>>,
    pub weights: Option<Vec<Vec<f64>>>,
}

/// A Bezier surface
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BezierSurface {
    pub u_profile: BezierCurveProfile,
    pub v_profile: BezierCurveProfile,
    pub poles: Vec<Vec<DVec3>>,
    pub weights: Option<Vec<Vec<f64>>>,
}

/// Detailed information about a surface's geometric properties
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", content = "data")]
pub enum SurfaceDetails {
    /// A planar surface
    Plane(Plane),
    /// A cylindrical surface
    Cylinder(Cylinder),
    /// A conical surface
    Cone(Cone),
    /// A spherical surface
    Sphere(Sphere),
    /// A toroidal surface
    Torus(Torus),
    /// A B-Spline surface
    BSpline(BSplineSurface),
    /// A Bezier surface
    Bezier(BezierSurface),
    /// Unknown or unsupported surface type
    Unknown(String),
}

