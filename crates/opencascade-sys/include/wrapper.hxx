#include "rust/cxx.h"
#include <sstream>
#include <fstream>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <BOPAlgo_GlueEnum.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeShapeOnMesh.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <ShapeFix_Shape.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepFeat_MakeCylindricalHole.hxx>
#include <BRepFeat_MakeDPrism.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepFilletAPI_MakeFillet2d.hxx>
#include <BRepGProp.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepIntCurveSurface_Inter.hxx>
#include <BRepLib.hxx>
#include <BRepLib_ToolTriangulatedShape.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Builder.hxx>
#include <BinTools.hxx>
#include <GCE2d_MakeSegment.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GCPnts_TangentialDeflection.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GC_MakeSegment.hxx>
#include <GProp_GProps.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_Circle.hxx>
#include <Geom2d_Ellipse.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_BezierCurve.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_JoinType.hxx>
#include <GeomConvert.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_BezierSurface.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_Hyperbola.hxx>
#include <Geom_Line.hxx>
#include <Geom_OffsetCurve.hxx>
#include <Geom_Parabola.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom_ConicalSurface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Plane.hxx>
#include <Geom_SphericalSurface.hxx>
#include <Geom_Surface.hxx>
#include <Geom_ToroidalSurface.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <IGESControl_Reader.hxx>
#include <IGESControl_Writer.hxx>
#include <Law_Function.hxx>
#include <Law_Interpol.hxx>
#include <NCollection_Array1.hxx>
#include <NCollection_Array2.hxx>
#include <Poly_Connect.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <ShapeAnalysis_FreeBounds.hxx>
#include <ShapeConstruct.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Type.hxx>
#include <StlAPI_Writer.hxx>
#include <TColgp_Array1OfDir.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array2OfReal.hxx>
#include <Geom_BSplineCurve.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Circ.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <Standard_Failure.hxx>
#include <exception>

// Make `Result`-returning cxx bindings catch OpenCASCADE exceptions.
//
// cxx's default `trycatch` only catches `std::exception`, but OCCT's
// `Standard_Failure` derives from `Standard_Transient`, not `std::exception`, so
// an OCCT raise (e.g. an impossible `BRepFilletAPI_MakeFillet::Build`) would
// escape uncaught and abort the process via std::terminate. Defining our own
// `trycatch` overload disables cxx's SFINAE default and lets every Result-typed
// binding surface OCCT failures as a normal Rust `Err`.
namespace rust {
namespace behavior {
template <typename Try, typename Fail>
static void trycatch(Try &&func, Fail &&fail) noexcept try {
  func();
} catch (const Standard_Failure &e) {
  const char *msg = e.GetMessageString();
  fail(msg != nullptr && *msg != '\0' ? msg : "OpenCASCADE operation failed");
} catch (const std::exception &e) {
  fail(e.what());
}
} // namespace behavior
} // namespace rust

// Generic template constructor
template <typename T, typename... Args> std::unique_ptr<T> construct_unique(Args... args) {
  return std::unique_ptr<T>(new T(args...));
}

// Generic List
template <typename T> std::unique_ptr<std::vector<T>> list_to_vector(const NCollection_List<T> &list) {
  return std::unique_ptr<std::vector<T>>(new std::vector<T>(list.begin(), list.end()));
}

// Handles
typedef opencascade::handle<Standard_Type> HandleStandardType;
typedef opencascade::handle<Geom_Curve> HandleGeomCurve;
typedef opencascade::handle<Geom_BSplineCurve> HandleGeomBSplineCurve;
typedef opencascade::handle<Geom_BezierCurve> HandleGeomBezierCurve;
typedef opencascade::handle<Geom_TrimmedCurve> HandleGeomTrimmedCurve;
typedef opencascade::handle<Geom_Line> HandleGeom_Line;
typedef opencascade::handle<Geom_Circle> HandleGeom_Circle;
typedef opencascade::handle<Geom_Ellipse> HandleGeom_Ellipse;
typedef opencascade::handle<Geom_Hyperbola> HandleGeom_Hyperbola;
typedef opencascade::handle<Geom_Parabola> HandleGeom_Parabola;
typedef opencascade::handle<Geom_OffsetCurve> HandleGeom_OffsetCurve;
typedef opencascade::handle<Geom_Surface> HandleGeomSurface;
typedef opencascade::handle<Geom_BezierSurface> HandleGeomBezierSurface;
typedef opencascade::handle<Geom_BSplineSurface> HandleGeom_BSplineSurface;
typedef opencascade::handle<Geom_Plane> HandleGeomPlane;
typedef opencascade::handle<Geom_ConicalSurface> HandleGeom_ConicalSurface;
typedef opencascade::handle<Geom_CylindricalSurface> HandleGeom_CylindricalSurface;
typedef opencascade::handle<Geom_SphericalSurface> HandleGeom_SphericalSurface;
typedef opencascade::handle<Geom_ToroidalSurface> HandleGeom_ToroidalSurface;
typedef opencascade::handle<Geom2d_Curve> HandleGeom2d_Curve;
typedef opencascade::handle<Geom2d_Line> HandleGeom2d_Line;
typedef opencascade::handle<Geom2d_Circle> HandleGeom2d_Circle;
typedef opencascade::handle<Geom2d_Ellipse> HandleGeom2d_Ellipse;
typedef opencascade::handle<Geom2d_BSplineCurve> HandleGeom2d_BSplineCurve;
typedef opencascade::handle<Geom2d_BezierCurve> HandleGeom2d_BezierCurve;
typedef opencascade::handle<Geom2d_TrimmedCurve> HandleGeom2d_TrimmedCurve;
typedef opencascade::handle<Poly_Triangulation> HandlePoly_Triangulation;
typedef opencascade::handle<TopTools_HSequenceOfShape> HandleTopTools_HSequenceOfShape;
typedef opencascade::handle<Law_Function> HandleLawFunction;

typedef opencascade::handle<TColgp_HArray1OfPnt> Handle_TColgpHArray1OfPnt;

inline std::unique_ptr<Handle_TColgpHArray1OfPnt>
new_HandleTColgpHArray1OfPnt_from_TColgpHArray1OfPnt(std::unique_ptr<TColgp_HArray1OfPnt> array) {
  return std::unique_ptr<Handle_TColgpHArray1OfPnt>(new Handle_TColgpHArray1OfPnt(array.release()));
}

// Handle stuff
template <typename T> const T &handle_try_deref(const opencascade::handle<T> &handle) {
  if (handle.IsNull()) {
    throw std::runtime_error("null handle dereference");
  }
  return *handle;
}

inline const HandleStandardType &DynamicType(const HandleGeomSurface &surface) { return surface->DynamicType(); }

inline const HandleStandardType &DynamicTypeCurve(const HandleGeomCurve &curve) { return curve->DynamicType(); }

inline rust::String type_name(const HandleStandardType &handle) { return std::string(handle->Name()); }

inline std::unique_ptr<gp_Pnt> HandleGeomCurve_Value(const HandleGeomCurve &curve, const Standard_Real U) {
  return std::unique_ptr<gp_Pnt>(new gp_Pnt(curve->Value(U)));
}

inline std::unique_ptr<gp_Pnt> HandleGeomSurface_Value(const HandleGeomSurface &surface, const Standard_Real U,
                                                       const Standard_Real V) {
  return std::unique_ptr<gp_Pnt>(new gp_Pnt(surface->Value(U, V)));
}

inline std::unique_ptr<gp_Pnt> GCPnts_TangentialDeflection_Value(const GCPnts_TangentialDeflection &approximator,
                                                                 Standard_Integer i) {
  return std::unique_ptr<gp_Pnt>(new gp_Pnt(approximator.Value(i)));
}

inline std::unique_ptr<HandleGeomPlane> new_HandleGeomPlane_from_HandleGeomSurface(const HandleGeomSurface &surface) {
  HandleGeomPlane plane_handle = opencascade::handle<Geom_Plane>::DownCast(surface);
  return std::unique_ptr<HandleGeomPlane>(new opencascade::handle<Geom_Plane>(plane_handle));
}

// Collections
inline void shape_list_append_face(TopTools_ListOfShape &list, const TopoDS_Face &face) { list.Append(face); }

// Geometry
inline const gp_Pnt &handle_geom_plane_location(const HandleGeomPlane &plane) { return plane->Location(); }

// Cast surface to specific types
inline std::unique_ptr<HandleGeomPlane> cast_surface_to_plane(const HandleGeomSurface &surface) {
  Handle(Geom_Plane) plane = Handle(Geom_Plane)::DownCast(surface);
  if (plane.IsNull()) {
    return std::unique_ptr<HandleGeomPlane>();
  }
  return std::unique_ptr<HandleGeomPlane>(new HandleGeomPlane(plane));
}

inline std::unique_ptr<HandleGeom_CylindricalSurface> cast_surface_to_cylinder(const HandleGeomSurface &surface) {
  Handle(Geom_CylindricalSurface) cylinder = Handle(Geom_CylindricalSurface)::DownCast(surface);
  if (cylinder.IsNull()) {
    return std::unique_ptr<HandleGeom_CylindricalSurface>();
  }
  return std::unique_ptr<HandleGeom_CylindricalSurface>(new HandleGeom_CylindricalSurface(cylinder));
}

// Plane properties
inline const gp_Pnt &geom_plane_location(const HandleGeomPlane &plane) { return plane->Location(); }
inline const gp_Ax1 &geom_plane_axis(const HandleGeomPlane &plane) { return plane->Axis(); }
inline std::unique_ptr<gp_Dir> geom_plane_x_direction(const HandleGeomPlane &plane) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(plane->Position().XDirection()));
}
inline std::unique_ptr<gp_Dir> geom_plane_y_direction(const HandleGeomPlane &plane) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(plane->Position().YDirection()));
}

// Cylinder properties
inline const gp_Pnt &geom_cylinder_location(const HandleGeom_CylindricalSurface &cylinder) { return cylinder->Location(); }
inline const gp_Ax1 &geom_cylinder_axis(const HandleGeom_CylindricalSurface &cylinder) { return cylinder->Axis(); }
inline double geom_cylinder_radius(const HandleGeom_CylindricalSurface &cylinder) { return cylinder->Radius(); }
inline std::unique_ptr<gp_Dir> geom_cylinder_x_direction(const HandleGeom_CylindricalSurface &cylinder) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(cylinder->Position().XDirection()));
}
inline std::unique_ptr<gp_Dir> geom_cylinder_y_direction(const HandleGeom_CylindricalSurface &cylinder) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(cylinder->Position().YDirection()));
}

// Cast surface to specific types
inline std::unique_ptr<HandleGeom_BSplineSurface> cast_surface_to_bspline(const HandleGeomSurface &surface) {
  Handle(Geom_BSplineSurface) bspline = Handle(Geom_BSplineSurface)::DownCast(surface);
  if (bspline.IsNull()) {
    return std::unique_ptr<HandleGeom_BSplineSurface>();
  }
  return std::unique_ptr<HandleGeom_BSplineSurface>(new HandleGeom_BSplineSurface(bspline));
}

// Convert any surface to BSpline surface
inline std::unique_ptr<HandleGeom_BSplineSurface> convert_surface_to_bspline_with_bounds(
    const HandleGeomSurface &surface,
    double u_min, double u_max,
    double v_min, double v_max) {
  try {
    Handle(Geom_BSplineSurface) converted = ShapeConstruct::ConvertSurfaceToBSpline(
        surface, u_min, u_max, v_min, v_max,
        1e-6, GeomAbs_C1, 100, 14);
    if (converted.IsNull()) {
      return std::unique_ptr<HandleGeom_BSplineSurface>();
    }
    return std::unique_ptr<HandleGeom_BSplineSurface>(new HandleGeom_BSplineSurface(converted));
  } catch (...) {
    return std::unique_ptr<HandleGeom_BSplineSurface>();
  }
}

inline std::unique_ptr<HandleGeom_BSplineSurface> convert_surface_to_bspline(const HandleGeomSurface &surface) {
  // Try direct cast first
  Handle(Geom_BSplineSurface) bspline = Handle(Geom_BSplineSurface)::DownCast(surface);
  if (!bspline.IsNull()) {
    return std::unique_ptr<HandleGeom_BSplineSurface>(new HandleGeom_BSplineSurface(bspline));
  }

  // Convert to BSpline surface
  try {
    Handle(Geom_BSplineSurface) converted = GeomConvert::SurfaceToBSplineSurface(surface);
    if (converted.IsNull()) {
      return std::unique_ptr<HandleGeom_BSplineSurface>();
    }
    return std::unique_ptr<HandleGeom_BSplineSurface>(new HandleGeom_BSplineSurface(converted));
  } catch (...) {
    // Conversion failed, return null
    return std::unique_ptr<HandleGeom_BSplineSurface>();
  }
}

inline std::unique_ptr<HandleGeomBezierSurface> cast_surface_to_bezier(const HandleGeomSurface &surface) {
  Handle(Geom_BezierSurface) bezier = Handle(Geom_BezierSurface)::DownCast(surface);
  if (bezier.IsNull()) {
    return std::unique_ptr<HandleGeomBezierSurface>();
  }
  return std::unique_ptr<HandleGeomBezierSurface>(new HandleGeomBezierSurface(bezier));
}

inline std::unique_ptr<HandleGeom_ConicalSurface> cast_surface_to_cone(const HandleGeomSurface &surface) {
  Handle(Geom_ConicalSurface) cone = Handle(Geom_ConicalSurface)::DownCast(surface);
  if (cone.IsNull()) {
    return std::unique_ptr<HandleGeom_ConicalSurface>();
  }
  return std::unique_ptr<HandleGeom_ConicalSurface>(new HandleGeom_ConicalSurface(cone));
}

inline std::unique_ptr<HandleGeom_SphericalSurface> cast_surface_to_sphere(const HandleGeomSurface &surface) {
  Handle(Geom_SphericalSurface) sphere = Handle(Geom_SphericalSurface)::DownCast(surface);
  if (sphere.IsNull()) {
    return std::unique_ptr<HandleGeom_SphericalSurface>();
  }
  return std::unique_ptr<HandleGeom_SphericalSurface>(new HandleGeom_SphericalSurface(sphere));
}

inline std::unique_ptr<HandleGeom_ToroidalSurface> cast_surface_to_torus(const HandleGeomSurface &surface) {
  Handle(Geom_ToroidalSurface) torus = Handle(Geom_ToroidalSurface)::DownCast(surface);
  if (torus.IsNull()) {
    return std::unique_ptr<HandleGeom_ToroidalSurface>();
  }
  return std::unique_ptr<HandleGeom_ToroidalSurface>(new HandleGeom_ToroidalSurface(torus));
}

// BSpline surface properties
inline int geom_bspline_surface_nb_u_poles(const HandleGeom_BSplineSurface &bspline) { return bspline->NbUPoles(); }
inline int geom_bspline_surface_nb_v_poles(const HandleGeom_BSplineSurface &bspline) { return bspline->NbVPoles(); }
inline int geom_bspline_surface_u_degree(const HandleGeom_BSplineSurface &bspline) { return bspline->UDegree(); }
inline int geom_bspline_surface_v_degree(const HandleGeom_BSplineSurface &bspline) { return bspline->VDegree(); }
inline bool geom_bspline_surface_is_u_rational(const HandleGeom_BSplineSurface &bspline) { return bspline->IsURational(); }
inline bool geom_bspline_surface_is_v_rational(const HandleGeom_BSplineSurface &bspline) { return bspline->IsVRational(); }
inline bool geom_bspline_surface_is_u_periodic(const HandleGeom_BSplineSurface &bspline) { return bspline->IsUPeriodic(); }
inline bool geom_bspline_surface_is_v_periodic(const HandleGeom_BSplineSurface &bspline) { return bspline->IsVPeriodic(); }
inline void geom_bspline_surface_set_u_not_periodic(HandleGeom_BSplineSurface &bspline) { bspline->SetUNotPeriodic(); }
inline void geom_bspline_surface_set_v_not_periodic(HandleGeom_BSplineSurface &bspline) { bspline->SetVNotPeriodic(); }

// BSpline surface knots and multiplicities
inline int geom_bspline_surface_nb_u_knots(const HandleGeom_BSplineSurface &bspline) { return bspline->NbUKnots(); }
inline int geom_bspline_surface_nb_v_knots(const HandleGeom_BSplineSurface &bspline) { return bspline->NbVKnots(); }
inline double geom_bspline_surface_u_knot(const HandleGeom_BSplineSurface &bspline, int index) { return bspline->UKnot(index); }
inline double geom_bspline_surface_v_knot(const HandleGeom_BSplineSurface &bspline, int index) { return bspline->VKnot(index); }
inline int geom_bspline_surface_u_multiplicity(const HandleGeom_BSplineSurface &bspline, int index) { return bspline->UMultiplicity(index); }
inline int geom_bspline_surface_v_multiplicity(const HandleGeom_BSplineSurface &bspline, int index) { return bspline->VMultiplicity(index); }

inline const gp_Pnt &geom_bspline_surface_pole(const HandleGeom_BSplineSurface &bspline, int u_index, int v_index) {
  return bspline->Pole(u_index, v_index);
}
inline double geom_bspline_surface_weight(const HandleGeom_BSplineSurface &bspline, int u_index, int v_index) {
  return bspline->Weight(u_index, v_index);
}

// Bezier surface properties
inline int geom_bezier_surface_nb_u_poles(const HandleGeomBezierSurface &bezier) { return bezier->NbUPoles(); }
inline int geom_bezier_surface_nb_v_poles(const HandleGeomBezierSurface &bezier) { return bezier->NbVPoles(); }
inline int geom_bezier_surface_u_degree(const HandleGeomBezierSurface &bezier) { return bezier->UDegree(); }
inline int geom_bezier_surface_v_degree(const HandleGeomBezierSurface &bezier) { return bezier->VDegree(); }
inline bool geom_bezier_surface_is_u_rational(const HandleGeomBezierSurface &bezier) { return bezier->IsURational(); }
inline bool geom_bezier_surface_is_v_rational(const HandleGeomBezierSurface &bezier) { return bezier->IsVRational(); }
inline const gp_Pnt &geom_bezier_surface_pole(const HandleGeomBezierSurface &bezier, int u_index, int v_index) {
  return bezier->Pole(u_index, v_index);
}
inline double geom_bezier_surface_weight(const HandleGeomBezierSurface &bezier, int u_index, int v_index) {
  return bezier->Weight(u_index, v_index);
}

// Cone properties
inline const gp_Pnt &geom_cone_location(const HandleGeom_ConicalSurface &cone) { return cone->Location(); }
inline const gp_Ax1 &geom_cone_axis(const HandleGeom_ConicalSurface &cone) { return cone->Axis(); }
inline double geom_cone_ref_radius(const HandleGeom_ConicalSurface &cone) { return cone->RefRadius(); }
inline double geom_cone_semi_angle(const HandleGeom_ConicalSurface &cone) { return cone->SemiAngle(); }
inline std::unique_ptr<gp_Dir> geom_cone_x_direction(const HandleGeom_ConicalSurface &cone) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(cone->Position().XDirection()));
}
inline std::unique_ptr<gp_Dir> geom_cone_y_direction(const HandleGeom_ConicalSurface &cone) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(cone->Position().YDirection()));
}

// Sphere properties
inline const gp_Pnt &geom_sphere_location(const HandleGeom_SphericalSurface &sphere) { return sphere->Location(); }
inline const gp_Ax1 &geom_sphere_axis(const HandleGeom_SphericalSurface &sphere) { return sphere->Axis(); }
inline double geom_sphere_radius(const HandleGeom_SphericalSurface &sphere) { return sphere->Radius(); }
inline std::unique_ptr<gp_Dir> geom_sphere_x_direction(const HandleGeom_SphericalSurface &sphere) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(sphere->Position().XDirection()));
}
inline std::unique_ptr<gp_Dir> geom_sphere_y_direction(const HandleGeom_SphericalSurface &sphere) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(sphere->Position().YDirection()));
}

// Torus properties
inline const gp_Pnt &geom_torus_location(const HandleGeom_ToroidalSurface &torus) { return torus->Location(); }
inline const gp_Ax1 &geom_torus_axis(const HandleGeom_ToroidalSurface &torus) { return torus->Axis(); }
inline double geom_torus_major_radius(const HandleGeom_ToroidalSurface &torus) { return torus->MajorRadius(); }
inline double geom_torus_minor_radius(const HandleGeom_ToroidalSurface &torus) { return torus->MinorRadius(); }
inline std::unique_ptr<gp_Dir> geom_torus_x_direction(const HandleGeom_ToroidalSurface &torus) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(torus->Position().XDirection()));
}
inline std::unique_ptr<gp_Dir> geom_torus_y_direction(const HandleGeom_ToroidalSurface &torus) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(torus->Position().YDirection()));
}

// Cast curve to specific types
inline std::unique_ptr<HandleGeom_Line> cast_curve_to_line(const HandleGeomCurve &curve) {
  Handle(Geom_Line) line = Handle(Geom_Line)::DownCast(curve);
  if (line.IsNull()) {
    return std::unique_ptr<HandleGeom_Line>();
  }
  return std::unique_ptr<HandleGeom_Line>(new HandleGeom_Line(line));
}

inline std::unique_ptr<HandleGeom_Circle> cast_curve_to_circle(const HandleGeomCurve &curve) {
  Handle(Geom_Circle) circle = Handle(Geom_Circle)::DownCast(curve);
  if (circle.IsNull()) {
    return std::unique_ptr<HandleGeom_Circle>();
  }
  return std::unique_ptr<HandleGeom_Circle>(new HandleGeom_Circle(circle));
}

inline std::unique_ptr<HandleGeom_Ellipse> cast_curve_to_ellipse(const HandleGeomCurve &curve) {
  Handle(Geom_Ellipse) ellipse = Handle(Geom_Ellipse)::DownCast(curve);
  if (ellipse.IsNull()) {
    return std::unique_ptr<HandleGeom_Ellipse>();
  }
  return std::unique_ptr<HandleGeom_Ellipse>(new HandleGeom_Ellipse(ellipse));
}

inline std::unique_ptr<HandleGeomBSplineCurve> cast_curve_to_bspline_curve(const HandleGeomCurve &curve) {
  Handle(Geom_BSplineCurve) bspline = Handle(Geom_BSplineCurve)::DownCast(curve);
  if (bspline.IsNull()) {
    return std::unique_ptr<HandleGeomBSplineCurve>();
  }
  return std::unique_ptr<HandleGeomBSplineCurve>(new HandleGeomBSplineCurve(bspline));
}

inline std::unique_ptr<HandleGeomBezierCurve> cast_curve_to_bezier_curve(const HandleGeomCurve &curve) {
  Handle(Geom_BezierCurve) bezier = Handle(Geom_BezierCurve)::DownCast(curve);
  if (bezier.IsNull()) {
    return std::unique_ptr<HandleGeomBezierCurve>();
  }
  return std::unique_ptr<HandleGeomBezierCurve>(new HandleGeomBezierCurve(bezier));
}

inline std::unique_ptr<HandleGeom_Hyperbola> cast_curve_to_hyperbola(const HandleGeomCurve &curve) {
  Handle(Geom_Hyperbola) hyperbola = Handle(Geom_Hyperbola)::DownCast(curve);
  if (hyperbola.IsNull()) {
    return std::unique_ptr<HandleGeom_Hyperbola>();
  }
  return std::unique_ptr<HandleGeom_Hyperbola>(new HandleGeom_Hyperbola(hyperbola));
}

inline std::unique_ptr<HandleGeom_Parabola> cast_curve_to_parabola(const HandleGeomCurve &curve) {
  Handle(Geom_Parabola) parabola = Handle(Geom_Parabola)::DownCast(curve);
  if (parabola.IsNull()) {
    return std::unique_ptr<HandleGeom_Parabola>();
  }
  return std::unique_ptr<HandleGeom_Parabola>(new HandleGeom_Parabola(parabola));
}

inline std::unique_ptr<HandleGeom_OffsetCurve> cast_curve_to_offset_curve(const HandleGeomCurve &curve) {
  Handle(Geom_OffsetCurve) offset = Handle(Geom_OffsetCurve)::DownCast(curve);
  if (offset.IsNull()) {
    return std::unique_ptr<HandleGeom_OffsetCurve>();
  }
  return std::unique_ptr<HandleGeom_OffsetCurve>(new HandleGeom_OffsetCurve(offset));
}

inline std::unique_ptr<HandleGeomTrimmedCurve> cast_curve_to_trimmed_curve(const HandleGeomCurve &curve) {
  Handle(Geom_TrimmedCurve) trimmed = Handle(Geom_TrimmedCurve)::DownCast(curve);
  if (trimmed.IsNull()) {
    return std::unique_ptr<HandleGeomTrimmedCurve>();
  }
  return std::unique_ptr<HandleGeomTrimmedCurve>(new HandleGeomTrimmedCurve(trimmed));
}

// Line properties
inline const gp_Ax1 &geom_line_position(const HandleGeom_Line &line) { return line->Position(); }

// Circle properties
inline const gp_Pnt &geom_circle_location(const HandleGeom_Circle &circle) { return circle->Location(); }
inline const gp_Ax1 &geom_circle_axis(const HandleGeom_Circle &circle) { return circle->Axis(); }
inline double geom_circle_radius(const HandleGeom_Circle &circle) { return circle->Radius(); }
inline std::unique_ptr<gp_Dir> geom_circle_x_direction(const HandleGeom_Circle &circle) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(circle->Position().XDirection()));
}
inline std::unique_ptr<gp_Dir> geom_circle_y_direction(const HandleGeom_Circle &circle) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(circle->Position().YDirection()));
}

// Ellipse properties
inline const gp_Pnt &geom_ellipse_location(const HandleGeom_Ellipse &ellipse) { return ellipse->Location(); }
inline const gp_Ax1 &geom_ellipse_axis(const HandleGeom_Ellipse &ellipse) { return ellipse->Axis(); }
inline double geom_ellipse_major_radius(const HandleGeom_Ellipse &ellipse) { return ellipse->MajorRadius(); }
inline double geom_ellipse_minor_radius(const HandleGeom_Ellipse &ellipse) { return ellipse->MinorRadius(); }
inline std::unique_ptr<gp_Dir> geom_ellipse_x_direction(const HandleGeom_Ellipse &ellipse) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(ellipse->Position().XDirection()));
}
inline std::unique_ptr<gp_Dir> geom_ellipse_y_direction(const HandleGeom_Ellipse &ellipse) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(ellipse->Position().YDirection()));
}

// BSpline curve properties
inline int geom_bspline_curve_nb_poles(const HandleGeomBSplineCurve &bspline) { return bspline->NbPoles(); }
inline int geom_bspline_curve_degree(const HandleGeomBSplineCurve &bspline) { return bspline->Degree(); }
inline bool geom_bspline_curve_is_rational(const HandleGeomBSplineCurve &bspline) { return bspline->IsRational(); }
inline bool geom_bspline_curve_is_periodic(const HandleGeomBSplineCurve &bspline) { return bspline->IsPeriodic(); }
inline void geom_bspline_curve_set_not_periodic(HandleGeomBSplineCurve &bspline) { bspline->SetNotPeriodic(); }
inline void geom_bspline_curve_segment(HandleGeomBSplineCurve &bspline, Standard_Real u1, Standard_Real u2) { bspline->Segment(u1, u2); }

// BSpline curve knots and multiplicities
inline int geom_bspline_curve_nb_knots(const HandleGeomBSplineCurve &bspline) { return bspline->NbKnots(); }
inline double geom_bspline_curve_knot(const HandleGeomBSplineCurve &bspline, int index) { return bspline->Knot(index); }
inline int geom_bspline_curve_multiplicity(const HandleGeomBSplineCurve &bspline, int index) { return bspline->Multiplicity(index); }

// BSpline curve poles (control points)
inline const gp_Pnt &geom_bspline_curve_pole(const HandleGeomBSplineCurve &bspline, int index) { return bspline->Pole(index); }
inline double geom_bspline_curve_weight(const HandleGeomBSplineCurve &bspline, int index) { return bspline->Weight(index); }

// Bezier curve properties
inline int geom_bezier_curve_nb_poles(const HandleGeomBezierCurve &bezier) { return bezier->NbPoles(); }
inline int geom_bezier_curve_degree(const HandleGeomBezierCurve &bezier) { return bezier->Degree(); }
inline bool geom_bezier_curve_is_rational(const HandleGeomBezierCurve &bezier) { return bezier->IsRational(); }

// Bezier curve poles (control points)
inline const gp_Pnt &geom_bezier_curve_pole(const HandleGeomBezierCurve &bezier, int index) { return bezier->Pole(index); }
inline double geom_bezier_curve_weight(const HandleGeomBezierCurve &bezier, int index) { return bezier->Weight(index); }

// Hyperbola properties
inline const gp_Pnt &geom_hyperbola_location(const HandleGeom_Hyperbola &hyperbola) { return hyperbola->Location(); }
inline const gp_Ax1 &geom_hyperbola_axis(const HandleGeom_Hyperbola &hyperbola) { return hyperbola->Axis(); }
inline double geom_hyperbola_major_radius(const HandleGeom_Hyperbola &hyperbola) { return hyperbola->MajorRadius(); }
inline double geom_hyperbola_minor_radius(const HandleGeom_Hyperbola &hyperbola) { return hyperbola->MinorRadius(); }

// Parabola properties
inline const gp_Pnt &geom_parabola_location(const HandleGeom_Parabola &parabola) { return parabola->Location(); }
inline const gp_Ax1 &geom_parabola_axis(const HandleGeom_Parabola &parabola) { return parabola->Axis(); }
inline double geom_parabola_focal(const HandleGeom_Parabola &parabola) { return parabola->Focal(); }

// OffsetCurve properties
inline std::unique_ptr<HandleGeomCurve> geom_offset_curve_basis_curve(const HandleGeom_OffsetCurve &offset) {
  return std::unique_ptr<HandleGeomCurve>(new HandleGeomCurve(offset->BasisCurve()));
}
inline double geom_offset_curve_offset(const HandleGeom_OffsetCurve &offset) { return offset->Offset(); }

// TrimmedCurve properties
inline std::unique_ptr<HandleGeomCurve> geom_trimmed_curve_basis_curve(const HandleGeomTrimmedCurve &trimmed) {
  return std::unique_ptr<HandleGeomCurve>(new HandleGeomCurve(trimmed->BasisCurve()));
}
inline double geom_trimmed_curve_first_parameter(const HandleGeomTrimmedCurve &trimmed) { return trimmed->FirstParameter(); }
inline double geom_trimmed_curve_last_parameter(const HandleGeomTrimmedCurve &trimmed) { return trimmed->LastParameter(); }

inline std::unique_ptr<HandleGeom_CylindricalSurface> Geom_CylindricalSurface_ctor(const gp_Ax3 &axis, double radius) {
  return std::unique_ptr<HandleGeom_CylindricalSurface>(
      new opencascade::handle<Geom_CylindricalSurface>(new Geom_CylindricalSurface(axis, radius)));
}

inline std::unique_ptr<HandleGeomBSplineCurve> GeomAPI_Interpolate_Curve(const GeomAPI_Interpolate &interpolate) {
  return std::unique_ptr<HandleGeomBSplineCurve>(new opencascade::handle<Geom_BSplineCurve>(interpolate.Curve()));
}

inline std::unique_ptr<HandleGeomBezierCurve>
Geom_BezierCurve_to_handle(std::unique_ptr<Geom_BezierCurve> bezier_curve) {
  return std::unique_ptr<HandleGeomBezierCurve>(new HandleGeomBezierCurve(bezier_curve.release()));
}

inline std::unique_ptr<HandleGeomSurface> cylinder_to_surface(const HandleGeom_CylindricalSurface &cylinder_handle) {
  return std::unique_ptr<HandleGeomSurface>(new opencascade::handle<Geom_Surface>(cylinder_handle));
}

inline std::unique_ptr<HandleGeomBezierSurface> Geom_BezierSurface_ctor(const TColgp_Array2OfPnt &poles) {
  return std::unique_ptr<HandleGeomBezierSurface>(
      new opencascade::handle<Geom_BezierSurface>(new Geom_BezierSurface(poles)));
}

inline std::unique_ptr<HandleGeomSurface> bezier_to_surface(const HandleGeomBezierSurface &bezier_handle) {
  return std::unique_ptr<HandleGeomSurface>(new opencascade::handle<Geom_Surface>(bezier_handle));
}

inline std::unique_ptr<HandleGeomSurface> bspline_surface_to_surface(const HandleGeom_BSplineSurface &bspline_handle) {
  return std::unique_ptr<HandleGeomSurface>(new opencascade::handle<Geom_Surface>(bspline_handle));
}

inline std::unique_ptr<HandleGeom2d_Ellipse> Geom2d_Ellipse_ctor(const gp_Ax2d &axis, double major_radius,
                                                                 double minor_radius) {
  return std::unique_ptr<HandleGeom2d_Ellipse>(
      new opencascade::handle<Geom2d_Ellipse>(new Geom2d_Ellipse(axis, major_radius, minor_radius)));
}

inline std::unique_ptr<HandleGeom2d_Curve> ellipse_to_HandleGeom2d_Curve(const HandleGeom2d_Ellipse &ellipse_handle) {
  return std::unique_ptr<HandleGeom2d_Curve>(new opencascade::handle<Geom2d_Curve>(ellipse_handle));
}

inline std::unique_ptr<HandleGeom2d_TrimmedCurve> Geom2d_TrimmedCurve_ctor(const HandleGeom2d_Curve &curve, double u1,
                                                                           double u2) {
  return std::unique_ptr<HandleGeom2d_TrimmedCurve>(
      new opencascade::handle<Geom2d_TrimmedCurve>(new Geom2d_TrimmedCurve(curve, u1, u2)));
}

inline std::unique_ptr<HandleGeom2d_Curve>
HandleGeom2d_TrimmedCurve_to_curve(const HandleGeom2d_TrimmedCurve &trimmed_curve) {
  return std::unique_ptr<HandleGeom2d_Curve>(new opencascade::handle<Geom2d_Curve>(trimmed_curve));
}

inline std::unique_ptr<gp_Pnt2d> ellipse_value(const HandleGeom2d_Ellipse &ellipse, double u) {
  return std::unique_ptr<gp_Pnt2d>(new gp_Pnt2d(ellipse->Value(u)));
}

// Segment Stuff
inline std::unique_ptr<HandleGeomTrimmedCurve> GC_MakeSegment_Value(const GC_MakeSegment &segment) {
  return std::unique_ptr<HandleGeomTrimmedCurve>(new opencascade::handle<Geom_TrimmedCurve>(segment.Value()));
}

inline std::unique_ptr<HandleGeom2d_TrimmedCurve> GCE2d_MakeSegment_point_point(const gp_Pnt2d &p1,
                                                                                const gp_Pnt2d &p2) {
  return std::unique_ptr<HandleGeom2d_TrimmedCurve>(
      new opencascade::handle<Geom2d_TrimmedCurve>(GCE2d_MakeSegment(p1, p2)));
}

// Arc stuff
inline std::unique_ptr<HandleGeomTrimmedCurve> GC_MakeArcOfCircle_Value(const GC_MakeArcOfCircle &arc) {
  return std::unique_ptr<HandleGeomTrimmedCurve>(new opencascade::handle<Geom_TrimmedCurve>(arc.Value()));
}

inline std::unique_ptr<gp_Pnt> BRepAdaptor_Curve_value(const BRepAdaptor_Curve &curve, const Standard_Real U) {
  return std::unique_ptr<gp_Pnt>(new gp_Pnt(curve.Value(U)));
}

inline void BRepAdaptor_Curve_D1(const BRepAdaptor_Curve &curve, const Standard_Real U, gp_Pnt &P, gp_Vec &V1) {
  curve.D1(U, P, V1);
}

inline Standard_Real BRepAdaptor_Curve_length(const BRepAdaptor_Curve &curve) {
  return GCPnts_AbscissaPoint::Length(curve);
}

inline bool BRepAdaptor_Curve_is_closed(const BRepAdaptor_Curve &curve) {
  return curve.IsClosed();
}

inline std::unique_ptr<HandleGeomBSplineCurve> GeomConvert_CurveToBSplineCurve(
    const HandleGeomCurve &curve) {
  try {
    Handle(Geom_BSplineCurve) converted = GeomConvert::CurveToBSplineCurve(curve);
    if (converted.IsNull()) {
      return std::unique_ptr<HandleGeomBSplineCurve>();
    }
    return std::unique_ptr<HandleGeomBSplineCurve>(new HandleGeomBSplineCurve(converted));
  } catch (...) {
    return std::unique_ptr<HandleGeomBSplineCurve>();
  }
}

// GeomAPI_ProjectPointOnCurve
inline std::unique_ptr<GeomAPI_ProjectPointOnCurve> GeomAPI_ProjectPointOnCurve_ctor(
    const gp_Pnt &point, const HandleGeomCurve &curve) {
  return std::unique_ptr<GeomAPI_ProjectPointOnCurve>(
      new GeomAPI_ProjectPointOnCurve(point, curve));
}

inline Standard_Integer GeomAPI_ProjectPointOnCurve_NbPoints(
    const GeomAPI_ProjectPointOnCurve &proj) {
  return proj.NbPoints();
}

inline std::unique_ptr<gp_Pnt> GeomAPI_ProjectPointOnCurve_NearestPoint(
    const GeomAPI_ProjectPointOnCurve &proj) {
  return std::unique_ptr<gp_Pnt>(new gp_Pnt(proj.NearestPoint()));
}

inline Standard_Real GeomAPI_ProjectPointOnCurve_LowerDistanceParameter(
    const GeomAPI_ProjectPointOnCurve &proj) {
  return proj.LowerDistanceParameter();
}

// Geom_TrimmedCurve construction
inline std::unique_ptr<HandleGeomTrimmedCurve> Geom_TrimmedCurve_ctor(
    const HandleGeomCurve &curve, const Standard_Real u1, const Standard_Real u2) {
  return std::unique_ptr<HandleGeomTrimmedCurve>(
      new HandleGeomTrimmedCurve(new Geom_TrimmedCurve(curve, u1, u2)));
}

// Geom_OffsetCurve construction
inline std::unique_ptr<HandleGeom_OffsetCurve> Geom_OffsetCurve_ctor(
    const HandleGeomCurve &curve, const Standard_Real offset, const gp_Dir &direction) {
  return std::unique_ptr<HandleGeom_OffsetCurve>(
      new HandleGeom_OffsetCurve(new Geom_OffsetCurve(curve, offset, direction)));
}

// BRepLib
inline bool BRepLibBuildCurves3d(const TopoDS_Shape &shape) { return BRepLib::BuildCurves3d(shape); }

inline void MakeThickSolidByJoin(BRepOffsetAPI_MakeThickSolid &make_thick_solid, const TopoDS_Shape &shape,
                                 const TopTools_ListOfShape &closing_faces, const Standard_Real offset,
                                 const Standard_Real tolerance) {
  make_thick_solid.MakeThickSolidByJoin(shape, closing_faces, offset, tolerance);
}

// Geometric processing
inline const gp_Ax1 &gp_OX() { return gp::OX(); }
inline const gp_Ax1 &gp_OY() { return gp::OY(); }
inline const gp_Ax1 &gp_OZ() { return gp::OZ(); }

inline const gp_Dir &gp_DZ() { return gp::DZ(); }

inline std::unique_ptr<gp_Ax1> gp_Ax1_ctor(const gp_Pnt &origin, const gp_Dir &main_dir) {
  return std::unique_ptr<gp_Ax1>(new gp_Ax1(origin, main_dir));
}

inline const gp_Pnt &gp_Ax1_location(const gp_Ax1 &axis) { return axis.Location(); }
inline const gp_Dir &gp_Ax1_direction(const gp_Ax1 &axis) { return axis.Direction(); }

inline std::unique_ptr<gp_Ax2> gp_Ax2_ctor(const gp_Pnt &origin, const gp_Dir &main_dir) {
  return std::unique_ptr<gp_Ax2>(new gp_Ax2(origin, main_dir));
}

inline std::unique_ptr<gp_Ax3> gp_Ax3_from_gp_Ax2(const gp_Ax2 &axis) {
  return std::unique_ptr<gp_Ax3>(new gp_Ax3(axis));
}

inline std::unique_ptr<gp_Dir> gp_Dir_ctor(double x, double y, double z) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(x, y, z));
}

inline std::unique_ptr<gp_Dir2d> gp_Dir2d_ctor(double x, double y) {
  return std::unique_ptr<gp_Dir2d>(new gp_Dir2d(x, y));
}

inline std::unique_ptr<gp_Ax2d> gp_Ax2d_ctor(const gp_Pnt2d &point, const gp_Dir2d &dir) {
  return std::unique_ptr<gp_Ax2d>(new gp_Ax2d(point, dir));
}

// Law_Function stuff
inline std::unique_ptr<HandleLawFunction> Law_Function_to_handle(std::unique_ptr<Law_Function> law_function) {
  return std::unique_ptr<HandleLawFunction>(new HandleLawFunction(law_function.release()));
}

// Law_Interpol stuff
inline std::unique_ptr<Law_Function> Law_Interpol_into_Law_Function(std::unique_ptr<Law_Interpol> law_interpol) {
  return std::unique_ptr<Law_Function>(law_interpol.release());
}

// Shape stuff
inline const TopoDS_Vertex &TopoDS_cast_to_vertex(const TopoDS_Shape &shape) { return TopoDS::Vertex(shape); }
inline const TopoDS_Edge &TopoDS_cast_to_edge(const TopoDS_Shape &shape) { return TopoDS::Edge(shape); }
inline const TopoDS_Wire &TopoDS_cast_to_wire(const TopoDS_Shape &shape) { return TopoDS::Wire(shape); }
inline const TopoDS_Face &TopoDS_cast_to_face(const TopoDS_Shape &shape) { return TopoDS::Face(shape); }
inline const TopoDS_Shell &TopoDS_cast_to_shell(const TopoDS_Shape &shape) { return TopoDS::Shell(shape); }
inline const TopoDS_Solid &TopoDS_cast_to_solid(const TopoDS_Shape &shape) { return TopoDS::Solid(shape); }
inline const TopoDS_Compound &TopoDS_cast_to_compound(const TopoDS_Shape &shape) { return TopoDS::Compound(shape); }

inline const TopoDS_Shape &cast_vertex_to_shape(const TopoDS_Vertex &vertex) { return vertex; }
inline const TopoDS_Shape &cast_edge_to_shape(const TopoDS_Edge &edge) { return edge; }
inline const TopoDS_Shape &cast_wire_to_shape(const TopoDS_Wire &wire) { return wire; }
inline const TopoDS_Shape &cast_face_to_shape(const TopoDS_Face &face) { return face; }
inline const TopoDS_Shape &cast_shell_to_shape(const TopoDS_Shell &shell) { return shell; }
inline const TopoDS_Shape &cast_solid_to_shape(const TopoDS_Solid &solid) { return solid; }
inline const TopoDS_Shape &cast_compound_to_shape(const TopoDS_Compound &compound) { return compound; }

// Get unique ID for a shape based on its TShape pointer
inline uintptr_t TopoDS_Shape_get_tshape_id(const TopoDS_Shape &shape) {
  return reinterpret_cast<uintptr_t>(shape.TShape().get());
}

// Compound shapes
inline std::unique_ptr<TopoDS_Shape> TopoDS_Compound_as_shape(std::unique_ptr<TopoDS_Compound> compound) {
  return compound;
}

inline std::unique_ptr<TopoDS_Shape> TopoDS_Shell_as_shape(std::unique_ptr<TopoDS_Shell> shell) { return shell; }

inline const TopoDS_Builder &BRep_Builder_upcast_to_topods_builder(const BRep_Builder &builder) { return builder; }

// Transforms
inline std::unique_ptr<HandleGeomSurface> BRep_Tool_Surface(const TopoDS_Face &face) {
  return std::unique_ptr<HandleGeomSurface>(new opencascade::handle<Geom_Surface>(BRep_Tool::Surface(face)));
}

inline std::unique_ptr<HandleGeomCurve> BRep_Tool_Curve(const TopoDS_Edge &edge, Standard_Real &first,
                                                        Standard_Real &last) {
  return std::unique_ptr<HandleGeomCurve>(new opencascade::handle<Geom_Curve>(BRep_Tool::Curve(edge, first, last)));
}

inline std::unique_ptr<gp_Pnt> BRep_Tool_Pnt(const TopoDS_Vertex &vertex) {
  return std::unique_ptr<gp_Pnt>(new gp_Pnt(BRep_Tool::Pnt(vertex)));
}

inline std::unique_ptr<HandleGeom2d_Curve> BRep_Tool_CurveOnSurface(const TopoDS_Edge &edge, const TopoDS_Face &face,
                                                                     Standard_Real &first, Standard_Real &last) {
  return std::unique_ptr<HandleGeom2d_Curve>(
      new opencascade::handle<Geom2d_Curve>(BRep_Tool::CurveOnSurface(edge, face, first, last)));
}

inline std::unique_ptr<gp_Pnt2d> Geom2d_Curve_Value(const HandleGeom2d_Curve &curve, Standard_Real u) {
  return std::unique_ptr<gp_Pnt2d>(new gp_Pnt2d(curve->Value(u)));
}

inline bool Geom2d_Curve_IsNull(const HandleGeom2d_Curve &curve) {
  return curve.IsNull();
}

// Geom2d_Curve type information
inline rust::String Geom2d_Curve_DynamicType(const HandleGeom2d_Curve &curve) {
  return rust::String(curve->DynamicType()->Name());
}

// Geom2d_Line
inline std::unique_ptr<HandleGeom2d_Line> cast_geom2d_curve_to_line(const HandleGeom2d_Curve &curve) {
  return std::unique_ptr<HandleGeom2d_Line>(new opencascade::handle<Geom2d_Line>(
      opencascade::handle<Geom2d_Line>::DownCast(curve)));
}

inline bool HandleGeom2d_Line_IsNull(const HandleGeom2d_Line &handle) {
  return handle.IsNull();
}

inline std::unique_ptr<gp_Pnt2d> geom2d_line_location(const HandleGeom2d_Line &line) {
  return std::unique_ptr<gp_Pnt2d>(new gp_Pnt2d(line->Location()));
}

inline std::unique_ptr<gp_Dir2d> geom2d_line_direction(const HandleGeom2d_Line &line) {
  return std::unique_ptr<gp_Dir2d>(new gp_Dir2d(line->Direction()));
}

inline Standard_Real gp_Dir2d_X(const gp_Dir2d &dir) {
  return dir.X();
}

inline Standard_Real gp_Dir2d_Y(const gp_Dir2d &dir) {
  return dir.Y();
}

// Geom2d_Circle
inline std::unique_ptr<HandleGeom2d_Circle> cast_geom2d_curve_to_circle(const HandleGeom2d_Curve &curve) {
  return std::unique_ptr<HandleGeom2d_Circle>(new opencascade::handle<Geom2d_Circle>(
      opencascade::handle<Geom2d_Circle>::DownCast(curve)));
}

inline bool HandleGeom2d_Circle_IsNull(const HandleGeom2d_Circle &handle) {
  return handle.IsNull();
}

inline std::unique_ptr<gp_Pnt2d> geom2d_circle_location(const HandleGeom2d_Circle &circle) {
  return std::unique_ptr<gp_Pnt2d>(new gp_Pnt2d(circle->Location()));
}

inline Standard_Real geom2d_circle_radius(const HandleGeom2d_Circle &circle) {
  return circle->Radius();
}

inline std::unique_ptr<gp_Dir2d> geom2d_circle_x_direction(const HandleGeom2d_Circle &circle) {
  return std::unique_ptr<gp_Dir2d>(new gp_Dir2d(circle->Position().XDirection()));
}

inline std::unique_ptr<gp_Dir2d> geom2d_circle_y_direction(const HandleGeom2d_Circle &circle) {
  return std::unique_ptr<gp_Dir2d>(new gp_Dir2d(circle->Position().YDirection()));
}

// Geom2d_Ellipse
inline std::unique_ptr<HandleGeom2d_Ellipse> cast_geom2d_curve_to_ellipse(const HandleGeom2d_Curve &curve) {
  return std::unique_ptr<HandleGeom2d_Ellipse>(new opencascade::handle<Geom2d_Ellipse>(
      opencascade::handle<Geom2d_Ellipse>::DownCast(curve)));
}

inline bool HandleGeom2d_Ellipse_IsNull(const HandleGeom2d_Ellipse &handle) {
  return handle.IsNull();
}

inline std::unique_ptr<gp_Pnt2d> geom2d_ellipse_location(const HandleGeom2d_Ellipse &ellipse) {
  return std::unique_ptr<gp_Pnt2d>(new gp_Pnt2d(ellipse->Location()));
}

inline Standard_Real geom2d_ellipse_major_radius(const HandleGeom2d_Ellipse &ellipse) {
  return ellipse->MajorRadius();
}

inline Standard_Real geom2d_ellipse_minor_radius(const HandleGeom2d_Ellipse &ellipse) {
  return ellipse->MinorRadius();
}

inline std::unique_ptr<gp_Dir2d> geom2d_ellipse_x_direction(const HandleGeom2d_Ellipse &ellipse) {
  return std::unique_ptr<gp_Dir2d>(new gp_Dir2d(ellipse->Position().XDirection()));
}

inline std::unique_ptr<gp_Dir2d> geom2d_ellipse_y_direction(const HandleGeom2d_Ellipse &ellipse) {
  return std::unique_ptr<gp_Dir2d>(new gp_Dir2d(ellipse->Position().YDirection()));
}

// Geom2d_BSplineCurve
inline std::unique_ptr<HandleGeom2d_BSplineCurve> cast_geom2d_curve_to_bspline(const HandleGeom2d_Curve &curve) {
  return std::unique_ptr<HandleGeom2d_BSplineCurve>(new opencascade::handle<Geom2d_BSplineCurve>(
      opencascade::handle<Geom2d_BSplineCurve>::DownCast(curve)));
}

inline bool HandleGeom2d_BSplineCurve_IsNull(const HandleGeom2d_BSplineCurve &handle) {
  return handle.IsNull();
}

inline Standard_Integer geom2d_bspline_curve_nb_poles(const HandleGeom2d_BSplineCurve &bspline) {
  return bspline->NbPoles();
}

inline Standard_Integer geom2d_bspline_curve_degree(const HandleGeom2d_BSplineCurve &bspline) {
  return bspline->Degree();
}

inline bool geom2d_bspline_curve_is_rational(const HandleGeom2d_BSplineCurve &bspline) {
  return bspline->IsRational();
}

inline bool geom2d_bspline_curve_is_periodic(const HandleGeom2d_BSplineCurve &bspline) {
  return bspline->IsPeriodic();
}

inline void geom2d_bspline_curve_set_not_periodic(HandleGeom2d_BSplineCurve &bspline) {
  bspline->SetNotPeriodic();
}

inline Standard_Integer geom2d_bspline_curve_nb_knots(const HandleGeom2d_BSplineCurve &bspline) {
  return bspline->NbKnots();
}

inline Standard_Real geom2d_bspline_curve_knot(const HandleGeom2d_BSplineCurve &bspline, Standard_Integer index) {
  return bspline->Knot(index);
}

inline Standard_Integer geom2d_bspline_curve_multiplicity(const HandleGeom2d_BSplineCurve &bspline, Standard_Integer index) {
  return bspline->Multiplicity(index);
}

inline std::unique_ptr<gp_Pnt2d> geom2d_bspline_curve_pole(const HandleGeom2d_BSplineCurve &bspline, Standard_Integer index) {
  return std::unique_ptr<gp_Pnt2d>(new gp_Pnt2d(bspline->Pole(index)));
}

inline Standard_Real geom2d_bspline_curve_weight(const HandleGeom2d_BSplineCurve &bspline, Standard_Integer index) {
  return bspline->Weight(index);
}

// Geom2d_BezierCurve
inline std::unique_ptr<HandleGeom2d_BezierCurve> cast_geom2d_curve_to_bezier(const HandleGeom2d_Curve &curve) {
  return std::unique_ptr<HandleGeom2d_BezierCurve>(new opencascade::handle<Geom2d_BezierCurve>(
      opencascade::handle<Geom2d_BezierCurve>::DownCast(curve)));
}

inline bool HandleGeom2d_BezierCurve_IsNull(const HandleGeom2d_BezierCurve &handle) {
  return handle.IsNull();
}

inline Standard_Integer geom2d_bezier_curve_nb_poles(const HandleGeom2d_BezierCurve &bezier) {
  return bezier->NbPoles();
}

inline Standard_Integer geom2d_bezier_curve_degree(const HandleGeom2d_BezierCurve &bezier) {
  return bezier->Degree();
}

inline bool geom2d_bezier_curve_is_rational(const HandleGeom2d_BezierCurve &bezier) {
  return bezier->IsRational();
}

inline std::unique_ptr<gp_Pnt2d> geom2d_bezier_curve_pole(const HandleGeom2d_BezierCurve &bezier, Standard_Integer index) {
  return std::unique_ptr<gp_Pnt2d>(new gp_Pnt2d(bezier->Pole(index)));
}

inline Standard_Real geom2d_bezier_curve_weight(const HandleGeom2d_BezierCurve &bezier, Standard_Integer index) {
  return bezier->Weight(index);
}

// Geom2d_TrimmedCurve
inline std::unique_ptr<HandleGeom2d_TrimmedCurve> cast_geom2d_curve_to_trimmed(const HandleGeom2d_Curve &curve) {
  return std::unique_ptr<HandleGeom2d_TrimmedCurve>(new opencascade::handle<Geom2d_TrimmedCurve>(
      opencascade::handle<Geom2d_TrimmedCurve>::DownCast(curve)));
}

inline bool HandleGeom2d_TrimmedCurve_IsNull(const HandleGeom2d_TrimmedCurve &handle) {
  return handle.IsNull();
}

inline std::unique_ptr<HandleGeom2d_Curve> geom2d_trimmed_curve_basis_curve(const HandleGeom2d_TrimmedCurve &trimmed) {
  return std::unique_ptr<HandleGeom2d_Curve>(new opencascade::handle<Geom2d_Curve>(trimmed->BasisCurve()));
}

inline Standard_Real geom2d_trimmed_curve_first_parameter(const HandleGeom2d_TrimmedCurve &trimmed) {
  return trimmed->FirstParameter();
}

inline Standard_Real geom2d_trimmed_curve_last_parameter(const HandleGeom2d_TrimmedCurve &trimmed) {
  return trimmed->LastParameter();
}

inline std::unique_ptr<gp_Trsf> TopLoc_Location_Transformation(const TopLoc_Location &location) {
  return std::unique_ptr<gp_Trsf>(new gp_Trsf(location.Transformation()));
}

inline std::unique_ptr<HandlePoly_Triangulation>
HandlePoly_Triangulation_ctor(std::unique_ptr<Poly_Triangulation> triangulation) {
  return std::unique_ptr<HandlePoly_Triangulation>(new HandlePoly_Triangulation(triangulation.release()));
}

inline std::unique_ptr<HandlePoly_Triangulation> BRep_Tool_Triangulation(const TopoDS_Face &face,
                                                                         TopLoc_Location &location) {
  return std::unique_ptr<HandlePoly_Triangulation>(
      new opencascade::handle<Poly_Triangulation>(BRep_Tool::Triangulation(face, location)));
}

inline std::unique_ptr<TopoDS_Shape> ExplorerCurrentShape(const TopExp_Explorer &explorer) {
  return std::unique_ptr<TopoDS_Shape>(new TopoDS_Shape(explorer.Current()));
}

inline std::unique_ptr<TopoDS_Vertex> TopExp_FirstVertex(const TopoDS_Edge &edge) {
  return std::unique_ptr<TopoDS_Vertex>(new TopoDS_Vertex(TopExp::FirstVertex(edge)));
}

inline std::unique_ptr<TopoDS_Vertex> TopExp_LastVertex(const TopoDS_Edge &edge) {
  return std::unique_ptr<TopoDS_Vertex>(new TopoDS_Vertex(TopExp::LastVertex(edge)));
}

inline void TopExp_EdgeVertices(const TopoDS_Edge &edge, TopoDS_Vertex &vertex1, TopoDS_Vertex &vertex2) {
  return TopExp::Vertices(edge, vertex1, vertex2);
}

inline void TopExp_WireVertices(const TopoDS_Wire &wire, TopoDS_Vertex &vertex1, TopoDS_Vertex &vertex2) {
  return TopExp::Vertices(wire, vertex1, vertex2);
}

inline bool TopExp_CommonVertex(const TopoDS_Edge &edge1, const TopoDS_Edge &edge2, TopoDS_Vertex &vertex) {
  return TopExp::CommonVertex(edge1, edge2, vertex);
}

inline std::unique_ptr<TopoDS_Face> BRepIntCurveSurface_Inter_face(const BRepIntCurveSurface_Inter &intersector) {
  return std::unique_ptr<TopoDS_Face>(new TopoDS_Face(intersector.Face()));
}

inline std::unique_ptr<gp_Pnt> BRepIntCurveSurface_Inter_point(const BRepIntCurveSurface_Inter &intersector) {
  return std::unique_ptr<gp_Pnt>(new gp_Pnt(intersector.Pnt()));
}

// BRepFeat
inline std::unique_ptr<BRepFeat_MakeCylindricalHole> BRepFeat_MakeCylindricalHole_ctor() {
  return std::unique_ptr<BRepFeat_MakeCylindricalHole>(new BRepFeat_MakeCylindricalHole());
}

// Data Import
inline IFSelect_ReturnStatus read_step(STEPControl_Reader &reader, rust::String theFileName) {
  return reader.ReadFile(theFileName.c_str());
}

inline IFSelect_ReturnStatus read_step_from_bytes(STEPControl_Reader &reader, rust::Slice<const uint8_t> data) {
  // Create a string from the byte data
  std::string str(reinterpret_cast<const char*>(data.data()), data.size());

  // Create an input string stream from the string
  std::istringstream stream(str);

  // Read from the stream
  return reader.ReadStream("memory_stream.step", stream);
}

inline IFSelect_ReturnStatus read_iges(IGESControl_Reader &reader, rust::String theFileName) {
  return reader.ReadFile(theFileName.c_str());
}

inline IFSelect_ReturnStatus read_iges_from_bytes(IGESControl_Reader &reader, rust::Slice<const uint8_t> data) {
  // IGESControl_Reader does not support ReadStream, so use a temporary file.
#ifdef _WIN32
  char tmp_path[MAX_PATH];
  if (GetTempPathA(MAX_PATH, tmp_path) == 0) {
    return IFSelect_ReturnStatus::IFSelect_RetFail;
  }
  char tmp_file[MAX_PATH];
  if (GetTempFileNameA(tmp_path, "iges", 0, tmp_file) == 0) {
    return IFSelect_ReturnStatus::IFSelect_RetFail;
  }
#else
  char tmp_file[] = "/tmp/occ_iges_XXXXXX";
  int fd = mkstemp(tmp_file);
  if (fd == -1) {
    return IFSelect_ReturnStatus::IFSelect_RetFail;
  }
  close(fd);
#endif
  {
    std::ofstream ofs(tmp_file, std::ios::binary);
    if (!ofs) {
      return IFSelect_ReturnStatus::IFSelect_RetFail;
    }
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
  }
  auto status = reader.ReadFile(tmp_file);
  std::remove(tmp_file);
  return status;
}

inline std::unique_ptr<TopoDS_Shape> one_shape_step(const STEPControl_Reader &reader) {
  return std::unique_ptr<TopoDS_Shape>(new TopoDS_Shape(reader.OneShape()));
}

inline std::unique_ptr<TopoDS_Shape> one_shape_iges(const IGESControl_Reader &reader) {
  return std::unique_ptr<TopoDS_Shape>(new TopoDS_Shape(reader.OneShape()));
}

// Data Export
inline IFSelect_ReturnStatus transfer_shape(STEPControl_Writer &writer, const TopoDS_Shape &theShape) {
  return writer.Transfer(theShape, STEPControl_AsIs);
}

inline void compute_model(IGESControl_Writer &writer) { writer.ComputeModel(); }

inline bool add_shape(IGESControl_Writer &writer, const TopoDS_Shape &theShape) { return writer.AddShape(theShape); }

inline IFSelect_ReturnStatus write_step(STEPControl_Writer &writer, rust::String theFileName) {
  return writer.Write(theFileName.c_str());
}

inline bool write_iges(IGESControl_Writer &writer, rust::String theFileName) {
  return writer.Write(theFileName.c_str());
}

inline bool write_stl(StlAPI_Writer &writer, const TopoDS_Shape &theShape, rust::String theFileName) {
  return writer.Write(theShape, theFileName.c_str());
}

inline std::unique_ptr<gp_Dir> Poly_Triangulation_Normal(const Poly_Triangulation &triangulation,
                                                         const Standard_Integer index) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(triangulation.Normal(index)));
}

inline std::unique_ptr<gp_Pnt> Poly_Triangulation_Node(const Poly_Triangulation &triangulation,
                                                       const Standard_Integer index) {
  return std::unique_ptr<gp_Pnt>(new gp_Pnt(triangulation.Node(index)));
}

inline std::unique_ptr<gp_Pnt2d> Poly_Triangulation_UV(const Poly_Triangulation &triangulation,
                                                       const Standard_Integer index) {
  return std::unique_ptr<gp_Pnt2d>(new gp_Pnt2d(triangulation.UVNode(index)));
}

inline void compute_normals(const TopoDS_Face &face, const Handle(Poly_Triangulation) & triangulation) {
  BRepLib_ToolTriangulatedShape::ComputeNormals(face, triangulation);
}

// Shape Properties
inline std::unique_ptr<gp_Pnt> GProp_GProps_CentreOfMass(const GProp_GProps &props) {
  return std::unique_ptr<gp_Pnt>(new gp_Pnt(props.CentreOfMass()));
}

inline void BRepGProp_LinearProperties(const TopoDS_Shape &shape, GProp_GProps &props) {
  BRepGProp::LinearProperties(shape, props);
}

inline void BRepGProp_SurfaceProperties(const TopoDS_Shape &shape, GProp_GProps &props) {
  BRepGProp::SurfaceProperties(shape, props);
}

inline void BRepGProp_VolumeProperties(const TopoDS_Shape &shape, GProp_GProps &props) {
  BRepGProp::VolumeProperties(shape, props);
}

// Fillets
inline std::unique_ptr<TopoDS_Edge> BRepFilletAPI_MakeFillet2d_add_fillet(BRepFilletAPI_MakeFillet2d &make_fillet,
                                                                          const TopoDS_Vertex &vertex,
                                                                          Standard_Real radius) {
  return std::unique_ptr<TopoDS_Edge>(new TopoDS_Edge(make_fillet.AddFillet(vertex, radius)));
}

// Chamfers
inline std::unique_ptr<TopoDS_Edge>
BRepFilletAPI_MakeFillet2d_add_chamfer(BRepFilletAPI_MakeFillet2d &make_fillet, const TopoDS_Edge &edge1,
                                       const TopoDS_Edge &edge2, const Standard_Real dist1, const Standard_Real dist2) {
  return std::unique_ptr<TopoDS_Edge>(new TopoDS_Edge(make_fillet.AddChamfer(edge1, edge2, dist1, dist2)));
}

inline std::unique_ptr<TopoDS_Edge>
BRepFilletAPI_MakeFillet2d_add_chamfer_angle(BRepFilletAPI_MakeFillet2d &make_fillet, const TopoDS_Edge &edge,
                                             const TopoDS_Vertex &vertex, const Standard_Real dist,
                                             const Standard_Real angle) {
  return std::unique_ptr<TopoDS_Edge>(new TopoDS_Edge(make_fillet.AddChamfer(edge, vertex, dist, angle)));
}

// BRepTools
inline std::unique_ptr<TopoDS_Wire> outer_wire(const TopoDS_Face &face) {
  return std::unique_ptr<TopoDS_Wire>(new TopoDS_Wire(BRepTools::OuterWire(face)));
}

inline void face_uv_bounds(const TopoDS_Face &face, Standard_Real &u_min, Standard_Real &u_max,
                           Standard_Real &v_min, Standard_Real &v_max) {
  BRepTools::UVBounds(face, u_min, u_max, v_min, v_max);
}

inline bool are_shapes_same(const TopoDS_Shape &shape1, const TopoDS_Shape &shape2) {
  return shape1.IsSame(shape2);
}

// BRepTools_WireExplorer
inline std::unique_ptr<BRepTools_WireExplorer> BRepTools_WireExplorer_ctor(const TopoDS_Wire &wire) {
  return std::unique_ptr<BRepTools_WireExplorer>(new BRepTools_WireExplorer(wire));
}

inline bool BRepTools_WireExplorer_More(const BRepTools_WireExplorer &explorer) {
  return explorer.More();
}

inline void BRepTools_WireExplorer_Next(BRepTools_WireExplorer &explorer) {
  explorer.Next();
}

inline std::unique_ptr<TopoDS_Edge> BRepTools_WireExplorer_Current(const BRepTools_WireExplorer &explorer) {
  return std::unique_ptr<TopoDS_Edge>(new TopoDS_Edge(explorer.Current()));
}

inline TopAbs_Orientation BRepTools_WireExplorer_Orientation(const BRepTools_WireExplorer &explorer) {
  return explorer.Orientation();
}

inline std::unique_ptr<TopoDS_Vertex> BRepTools_WireExplorer_CurrentVertex(const BRepTools_WireExplorer &explorer) {
  return std::unique_ptr<TopoDS_Vertex>(new TopoDS_Vertex(explorer.CurrentVertex()));
}

inline void BRepTools_WireExplorer_Clear(BRepTools_WireExplorer &explorer) {
  explorer.Clear();
}

inline bool write_brep_text(const TopoDS_Shape &shape, rust::String path) {
  return BRepTools::Write(shape, path.c_str());
}

inline std::unique_ptr<TopoDS_Shape> read_brep_text(rust::String path) {
  BRep_Builder builder;
  auto shape = std::unique_ptr<TopoDS_Shape>(new TopoDS_Shape());
  if (BRepTools::Read(*shape, path.c_str(), builder)) {
    return shape;
  }
  return std::unique_ptr<TopoDS_Shape>(nullptr);
}

// BinTools
inline bool write_brep_bin(const TopoDS_Shape &shape, rust::String path) {
  return BinTools::Write(shape, path.c_str());
}

inline std::unique_ptr<TopoDS_Shape> read_brep_bin(rust::String path) {
  auto shape = std::unique_ptr<TopoDS_Shape>(new TopoDS_Shape());
  if (BinTools::Read(*shape, path.c_str())) {
    return shape;
  }
  return std::unique_ptr<TopoDS_Shape>(nullptr);
}


// Collections
inline void map_shapes(const TopoDS_Shape &S, const TopAbs_ShapeEnum T, TopTools_IndexedMapOfShape &M) {
  TopExp::MapShapes(S, T, M);
}

inline void map_shapes_and_ancestors(const TopoDS_Shape &S, const TopAbs_ShapeEnum TS, const TopAbs_ShapeEnum TA,
                                     TopTools_IndexedDataMapOfShapeListOfShape &M) {
  TopExp::MapShapesAndAncestors(S, TS, TA, M);
}

inline void map_shapes_and_unique_ancestors(const TopoDS_Shape &S, const TopAbs_ShapeEnum TS, const TopAbs_ShapeEnum TA,
                                            TopTools_IndexedDataMapOfShapeListOfShape &M) {
  TopExp::MapShapesAndUniqueAncestors(S, TS, TA, M);
}

inline std::unique_ptr<gp_Dir> TColgp_Array1OfDir_Value(const TColgp_Array1OfDir &array, Standard_Integer index) {
  return std::unique_ptr<gp_Dir>(new gp_Dir(array.Value(index)));
}

inline std::unique_ptr<gp_Pnt2d> TColgp_Array1OfPnt2d_Value(const TColgp_Array1OfPnt2d &array, Standard_Integer index) {
  return std::unique_ptr<gp_Pnt2d>(new gp_Pnt2d(array.Value(index)));
}

inline std::unique_ptr<gp_Pnt> TColgp_HArray1OfPnt_Value(const TColgp_HArray1OfPnt &array, Standard_Integer index) {
  return std::unique_ptr<gp_Pnt>(new gp_Pnt(array.Value(index)));
}

inline void connect_edges_to_wires(HandleTopTools_HSequenceOfShape &edges, const Standard_Real toler,
                                   const Standard_Boolean shared, HandleTopTools_HSequenceOfShape &wires) {
  ShapeAnalysis_FreeBounds::ConnectEdgesToWires(edges, toler, shared, wires);
}

inline std::unique_ptr<HandleTopTools_HSequenceOfShape> new_HandleTopTools_HSequenceOfShape() {
  auto sequence = new TopTools_HSequenceOfShape();
  auto handle = new opencascade::handle<TopTools_HSequenceOfShape>(sequence);

  return std::unique_ptr<HandleTopTools_HSequenceOfShape>(handle);
}

inline void TopTools_HSequenceOfShape_append(HandleTopTools_HSequenceOfShape &handle, const TopoDS_Shape &shape) {
  handle->Append(shape);
}

inline Standard_Integer TopTools_HSequenceOfShape_length(const HandleTopTools_HSequenceOfShape &handle) {
  return handle->Length();
}

inline const TopoDS_Shape &TopTools_HSequenceOfShape_value(const HandleTopTools_HSequenceOfShape &handle,
                                                           Standard_Integer index) {
  return handle->Value(index);
}

// BRep Algo API
inline std::unique_ptr<BRepAlgoAPI_BuilderAlgo>
cast_section_to_builderalgo(std::unique_ptr<BRepAlgoAPI_Section> section) {
  return section;
}
// namespace BRepAlgoAPI

// Bnd_Box
inline std::unique_ptr<Bnd_Box> Bnd_Box_ctor() { return std::unique_ptr<Bnd_Box>(new Bnd_Box()); }
inline std::unique_ptr<gp_Pnt> Bnd_Box_CornerMin(const Bnd_Box &box) {
  auto p = box.CornerMin();
  return std::unique_ptr<gp_Pnt>(new gp_Pnt(p));
}
inline std::unique_ptr<gp_Pnt> Bnd_Box_CornerMax(const Bnd_Box &box) {
  auto p = box.CornerMax();
  return std::unique_ptr<gp_Pnt>(new gp_Pnt(p));
}

// BRepBndLib
inline void BRepBndLib_Add(const TopoDS_Shape &shape, Bnd_Box &box, const Standard_Boolean useTriangulation) {
  BRepBndLib::Add(shape, box, useTriangulation);
}

// ============================================
// Array types for BSpline construction
// ============================================

// TColStd_Array1OfReal
inline std::unique_ptr<TColStd_Array1OfReal> TColStd_Array1OfReal_ctor(int lower, int upper) {
  return std::unique_ptr<TColStd_Array1OfReal>(new TColStd_Array1OfReal(lower, upper));
}
inline void TColStd_Array1OfReal_SetValue(TColStd_Array1OfReal &arr, int index, double value) {
  arr.SetValue(index, value);
}

// TColStd_Array1OfInteger
inline std::unique_ptr<TColStd_Array1OfInteger> TColStd_Array1OfInteger_ctor(int lower, int upper) {
  return std::unique_ptr<TColStd_Array1OfInteger>(new TColStd_Array1OfInteger(lower, upper));
}
inline void TColStd_Array1OfInteger_SetValue(TColStd_Array1OfInteger &arr, int index, int value) {
  arr.SetValue(index, value);
}

// TColStd_Array2OfReal (for surface weights)
inline std::unique_ptr<TColStd_Array2OfReal> TColStd_Array2OfReal_ctor(int row_lower, int row_upper, int col_lower, int col_upper) {
  return std::unique_ptr<TColStd_Array2OfReal>(new TColStd_Array2OfReal(row_lower, row_upper, col_lower, col_upper));
}
inline void TColStd_Array2OfReal_SetValue(TColStd_Array2OfReal &arr, int row, int col, double value) {
  arr.SetValue(row, col, value);
}

// TColgp_Array1OfPnt (non-handle, for curve poles)
inline std::unique_ptr<TColgp_Array1OfPnt> TColgp_Array1OfPnt_ctor(int lower, int upper) {
  return std::unique_ptr<TColgp_Array1OfPnt>(new TColgp_Array1OfPnt(lower, upper));
}
inline void TColgp_Array1OfPnt_SetValue(TColgp_Array1OfPnt &arr, int index, const gp_Pnt &pnt) {
  arr.SetValue(index, pnt);
}

// ============================================
// BSpline surface construction
// ============================================

inline std::unique_ptr<HandleGeom_BSplineSurface> Geom_BSplineSurface_ctor(
    const TColgp_Array2OfPnt &poles,
    const TColStd_Array1OfReal &u_knots,
    const TColStd_Array1OfReal &v_knots,
    const TColStd_Array1OfInteger &u_mults,
    const TColStd_Array1OfInteger &v_mults,
    int u_degree, int v_degree,
    bool u_periodic, bool v_periodic) {
  Handle(Geom_BSplineSurface) surf = new Geom_BSplineSurface(
      poles, u_knots, v_knots, u_mults, v_mults, u_degree, v_degree, u_periodic, v_periodic);
  return std::unique_ptr<HandleGeom_BSplineSurface>(new HandleGeom_BSplineSurface(surf));
}

inline std::unique_ptr<HandleGeom_BSplineSurface> Geom_BSplineSurface_ctor_weighted(
    const TColgp_Array2OfPnt &poles,
    const TColStd_Array2OfReal &weights,
    const TColStd_Array1OfReal &u_knots,
    const TColStd_Array1OfReal &v_knots,
    const TColStd_Array1OfInteger &u_mults,
    const TColStd_Array1OfInteger &v_mults,
    int u_degree, int v_degree,
    bool u_periodic, bool v_periodic) {
  Handle(Geom_BSplineSurface) surf = new Geom_BSplineSurface(
      poles, weights, u_knots, v_knots, u_mults, v_mults, u_degree, v_degree, u_periodic, v_periodic);
  return std::unique_ptr<HandleGeom_BSplineSurface>(new HandleGeom_BSplineSurface(surf));
}

inline std::unique_ptr<HandleGeomSurface> bspline_surface_to_geom_surface(const HandleGeom_BSplineSurface &bspline) {
  Handle(Geom_Surface) surface = bspline;
  return std::unique_ptr<HandleGeomSurface>(new HandleGeomSurface(surface));
}

// ============================================
// BSpline curve construction
// ============================================

inline std::unique_ptr<HandleGeomBSplineCurve> Geom_BSplineCurve_ctor(
    const TColgp_Array1OfPnt &poles,
    const TColStd_Array1OfReal &knots,
    const TColStd_Array1OfInteger &mults,
    int degree,
    bool periodic) {
  Handle(Geom_BSplineCurve) curve = new Geom_BSplineCurve(poles, knots, mults, degree, periodic);
  return std::unique_ptr<HandleGeomBSplineCurve>(new HandleGeomBSplineCurve(curve));
}

inline std::unique_ptr<HandleGeomBSplineCurve> Geom_BSplineCurve_ctor_weighted(
    const TColgp_Array1OfPnt &poles,
    const TColStd_Array1OfReal &weights,
    const TColStd_Array1OfReal &knots,
    const TColStd_Array1OfInteger &mults,
    int degree,
    bool periodic) {
  Handle(Geom_BSplineCurve) curve = new Geom_BSplineCurve(poles, weights, knots, mults, degree, periodic);
  return std::unique_ptr<HandleGeomBSplineCurve>(new HandleGeomBSplineCurve(curve));
}

inline std::unique_ptr<HandleGeomCurve> bspline_curve_to_geom_curve(const HandleGeomBSplineCurve &bspline) {
  Handle(Geom_Curve) curve = bspline;
  return std::unique_ptr<HandleGeomCurve>(new HandleGeomCurve(curve));
}

// ============================================
// BRepBuilderAPI_MakeFace - add interior wire
// ============================================

inline void BRepBuilderAPI_MakeFace_Add(BRepBuilderAPI_MakeFace &maker, const TopoDS_Wire &wire) {
  maker.Add(wire);
}

// BRepBuilderAPI_MakeFace from surface + outer wire
inline std::unique_ptr<BRepBuilderAPI_MakeFace> BRepBuilderAPI_MakeFace_surface_wire(
    const HandleGeomSurface &surface,
    const TopoDS_Wire &wire,
    bool inside) {
  return std::unique_ptr<BRepBuilderAPI_MakeFace>(
    new BRepBuilderAPI_MakeFace(surface, wire, inside));
}

// ============================================
// Geom2d_BSplineCurve construction (for PCurves / trim curves)
// ============================================

inline std::unique_ptr<HandleGeom2d_BSplineCurve> Geom2d_BSplineCurve_ctor(
    const TColgp_Array1OfPnt2d &poles,
    const TColStd_Array1OfReal &knots,
    const TColStd_Array1OfInteger &mults,
    int degree,
    bool periodic) {
  Handle(Geom2d_BSplineCurve) curve = new Geom2d_BSplineCurve(poles, knots, mults, degree, periodic);
  return std::unique_ptr<HandleGeom2d_BSplineCurve>(new HandleGeom2d_BSplineCurve(curve));
}

inline std::unique_ptr<HandleGeom2d_BSplineCurve> Geom2d_BSplineCurve_ctor_weighted(
    const TColgp_Array1OfPnt2d &poles,
    const TColStd_Array1OfReal &weights,
    const TColStd_Array1OfReal &knots,
    const TColStd_Array1OfInteger &mults,
    int degree,
    bool periodic) {
  Handle(Geom2d_BSplineCurve) curve = new Geom2d_BSplineCurve(poles, weights, knots, mults, degree, periodic);
  return std::unique_ptr<HandleGeom2d_BSplineCurve>(new HandleGeom2d_BSplineCurve(curve));
}

inline std::unique_ptr<HandleGeom2d_Curve> bspline2d_curve_to_geom2d_curve(const HandleGeom2d_BSplineCurve &bspline) {
  Handle(Geom2d_Curve) curve = bspline;
  return std::unique_ptr<HandleGeom2d_Curve>(new HandleGeom2d_Curve(curve));
}

// BRepLib::SameParameter - ensure edge consistency after PCurve construction
inline void BRepLib_SameParameter(const TopoDS_Edge &edge, double tolerance) {
  BRepLib::SameParameter(edge, tolerance);
}

// Return a reversed copy of a wire (flips its orientation). Used to turn a hole
// boundary into the opposite winding so MakeFace treats it as a hole, not as
// extra material.
inline std::unique_ptr<TopoDS_Wire> reverse_wire(const TopoDS_Wire &wire) {
  return std::unique_ptr<TopoDS_Wire>(new TopoDS_Wire(TopoDS::Wire(wire.Reversed())));
}

// Repair a shape (add missing p-curves, fix edge consistency, tolerances, …).
// Trimmed faces built from 3D wires alone lack the UV p-curves OCCT needs for
// sewing/booleans/mass-props; ShapeFix_Shape reconstructs them by projection.
inline std::unique_ptr<TopoDS_Shape> shapefix_shape(const TopoDS_Shape &shape,
                                                    double precision) {
  Handle(ShapeFix_Shape) fixer = new ShapeFix_Shape(shape);
  fixer->SetPrecision(precision);
  fixer->Perform();
  return std::unique_ptr<TopoDS_Shape>(new TopoDS_Shape(fixer->Shape()));
}

// Build a solid from a (closed) shell and orient it so material is inside —
// i.e. outward face normals / positive volume. A shell sewn from independent
// faces can come out inverted; BRepLib::OrientClosedSolid fixes the solid so it
// is a valid operand for BRepAlgoAPI_* booleans.
inline std::unique_ptr<TopoDS_Shape> make_oriented_solid(const TopoDS_Shell &shell) {
  BRepBuilderAPI_MakeSolid maker(shell);
  maker.Build();
  TopoDS_Solid solid = maker.Solid();
  BRepLib::OrientClosedSolid(solid);
  return std::unique_ptr<TopoDS_Shape>(new TopoDS_Shape(solid));
}

// BRep_Builder::MakeFace - create an empty face with a surface
inline void BRep_Builder_MakeFace(const BRep_Builder &builder, TopoDS_Face &face,
                                   const HandleGeomSurface &surface, double tolerance) {
  builder.MakeFace(face, surface, tolerance);
}

// BRep_Builder::Add - add a wire to a face
inline void BRep_Builder_Add_Wire(const TopoDS_Builder &builder, TopoDS_Face &face, const TopoDS_Wire &wire) {
  builder.Add(face, wire);
}

// BRep_Builder::UpdateEdge - add a PCurve (2D curve on surface) to an edge
inline void BRep_Builder_UpdateEdge(const BRep_Builder &builder, TopoDS_Edge &edge,
                                     const HandleGeom2d_Curve &pcurve, const TopoDS_Face &face,
                                     double tolerance) {
  builder.UpdateEdge(edge, pcurve, face, tolerance);
}

