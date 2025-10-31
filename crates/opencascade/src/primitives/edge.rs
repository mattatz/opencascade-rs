use crate::primitives::{make_axis_2, make_point};
use cxx::UniquePtr;
use glam::{dvec3, DVec3};
use opencascade_sys::ffi;

use super::make_vec;

/// A line
#[derive(Debug, Clone)]
pub struct Line {
    pub origin: DVec3,
    pub direction: DVec3,
}

/// A circle
#[derive(Debug, Clone)]
pub struct Circle {
    pub center: DVec3,
    pub axis: DVec3,
    pub radius: f64,
}

/// An ellipse
#[derive(Debug, Clone)]
pub struct Ellipse {
    pub center: DVec3,
    pub axis: DVec3,
    pub major_radius: f64,
    pub minor_radius: f64,
}

/// A B-Spline curve profile
#[derive(Debug, Clone)]
pub struct BSplineCurveProfile {
    pub nb_poles: usize,
    pub degree: usize,
    pub is_rational: bool,
    pub is_periodic: bool,
    pub knots: Vec<f64>,
    pub multiplicities: Vec<usize>,
}

/// A B-Spline curve
#[derive(Debug, Clone)]
pub struct BSplineCurve {
    pub profile: BSplineCurveProfile,
    pub poles: Vec<DVec3>,
    pub weights: Vec<f64>,
}

/// A Bezier curve profile
#[derive(Debug, Clone)]
pub struct BezierCurveProfile {
    pub nb_poles: usize,
    pub degree: usize,
}

/// A Bezier curve
#[derive(Debug, Clone)]
pub struct BezierCurve {
    pub profile: BezierCurveProfile,
    pub poles: Vec<DVec3>,
    pub weights: Vec<f64>,
}

/// A hyperbola
#[derive(Debug, Clone)]
pub struct Hyperbola {
    pub center: DVec3,
    pub axis: DVec3,
    pub major_radius: f64,
    pub minor_radius: f64,
}

/// A parabola
#[derive(Debug, Clone)]
pub struct Parabola {
    pub vertex: DVec3,
    pub axis: DVec3,
    pub focal: f64,
}

/// An offset curve
#[derive(Debug, Clone)]
pub struct OffsetCurve {
    pub basis_curve_type: String,
    pub offset: f64,
}

/// A trimmed curve
#[derive(Debug, Clone)]
pub struct TrimmedCurve {
    pub basis_curve_type: String,
    pub first_parameter: f64,
    pub last_parameter: f64,
}

/// Detailed information about a curve's geometric properties
#[derive(Debug, Clone)]
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

#[derive(Debug, Copy, Clone, PartialEq)]
pub enum EdgeType {
    Line,
    Circle,
    Ellipse,
    Hyperbola,
    Parabola,
    BezierCurve,
    BSplineCurve,
    OffsetCurve,
    OtherCurve,
}

impl From<ffi::GeomAbs_CurveType> for EdgeType {
    fn from(curve_type: ffi::GeomAbs_CurveType) -> Self {
        match curve_type {
            ffi::GeomAbs_CurveType::GeomAbs_Line => Self::Line,
            ffi::GeomAbs_CurveType::GeomAbs_Circle => Self::Circle,
            ffi::GeomAbs_CurveType::GeomAbs_Ellipse => Self::Ellipse,
            ffi::GeomAbs_CurveType::GeomAbs_Hyperbola => Self::Hyperbola,
            ffi::GeomAbs_CurveType::GeomAbs_Parabola => Self::Parabola,
            ffi::GeomAbs_CurveType::GeomAbs_BezierCurve => Self::BezierCurve,
            ffi::GeomAbs_CurveType::GeomAbs_BSplineCurve => Self::BSplineCurve,
            ffi::GeomAbs_CurveType::GeomAbs_OffsetCurve => Self::OffsetCurve,
            ffi::GeomAbs_CurveType::GeomAbs_OtherCurve => Self::OtherCurve,
            ffi::GeomAbs_CurveType { repr } => panic!("Unexpected curve type: {repr}"),
        }
    }
}

pub struct Edge {
    pub(crate) inner: UniquePtr<ffi::TopoDS_Edge>,
}

impl AsRef<Edge> for Edge {
    fn as_ref(&self) -> &Edge {
        self
    }
}

impl Edge {
    pub(crate) fn from_edge(edge: &ffi::TopoDS_Edge) -> Self {
        let inner = ffi::TopoDS_Edge_to_owned(edge);

        Self { inner }
    }

    fn from_make_edge(mut make_edge: UniquePtr<ffi::BRepBuilderAPI_MakeEdge>) -> Self {
        Self::from_edge(make_edge.pin_mut().Edge())
    }

    pub fn segment(p1: DVec3, p2: DVec3) -> Self {
        let make_edge =
            ffi::BRepBuilderAPI_MakeEdge_gp_Pnt_gp_Pnt(&make_point(p1), &make_point(p2));

        Self::from_make_edge(make_edge)
    }

    pub fn bezier(points: impl IntoIterator<Item = DVec3>) -> Self {
        let points: Vec<_> = points.into_iter().collect();
        let mut array = ffi::TColgp_HArray1OfPnt_ctor(1, points.len() as i32);
        for (index, point) in points.into_iter().enumerate() {
            array.pin_mut().SetValue(index as i32 + 1, &make_point(point));
        }

        let bezier = ffi::Geom_BezierCurve_ctor_points(&array);
        let bezier_handle = ffi::Geom_BezierCurve_to_handle(bezier);
        let curve_handle = ffi::new_HandleGeomCurve_from_HandleGeom_BezierCurve(&bezier_handle);

        let mut make_edge = ffi::BRepBuilderAPI_MakeEdge_HandleGeomCurve(&curve_handle);
        let edge = make_edge.pin_mut().Edge();
        Self::from_edge(edge)
    }

    pub fn circle(center: DVec3, normal: DVec3, radius: f64) -> Self {
        let axis = make_axis_2(center, normal);

        let make_circle = ffi::gp_Circ_ctor(&axis, radius);
        let make_edge = ffi::BRepBuilderAPI_MakeEdge_circle(&make_circle);

        Self::from_make_edge(make_edge)
    }

    pub fn ellipse() {}

    pub fn spline_from_points(
        points: impl IntoIterator<Item = DVec3>,
        tangents: Option<(DVec3, DVec3)>,
    ) -> Self {
        let points: Vec<_> = points.into_iter().collect();
        let mut array = ffi::TColgp_HArray1OfPnt_ctor(1, points.len() as i32);
        for (index, point) in points.into_iter().enumerate() {
            array.pin_mut().SetValue(index as i32 + 1, &make_point(point));
        }
        let array_handle = ffi::new_HandleTColgpHArray1OfPnt_from_TColgpHArray1OfPnt(array);

        let periodic = false;
        let tolerance = 1.0e-7;
        let mut interpolate = ffi::GeomAPI_Interpolate_ctor(&array_handle, periodic, tolerance);
        if let Some((t_start, t_end)) = tangents {
            interpolate.pin_mut().Load(&make_vec(t_start), &make_vec(t_end), true);
        }

        interpolate.pin_mut().Perform();
        let bspline_handle = ffi::GeomAPI_Interpolate_Curve(&interpolate);
        let curve_handle = ffi::new_HandleGeomCurve_from_HandleGeom_BSplineCurve(&bspline_handle);

        let mut make_edge = ffi::BRepBuilderAPI_MakeEdge_HandleGeomCurve(&curve_handle);
        let edge = make_edge.pin_mut().Edge();
        Self::from_edge(edge)
    }

    pub fn arc(p1: DVec3, p2: DVec3, p3: DVec3) -> Self {
        let make_arc = ffi::GC_MakeArcOfCircle_point_point_point(
            &make_point(p1),
            &make_point(p2),
            &make_point(p3),
        );

        let make_edge = ffi::BRepBuilderAPI_MakeEdge_HandleGeomCurve(
            &ffi::new_HandleGeomCurve_from_HandleGeom_TrimmedCurve(&ffi::GC_MakeArcOfCircle_Value(
                &make_arc,
            )),
        );

        Self::from_make_edge(make_edge)
    }

    pub fn start_point(&self) -> DVec3 {
        let curve = ffi::BRepAdaptor_Curve_ctor(&self.inner);
        let start_param = curve.FirstParameter();
        let point = ffi::BRepAdaptor_Curve_value(&curve, start_param);

        dvec3(point.X(), point.Y(), point.Z())
    }

    pub fn end_point(&self) -> DVec3 {
        let curve = ffi::BRepAdaptor_Curve_ctor(&self.inner);
        let last_param = curve.LastParameter();
        let point = ffi::BRepAdaptor_Curve_value(&curve, last_param);

        dvec3(point.X(), point.Y(), point.Z())
    }

    pub fn approximation_segments(&self) -> ApproximationSegmentIterator {
        let adaptor_curve = ffi::BRepAdaptor_Curve_ctor(&self.inner);
        let approximator = ffi::GCPnts_TangentialDeflection_ctor(&adaptor_curve, 0.1, 0.1);

        ApproximationSegmentIterator { count: 1, approximator }
    }

    pub fn tangent_arc(_p1: DVec3, _tangent: DVec3, _p3: DVec3) {}

    pub fn edge_type(&self) -> EdgeType {
        let curve = ffi::BRepAdaptor_Curve_ctor(&self.inner);

        EdgeType::from(curve.GetType())
    }

    /// Get the type name of the underlying curve (e.g., "Geom_Line", "Geom_Circle")
    pub fn curve_type(&self) -> String {
        let mut first = 0.0;
        let mut last = 0.0;
        let curve = ffi::BRep_Tool_Curve(&self.inner, &mut first, &mut last);

        if curve.IsNull() {
            return "Unknown".to_string();
        }

        let dynamic_type = ffi::DynamicTypeCurve(&curve);
        ffi::type_name(&dynamic_type)
    }

    /// Get detailed information about the underlying curve
    pub fn curve_details(&self) -> CurveDetails {
        let mut first = 0.0;
        let mut last = 0.0;
        let curve = ffi::BRep_Tool_Curve(&self.inner, &mut first, &mut last);

        if curve.IsNull() {
            return CurveDetails::Unknown("Null curve".to_string());
        }

        let curve_type = self.curve_type();

        match curve_type.as_str() {
            "Geom_Line" => {
                let line = ffi::cast_curve_to_line(&curve);
                if !line.IsNull() {
                    let position = ffi::geom_line_position(&line);
                    let origin = ffi::gp_Ax1_location(&position);
                    let direction = ffi::gp_Ax1_direction(&position);

                    CurveDetails::Line(Line {
                        origin: dvec3(origin.X(), origin.Y(), origin.Z()),
                        direction: dvec3(direction.X(), direction.Y(), direction.Z()),
                    })
                } else {
                    CurveDetails::Unknown(curve_type)
                }
            },
            "Geom_Circle" => {
                let circle = ffi::cast_curve_to_circle(&curve);
                if !circle.IsNull() {
                    let center = ffi::geom_circle_location(&circle);
                    let axis = ffi::geom_circle_axis(&circle);
                    let axis_dir = ffi::gp_Ax1_direction(&axis);
                    let radius = ffi::geom_circle_radius(&circle);

                    CurveDetails::Circle(Circle {
                        center: dvec3(center.X(), center.Y(), center.Z()),
                        axis: dvec3(axis_dir.X(), axis_dir.Y(), axis_dir.Z()),
                        radius,
                    })
                } else {
                    CurveDetails::Unknown(curve_type)
                }
            },
            "Geom_Ellipse" => {
                let ellipse = ffi::cast_curve_to_ellipse(&curve);
                if !ellipse.IsNull() {
                    let center = ffi::geom_ellipse_location(&ellipse);
                    let axis = ffi::geom_ellipse_axis(&ellipse);
                    let axis_dir = ffi::gp_Ax1_direction(&axis);
                    let major_radius = ffi::geom_ellipse_major_radius(&ellipse);
                    let minor_radius = ffi::geom_ellipse_minor_radius(&ellipse);

                    CurveDetails::Ellipse(Ellipse {
                        center: dvec3(center.X(), center.Y(), center.Z()),
                        axis: dvec3(axis_dir.X(), axis_dir.Y(), axis_dir.Z()),
                        major_radius,
                        minor_radius,
                    })
                } else {
                    CurveDetails::Unknown(curve_type)
                }
            },
            "Geom_BSplineCurve" => {
                let bspline = ffi::cast_curve_to_bspline_curve(&curve);
                if !bspline.IsNull() {
                    let nb_poles = ffi::geom_bspline_curve_nb_poles(&bspline) as usize;
                    let degree = ffi::geom_bspline_curve_degree(&bspline) as usize;
                    let is_rational = ffi::geom_bspline_curve_is_rational(&bspline);
                    let is_periodic = ffi::geom_bspline_curve_is_periodic(&bspline);

                    // Extract knot vectors
                    let nb_knots = ffi::geom_bspline_curve_nb_knots(&bspline);
                    let mut knots = Vec::with_capacity(nb_knots as usize);
                    let mut multiplicities = Vec::with_capacity(nb_knots as usize);

                    for i in 1..=nb_knots {
                        knots.push(ffi::geom_bspline_curve_knot(&bspline, i));
                        multiplicities.push(ffi::geom_bspline_curve_multiplicity(&bspline, i) as usize);
                    }

                    // Extract poles (control points) and weights
                    let mut poles = Vec::with_capacity(nb_poles);
                    let mut weights = Vec::with_capacity(if is_rational { nb_poles } else { 0 });
                    for i in 1..=nb_poles as i32 {
                        let pole = ffi::geom_bspline_curve_pole(&bspline, i);
                        poles.push(dvec3(pole.X(), pole.Y(), pole.Z()));
                        if is_rational {
                            weights.push(ffi::geom_bspline_curve_weight(&bspline, i));
                        }
                    }

                    CurveDetails::BSplineCurve(BSplineCurve {
                        profile: BSplineCurveProfile {
                            nb_poles,
                            degree,
                            is_rational,
                            is_periodic,
                            knots,
                            multiplicities,
                        },
                        poles,
                        weights,
                    })
                } else {
                    CurveDetails::Unknown(curve_type)
                }
            },
            "Geom_BezierCurve" => {
                let bezier = ffi::cast_curve_to_bezier_curve(&curve);
                if !bezier.IsNull() {
                    let nb_poles = ffi::geom_bezier_curve_nb_poles(&bezier) as usize;
                    let degree = ffi::geom_bezier_curve_degree(&bezier) as usize;

                    // Extract poles (control points) and weights
                    let mut poles = Vec::with_capacity(nb_poles);
                    let mut weights = Vec::with_capacity(nb_poles);
                    for i in 1..=nb_poles as i32 {
                        let pole = ffi::geom_bezier_curve_pole(&bezier, i);
                        poles.push(dvec3(pole.X(), pole.Y(), pole.Z()));
                        weights.push(ffi::geom_bezier_curve_weight(&bezier, i));
                    }

                    CurveDetails::BezierCurve(BezierCurve {
                        profile: BezierCurveProfile { nb_poles, degree },
                        poles,
                        weights,
                    })
                } else {
                    CurveDetails::Unknown(curve_type)
                }
            },
            "Geom_Hyperbola" => {
                let hyperbola = ffi::cast_curve_to_hyperbola(&curve);
                if !hyperbola.IsNull() {
                    let center = ffi::geom_hyperbola_location(&hyperbola);
                    let axis = ffi::geom_hyperbola_axis(&hyperbola);
                    let axis_dir = ffi::gp_Ax1_direction(&axis);
                    let major_radius = ffi::geom_hyperbola_major_radius(&hyperbola);
                    let minor_radius = ffi::geom_hyperbola_minor_radius(&hyperbola);

                    CurveDetails::Hyperbola(Hyperbola {
                        center: dvec3(center.X(), center.Y(), center.Z()),
                        axis: dvec3(axis_dir.X(), axis_dir.Y(), axis_dir.Z()),
                        major_radius,
                        minor_radius,
                    })
                } else {
                    CurveDetails::Unknown(curve_type)
                }
            },
            "Geom_Parabola" => {
                let parabola = ffi::cast_curve_to_parabola(&curve);
                if !parabola.IsNull() {
                    let vertex = ffi::geom_parabola_location(&parabola);
                    let axis = ffi::geom_parabola_axis(&parabola);
                    let axis_dir = ffi::gp_Ax1_direction(&axis);
                    let focal = ffi::geom_parabola_focal(&parabola);

                    CurveDetails::Parabola(Parabola {
                        vertex: dvec3(vertex.X(), vertex.Y(), vertex.Z()),
                        axis: dvec3(axis_dir.X(), axis_dir.Y(), axis_dir.Z()),
                        focal,
                    })
                } else {
                    CurveDetails::Unknown(curve_type)
                }
            },
            "Geom_OffsetCurve" => {
                let offset = ffi::cast_curve_to_offset_curve(&curve);
                if !offset.IsNull() {
                    let basis_curve = ffi::geom_offset_curve_basis_curve(&offset);
                    let offset_value = ffi::geom_offset_curve_offset(&offset);

                    let basis_curve_type = if basis_curve.is_null() {
                        "Unknown".to_string()
                    } else {
                        let dynamic_type = ffi::DynamicTypeCurve(&basis_curve);
                        ffi::type_name(&dynamic_type)
                    };

                    CurveDetails::OffsetCurve(OffsetCurve {
                        basis_curve_type,
                        offset: offset_value,
                    })
                } else {
                    CurveDetails::Unknown(curve_type)
                }
            },
            "Geom_TrimmedCurve" => {
                let trimmed = ffi::cast_curve_to_trimmed_curve(&curve);
                if !trimmed.IsNull() {
                    let basis_curve = ffi::geom_trimmed_curve_basis_curve(&trimmed);
                    let first_parameter = ffi::geom_trimmed_curve_first_parameter(&trimmed);
                    let last_parameter = ffi::geom_trimmed_curve_last_parameter(&trimmed);

                    let basis_curve_type = if basis_curve.is_null() {
                        "Unknown".to_string()
                    } else {
                        let dynamic_type = ffi::DynamicTypeCurve(&basis_curve);
                        ffi::type_name(&dynamic_type)
                    };

                    CurveDetails::TrimmedCurve(TrimmedCurve {
                        basis_curve_type,
                        first_parameter,
                        last_parameter,
                    })
                } else {
                    CurveDetails::Unknown(curve_type)
                }
            },
            _ => CurveDetails::Unknown(curve_type),
        }
    }
}

pub struct ApproximationSegmentIterator {
    count: usize,
    approximator: UniquePtr<ffi::GCPnts_TangentialDeflection>,
}

impl Iterator for ApproximationSegmentIterator {
    type Item = DVec3;

    fn next(&mut self) -> Option<Self::Item> {
        if self.count <= self.approximator.NbPoints() as usize {
            let point =
                ffi::GCPnts_TangentialDeflection_Value(&self.approximator, self.count as i32);

            self.count += 1;
            Some(dvec3(point.X(), point.Y(), point.Z()))
        } else {
            None
        }
    }
}
