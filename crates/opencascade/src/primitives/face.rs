use crate::{
    angle::Angle,
    law_function::law_function_from_graph,
    make_pipe_shell::make_pipe_shell_with_law_function,
    primitives::{
        make_axis_1, make_point, make_vec, EdgeIterator, JoinType, Shape, Solid, Surface, Wire,
        WireIterator, WireWithRoleIterator,
    },
    workplane::Workplane,
};
use cxx::UniquePtr;
use glam::{dvec3, DVec3};
use interop::*;
use opencascade_sys::ffi;
use serde::{Deserialize, Serialize};

/// The specific geometric surface type (e.g., Geom_Plane, Geom_CylindricalSurface)
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum SurfaceType {
    /// Geom_Plane - A planar surface
    Plane,
    /// Geom_CylindricalSurface - A cylindrical surface
    CylindricalSurface,
    /// Geom_ConicalSurface - A conical surface
    ConicalSurface,
    /// Geom_SphericalSurface - A spherical surface
    SphericalSurface,
    /// Geom_ToroidalSurface - A toroidal surface
    ToroidalSurface,
    /// Geom_BSplineSurface - A B-Spline surface
    BSplineSurface,
    /// Geom_BezierSurface - A Bezier surface
    BezierSurface,
    /// Unknown or unsupported surface type
    Unknown(String),
}

impl SurfaceType {
    /// Get the OpenCASCADE type name as a string (e.g., "Geom_Plane")
    pub fn as_str(&self) -> &str {
        match self {
            Self::Plane => "Geom_Plane",
            Self::CylindricalSurface => "Geom_CylindricalSurface",
            Self::ConicalSurface => "Geom_ConicalSurface",
            Self::SphericalSurface => "Geom_SphericalSurface",
            Self::ToroidalSurface => "Geom_ToroidalSurface",
            Self::BSplineSurface => "Geom_BSplineSurface",
            Self::BezierSurface => "Geom_BezierSurface",
            Self::Unknown(s) => s.as_str(),
        }
    }
}

impl From<String> for SurfaceType {
    fn from(type_name: String) -> Self {
        match type_name.as_str() {
            "Geom_Plane" => Self::Plane,
            "Geom_CylindricalSurface" => Self::CylindricalSurface,
            "Geom_ConicalSurface" => Self::ConicalSurface,
            "Geom_SphericalSurface" => Self::SphericalSurface,
            "Geom_ToroidalSurface" => Self::ToroidalSurface,
            "Geom_BSplineSurface" => Self::BSplineSurface,
            "Geom_BezierSurface" => Self::BezierSurface,
            _ => Self::Unknown(type_name),
        }
    }
}

impl From<&str> for SurfaceType {
    fn from(type_name: &str) -> Self {
        Self::from(type_name.to_string())
    }
}

impl std::fmt::Display for SurfaceType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

pub struct Face {
    pub(crate) inner: UniquePtr<ffi::TopoDS_Face>,
}

impl AsRef<Face> for Face {
    fn as_ref(&self) -> &Face {
        self
    }
}

impl Face {
    pub(crate) fn from_face(face: &ffi::TopoDS_Face) -> Self {
        let inner = ffi::TopoDS_Face_to_owned(face);

        Self { inner }
    }

    fn from_make_face(make_face: UniquePtr<ffi::BRepBuilderAPI_MakeFace>) -> Self {
        Self::from_face(make_face.Face())
    }

    pub fn from_wire(wire: &Wire) -> Self {
        let only_plane = false;
        let make_face = ffi::BRepBuilderAPI_MakeFace_wire(&wire.inner, only_plane);

        Self::from_make_face(make_face)
    }

    pub fn from_surface(surface: &Surface) -> Self {
        const EDGE_TOLERANCE: f64 = 0.0001;

        let make_face = ffi::BRepBuilderAPI_MakeFace_surface(&surface.inner, EDGE_TOLERANCE);

        Self::from_make_face(make_face)
    }

    #[must_use]
    pub fn extrude(&self, dir: DVec3) -> Solid {
        let prism_vec = make_vec(dir);

        let copy = false;
        let canonize = true;

        let inner_shape = ffi::cast_face_to_shape(&self.inner);
        let mut make_solid =
            ffi::BRepPrimAPI_MakePrism_ctor(inner_shape, &prism_vec, copy, canonize);
        let extruded_shape = make_solid.pin_mut().Shape();
        let solid = ffi::TopoDS_cast_to_solid(extruded_shape);

        Solid::from_solid(solid)
    }

    #[must_use]
    pub fn extrude_to_face(&self, shape_with_face: &Shape, face: &Face) -> Shape {
        let profile_base = &self.inner;
        let sketch_base = ffi::TopoDS_Face_ctor();
        let angle = 0.0;
        let fuse = 1; // 0 = subtractive, 1 = additive
        let modify = false;

        let mut make_prism = ffi::BRepFeat_MakeDPrism_ctor(
            &shape_with_face.inner,
            profile_base,
            &sketch_base,
            angle,
            fuse,
            modify,
        );

        let until_face = ffi::cast_face_to_shape(&face.inner);
        make_prism.pin_mut().perform_until_face(until_face);

        Shape::from_shape(make_prism.pin_mut().Shape())
    }

    #[must_use]
    pub fn subtractive_extrude(&self, shape_with_face: &Shape, height: f64) -> Shape {
        let profile_base = &self.inner;
        let sketch_base = ffi::TopoDS_Face_ctor();
        let angle = 0.0;
        let fuse = 0; // 0 = subtractive, 1 = additive
        let modify = false;

        let mut make_prism = ffi::BRepFeat_MakeDPrism_ctor(
            &shape_with_face.inner,
            profile_base,
            &sketch_base,
            angle,
            fuse,
            modify,
        );

        make_prism.pin_mut().perform_with_height(height);

        Shape::from_shape(make_prism.pin_mut().Shape())
    }

    #[must_use]
    pub fn revolve(&self, origin: DVec3, axis: DVec3, angle: Option<Angle>) -> Solid {
        let revol_vec = make_axis_1(origin, axis);

        let angle = angle.map(Angle::radians).unwrap_or(std::f64::consts::PI * 2.0);
        let copy = false;

        let inner_shape = ffi::cast_face_to_shape(&self.inner);
        let mut make_solid = ffi::BRepPrimAPI_MakeRevol_ctor(inner_shape, &revol_vec, angle, copy);
        let revolved_shape = make_solid.pin_mut().Shape();
        let solid = ffi::TopoDS_cast_to_solid(revolved_shape);

        Solid::from_solid(solid)
    }

    /// Fillets the face edges by a given radius at each vertex
    #[must_use]
    pub fn fillet(&self, radius: f64) -> Self {
        let mut make_fillet = ffi::BRepFilletAPI_MakeFillet2d_ctor(&self.inner);

        let face_shape = ffi::cast_face_to_shape(&self.inner);

        // We use a shape map here to avoid duplicates.
        let mut shape_map = ffi::new_indexed_map_of_shape();
        ffi::map_shapes(face_shape, ffi::TopAbs_ShapeEnum::TopAbs_VERTEX, shape_map.pin_mut());

        for i in 1..=shape_map.Extent() {
            let vertex = ffi::TopoDS_cast_to_vertex(shape_map.FindKey(i));
            ffi::BRepFilletAPI_MakeFillet2d_add_fillet(make_fillet.pin_mut(), vertex, radius);
        }

        make_fillet.pin_mut().Build(&ffi::Message_ProgressRange_ctor());

        let result_shape = make_fillet.pin_mut().Shape();
        let result_face = ffi::TopoDS_cast_to_face(result_shape);

        Self::from_face(result_face)
    }

    /// Chamfer the wire edges at each vertex by a given distance
    #[must_use]
    pub fn chamfer(&self, distance_1: f64) -> Self {
        // TODO - Support asymmetric chamfers.
        let distance_2 = distance_1;

        let face_shape = ffi::cast_face_to_shape(&self.inner);

        let mut make_fillet = ffi::BRepFilletAPI_MakeFillet2d_ctor(&self.inner);

        let mut vertex_map = ffi::new_indexed_map_of_shape();
        ffi::map_shapes(face_shape, ffi::TopAbs_ShapeEnum::TopAbs_VERTEX, vertex_map.pin_mut());

        // Get map of vertices to edges so we can find the edges connected to each vertex.
        let mut data_map = ffi::new_indexed_data_map_of_shape_list_of_shape();
        ffi::map_shapes_and_ancestors(
            face_shape,
            ffi::TopAbs_ShapeEnum::TopAbs_VERTEX,
            ffi::TopAbs_ShapeEnum::TopAbs_EDGE,
            data_map.pin_mut(),
        );

        // Chamfer at vertex of all edges.
        for i in 1..=vertex_map.Extent() {
            let edges = ffi::shape_list_to_vector(data_map.FindFromIndex(i));
            let edge_1 = edges.get(0).expect("Vertex has no edges");
            let edge_2 = edges.get(1).expect("Vertex has only one edge");
            ffi::BRepFilletAPI_MakeFillet2d_add_chamfer(
                make_fillet.pin_mut(),
                ffi::TopoDS_cast_to_edge(edge_1),
                ffi::TopoDS_cast_to_edge(edge_2),
                distance_1,
                distance_2,
            );
        }

        let filleted_shape = make_fillet.pin_mut().Shape();
        let result_face = ffi::TopoDS_cast_to_face(filleted_shape);

        Self::from_face(result_face)
    }

    /// Offset the face by a given distance and join settings
    #[must_use]
    pub fn offset(&self, distance: f64, join_type: JoinType) -> Self {
        let mut make_offset =
            ffi::BRepOffsetAPI_MakeOffset_face_ctor(&self.inner, join_type.into());
        make_offset.pin_mut().Perform(distance, 0.0);

        let offset_shape = make_offset.pin_mut().Shape();
        let result_wire = ffi::TopoDS_cast_to_wire(offset_shape);
        let wire = Wire::from_wire(result_wire);

        wire.to_face()
    }

    /// Sweep the face along a path to produce a solid
    #[must_use]
    pub fn sweep_along(&self, path: &Wire) -> Solid {
        let profile_shape = ffi::cast_face_to_shape(&self.inner);
        let mut make_pipe = ffi::BRepOffsetAPI_MakePipe_ctor(&path.inner, profile_shape);

        let pipe_shape = make_pipe.pin_mut().Shape();
        let result_solid = ffi::TopoDS_cast_to_solid(pipe_shape);

        Solid::from_solid(result_solid)
    }

    /// Sweep the face along a path, modulated by a function, to produce a solid
    #[must_use]
    pub fn sweep_along_with_radius_values(
        &self,
        path: &Wire,
        radius_values: impl IntoIterator<Item = (f64, f64)>,
    ) -> Solid {
        let law_function = law_function_from_graph(radius_values);
        let law_handle = ffi::Law_Function_to_handle(law_function);

        let profile_wire = ffi::outer_wire(&self.inner);
        let mut make_pipe_shell =
            make_pipe_shell_with_law_function(&profile_wire, &path.inner, &law_handle);

        make_pipe_shell.pin_mut().Build(&ffi::Message_ProgressRange_ctor());
        make_pipe_shell.pin_mut().MakeSolid();
        let pipe_shape = make_pipe_shell.pin_mut().Shape();
        let result_solid = ffi::TopoDS_cast_to_solid(pipe_shape);

        Solid::from_solid(result_solid)
    }

    pub fn edges(&self) -> EdgeIterator {
        let explorer = ffi::TopExp_Explorer_ctor(
            ffi::cast_face_to_shape(&self.inner),
            ffi::TopAbs_ShapeEnum::TopAbs_EDGE,
        );

        EdgeIterator { explorer }
    }

    /// Returns an iterator over all wires in the face (outer wire + inner wires/holes)
    pub fn wires(&self) -> WireIterator {
        let explorer = ffi::TopExp_Explorer_ctor(
            ffi::cast_face_to_shape(&self.inner),
            ffi::TopAbs_ShapeEnum::TopAbs_WIRE,
        );

        WireIterator { explorer }
    }

    /// Returns an iterator over all wires with information about whether each is the outer wire
    pub fn wires_with_roles(&self) -> WireWithRoleIterator {
        let explorer = ffi::TopExp_Explorer_ctor(
            ffi::cast_face_to_shape(&self.inner),
            ffi::TopAbs_ShapeEnum::TopAbs_WIRE,
        );

        let outer_wire = self.outer_wire();

        WireWithRoleIterator { explorer, outer_wire }
    }

    pub fn center_of_mass(&self) -> DVec3 {
        let mut props = ffi::GProp_GProps_ctor();

        let inner_shape = ffi::cast_face_to_shape(&self.inner);
        ffi::BRepGProp_SurfaceProperties(inner_shape, props.pin_mut());

        let center = ffi::GProp_GProps_CentreOfMass(&props);

        dvec3(center.X(), center.Y(), center.Z())
    }

    pub fn normal_at(&self, pos: DVec3) -> DVec3 {
        let surface = ffi::BRep_Tool_Surface(&self.inner);
        let projector = ffi::GeomAPI_ProjectPointOnSurf_ctor(&make_point(pos), &surface);
        let mut u: f64 = 0.0;
        let mut v: f64 = 0.0;

        projector.LowerDistanceParameters(&mut u, &mut v);

        let mut p = ffi::new_point(0.0, 0.0, 0.0);
        let mut normal = ffi::new_vec(0.0, 1.0, 0.0);

        let face = ffi::BRepGProp_Face_ctor(&self.inner);
        face.Normal(u, v, p.pin_mut(), normal.pin_mut());

        dvec3(normal.X(), normal.Y(), normal.Z())
    }

    pub fn normal_at_center(&self) -> DVec3 {
        let center = self.center_of_mass();
        self.normal_at(center)
    }

    pub fn workplane(&self) -> Workplane {
        const NORMAL_DIFF_TOLERANCE: f64 = 0.0001;

        let center = self.center_of_mass();
        let normal = self.normal_at(center);
        let mut x_dir = dvec3(0.0, 0.0, 1.0).cross(normal);

        if x_dir.length() < NORMAL_DIFF_TOLERANCE {
            // The normal of this face is too close to the same direction
            // as the global Z axis. Use the global X axis for X instead.
            x_dir = dvec3(1.0, 0.0, 0.0);
        }

        let mut workplane = Workplane::new(x_dir, normal);
        workplane.set_translation(center);
        workplane
    }

    pub fn union(&self, other: &Face) -> CompoundFace {
        let inner_shape = ffi::cast_face_to_shape(&self.inner);
        let other_inner_shape = ffi::cast_face_to_shape(&other.inner);

        let mut fuse_operation = ffi::BRepAlgoAPI_Fuse_ctor(inner_shape, other_inner_shape);

        let fuse_shape = fuse_operation.pin_mut().Shape();

        let compound = ffi::TopoDS_cast_to_compound(fuse_shape);

        CompoundFace::from_compound(compound)
    }

    #[must_use]
    pub fn intersect(&self, other: &Face) -> CompoundFace {
        let inner_shape = ffi::cast_face_to_shape(&self.inner);
        let other_inner_shape = ffi::cast_face_to_shape(&other.inner);

        let mut common_operation = ffi::BRepAlgoAPI_Common_ctor(inner_shape, other_inner_shape);

        let common_shape = common_operation.pin_mut().Shape();

        let compound = ffi::TopoDS_cast_to_compound(common_shape);

        CompoundFace::from_compound(compound)
    }

    pub fn subtract(&self, other: &Face) -> CompoundFace {
        let inner_shape = ffi::cast_face_to_shape(&self.inner);
        let other_inner_shape = ffi::cast_face_to_shape(&other.inner);

        let mut fuse_operation = ffi::BRepAlgoAPI_Cut_ctor(inner_shape, other_inner_shape);

        let cut_shape = fuse_operation.pin_mut().Shape();

        let compound = ffi::TopoDS_cast_to_compound(cut_shape);

        CompoundFace::from_compound(compound)
    }

    pub fn surface_area(&self) -> f64 {
        let mut props = ffi::GProp_GProps_ctor();

        let inner_shape = ffi::cast_face_to_shape(&self.inner);
        ffi::BRepGProp_SurfaceProperties(inner_shape, props.pin_mut());

        // Returns surface area, obviously.
        props.Mass()
    }

    pub fn orientation(&self) -> FaceOrientation {
        FaceOrientation::from(self.inner.Orientation())
    }

    /// Get the UV parameter bounds of the face
    pub fn uv_bounds(&self) -> UVBounds {
        let mut u_min = 0.0;
        let mut u_max = 0.0;
        let mut v_min = 0.0;
        let mut v_max = 0.0;

        ffi::face_uv_bounds(&self.inner, &mut u_min, &mut u_max, &mut v_min, &mut v_max);

        UVBounds::new(u_min, u_max, v_min, v_max)
    }

    #[must_use]
    pub fn outer_wire(&self) -> Wire {
        let inner = ffi::outer_wire(&self.inner);

        Wire { inner }
    }

    /// Get the type of the underlying geometric surface (e.g., SurfaceType::Plane, SurfaceType::CylindricalSurface)
    pub fn surface_type(&self) -> SurfaceType {
        let surface = ffi::BRep_Tool_Surface(&self.inner);
        let dynamic_type = ffi::DynamicType(&surface);
        let type_name = ffi::type_name(&dynamic_type);
        SurfaceType::from(type_name)
    }

    /// Get detailed information about the underlying surface
    pub fn surface_details(&self) -> SurfaceDetails {
        let surface = ffi::BRep_Tool_Surface(&self.inner);
        let surface_type = self.surface_type();

        match surface_type {
            SurfaceType::Plane => {
                let plane = ffi::cast_surface_to_plane(&surface);
                if !plane.IsNull() {
                    let location = ffi::geom_plane_location(&plane);
                    let axis = ffi::geom_plane_axis(&plane);
                    let axis_location = ffi::gp_Ax1_location(&axis);
                    let axis_direction = ffi::gp_Ax1_direction(&axis);
                    let x_dir_ptr = ffi::geom_plane_x_direction(&plane);
                    let y_dir_ptr = ffi::geom_plane_y_direction(&plane);

                    // Get UV bounds for the face
                    let bounds = self.uv_bounds();

                    SurfaceDetails::Plane(Plane {
                        location: dvec3(location.X(), location.Y(), location.Z()).into(),
                        axis_location: dvec3(
                            axis_location.X(),
                            axis_location.Y(),
                            axis_location.Z(),
                        )
                        .into(),
                        axis_direction: dvec3(
                            axis_direction.X(),
                            axis_direction.Y(),
                            axis_direction.Z(),
                        )
                        .into(),
                        x_direction: dvec3(x_dir_ptr.X(), x_dir_ptr.Y(), x_dir_ptr.Z()).into(),
                        y_direction: dvec3(y_dir_ptr.X(), y_dir_ptr.Y(), y_dir_ptr.Z()).into(),
                        bounds,
                    })
                } else {
                    SurfaceDetails::Unknown(surface_type.to_string())
                }
            },
            SurfaceType::CylindricalSurface => {
                let cylinder = ffi::cast_surface_to_cylinder(&surface);
                if !cylinder.IsNull() {
                    let location = ffi::geom_cylinder_location(&cylinder);
                    let axis = ffi::geom_cylinder_axis(&cylinder);
                    let axis_location = ffi::gp_Ax1_location(&axis);
                    let axis_direction = ffi::gp_Ax1_direction(&axis);
                    let x_dir_ptr = ffi::geom_cylinder_x_direction(&cylinder);
                    let y_dir_ptr = ffi::geom_cylinder_y_direction(&cylinder);
                    let radius = ffi::geom_cylinder_radius(&cylinder);

                    SurfaceDetails::Cylinder(Cylinder {
                        location: dvec3(location.X(), location.Y(), location.Z()).into(),
                        axis_location: dvec3(
                            axis_location.X(),
                            axis_location.Y(),
                            axis_location.Z(),
                        )
                        .into(),
                        axis_direction: dvec3(
                            axis_direction.X(),
                            axis_direction.Y(),
                            axis_direction.Z(),
                        )
                        .into(),
                        x_direction: dvec3(x_dir_ptr.X(), x_dir_ptr.Y(), x_dir_ptr.Z()).into(),
                        y_direction: dvec3(y_dir_ptr.X(), y_dir_ptr.Y(), y_dir_ptr.Z()).into(),
                        radius,
                    })
                } else {
                    SurfaceDetails::Unknown(surface_type.to_string())
                }
            },
            SurfaceType::ConicalSurface => {
                let cone = ffi::cast_surface_to_cone(&surface);
                if !cone.IsNull() {
                    let location = ffi::geom_cone_location(&cone);
                    let axis = ffi::geom_cone_axis(&cone);
                    let axis_location = ffi::gp_Ax1_location(&axis);
                    let axis_direction = ffi::gp_Ax1_direction(&axis);
                    let x_dir_ptr = ffi::geom_cone_x_direction(&cone);
                    let y_dir_ptr = ffi::geom_cone_y_direction(&cone);
                    let ref_radius = ffi::geom_cone_ref_radius(&cone);
                    let semi_angle = ffi::geom_cone_semi_angle(&cone);

                    SurfaceDetails::Cone(Cone {
                        location: dvec3(location.X(), location.Y(), location.Z()).into(),
                        axis_location: dvec3(
                            axis_location.X(),
                            axis_location.Y(),
                            axis_location.Z(),
                        )
                        .into(),
                        axis_direction: dvec3(
                            axis_direction.X(),
                            axis_direction.Y(),
                            axis_direction.Z(),
                        )
                        .into(),
                        x_direction: dvec3(x_dir_ptr.X(), x_dir_ptr.Y(), x_dir_ptr.Z()).into(),
                        y_direction: dvec3(y_dir_ptr.X(), y_dir_ptr.Y(), y_dir_ptr.Z()).into(),
                        ref_radius,
                        semi_angle,
                    })
                } else {
                    SurfaceDetails::Unknown(surface_type.to_string())
                }
            },
            SurfaceType::SphericalSurface => {
                let sphere = ffi::cast_surface_to_sphere(&surface);
                if !sphere.IsNull() {
                    let location = ffi::geom_sphere_location(&sphere);
                    let axis = ffi::geom_sphere_axis(&sphere);
                    let axis_location = ffi::gp_Ax1_location(&axis);
                    let axis_direction = ffi::gp_Ax1_direction(&axis);
                    let x_dir_ptr = ffi::geom_sphere_x_direction(&sphere);
                    let y_dir_ptr = ffi::geom_sphere_y_direction(&sphere);
                    let radius = ffi::geom_sphere_radius(&sphere);

                    SurfaceDetails::Sphere(Sphere {
                        location: dvec3(location.X(), location.Y(), location.Z()).into(),
                        axis_location: dvec3(
                            axis_location.X(),
                            axis_location.Y(),
                            axis_location.Z(),
                        )
                        .into(),
                        axis_direction: dvec3(
                            axis_direction.X(),
                            axis_direction.Y(),
                            axis_direction.Z(),
                        )
                        .into(),
                        x_direction: dvec3(x_dir_ptr.X(), x_dir_ptr.Y(), x_dir_ptr.Z()).into(),
                        y_direction: dvec3(y_dir_ptr.X(), y_dir_ptr.Y(), y_dir_ptr.Z()).into(),
                        radius,
                    })
                } else {
                    SurfaceDetails::Unknown(surface_type.to_string())
                }
            },
            SurfaceType::ToroidalSurface => {
                let torus = ffi::cast_surface_to_torus(&surface);
                if !torus.IsNull() {
                    let location = ffi::geom_torus_location(&torus);
                    let axis = ffi::geom_torus_axis(&torus);
                    let axis_location = ffi::gp_Ax1_location(&axis);
                    let axis_direction = ffi::gp_Ax1_direction(&axis);
                    let x_dir_ptr = ffi::geom_torus_x_direction(&torus);
                    let y_dir_ptr = ffi::geom_torus_y_direction(&torus);
                    let major_radius = ffi::geom_torus_major_radius(&torus);
                    let minor_radius = ffi::geom_torus_minor_radius(&torus);

                    SurfaceDetails::Torus(Torus {
                        location: dvec3(location.X(), location.Y(), location.Z()).into(),
                        axis_location: dvec3(
                            axis_location.X(),
                            axis_location.Y(),
                            axis_location.Z(),
                        )
                        .into(),
                        axis_direction: dvec3(
                            axis_direction.X(),
                            axis_direction.Y(),
                            axis_direction.Z(),
                        )
                        .into(),
                        x_direction: dvec3(x_dir_ptr.X(), x_dir_ptr.Y(), x_dir_ptr.Z()).into(),
                        y_direction: dvec3(y_dir_ptr.X(), y_dir_ptr.Y(), y_dir_ptr.Z()).into(),
                        major_radius,
                        minor_radius,
                    })
                } else {
                    SurfaceDetails::Unknown(surface_type.to_string())
                }
            },
            SurfaceType::BSplineSurface => {
                let bspline = ffi::cast_surface_to_bspline(&surface);
                if !bspline.IsNull() {
                    let nb_u_poles = ffi::geom_bspline_surface_nb_u_poles(&bspline);
                    let nb_v_poles = ffi::geom_bspline_surface_nb_v_poles(&bspline);
                    let u_degree = ffi::geom_bspline_surface_u_degree(&bspline) as u32;
                    let v_degree = ffi::geom_bspline_surface_v_degree(&bspline) as u32;
                    let is_u_rational = ffi::geom_bspline_surface_is_u_rational(&bspline);
                    let is_v_rational = ffi::geom_bspline_surface_is_v_rational(&bspline);
                    let is_u_periodic = ffi::geom_bspline_surface_is_u_periodic(&bspline);
                    let is_v_periodic = ffi::geom_bspline_surface_is_v_periodic(&bspline);

                    // Extract knot vectors for U direction
                    let nb_u_knots = ffi::geom_bspline_surface_nb_u_knots(&bspline);
                    let mut u_knots = Vec::with_capacity(nb_u_knots as usize);
                    let mut u_multiplicities = Vec::with_capacity(nb_u_knots as usize);

                    for i in 1..=nb_u_knots {
                        u_knots.push(ffi::geom_bspline_surface_u_knot(&bspline, i));
                        u_multiplicities
                            .push(ffi::geom_bspline_surface_u_multiplicity(&bspline, i) as u32);
                    }

                    // Extract knot vectors for V direction
                    let nb_v_knots = ffi::geom_bspline_surface_nb_v_knots(&bspline);
                    let mut v_knots = Vec::with_capacity(nb_v_knots as usize);
                    let mut v_multiplicities = Vec::with_capacity(nb_v_knots as usize);

                    for i in 1..=nb_v_knots {
                        v_knots.push(ffi::geom_bspline_surface_v_knot(&bspline, i));
                        v_multiplicities
                            .push(ffi::geom_bspline_surface_v_multiplicity(&bspline, i) as u32);
                    }

                    // Extract poles (control points) and weights - 2D grid [u][v]
                    let poles = (1..=nb_u_poles as i32)
                        .map(|u| {
                            (1..=nb_v_poles as i32)
                                .map(|v| {
                                    let pole = ffi::geom_bspline_surface_pole(&bspline, u, v);
                                    dvec3(pole.X(), pole.Y(), pole.Z()).into()
                                })
                                .collect::<Vec<_>>()
                        })
                        .collect();

                    let weights = if is_u_rational || is_v_rational {
                        Some(
                            (1..=nb_u_poles as i32)
                                .map(|u| {
                                    (1..=nb_v_poles as i32)
                                        .map(|v| ffi::geom_bspline_surface_weight(&bspline, u, v))
                                        .collect()
                                })
                                .collect(),
                        )
                    } else {
                        None
                    };

                    SurfaceDetails::BSpline(BSplineSurface {
                        u_profile: BSplineCurveProfile {
                            nb_poles: nb_u_poles as u32,
                            degree: u_degree,
                            is_rational: is_u_rational,
                            is_periodic: is_u_periodic,
                            knots: u_knots,
                            multiplicities: u_multiplicities,
                        },
                        v_profile: BSplineCurveProfile {
                            nb_poles: nb_v_poles as u32,
                            degree: v_degree,
                            is_rational: is_v_rational,
                            is_periodic: is_v_periodic,
                            knots: v_knots,
                            multiplicities: v_multiplicities,
                        },
                        poles,
                        weights,
                    })
                } else {
                    SurfaceDetails::Unknown(surface_type.to_string())
                }
            },
            SurfaceType::BezierSurface => {
                let bezier = ffi::cast_surface_to_bezier(&surface);
                if !bezier.IsNull() {
                    let nb_u_poles = ffi::geom_bezier_surface_nb_u_poles(&bezier);
                    let nb_v_poles = ffi::geom_bezier_surface_nb_v_poles(&bezier);
                    let u_degree = ffi::geom_bezier_surface_u_degree(&bezier) as u32;
                    let v_degree = ffi::geom_bezier_surface_v_degree(&bezier) as u32;
                    let is_u_rational = ffi::geom_bezier_surface_is_u_rational(&bezier);
                    let is_v_rational = ffi::geom_bezier_surface_is_v_rational(&bezier);

                    // Extract poles (control points) and weights - 2D grid [u][v]
                    let poles = (1..=nb_u_poles as i32)
                        .map(|u| {
                            (1..=nb_v_poles as i32)
                                .map(|v| {
                                    let pole = ffi::geom_bezier_surface_pole(&bezier, u, v);
                                    dvec3(pole.X(), pole.Y(), pole.Z()).into()
                                })
                                .collect::<Vec<_>>()
                        })
                        .collect();

                    let weights = if is_u_rational || is_v_rational {
                        Some(
                            (1..=nb_u_poles as i32)
                                .map(|u| {
                                    (1..=nb_v_poles as i32)
                                        .map(|v| ffi::geom_bezier_surface_weight(&bezier, u, v))
                                        .collect()
                                })
                                .collect(),
                        )
                    } else {
                        None
                    };

                    SurfaceDetails::Bezier(BezierSurface {
                        u_profile: BezierCurveProfile {
                            nb_poles: nb_u_poles as u32,
                            degree: u_degree,
                        },
                        v_profile: BezierCurveProfile {
                            nb_poles: nb_v_poles as u32,
                            degree: v_degree,
                        },
                        poles,
                        weights,
                    })
                } else {
                    SurfaceDetails::Unknown(surface_type.to_string())
                }
            },
            SurfaceType::Unknown(type_name) => SurfaceDetails::Unknown(type_name),
        }
    }

    /// Convert the face's surface to a B-Spline surface representation and create a new Face.
    /// This works for any surface type (plane, cylinder, sphere, torus, etc.)
    /// The UV bounds from the original face are preserved.
    ///
    /// # Arguments
    ///
    /// * `non_periodic` - If true, the resulting B-Spline surface will be set to non-periodic
    ///                    in both U and V directions
    ///
    /// Returns None if the conversion fails.
    pub fn to_bspline_face(&self, non_periodic: bool) -> Option<Face> {
        let surface = ffi::BRep_Tool_Surface(&self.inner);
        let mut bspline = ffi::convert_surface_to_bspline(&surface);

        if bspline.is_null() || bspline.IsNull() {
            // println!("Conversion failed: {:?}", self.surface_type());
            return None;
        }

        // Set to non-periodic if requested
        if non_periodic {
            if ffi::geom_bspline_surface_is_u_periodic(&bspline) {
                ffi::geom_bspline_surface_set_u_not_periodic(bspline.pin_mut());
            }
            if ffi::geom_bspline_surface_is_v_periodic(&bspline) {
                ffi::geom_bspline_surface_set_v_not_periodic(bspline.pin_mut());
            }
        }

        // Convert BSpline surface handle to generic surface handle
        let surface_handle = ffi::bspline_surface_to_surface(&bspline);

        // Get UV bounds from the original face
        let bounds = self.uv_bounds();

        // Create a Face from the surface with UV bounds
        const EDGE_TOLERANCE: f64 = 0.0001;
        let make_face = ffi::BRepBuilderAPI_MakeFace_surface_with_bounds(
            &surface_handle,
            bounds.u_min,
            bounds.u_max,
            bounds.v_min,
            bounds.v_max,
            EDGE_TOLERANCE,
        );

        let f = Face::from_face(make_face.Face());
        // println!("Surface details: {:?}", f.surface_details());
        Some(f)
    }

    /// Convert the face's surface to a B-Spline surface representation.
    /// This works for any surface type (plane, cylinder, sphere, torus, etc.)
    /// Returns None if the conversion fails.
    pub fn to_bspline_surface(&self, non_periodic: bool) -> Option<BSplineSurface> {
        let surface = ffi::BRep_Tool_Surface(&self.inner);
        let mut bspline = ffi::convert_surface_to_bspline(&surface);

        // Check if the UniquePtr is null (conversion failed)
        if bspline.is_null() {
            return None;
        }

        // Also check if the Handle itself is null
        if bspline.IsNull() {
            return None;
        }

        // Set to non-periodic if requested
        if non_periodic {
            if ffi::geom_bspline_surface_is_u_periodic(&bspline) {
                ffi::geom_bspline_surface_set_u_not_periodic(bspline.pin_mut());
            }
            if ffi::geom_bspline_surface_is_v_periodic(&bspline) {
                ffi::geom_bspline_surface_set_v_not_periodic(bspline.pin_mut());
            }
        }

        let nb_u_poles = ffi::geom_bspline_surface_nb_u_poles(&bspline);
        let nb_v_poles = ffi::geom_bspline_surface_nb_v_poles(&bspline);
        let u_degree = ffi::geom_bspline_surface_u_degree(&bspline) as u32;
        let v_degree = ffi::geom_bspline_surface_v_degree(&bspline) as u32;
        let is_u_rational = ffi::geom_bspline_surface_is_u_rational(&bspline);
        let is_v_rational = ffi::geom_bspline_surface_is_v_rational(&bspline);
        let is_u_periodic = ffi::geom_bspline_surface_is_u_periodic(&bspline);
        let is_v_periodic = ffi::geom_bspline_surface_is_v_periodic(&bspline);

        let nb_u_knots = ffi::geom_bspline_surface_nb_u_knots(&bspline) as usize;
        let nb_v_knots = ffi::geom_bspline_surface_nb_v_knots(&bspline) as usize;

        let mut u_knots = Vec::with_capacity(nb_u_knots);
        let mut u_multiplicities = Vec::with_capacity(nb_u_knots);
        for i in 1..=nb_u_knots as i32 {
            u_knots.push(ffi::geom_bspline_surface_u_knot(&bspline, i));
            u_multiplicities.push(ffi::geom_bspline_surface_u_multiplicity(&bspline, i) as u32);
        }

        let mut v_knots = Vec::with_capacity(nb_v_knots);
        let mut v_multiplicities = Vec::with_capacity(nb_v_knots);
        for i in 1..=nb_v_knots as i32 {
            v_knots.push(ffi::geom_bspline_surface_v_knot(&bspline, i));
            v_multiplicities.push(ffi::geom_bspline_surface_v_multiplicity(&bspline, i) as u32);
        }

        let mut poles = Vec::with_capacity(nb_u_poles as usize);
        for u in 1..=nb_u_poles as i32 {
            let mut row = Vec::with_capacity(nb_v_poles as usize);
            for v in 1..=nb_v_poles as i32 {
                let pole = ffi::geom_bspline_surface_pole(&bspline, u, v);
                row.push(dvec3(pole.X(), pole.Y(), pole.Z()).into());
            }
            poles.push(row);
        }

        let weights = if is_u_rational || is_v_rational {
            let mut weights = Vec::with_capacity(nb_u_poles as usize);
            for u in 1..=nb_u_poles as i32 {
                let mut row = Vec::with_capacity(nb_v_poles as usize);
                for v in 1..=nb_v_poles as i32 {
                    row.push(ffi::geom_bspline_surface_weight(&bspline, u, v));
                }
                weights.push(row);
            }
            Some(weights)
        } else {
            None
        };

        Some(BSplineSurface {
            u_profile: BSplineCurveProfile {
                nb_poles: nb_u_poles as u32,
                degree: u_degree,
                is_rational: is_u_rational,
                is_periodic: is_u_periodic,
                knots: u_knots,
                multiplicities: u_multiplicities,
            },
            v_profile: BSplineCurveProfile {
                nb_poles: nb_v_poles as u32,
                degree: v_degree,
                is_rational: is_v_rational,
                is_periodic: is_v_periodic,
                knots: v_knots,
                multiplicities: v_multiplicities,
            },
            poles,
            weights,
        })
    }
}

pub struct CompoundFace {
    inner: UniquePtr<ffi::TopoDS_Compound>,
}

impl AsRef<CompoundFace> for CompoundFace {
    fn as_ref(&self) -> &CompoundFace {
        self
    }
}

impl From<Face> for CompoundFace {
    fn from(face: Face) -> Self {
        let face = ffi::cast_face_to_shape(&face.inner);
        let mut compound = ffi::TopoDS_Compound_ctor();
        let brep_builder = ffi::BRep_Builder_ctor();
        let topo_builder = ffi::BRep_Builder_upcast_to_topods_builder(&brep_builder);
        topo_builder.MakeCompound(compound.pin_mut());
        let mut compound_shape = ffi::TopoDS_Compound_as_shape(compound);
        topo_builder.Add(compound_shape.pin_mut(), face);
        Self::from_compound(ffi::TopoDS_cast_to_compound(&compound_shape))
    }
}

impl CompoundFace {
    pub(crate) fn from_compound(compound: &ffi::TopoDS_Compound) -> Self {
        let inner = ffi::TopoDS_Compound_to_owned(compound);

        Self { inner }
    }

    #[must_use]
    pub fn clean(&self) -> Self {
        let shape = ffi::cast_compound_to_shape(&self.inner);
        let shape = Shape::from_shape(shape).clean();

        let compound = ffi::TopoDS_cast_to_compound(&shape.inner);

        Self::from_compound(compound)
    }

    #[must_use]
    pub fn extrude(&self, dir: DVec3) -> Shape {
        let prism_vec = make_vec(dir);

        let copy = false;
        let canonize = true;

        let inner_shape = ffi::cast_compound_to_shape(&self.inner);

        let mut make_solid =
            ffi::BRepPrimAPI_MakePrism_ctor(inner_shape, &prism_vec, copy, canonize);
        let extruded_shape = make_solid.pin_mut().Shape();

        Shape::from_shape(extruded_shape)
    }

    #[must_use]
    pub fn revolve(&self, origin: DVec3, axis: DVec3, angle: Option<Angle>) -> Shape {
        let revol_axis = make_axis_1(origin, axis);

        let angle = angle.map(Angle::radians).unwrap_or(std::f64::consts::PI * 2.0);
        let copy = false;

        let inner_shape = ffi::cast_compound_to_shape(&self.inner);

        let mut make_solid = ffi::BRepPrimAPI_MakeRevol_ctor(inner_shape, &revol_axis, angle, copy);
        let revolved_shape = make_solid.pin_mut().Shape();

        Shape::from_shape(revolved_shape)
    }

    #[must_use]
    pub fn union(&self, other: &CompoundFace) -> CompoundFace {
        let inner_shape = ffi::cast_compound_to_shape(&self.inner);
        let other_inner_shape = ffi::cast_compound_to_shape(&other.inner);

        let mut fuse_operation = ffi::BRepAlgoAPI_Fuse_ctor(inner_shape, other_inner_shape);

        let fuse_shape = fuse_operation.pin_mut().Shape();

        let compound = ffi::TopoDS_cast_to_compound(fuse_shape);

        CompoundFace::from_compound(compound)
    }

    #[must_use]
    pub fn intersect(&self, other: &CompoundFace) -> CompoundFace {
        let inner_shape = ffi::cast_compound_to_shape(&self.inner);
        let other_inner_shape = ffi::cast_compound_to_shape(&other.inner);

        let mut common_operation = ffi::BRepAlgoAPI_Common_ctor(inner_shape, other_inner_shape);

        let common_shape = common_operation.pin_mut().Shape();

        let compound = ffi::TopoDS_cast_to_compound(common_shape);

        CompoundFace::from_compound(compound)
    }

    #[must_use]
    pub fn subtract(&self, other: &CompoundFace) -> CompoundFace {
        let inner_shape = ffi::cast_compound_to_shape(&self.inner);
        let other_inner_shape = ffi::cast_compound_to_shape(&other.inner);

        let mut fuse_operation = ffi::BRepAlgoAPI_Cut_ctor(inner_shape, other_inner_shape);

        let cut_shape = fuse_operation.pin_mut().Shape();

        let compound = ffi::TopoDS_cast_to_compound(cut_shape);

        CompoundFace::from_compound(compound)
    }

    pub fn set_global_translation(&mut self, translation: DVec3) {
        let shape = ffi::cast_compound_to_shape(&self.inner);
        let mut shape = Shape::from_shape(shape);

        shape.set_global_translation(translation);

        let compound = ffi::TopoDS_cast_to_compound(&shape.inner);
        *self = Self::from_compound(compound);
    }
}

#[derive(Debug, Copy, Clone, PartialEq)]
pub enum FaceOrientation {
    Forward,
    Reversed,
    Internal,
    External,
}

impl From<ffi::TopAbs_Orientation> for FaceOrientation {
    fn from(orientation: ffi::TopAbs_Orientation) -> Self {
        match orientation {
            ffi::TopAbs_Orientation::TopAbs_FORWARD => Self::Forward,
            ffi::TopAbs_Orientation::TopAbs_REVERSED => Self::Reversed,
            ffi::TopAbs_Orientation::TopAbs_INTERNAL => Self::Internal,
            ffi::TopAbs_Orientation::TopAbs_EXTERNAL => Self::External,
            ffi::TopAbs_Orientation { repr } => {
                panic!("TopAbs_Orientation had an unrepresentable value: {repr}")
            },
        }
    }
}

#[cfg(test)]
mod tests {}
