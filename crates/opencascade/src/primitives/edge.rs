use crate::primitives::{make_axis_2, make_point, Face};
use cxx::UniquePtr;
use glam::{dvec2, dvec3, DVec2, DVec3};
use occt_interop::*;
use opencascade_sys::ffi;
use serde::{Deserialize, Serialize};

use super::make_vec;

/// A 2D parametric curve on a face's UV surface
pub struct ParametricCurve2d {
    curve: UniquePtr<ffi::HandleGeom2d_Curve>,
    /// First parameter of the curve
    pub first: f64,
    /// Last parameter of the curve
    pub last: f64,
}

impl ParametricCurve2d {
    /// Get a 2D point on the curve at parameter u
    pub fn value(&self, u: f64) -> Option<DVec2> {
        if ffi::Geom2d_Curve_IsNull(&self.curve) {
            return None;
        }
        let point = ffi::Geom2d_Curve_Value(&self.curve, u);
        Some(dvec2(point.X(), point.Y()))
    }

    /// Check if the curve is valid
    pub fn is_valid(&self) -> bool {
        !ffi::Geom2d_Curve_IsNull(&self.curve)
    }

    /// Get detailed information about the 2D curve's geometric properties
    pub fn curve_details(&self) -> Curve2dDetails {
        if !self.is_valid() {
            return Curve2dDetails::Unknown("Null curve".to_string());
        }

        let curve_type = ffi::Geom2d_Curve_DynamicType(&self.curve);

        match curve_type.as_str() {
            "Geom2d_Line" => {
                let line = ffi::cast_geom2d_curve_to_line(&self.curve);
                if !ffi::HandleGeom2d_Line_IsNull(&line) {
                    let location = ffi::geom2d_line_location(&line);
                    let direction = ffi::geom2d_line_direction(&line);

                    Curve2dDetails::Line(Line2d {
                        origin: dvec2(location.X(), location.Y()).into(),
                        direction: dvec2(ffi::gp_Dir2d_X(&direction), ffi::gp_Dir2d_Y(&direction))
                            .into(),
                        first_parameter: self.first,
                        last_parameter: self.last,
                    })
                } else {
                    Curve2dDetails::Unknown(curve_type)
                }
            },
            "Geom2d_Circle" => {
                let circle = ffi::cast_geom2d_curve_to_circle(&self.curve);
                if !ffi::HandleGeom2d_Circle_IsNull(&circle) {
                    let location = ffi::geom2d_circle_location(&circle);
                    let x_dir = ffi::geom2d_circle_x_direction(&circle);
                    let y_dir = ffi::geom2d_circle_y_direction(&circle);
                    let radius = ffi::geom2d_circle_radius(&circle);

                    Curve2dDetails::Circle(Circle2d {
                        center: dvec2(location.X(), location.Y()).into(),
                        x_direction: dvec2(ffi::gp_Dir2d_X(&x_dir), ffi::gp_Dir2d_Y(&x_dir)).into(),
                        y_direction: dvec2(ffi::gp_Dir2d_X(&y_dir), ffi::gp_Dir2d_Y(&y_dir)).into(),
                        radius,
                        first_parameter: self.first,
                        last_parameter: self.last,
                    })
                } else {
                    Curve2dDetails::Unknown(curve_type)
                }
            },
            "Geom2d_Ellipse" => {
                let ellipse = ffi::cast_geom2d_curve_to_ellipse(&self.curve);
                if !ffi::HandleGeom2d_Ellipse_IsNull(&ellipse) {
                    let location = ffi::geom2d_ellipse_location(&ellipse);
                    let x_dir = ffi::geom2d_ellipse_x_direction(&ellipse);
                    let y_dir = ffi::geom2d_ellipse_y_direction(&ellipse);
                    let major_radius = ffi::geom2d_ellipse_major_radius(&ellipse);
                    let minor_radius = ffi::geom2d_ellipse_minor_radius(&ellipse);

                    Curve2dDetails::Ellipse(Ellipse2d {
                        center: dvec2(location.X(), location.Y()).into(),
                        x_direction: dvec2(ffi::gp_Dir2d_X(&x_dir), ffi::gp_Dir2d_Y(&x_dir)).into(),
                        y_direction: dvec2(ffi::gp_Dir2d_X(&y_dir), ffi::gp_Dir2d_Y(&y_dir)).into(),
                        major_radius,
                        minor_radius,
                        first_parameter: self.first,
                        last_parameter: self.last,
                    })
                } else {
                    Curve2dDetails::Unknown(curve_type)
                }
            },
            "Geom2d_BSplineCurve" => {
                let mut bspline = ffi::cast_geom2d_curve_to_bspline(&self.curve);
                if !ffi::HandleGeom2d_BSplineCurve_IsNull(&bspline) {
                    // Periodic curves are unfolded to a non-periodic equivalent here so
                    // downstream consumers always receive a standard non-periodic NURBS
                    // representation. OCC's `SetNotPeriodic` (BSplCLib::Unperiodize) is
                    // the authoritative algorithm for this.
                    if ffi::geom2d_bspline_curve_is_periodic(&bspline) {
                        ffi::geom2d_bspline_curve_set_not_periodic(bspline.pin_mut());
                    }

                    let nb_poles = ffi::geom2d_bspline_curve_nb_poles(&bspline) as u32;
                    let degree = ffi::geom2d_bspline_curve_degree(&bspline) as u32;
                    let is_rational = ffi::geom2d_bspline_curve_is_rational(&bspline);
                    let is_periodic = ffi::geom2d_bspline_curve_is_periodic(&bspline);

                    // Extract knot vectors
                    let nb_knots = ffi::geom2d_bspline_curve_nb_knots(&bspline);
                    let mut knots = Vec::with_capacity(nb_knots as usize);
                    let mut multiplicities = Vec::with_capacity(nb_knots as usize);

                    for i in 1..=nb_knots {
                        knots.push(ffi::geom2d_bspline_curve_knot(&bspline, i));
                        multiplicities
                            .push(ffi::geom2d_bspline_curve_multiplicity(&bspline, i) as u32);
                    }

                    // Extract poles (control points) and weights
                    let poles = (1..=nb_poles as i32)
                        .map(|i| {
                            let pole = ffi::geom2d_bspline_curve_pole(&bspline, i);
                            dvec2(pole.X(), pole.Y()).into()
                        })
                        .collect();

                    let weights = if is_rational {
                        Some(
                            (1..=nb_poles as i32)
                                .map(|i| ffi::geom2d_bspline_curve_weight(&bspline, i))
                                .collect(),
                        )
                    } else {
                        None
                    };

                    Curve2dDetails::BSplineCurve(BSplineCurve2d {
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
                        first_parameter: self.first,
                        last_parameter: self.last,
                    })
                } else {
                    Curve2dDetails::Unknown(curve_type)
                }
            },
            "Geom2d_BezierCurve" => {
                let bezier = ffi::cast_geom2d_curve_to_bezier(&self.curve);
                if !ffi::HandleGeom2d_BezierCurve_IsNull(&bezier) {
                    let nb_poles = ffi::geom2d_bezier_curve_nb_poles(&bezier) as u32;
                    let degree = ffi::geom2d_bezier_curve_degree(&bezier) as u32;
                    let is_rational = ffi::geom2d_bezier_curve_is_rational(&bezier);

                    // Extract poles (control points) and weights
                    let poles = (1..=nb_poles as i32)
                        .map(|i| {
                            let pole = ffi::geom2d_bezier_curve_pole(&bezier, i);
                            dvec2(pole.X(), pole.Y()).into()
                        })
                        .collect();

                    let weights = if is_rational {
                        Some(
                            (1..=nb_poles as i32)
                                .map(|i| ffi::geom2d_bezier_curve_weight(&bezier, i))
                                .collect(),
                        )
                    } else {
                        None
                    };

                    Curve2dDetails::BezierCurve(BezierCurve2d {
                        profile: BezierCurveProfile { nb_poles, degree },
                        poles,
                        weights,
                        first_parameter: self.first,
                        last_parameter: self.last,
                    })
                } else {
                    Curve2dDetails::Unknown(curve_type)
                }
            },
            "Geom2d_TrimmedCurve" => {
                let trimmed = ffi::cast_geom2d_curve_to_trimmed(&self.curve);
                if !ffi::HandleGeom2d_TrimmedCurve_IsNull(&trimmed) {
                    let basis_curve_handle = ffi::geom2d_trimmed_curve_basis_curve(&trimmed);
                    let first_param = ffi::geom2d_trimmed_curve_first_parameter(&trimmed);
                    let last_param = ffi::geom2d_trimmed_curve_last_parameter(&trimmed);

                    // Recursively get the basis curve details
                    let basis_curve_temp = ParametricCurve2d {
                        curve: basis_curve_handle,
                        first: first_param,
                        last: last_param,
                    };
                    let basis_details = basis_curve_temp.curve_details();

                    Curve2dDetails::TrimmedCurve(TrimmedCurve2d {
                        basis_curve: Box::new(basis_details),
                        first_parameter: first_param,
                        last_parameter: last_param,
                    })
                } else {
                    Curve2dDetails::Unknown(curve_type)
                }
            },
            _ => Curve2dDetails::Unknown(curve_type),
        }
    }
}

/// The type of curve based on OpenCASCADE's GeomAbs classification
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

/// The specific geometric curve type (e.g., Geom_Line, Geom_Circle)
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum CurveType {
    /// Geom_Line - A line curve
    Line,
    /// Geom_Circle - A circular curve
    Circle,
    /// Geom_Ellipse - An elliptical curve
    Ellipse,
    /// Geom_Hyperbola - A hyperbolic curve
    Hyperbola,
    /// Geom_Parabola - A parabolic curve
    Parabola,
    /// Geom_BezierCurve - A Bezier curve
    BezierCurve,
    /// Geom_BSplineCurve - A B-Spline curve
    BSplineCurve,
    /// Geom_OffsetCurve - An offset curve
    OffsetCurve,
    /// Geom_TrimmedCurve - A trimmed curve
    TrimmedCurve,
    /// Unknown or unsupported curve type
    Unknown(String),
}

impl CurveType {
    /// Get the OpenCASCADE type name as a string (e.g., "Geom_Line")
    pub fn as_str(&self) -> &str {
        match self {
            Self::Line => "Geom_Line",
            Self::Circle => "Geom_Circle",
            Self::Ellipse => "Geom_Ellipse",
            Self::Hyperbola => "Geom_Hyperbola",
            Self::Parabola => "Geom_Parabola",
            Self::BezierCurve => "Geom_BezierCurve",
            Self::BSplineCurve => "Geom_BSplineCurve",
            Self::OffsetCurve => "Geom_OffsetCurve",
            Self::TrimmedCurve => "Geom_TrimmedCurve",
            Self::Unknown(s) => s.as_str(),
        }
    }
}

impl From<String> for CurveType {
    fn from(type_name: String) -> Self {
        match type_name.as_str() {
            "Geom_Line" => Self::Line,
            "Geom_Circle" => Self::Circle,
            "Geom_Ellipse" => Self::Ellipse,
            "Geom_Hyperbola" => Self::Hyperbola,
            "Geom_Parabola" => Self::Parabola,
            "Geom_BezierCurve" => Self::BezierCurve,
            "Geom_BSplineCurve" => Self::BSplineCurve,
            "Geom_OffsetCurve" => Self::OffsetCurve,
            "Geom_TrimmedCurve" => Self::TrimmedCurve,
            _ => Self::Unknown(type_name),
        }
    }
}

impl From<&str> for CurveType {
    fn from(type_name: &str) -> Self {
        Self::from(type_name.to_string())
    }
}

impl std::fmt::Display for CurveType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
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
    pub inner: UniquePtr<ffi::TopoDS_Edge>,
}

impl std::fmt::Debug for Edge {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Edge({:?}: {:?})", self.edge_type(), self.inner.as_ptr())
    }
}

impl AsRef<Edge> for Edge {
    fn as_ref(&self) -> &Edge {
        self
    }
}

impl Edge {
    pub fn unique_ptr(&self) -> &UniquePtr<ffi::TopoDS_Edge> {
        &self.inner
    }

    pub fn from_edge(edge: &ffi::TopoDS_Edge) -> Self {
        let inner = ffi::TopoDS_Edge_to_owned(edge);

        Self { inner }
    }

    /// Get a unique ID for this edge based on its underlying TShape.
    /// Edges that share the same geometric entity will have the same ID.
    /// This is useful for identifying edge connectivity in BREP reconstruction.
    pub fn id(&self) -> usize {
        let shape = ffi::cast_edge_to_shape(&self.inner);
        ffi::TopoDS_Shape_get_tshape_id(shape)
    }

    pub fn from_make_edge(mut make_edge: UniquePtr<ffi::BRepBuilderAPI_MakeEdge>) -> Self {
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

    pub fn point_at(&self, t: f64) -> DVec3 {
        let curve = ffi::BRepAdaptor_Curve_ctor(&self.inner);
        let point = ffi::BRepAdaptor_Curve_value(&curve, t);
        dvec3(point.X(), point.Y(), point.Z())
    }

    pub fn tangent_at(&self, t: f64) -> DVec3 {
        let curve = ffi::BRepAdaptor_Curve_ctor(&self.inner);
        let mut p = ffi::new_point(0.0, 0.0, 0.0);
        let mut v1 = ffi::new_vec(0.0, 0.0, 0.0);
        ffi::BRepAdaptor_Curve_D1(&curve, t, p.pin_mut(), v1.pin_mut());
        dvec3(v1.X(), v1.Y(), v1.Z())
    }

    pub fn param_range(&self) -> (f64, f64) {
        let curve = ffi::BRepAdaptor_Curve_ctor(&self.inner);
        (curve.FirstParameter(), curve.LastParameter())
    }

    pub fn curve_length(&self) -> f64 {
        let curve = ffi::BRepAdaptor_Curve_ctor(&self.inner);
        ffi::BRepAdaptor_Curve_length(&curve)
    }

    pub fn is_closed(&self) -> bool {
        let curve = ffi::BRepAdaptor_Curve_ctor(&self.inner);
        ffi::BRepAdaptor_Curve_is_closed(&curve)
    }

    /// Convert the underlying curve to a BSplineCurve, returning its details.
    /// Works for any curve type (Line, Circle, Ellipse, etc.) via GeomConvert.
    pub fn to_bspline_curve_details(&self) -> Option<BSplineCurve> {
        let mut first = 0.0_f64;
        let mut last = 0.0_f64;
        let curve_handle = ffi::BRep_Tool_Curve(&self.inner, &mut first, &mut last);
        if curve_handle.IsNull() {
            return None;
        }

        let mut bspline_handle = ffi::GeomConvert_CurveToBSplineCurve(&curve_handle);
        if bspline_handle.IsNull() {
            return None;
        }

        if ffi::geom_bspline_curve_is_periodic(&bspline_handle) {
            ffi::geom_bspline_curve_set_not_periodic(bspline_handle.pin_mut());
        }

        let nb_poles = ffi::geom_bspline_curve_nb_poles(&bspline_handle) as u32;
        let degree = ffi::geom_bspline_curve_degree(&bspline_handle) as u32;
        let is_rational = ffi::geom_bspline_curve_is_rational(&bspline_handle);
        let is_periodic = ffi::geom_bspline_curve_is_periodic(&bspline_handle);

        let nb_knots = ffi::geom_bspline_curve_nb_knots(&bspline_handle);
        let mut knots = Vec::with_capacity(nb_knots as usize);
        let mut multiplicities = Vec::with_capacity(nb_knots as usize);
        for i in 1..=nb_knots {
            knots.push(ffi::geom_bspline_curve_knot(&bspline_handle, i));
            multiplicities.push(ffi::geom_bspline_curve_multiplicity(&bspline_handle, i) as u32);
        }

        let poles = (1..=nb_poles as i32)
            .map(|i| {
                let pole = ffi::geom_bspline_curve_pole(&bspline_handle, i);
                dvec3(pole.X(), pole.Y(), pole.Z()).into()
            })
            .collect();
        let weights = if is_rational {
            Some(
                (1..=nb_poles as i32)
                    .map(|i| ffi::geom_bspline_curve_weight(&bspline_handle, i))
                    .collect(),
            )
        } else {
            None
        };

        Some(BSplineCurve {
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
            first_parameter: first,
            last_parameter: last,
        })
    }

    pub fn find_closest_point(&self, point: DVec3) -> Option<(DVec3, f64)> {
        self.find_closest_point_xyz(point.x, point.y, point.z)
            .map(|((x, y, z), t)| (dvec3(x, y, z), t))
    }

    pub fn find_closest_point_xyz(&self, x: f64, y: f64, z: f64) -> Option<((f64, f64, f64), f64)> {
        let mut first = 0.0_f64;
        let mut last = 0.0_f64;
        let curve_handle = ffi::BRep_Tool_Curve(&self.inner, &mut first, &mut last);
        if curve_handle.IsNull() {
            return None;
        }
        let p = ffi::new_point(x, y, z);
        let proj = ffi::GeomAPI_ProjectPointOnCurve_ctor(&p, &curve_handle);
        if ffi::GeomAPI_ProjectPointOnCurve_NbPoints(&proj) == 0 {
            return None;
        }
        let nearest = ffi::GeomAPI_ProjectPointOnCurve_NearestPoint(&proj);
        let param = ffi::GeomAPI_ProjectPointOnCurve_LowerDistanceParameter(&proj);
        Some(((nearest.X(), nearest.Y(), nearest.Z()), param))
    }

    pub fn trimmed(&self, u1: f64, u2: f64) -> Option<Edge> {
        let mut first = 0.0_f64;
        let mut last = 0.0_f64;
        let curve_handle = ffi::BRep_Tool_Curve(&self.inner, &mut first, &mut last);
        if curve_handle.IsNull() {
            return None;
        }
        let trimmed = ffi::Geom_TrimmedCurve_ctor(&curve_handle, u1, u2);
        if trimmed.IsNull() {
            return None;
        }
        let geom_curve = ffi::new_HandleGeomCurve_from_HandleGeom_TrimmedCurve(&trimmed);
        let make_edge = ffi::BRepBuilderAPI_MakeEdge_HandleGeomCurve(&geom_curve);
        Some(Edge::from_make_edge(make_edge))
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

    /// Get the orientation of this edge (Forward, Reversed, Internal, External)
    pub fn orientation(&self) -> Orientation {
        let shape = ffi::cast_edge_to_shape(&self.inner);
        let orientation = shape.Orientation();
        match orientation {
            ffi::TopAbs_Orientation::TopAbs_FORWARD => Orientation::Forward,
            ffi::TopAbs_Orientation::TopAbs_REVERSED => Orientation::Reversed,
            ffi::TopAbs_Orientation::TopAbs_INTERNAL => Orientation::Internal,
            ffi::TopAbs_Orientation::TopAbs_EXTERNAL => Orientation::External,
            ffi::TopAbs_Orientation { repr } => panic!("Unexpected orientation: {repr}"),
        }
    }

    /// Get the type of the underlying curve (e.g., CurveType::Line, CurveType::Circle)
    pub fn curve_type(&self) -> CurveType {
        let mut first = 0.0;
        let mut last = 0.0;
        let curve = ffi::BRep_Tool_Curve(&self.inner, &mut first, &mut last);

        if curve.IsNull() {
            return CurveType::Unknown("Null curve".to_string());
        }

        let dynamic_type = ffi::DynamicTypeCurve(&curve);
        let type_name = ffi::type_name(&dynamic_type);
        CurveType::from(type_name)
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

        match curve_type {
            CurveType::Line => {
                let line = ffi::cast_curve_to_line(&curve);
                if !line.IsNull() {
                    let position = ffi::geom_line_position(&line);
                    let origin = ffi::gp_Ax1_location(&position);
                    let direction = ffi::gp_Ax1_direction(&position);

                    CurveDetails::Line(Line {
                        origin: dvec3(origin.X(), origin.Y(), origin.Z()).into(),
                        direction: dvec3(direction.X(), direction.Y(), direction.Z()).into(),
                        first_parameter: first,
                        last_parameter: last,
                    })
                } else {
                    CurveDetails::Unknown(curve_type.to_string())
                }
            },
            CurveType::Circle => {
                let circle = ffi::cast_curve_to_circle(&curve);
                if !circle.IsNull() {
                    let center = ffi::geom_circle_location(&circle);
                    let axis = ffi::geom_circle_axis(&circle);
                    let axis_dir = ffi::gp_Ax1_direction(&axis);
                    let x_dir = ffi::geom_circle_x_direction(&circle);
                    let y_dir = ffi::geom_circle_y_direction(&circle);
                    let radius = ffi::geom_circle_radius(&circle);

                    CurveDetails::Circle(Circle {
                        center: dvec3(center.X(), center.Y(), center.Z()).into(),
                        axis: dvec3(axis_dir.X(), axis_dir.Y(), axis_dir.Z()).into(),
                        x_axis: dvec3(x_dir.X(), x_dir.Y(), x_dir.Z()).into(),
                        y_axis: dvec3(y_dir.X(), y_dir.Y(), y_dir.Z()).into(),
                        radius,
                        first_parameter: first,
                        last_parameter: last,
                    })
                } else {
                    CurveDetails::Unknown(curve_type.to_string())
                }
            },
            CurveType::Ellipse => {
                let ellipse = ffi::cast_curve_to_ellipse(&curve);
                if !ellipse.IsNull() {
                    let center = ffi::geom_ellipse_location(&ellipse);
                    let axis = ffi::geom_ellipse_axis(&ellipse);
                    let axis_dir = ffi::gp_Ax1_direction(&axis);
                    let x_dir = ffi::geom_ellipse_x_direction(&ellipse);
                    let y_dir = ffi::geom_ellipse_y_direction(&ellipse);
                    let major_radius = ffi::geom_ellipse_major_radius(&ellipse);
                    let minor_radius = ffi::geom_ellipse_minor_radius(&ellipse);

                    CurveDetails::Ellipse(Ellipse {
                        center: dvec3(center.X(), center.Y(), center.Z()).into(),
                        axis: dvec3(axis_dir.X(), axis_dir.Y(), axis_dir.Z()).into(),
                        x_axis: dvec3(x_dir.X(), x_dir.Y(), x_dir.Z()).into(),
                        y_axis: dvec3(y_dir.X(), y_dir.Y(), y_dir.Z()).into(),
                        major_radius,
                        minor_radius,
                        first_parameter: first,
                        last_parameter: last,
                    })
                } else {
                    CurveDetails::Unknown(curve_type.to_string())
                }
            },
            CurveType::BSplineCurve => {
                let mut bspline = ffi::cast_curve_to_bspline_curve(&curve);
                if !bspline.IsNull() {
                    // Periodic curves are unfolded to a non-periodic equivalent here so
                    // downstream consumers always receive a standard non-periodic NURBS
                    // representation. OCC's `SetNotPeriodic` (BSplCLib::Unperiodize) is
                    // the authoritative algorithm for this.
                    if ffi::geom_bspline_curve_is_periodic(&bspline) {
                        ffi::geom_bspline_curve_set_not_periodic(bspline.pin_mut());
                    }

                    let nb_poles = ffi::geom_bspline_curve_nb_poles(&bspline) as u32;
                    let degree = ffi::geom_bspline_curve_degree(&bspline) as u32;
                    let is_rational = ffi::geom_bspline_curve_is_rational(&bspline);
                    let is_periodic = ffi::geom_bspline_curve_is_periodic(&bspline);

                    // Extract knot vectors
                    let nb_knots = ffi::geom_bspline_curve_nb_knots(&bspline);
                    let mut knots = Vec::with_capacity(nb_knots as usize);
                    let mut multiplicities = Vec::with_capacity(nb_knots as usize);

                    for i in 1..=nb_knots {
                        knots.push(ffi::geom_bspline_curve_knot(&bspline, i));
                        multiplicities
                            .push(ffi::geom_bspline_curve_multiplicity(&bspline, i) as u32);
                    }

                    // Extract poles (control points) and weights
                    let poles = (1..=nb_poles as i32)
                        .map(|i| {
                            let pole = ffi::geom_bspline_curve_pole(&bspline, i);
                            dvec3(pole.X(), pole.Y(), pole.Z()).into()
                        })
                        .collect();
                    let weights = if is_rational {
                        Some(
                            (1..=nb_poles as i32)
                                .map(|i| ffi::geom_bspline_curve_weight(&bspline, i))
                                .collect(),
                        )
                    } else {
                        None
                    };

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
                        first_parameter: first,
                        last_parameter: last,
                    })
                } else {
                    CurveDetails::Unknown(curve_type.to_string())
                }
            },
            CurveType::BezierCurve => {
                let bezier = ffi::cast_curve_to_bezier_curve(&curve);
                if !bezier.IsNull() {
                    let nb_poles = ffi::geom_bezier_curve_nb_poles(&bezier) as u32;
                    let degree = ffi::geom_bezier_curve_degree(&bezier) as u32;
                    let is_rational = ffi::geom_bezier_curve_is_rational(&bezier);

                    // Extract poles (control points) and weights
                    let poles = (1..=nb_poles as i32)
                        .map(|i| {
                            let pole = ffi::geom_bezier_curve_pole(&bezier, i);
                            dvec3(pole.X(), pole.Y(), pole.Z()).into()
                        })
                        .collect();

                    let weights = if is_rational {
                        Some(
                            (1..=nb_poles as i32)
                                .map(|i| ffi::geom_bezier_curve_weight(&bezier, i))
                                .collect(),
                        )
                    } else {
                        None
                    };

                    CurveDetails::BezierCurve(BezierCurve {
                        profile: BezierCurveProfile { nb_poles, degree },
                        poles,
                        weights,
                        first_parameter: first,
                        last_parameter: last,
                    })
                } else {
                    CurveDetails::Unknown(curve_type.to_string())
                }
            },
            CurveType::Hyperbola => {
                let hyperbola = ffi::cast_curve_to_hyperbola(&curve);
                if !hyperbola.IsNull() {
                    let center = ffi::geom_hyperbola_location(&hyperbola);
                    let axis = ffi::geom_hyperbola_axis(&hyperbola);
                    let axis_dir = ffi::gp_Ax1_direction(&axis);
                    let major_radius = ffi::geom_hyperbola_major_radius(&hyperbola);
                    let minor_radius = ffi::geom_hyperbola_minor_radius(&hyperbola);

                    CurveDetails::Hyperbola(Hyperbola {
                        center: dvec3(center.X(), center.Y(), center.Z()).into(),
                        axis: dvec3(axis_dir.X(), axis_dir.Y(), axis_dir.Z()).into(),
                        major_radius,
                        minor_radius,
                    })
                } else {
                    CurveDetails::Unknown(curve_type.to_string())
                }
            },
            CurveType::Parabola => {
                let parabola = ffi::cast_curve_to_parabola(&curve);
                if !parabola.IsNull() {
                    let vertex = ffi::geom_parabola_location(&parabola);
                    let axis = ffi::geom_parabola_axis(&parabola);
                    let axis_dir = ffi::gp_Ax1_direction(&axis);
                    let focal = ffi::geom_parabola_focal(&parabola);

                    CurveDetails::Parabola(Parabola {
                        vertex: dvec3(vertex.X(), vertex.Y(), vertex.Z()).into(),
                        axis: dvec3(axis_dir.X(), axis_dir.Y(), axis_dir.Z()).into(),
                        focal,
                    })
                } else {
                    CurveDetails::Unknown(curve_type.to_string())
                }
            },
            CurveType::OffsetCurve => {
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
                    CurveDetails::Unknown(curve_type.to_string())
                }
            },
            CurveType::TrimmedCurve => {
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
                    CurveDetails::Unknown(curve_type.to_string())
                }
            },
            CurveType::Unknown(type_name) => CurveDetails::Unknown(type_name),
        }
    }

    /// Get the 2D parametric curve of this edge on the given face's UV surface
    /// Returns None if the edge does not lie on the face
    pub fn curve_on_surface(&self, face: &Face) -> Option<ParametricCurve2d> {
        let mut first = 0.0;
        let mut last = 0.0;
        let curve = ffi::BRep_Tool_CurveOnSurface(&self.inner, &face.inner, &mut first, &mut last);

        if ffi::Geom2d_Curve_IsNull(&curve) {
            return None;
        }

        Some(ParametricCurve2d { curve, first, last })
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
