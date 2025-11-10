use glam::dvec3;
use opencascade::primitives::{CurveType, Edge};

#[test]
fn test_line_curve_type() {
    let edge = Edge::segment(dvec3(0.0, 0.0, 0.0), dvec3(1.0, 0.0, 0.0));
    let curve_type = edge.curve_type();

    assert_eq!(curve_type, CurveType::Line);
    assert_eq!(curve_type.as_str(), "Geom_Line");
    assert_eq!(curve_type.to_string(), "Geom_Line");
}

#[test]
fn test_circle_curve_type() {
    let edge = Edge::circle(dvec3(0.0, 0.0, 0.0), dvec3(0.0, 0.0, 1.0), 5.0);
    let curve_type = edge.curve_type();

    assert_eq!(curve_type, CurveType::Circle);
    assert_eq!(curve_type.as_str(), "Geom_Circle");
}

#[test]
fn test_bezier_curve_type() {
    let points = vec![dvec3(0.0, 0.0, 0.0), dvec3(1.0, 1.0, 0.0), dvec3(2.0, 0.0, 0.0)];
    let edge = Edge::bezier(points);
    let curve_type = edge.curve_type();

    assert_eq!(curve_type, CurveType::BezierCurve);
    assert_eq!(curve_type.as_str(), "Geom_BezierCurve");
}

#[test]
fn test_bspline_curve_type() {
    let points = vec![
        dvec3(0.0, 0.0, 0.0),
        dvec3(1.0, 1.0, 0.0),
        dvec3(2.0, 1.0, 0.0),
        dvec3(3.0, 0.0, 0.0),
    ];
    let edge = Edge::spline_from_points(points, None);
    let curve_type = edge.curve_type();

    assert_eq!(curve_type, CurveType::BSplineCurve);
    assert_eq!(curve_type.as_str(), "Geom_BSplineCurve");
}

#[test]
fn test_arc_curve_type() {
    // Arc is typically represented as a Circle (trimmed by parameters)
    let edge = Edge::arc(dvec3(0.0, 0.0, 0.0), dvec3(1.0, 1.0, 0.0), dvec3(2.0, 0.0, 0.0));
    let curve_type = edge.curve_type();

    // OpenCASCADE represents arcs as Circle curves
    assert_eq!(curve_type, CurveType::Circle);
    assert_eq!(curve_type.as_str(), "Geom_Circle");
}

#[test]
fn test_curve_type_from_string() {
    assert_eq!(CurveType::from("Geom_Line"), CurveType::Line);
    assert_eq!(CurveType::from("Geom_Circle"), CurveType::Circle);
    assert_eq!(CurveType::from("Geom_Ellipse"), CurveType::Ellipse);
    assert_eq!(CurveType::from("Geom_Hyperbola"), CurveType::Hyperbola);
    assert_eq!(CurveType::from("Geom_Parabola"), CurveType::Parabola);
    assert_eq!(CurveType::from("Geom_BezierCurve"), CurveType::BezierCurve);
    assert_eq!(CurveType::from("Geom_BSplineCurve"), CurveType::BSplineCurve);
    assert_eq!(CurveType::from("Geom_OffsetCurve"), CurveType::OffsetCurve);
    assert_eq!(CurveType::from("Geom_TrimmedCurve"), CurveType::TrimmedCurve);

    // Unknown type
    let unknown = CurveType::from("Geom_SomeUnknownType");
    match unknown {
        CurveType::Unknown(ref s) => assert_eq!(s, "Geom_SomeUnknownType"),
        _ => panic!("Expected Unknown variant"),
    }
}

#[test]
fn test_curve_type_pattern_matching() {
    let line_edge = Edge::segment(dvec3(0.0, 0.0, 0.0), dvec3(1.0, 0.0, 0.0));
    let curve_type = line_edge.curve_type();

    match curve_type {
        CurveType::Line => {
            // Expected
        },
        _ => panic!("Expected CurveType::Line"),
    }

    let circle_edge = Edge::circle(dvec3(0.0, 0.0, 0.0), dvec3(0.0, 0.0, 1.0), 5.0);
    let curve_type = circle_edge.curve_type();

    match curve_type {
        CurveType::Circle => {
            // Expected
        },
        _ => panic!("Expected CurveType::Circle"),
    }
}

#[test]
fn test_curve_type_display() {
    let edge = Edge::segment(dvec3(0.0, 0.0, 0.0), dvec3(1.0, 0.0, 0.0));
    let curve_type = edge.curve_type();

    let display_str = format!("{}", curve_type);
    assert_eq!(display_str, "Geom_Line");
}
