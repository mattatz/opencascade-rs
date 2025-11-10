use glam::dvec3;
use opencascade::primitives::{Face, Shape, SurfaceType, Wire};

#[test]
fn test_plane_surface_type() {
    let wire = Wire::rect(10.0, 5.0);
    let face = Face::from_wire(&wire);
    let surface_type = face.surface_type();

    assert_eq!(surface_type, SurfaceType::Plane);
    assert_eq!(surface_type.as_str(), "Geom_Plane");
    assert_eq!(surface_type.to_string(), "Geom_Plane");
}

#[test]
fn test_cylinder_surface_type() {
    let shape = Shape::cylinder(dvec3(0.0, 0.0, 0.0), 5.0, dvec3(0.0, 0.0, 1.0), 10.0);

    // Find a cylindrical face
    let faces: Vec<_> = shape.faces().collect();
    let cylindrical_faces: Vec<_> =
        faces.iter().filter(|f| f.surface_type() == SurfaceType::CylindricalSurface).collect();

    assert!(!cylindrical_faces.is_empty(), "Should have at least one cylindrical face");

    let surface_type = cylindrical_faces[0].surface_type();
    assert_eq!(surface_type, SurfaceType::CylindricalSurface);
    assert_eq!(surface_type.as_str(), "Geom_CylindricalSurface");
}

#[test]
fn test_cone_surface_type() {
    let shape = Shape::cone()
        .at(dvec3(0.0, 0.0, 0.0))
        .height(10.0)
        .bottom_radius(5.0)
        .top_radius(0.0)
        .build();

    // Find a conical face
    let faces: Vec<_> = shape.faces().collect();
    let conical_faces: Vec<_> =
        faces.iter().filter(|f| f.surface_type() == SurfaceType::ConicalSurface).collect();

    assert!(!conical_faces.is_empty(), "Should have at least one conical face");

    let surface_type = conical_faces[0].surface_type();
    assert_eq!(surface_type, SurfaceType::ConicalSurface);
    assert_eq!(surface_type.as_str(), "Geom_ConicalSurface");
}

#[test]
fn test_sphere_surface_type() {
    let shape = Shape::sphere(5.0).at(dvec3(0.0, 0.0, 0.0)).build();

    // Find a spherical face
    let faces: Vec<_> = shape.faces().collect();
    assert!(!faces.is_empty(), "Sphere should have faces");

    let surface_type = faces[0].surface_type();
    assert_eq!(surface_type, SurfaceType::SphericalSurface);
    assert_eq!(surface_type.as_str(), "Geom_SphericalSurface");
}

#[test]
fn test_torus_surface_type() {
    let shape = Shape::torus().at(dvec3(0.0, 0.0, 0.0)).radius_1(10.0).radius_2(2.0).build();

    // Find a toroidal face
    let faces: Vec<_> = shape.faces().collect();
    assert!(!faces.is_empty(), "Torus should have faces");

    let surface_type = faces[0].surface_type();
    assert_eq!(surface_type, SurfaceType::ToroidalSurface);
    assert_eq!(surface_type.as_str(), "Geom_ToroidalSurface");
}

#[test]
fn test_surface_type_from_string() {
    assert_eq!(SurfaceType::from("Geom_Plane"), SurfaceType::Plane);
    assert_eq!(SurfaceType::from("Geom_CylindricalSurface"), SurfaceType::CylindricalSurface);
    assert_eq!(SurfaceType::from("Geom_ConicalSurface"), SurfaceType::ConicalSurface);
    assert_eq!(SurfaceType::from("Geom_SphericalSurface"), SurfaceType::SphericalSurface);
    assert_eq!(SurfaceType::from("Geom_ToroidalSurface"), SurfaceType::ToroidalSurface);
    assert_eq!(SurfaceType::from("Geom_BSplineSurface"), SurfaceType::BSplineSurface);
    assert_eq!(SurfaceType::from("Geom_BezierSurface"), SurfaceType::BezierSurface);

    // Unknown type
    let unknown = SurfaceType::from("Geom_SomeUnknownSurface");
    match unknown {
        SurfaceType::Unknown(ref s) => assert_eq!(s, "Geom_SomeUnknownSurface"),
        _ => panic!("Expected Unknown variant"),
    }
}

#[test]
fn test_surface_type_pattern_matching() {
    let wire = Wire::rect(10.0, 5.0);
    let face = Face::from_wire(&wire);
    let surface_type = face.surface_type();

    match surface_type {
        SurfaceType::Plane => {
            // Expected
        },
        _ => panic!("Expected SurfaceType::Plane"),
    }

    let sphere = Shape::sphere(5.0).at(dvec3(0.0, 0.0, 0.0)).build();
    let faces: Vec<_> = sphere.faces().collect();
    let surface_type = faces[0].surface_type();

    match surface_type {
        SurfaceType::SphericalSurface => {
            // Expected
        },
        _ => panic!("Expected SurfaceType::SphericalSurface"),
    }
}

#[test]
fn test_surface_type_display() {
    let wire = Wire::rect(10.0, 5.0);
    let face = Face::from_wire(&wire);
    let surface_type = face.surface_type();

    let display_str = format!("{}", surface_type);
    assert_eq!(display_str, "Geom_Plane");
}
