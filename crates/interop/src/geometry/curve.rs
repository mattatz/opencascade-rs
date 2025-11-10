use glam::{DVec2, DVec3};
use serde::{Deserialize, Serialize};
use typeshare::typeshare;

/// The orientation of a topological shape (edge, face, etc.)
#[typeshare]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum Orientation {
    /// Forward orientation - the default positive direction
    Forward,
    /// Reversed orientation - the opposite direction
    Reversed,
    /// Internal orientation - internal to a solid
    Internal,
    /// External orientation - external to a solid
    External,
}

impl std::fmt::Display for Orientation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Forward => write!(f, "Forward"),
            Self::Reversed => write!(f, "Reversed"),
            Self::Internal => write!(f, "Internal"),
            Self::External => write!(f, "External"),
        }
    }
}

/// A 2D line
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Line2d {
    pub origin: DVec2,
    pub direction: DVec2,
    pub first_parameter: f64,
    pub last_parameter: f64,
}

impl Line2d {
    /// Get the start point of the line segment
    pub fn start_point(&self) -> DVec2 {
        self.origin + self.direction * self.first_parameter
    }

    /// Get the end point of the line segment
    pub fn end_point(&self) -> DVec2 {
        self.origin + self.direction * self.last_parameter
    }

    /// Get the length of the line segment
    pub fn length(&self) -> f64 {
        (self.last_parameter - self.first_parameter) * self.direction.length()
    }
}

/// A 2D circle
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Circle2d {
    pub center: DVec2,
    pub x_direction: DVec2,
    pub y_direction: DVec2,
    pub radius: f64,
    pub first_parameter: f64,
    pub last_parameter: f64,
}

/// A 2D ellipse
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Ellipse2d {
    pub center: DVec2,
    pub x_direction: DVec2,
    pub y_direction: DVec2,
    pub major_radius: f64,
    pub minor_radius: f64,
    pub first_parameter: f64,
    pub last_parameter: f64,
}

/// A 2D B-Spline curve
#[derive(Debug, Clone)]
pub struct BSplineCurve2d {
    pub profile: BSplineCurveProfile,
    pub poles: Vec<DVec2>,
    pub weights: Option<Vec<f64>>,
    pub first_parameter: f64,
    pub last_parameter: f64,
}

/// A 2D Bezier curve
#[derive(Debug, Clone)]
pub struct BezierCurve2d {
    pub profile: BezierCurveProfile,
    pub poles: Vec<DVec2>,
    pub weights: Option<Vec<f64>>,
    pub first_parameter: f64,
    pub last_parameter: f64,
}

/// A 2D trimmed curve (基底カーブをパラメータ範囲で切り取ったもの)
#[derive(Debug, Clone)]
pub struct TrimmedCurve2d {
    pub basis_curve: Box<Curve2dDetails>,
    pub first_parameter: f64,
    pub last_parameter: f64,
}

/// Detailed information about a 2D curve's geometric properties
#[derive(Debug, Clone)]
pub enum Curve2dDetails {
    /// A 2D line
    Line(Line2d),
    /// A 2D circle
    Circle(Circle2d),
    /// A 2D ellipse
    Ellipse(Ellipse2d),
    /// A 2D B-Spline curve
    BSplineCurve(BSplineCurve2d),
    /// A 2D Bezier curve
    BezierCurve(BezierCurve2d),
    /// A 2D trimmed curve
    TrimmedCurve(TrimmedCurve2d),
    /// Unknown or unsupported curve type
    Unknown(String),
}

/// A line
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Line {
    pub origin: DVec3,
    pub direction: DVec3,
    pub first_parameter: f64,
    pub last_parameter: f64,
}

impl Line {
    /// Get the start point of the line segment
    pub fn start_point(&self) -> DVec3 {
        self.origin + self.direction * self.first_parameter
    }

    /// Get the end point of the line segment
    pub fn end_point(&self) -> DVec3 {
        self.origin + self.direction * self.last_parameter
    }

    /// Get the length of the line segment
    pub fn length(&self) -> f64 {
        (self.last_parameter - self.first_parameter) * self.direction.length()
    }
}

/// A circle
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Circle {
    pub center: DVec3,
    pub axis: DVec3,
    pub x_axis: DVec3,
    pub y_axis: DVec3,
    pub radius: f64,
    pub first_parameter: f64,
    pub last_parameter: f64,
}

/// An ellipse
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Ellipse {
    pub center: DVec3,
    pub axis: DVec3,
    pub x_axis: DVec3,
    pub y_axis: DVec3,
    pub major_radius: f64,
    pub minor_radius: f64,
    pub first_parameter: f64,
    pub last_parameter: f64,
}

/// A B-Spline curve profile
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BSplineCurveProfile {
    pub nb_poles: u32,
    pub degree: u32,
    pub is_rational: bool,
    pub is_periodic: bool,
    pub knots: Vec<f64>,
    pub multiplicities: Vec<u32>,
}

impl BSplineCurveProfile {
    pub fn expand_knots(&self) -> Vec<f64> {
        self.knots
            .iter()
            .zip(self.multiplicities.iter())
            .flat_map(|(knot, mult)| vec![*knot; *mult as usize])
            .collect()
    }
}

/// A B-Spline curve
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BSplineCurve {
    pub profile: BSplineCurveProfile,
    pub poles: Vec<DVec3>,
    pub weights: Option<Vec<f64>>,
    pub first_parameter: f64,
    pub last_parameter: f64,
}

/// A Bezier curve profile
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BezierCurveProfile {
    pub nb_poles: u32,
    pub degree: u32,
}

/// A Bezier curve
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BezierCurve {
    pub profile: BezierCurveProfile,
    pub poles: Vec<DVec3>,
    pub weights: Option<Vec<f64>>,
    pub first_parameter: f64,
    pub last_parameter: f64,
}

/// A hyperbola
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Hyperbola {
    pub center: DVec3,
    pub axis: DVec3,
    pub major_radius: f64,
    pub minor_radius: f64,
}

/// A parabola
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Parabola {
    pub vertex: DVec3,
    pub axis: DVec3,
    pub focal: f64,
}

/// An offset curve
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OffsetCurve {
    pub basis_curve_type: String,
    pub offset: f64,
}

/// A trimmed curve
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TrimmedCurve {
    pub basis_curve_type: String,
    pub first_parameter: f64,
    pub last_parameter: f64,
}

/// Detailed information about a curve's geometric properties
#[typeshare]
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", content = "data")]
pub enum CurveDetails {
    /// A line
    Line(Line),
    /// A circle
    Circle(Circle),
    /// An ellipse
    Ellipse(Ellipse),
    /// A B-Spline curve
    BSplineCurve(BSplineCurve),
    /// A Bezier curve
    BezierCurve(BezierCurve),
    /// A hyperbola
    Hyperbola(Hyperbola),
    /// A parabola
    Parabola(Parabola),
    /// An offset curve
    OffsetCurve(OffsetCurve),
    /// A trimmed curve
    TrimmedCurve(TrimmedCurve),
    /// Unknown or unsupported curve type
    Unknown(String),
}

